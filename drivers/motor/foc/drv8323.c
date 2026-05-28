/****************************************************************************
 * drivers/motor/foc/drv8323.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/arch.h>
#include <nuttx/debug.h>
#include <nuttx/kmalloc.h>
#include <nuttx/nuttx.h>

#include <nuttx/spi/spi.h>

#include <nuttx/motor/foc/drv8323.h>
#include <nuttx/motor/motor_ioctl.h>

#include <errno.h>
#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if FOC_BOARDCFG_GAINLIST_LEN < 4
#  error FOC_BOARDCFG_GAINLIST_LEN < 4 not supported
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* DRV8323 device */

struct drv8323_priv_s
{
  /* Common FOC power-stage driver - must be first */

  struct focpwr_dev_s       dev;

  FAR struct drv8323_ops_s *ops; /* Board ops */

  FAR struct spi_dev_s     *spi; /* SPI device reference */
  FAR struct drv8323_cfg_s  cfg; /* Configuration */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int drv8323_fault_isr(int irq, void *context, void *arg);

static int drv8323_gain_set(FAR struct focpwr_dev_s *dev, int gain);
static int drv8323_gain_get(FAR struct focpwr_dev_s *dev, FAR int *gain);

static int drv8323_setup(FAR struct focpwr_dev_s *dev);
static int drv8323_shutdown(FAR struct focpwr_dev_s *dev);
static int drv8323_calibration(FAR struct focpwr_dev_s *dev, bool state);
static int drv8323_ioctl(FAR struct focpwr_dev_s *dev, int cmd,
                         unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct focpwr_ops_s g_drv8323_ops =
{
  .setup       = drv8323_setup,
  .shutdown    = drv8323_shutdown,
  .calibration = drv8323_calibration,
  .ioctl       = drv8323_ioctl,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: drv8323_lock
 ****************************************************************************/

static void drv8323_lock(FAR struct drv8323_priv_s *priv)
{
  SPI_LOCK(priv->spi, 1);
  SPI_SETBITS(priv->spi, 16);
  SPI_SETMODE(priv->spi, SPIDEV_MODE1);
  SPI_SETFREQUENCY(priv->spi, priv->cfg.freq);
}

/****************************************************************************
 * Name: drv8323_unlock
 ****************************************************************************/

static void drv8323_unlock(FAR struct drv8323_priv_s *priv)
{
  SPI_LOCK(priv->spi, 0);
}

/****************************************************************************
 * Name: drv8323_read
 *
 * Description:
 *   Read an 11-bit register from the DRV8323.  Unlike the DRV8301, the
 *   response is shifted out on SDO during the same 16-bit frame (datasheet
 *   8.5.1.1), so no CS toggle is needed between command and data.
 *
 ****************************************************************************/

static void drv8323_read(FAR struct drv8323_priv_s *priv, uint8_t addr,
                         FAR uint16_t *data)
{
  uint16_t tx = DRV8323_SPI_W_READ |
                (((uint16_t)addr << DRV8323_SPI_ADDR_SHIFT) &
                 DRV8323_SPI_ADDR_MASK);
  uint16_t rx;

  drv8323_lock(priv);
  SPI_SELECT(priv->spi, SPIDEV_MOTOR(priv->dev.devno), true);

  /* SPI_SEND returns the word shifted in from SDO during the same frame */

  rx = (uint16_t)SPI_SEND(priv->spi, tx);

  SPI_SELECT(priv->spi, SPIDEV_MOTOR(priv->dev.devno), false);
  drv8323_unlock(priv);

  *data = rx & DRV8323_SPI_DATA_MASK;
}

/****************************************************************************
 * Name: drv8323_write
 ****************************************************************************/

static void drv8323_write(FAR struct drv8323_priv_s *priv, uint8_t addr,
                          uint16_t data)
{
  uint16_t tx = DRV8323_SPI_W_WRITE |
                (((uint16_t)addr << DRV8323_SPI_ADDR_SHIFT) &
                 DRV8323_SPI_ADDR_MASK) |
                (data & DRV8323_SPI_DATA_MASK);

  drv8323_lock(priv);
  SPI_SELECT(priv->spi, SPIDEV_MOTOR(priv->dev.devno), true);

  SPI_SEND(priv->spi, tx);

  SPI_SELECT(priv->spi, SPIDEV_MOTOR(priv->dev.devno), false);
  drv8323_unlock(priv);
}

/****************************************************************************
 * Name: drv8323_modify
 *
 * Description:
 *   Read-modify-write helper for control registers.  Clears the bits set in
 *   'clr' and then sets the bits in 'set'.
 *
 ****************************************************************************/

static void drv8323_modify(FAR struct drv8323_priv_s *priv, uint8_t addr,
                           uint16_t clr, uint16_t set)
{
  uint16_t regval = 0;

  drv8323_read(priv, addr, &regval);
  regval &= ~(clr & DRV8323_SPI_DATA_MASK);
  regval |= (set & DRV8323_SPI_DATA_MASK);
  drv8323_write(priv, addr, regval);
}

/****************************************************************************
 * Name: drv8323_fault_isr
 ****************************************************************************/

static int drv8323_fault_isr(int irq, FAR void *context, void *arg)
{
  FAR struct drv8323_priv_s *priv = (FAR struct drv8323_priv_s *)arg;

  DEBUGASSERT(priv != NULL);
  priv->ops->fault_handle(&priv->dev);
  return OK;
}

/****************************************************************************
 * Name: drv8323_compose_drvctl
 ****************************************************************************/

static uint16_t drv8323_compose_drvctl(FAR const struct drv8323_cfg_s *cfg)
{
  uint16_t v = 0;

  v |= DRV8323_DRVCTL_PWMMODE(cfg->pwm_mode);
  if (cfg->dis_cpuv)    v |= DRV8323_DRVCTL_DIS_CPUV;
  if (cfg->dis_gdf)     v |= DRV8323_DRVCTL_DIS_GDF;
  if (cfg->otw_rep)     v |= DRV8323_DRVCTL_OTW_REP;
  if (cfg->onepwm_com)  v |= DRV8323_DRVCTL_1PWM_COM;
  if (cfg->onepwm_dir)  v |= DRV8323_DRVCTL_1PWM_DIR;
  return v;
}

/****************************************************************************
 * Name: drv8323_compose_gdhs
 ****************************************************************************/

static uint16_t drv8323_compose_gdhs(FAR const struct drv8323_cfg_s *cfg)
{
  uint16_t v = 0;

  v |= DRV8323_GDHS_LOCK_UNLOCKED;
  v |= DRV8323_GDHS_IDRIVEP_HS(cfg->idrivep_hs);
  v |= DRV8323_GDHS_IDRIVEN_HS(cfg->idriven_hs);
  return v;
}

/****************************************************************************
 * Name: drv8323_compose_gdls
 ****************************************************************************/

static uint16_t drv8323_compose_gdls(FAR const struct drv8323_cfg_s *cfg)
{
  uint16_t v = 0;

  if (cfg->cbc) v |= DRV8323_GDLS_CBC;
  v |= DRV8323_GDLS_TDRIVE(cfg->tdrive);
  v |= DRV8323_GDLS_IDRIVEP_LS(cfg->idrivep_ls);
  v |= DRV8323_GDLS_IDRIVEN_LS(cfg->idriven_ls);
  return v;
}

/****************************************************************************
 * Name: drv8323_compose_ocp
 ****************************************************************************/

static uint16_t drv8323_compose_ocp(FAR const struct drv8323_cfg_s *cfg)
{
  uint16_t v = 0;

  if (cfg->tretry) v |= DRV8323_OCP_TRETRY;
  v |= DRV8323_OCP_DEADTIME(cfg->dead_time);
  v |= DRV8323_OCP_OCPMODE(cfg->ocp_mode);
  v |= DRV8323_OCP_OCPDEG(cfg->ocp_deg);
  v |= DRV8323_OCP_VDSLVL(cfg->vds_lvl);
  return v;
}

/****************************************************************************
 * Name: drv8323_compose_csa
 ****************************************************************************/

static uint16_t drv8323_compose_csa(FAR const struct drv8323_cfg_s *cfg)
{
  uint16_t v = 0;

  if (cfg->csa_fet)   v |= DRV8323_CSACTL_CSA_FET;
  if (cfg->vref_div)  v |= DRV8323_CSACTL_VREFDIV;
  if (cfg->ls_ref)    v |= DRV8323_CSACTL_LSREF;
  v |= DRV8323_CSACTL_GAIN(cfg->csa_gain);
  if (cfg->dis_sen)   v |= DRV8323_CSACTL_DIS_SEN;
  v |= DRV8323_CSACTL_SENLVL(cfg->sen_lvl);
  return v;
}

/****************************************************************************
 * Name: drv8323_setup
 ****************************************************************************/

static int drv8323_setup(FAR struct focpwr_dev_s *dev)
{
  FAR struct drv8323_priv_s *priv = (FAR struct drv8323_priv_s *)dev;
  uint16_t                   tmp  = 0;
  int                        ret  = OK;

  /* Toggle ENABLE: short low pulse forces the device through a clean
   * power-up sequence and clears any latent fault that survived a reset.
   * tWAKE is < 1 ms; give 2 ms of margin.
   */

  priv->ops->gate_enable(dev, false);
  up_mdelay(2);
  priv->ops->gate_enable(dev, true);
  up_mdelay(2);

  /* Attach nFAULT handler */

  priv->ops->fault_attach(dev, drv8323_fault_isr, priv);

  /* Drain power-up status registers (they're read-only and self-clear on
   * read once the underlying condition is gone).
   */

  drv8323_read(priv, DRV8323_REG_FAULT_STATUS1, &tmp);
  drv8323_read(priv, DRV8323_REG_VGS_STATUS2,   &tmp);

  /* Program control registers from the board-provided cfg.  Write
   * DRIVER_CONTROL first (with CLR_FLT to wipe any latched faults) and
   * GATE_DRIVE_HS with LOCK=011b so subsequent writes go through.
   */

  drv8323_write(priv, DRV8323_REG_DRIVER_CONTROL,
                drv8323_compose_drvctl(&priv->cfg) | DRV8323_DRVCTL_CLR_FLT);

  drv8323_write(priv, DRV8323_REG_GATE_DRIVE_HS,
                drv8323_compose_gdhs(&priv->cfg));

  drv8323_write(priv, DRV8323_REG_GATE_DRIVE_LS,
                drv8323_compose_gdls(&priv->cfg));

  drv8323_write(priv, DRV8323_REG_OCP_CONTROL,
                drv8323_compose_ocp(&priv->cfg));

  drv8323_write(priv, DRV8323_REG_CSA_CONTROL,
                drv8323_compose_csa(&priv->cfg));

  return ret;
}

/****************************************************************************
 * Name: drv8323_shutdown
 ****************************************************************************/

static int drv8323_shutdown(FAR struct focpwr_dev_s *dev)
{
  FAR struct drv8323_priv_s *priv = (FAR struct drv8323_priv_s *)dev;

  /* Disable chip */

  priv->ops->gate_enable(dev, false);

  /* Detach nFAULT IRQ */

  priv->ops->fault_attach(dev, NULL, NULL);

  return OK;
}

/****************************************************************************
 * Name: drv8323_gain_get
 ****************************************************************************/

static int drv8323_gain_get(FAR struct focpwr_dev_s *dev, FAR int *gain)
{
  FAR struct drv8323_priv_s *priv = (FAR struct drv8323_priv_s *)dev;
  uint16_t                   csa  = 0;
  int                        ret  = OK;

  drv8323_read(priv, DRV8323_REG_CSA_CONTROL, &csa);

  switch (csa & DRV8323_CSACTL_GAIN_MASK)
    {
      case DRV8323_CSACTL_GAIN_5:
        *gain = 5;
        break;

      case DRV8323_CSACTL_GAIN_10:
        *gain = 10;
        break;

      case DRV8323_CSACTL_GAIN_20:
        *gain = 20;
        break;

      case DRV8323_CSACTL_GAIN_40:
        *gain = 40;
        break;

      default:
        ret = -EINVAL;
        break;
    }

  return ret;
}

/****************************************************************************
 * Name: drv8323_gain_set
 ****************************************************************************/

static int drv8323_gain_set(FAR struct focpwr_dev_s *dev, int gain)
{
  FAR struct drv8323_priv_s *priv = (FAR struct drv8323_priv_s *)dev;
  uint16_t                   set;

  switch (gain)
    {
      case 5:
        set = DRV8323_CSACTL_GAIN_5;
        break;

      case 10:
        set = DRV8323_CSACTL_GAIN_10;
        break;

      case 20:
        set = DRV8323_CSACTL_GAIN_20;
        break;

      case 40:
        set = DRV8323_CSACTL_GAIN_40;
        break;

      default:
        return -EINVAL;
    }

  drv8323_modify(priv, DRV8323_REG_CSA_CONTROL,
                 DRV8323_CSACTL_GAIN_MASK, set);
  return OK;
}

/****************************************************************************
 * Name: drv8323_calibration
 *
 * Description:
 *   Honour the focpwr_ops_s::calibration contract: when 'state' is true,
 *   short all three CSA inputs to capture their offset; when false, return
 *   to normal operation.
 *
 ****************************************************************************/

static int drv8323_calibration(FAR struct focpwr_dev_s *dev, bool state)
{
  FAR struct drv8323_priv_s *priv = (FAR struct drv8323_priv_s *)dev;

  if (state)
    {
      drv8323_modify(priv, DRV8323_REG_CSA_CONTROL,
                     0, DRV8323_CSACTL_CSA_CAL_ALL);
    }
  else
    {
      drv8323_modify(priv, DRV8323_REG_CSA_CONTROL,
                     DRV8323_CSACTL_CSA_CAL_ALL, 0);
    }

  return OK;
}

/****************************************************************************
 * Name: drv8323_ioc_set_idrive
 ****************************************************************************/

static int drv8323_ioc_set_idrive(FAR struct drv8323_priv_s *priv,
                                  FAR const struct drv8323_idrive_s *id)
{
  drv8323_modify(priv, DRV8323_REG_GATE_DRIVE_HS,
                 DRV8323_GDHS_IDRIVEP_HS_MASK |
                   DRV8323_GDHS_IDRIVEN_HS_MASK,
                 DRV8323_GDHS_IDRIVEP_HS(id->idrivep_hs) |
                   DRV8323_GDHS_IDRIVEN_HS(id->idriven_hs));

  drv8323_modify(priv, DRV8323_REG_GATE_DRIVE_LS,
                 DRV8323_GDLS_IDRIVEP_LS_MASK |
                   DRV8323_GDLS_IDRIVEN_LS_MASK,
                 DRV8323_GDLS_IDRIVEP_LS(id->idrivep_ls) |
                   DRV8323_GDLS_IDRIVEN_LS(id->idriven_ls));
  return OK;
}

/****************************************************************************
 * Name: drv8323_ioc_get_idrive
 ****************************************************************************/

static int drv8323_ioc_get_idrive(FAR struct drv8323_priv_s *priv,
                                  FAR struct drv8323_idrive_s *id)
{
  uint16_t hs = 0;
  uint16_t ls = 0;

  drv8323_read(priv, DRV8323_REG_GATE_DRIVE_HS, &hs);
  drv8323_read(priv, DRV8323_REG_GATE_DRIVE_LS, &ls);

  id->idrivep_hs = (hs & DRV8323_GDHS_IDRIVEP_HS_MASK) >>
                   DRV8323_GDHS_IDRIVEP_HS_SHIFT;
  id->idriven_hs = (hs & DRV8323_GDHS_IDRIVEN_HS_MASK) >>
                   DRV8323_GDHS_IDRIVEN_HS_SHIFT;
  id->idrivep_ls = (ls & DRV8323_GDLS_IDRIVEP_LS_MASK) >>
                   DRV8323_GDLS_IDRIVEP_LS_SHIFT;
  id->idriven_ls = (ls & DRV8323_GDLS_IDRIVEN_LS_MASK) >>
                   DRV8323_GDLS_IDRIVEN_LS_SHIFT;
  return OK;
}

/****************************************************************************
 * Name: drv8323_ioc_csa_phase_cal
 ****************************************************************************/

static int drv8323_ioc_csa_phase_cal(FAR struct drv8323_priv_s *priv,
                                     FAR const struct drv8323_csa_cal_s *c)
{
  uint16_t set = 0;

  if (c->ch_a) set |= DRV8323_CSACTL_CSA_CAL_A;
  if (c->ch_b) set |= DRV8323_CSACTL_CSA_CAL_B;
  if (c->ch_c) set |= DRV8323_CSACTL_CSA_CAL_C;

  drv8323_modify(priv, DRV8323_REG_CSA_CONTROL,
                 DRV8323_CSACTL_CSA_CAL_ALL, set);
  return OK;
}

/****************************************************************************
 * Name: drv8323_ioctl
 ****************************************************************************/

static int drv8323_ioctl(FAR struct focpwr_dev_s *dev, int cmd,
                         unsigned long arg)
{
  FAR struct drv8323_priv_s *priv = (FAR struct drv8323_priv_s *)dev;
  int                        ret  = OK;

  switch (cmd)
    {
      case MTRIOC_SET_BOARDCFG:
        {
          FAR struct foc_set_boardcfg_s *cfg =
            (FAR struct foc_set_boardcfg_s *)arg;

          ret = drv8323_gain_set(dev, cfg->gain);
          break;
        }

      case MTRIOC_GET_BOARDCFG:
        {
          FAR struct foc_get_boardcfg_s *cfg =
            (FAR struct foc_get_boardcfg_s *)arg;

          ret = drv8323_gain_get(dev, &cfg->gain);

          cfg->gain_list[0] = 5;
          cfg->gain_list[1] = 10;
          cfg->gain_list[2] = 20;
          cfg->gain_list[3] = 40;
          break;
        }

      case DRV8323IOC_SET_IDRIVE:
        {
          FAR const struct drv8323_idrive_s *id =
            (FAR const struct drv8323_idrive_s *)arg;

          ret = drv8323_ioc_set_idrive(priv, id);
          break;
        }

      case DRV8323IOC_GET_IDRIVE:
        {
          FAR struct drv8323_idrive_s *id =
            (FAR struct drv8323_idrive_s *)arg;

          ret = drv8323_ioc_get_idrive(priv, id);
          break;
        }

      case DRV8323IOC_SET_VDS_LVL:
        {
          int lvl = (int)arg;

          if (lvl < 0 || lvl > 0xf)
            {
              ret = -EINVAL;
              break;
            }

          drv8323_modify(priv, DRV8323_REG_OCP_CONTROL,
                         DRV8323_OCP_VDSLVL_MASK,
                         DRV8323_OCP_VDSLVL(lvl));
          break;
        }

      case DRV8323IOC_GET_VDS_LVL:
        {
          FAR int *out = (FAR int *)arg;
          uint16_t  ocp = 0;

          drv8323_read(priv, DRV8323_REG_OCP_CONTROL, &ocp);
          *out = (int)((ocp & DRV8323_OCP_VDSLVL_MASK) >>
                       DRV8323_OCP_VDSLVL_SHIFT);
          break;
        }

      case DRV8323IOC_SET_OCP_MODE:
        {
          int mode = (int)arg;

          if (mode < 0 || mode > 0x3)
            {
              ret = -EINVAL;
              break;
            }

          drv8323_modify(priv, DRV8323_REG_OCP_CONTROL,
                         DRV8323_OCP_OCPMODE_MASK,
                         DRV8323_OCP_OCPMODE(mode));
          break;
        }

      case DRV8323IOC_GET_OCP_MODE:
        {
          FAR int *out = (FAR int *)arg;
          uint16_t  ocp = 0;

          drv8323_read(priv, DRV8323_REG_OCP_CONTROL, &ocp);
          *out = (int)((ocp & DRV8323_OCP_OCPMODE_MASK) >>
                       DRV8323_OCP_OCPMODE_SHIFT);
          break;
        }

      case DRV8323IOC_CSA_PHASE_CAL:
        {
          FAR const struct drv8323_csa_cal_s *c =
            (FAR const struct drv8323_csa_cal_s *)arg;

          ret = drv8323_ioc_csa_phase_cal(priv, c);
          break;
        }

      case DRV8323IOC_GET_FAULTS:
        {
          FAR struct drv8323_faults_s *f =
            (FAR struct drv8323_faults_s *)arg;

          drv8323_read(priv, DRV8323_REG_FAULT_STATUS1, &f->fs1);
          drv8323_read(priv, DRV8323_REG_VGS_STATUS2,   &f->fs2);
          break;
        }

      case DRV8323IOC_CLR_FAULTS:
        {
          /* CLR_FLT self-clears after the write completes. */

          drv8323_modify(priv, DRV8323_REG_DRIVER_CONTROL,
                         0, DRV8323_DRVCTL_CLR_FLT);
          break;
        }

      case DRV8323IOC_LOCK:
        {
          int lock = (int)arg;
          uint16_t set = lock ? DRV8323_GDHS_LOCK_LOCKED
                              : DRV8323_GDHS_LOCK_UNLOCKED;

          drv8323_modify(priv, DRV8323_REG_GATE_DRIVE_HS,
                         DRV8323_GDHS_LOCK_MASK, set);
          break;
        }

      default:
        {
          ret = -ENOTTY;
          break;
        }
    }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: drv8323_register
 ****************************************************************************/

int drv8323_register(FAR const char *path,
                     FAR struct foc_dev_s *dev,
                     FAR struct drv8323_board_s *board)
{
  FAR struct drv8323_priv_s *priv = NULL;
  int                        ret  = OK;

  DEBUGASSERT(path  != NULL);
  DEBUGASSERT(dev   != NULL);
  DEBUGASSERT(board != NULL);
  DEBUGASSERT(board->spi != NULL);
  DEBUGASSERT(board->ops != NULL);
  DEBUGASSERT(board->cfg != NULL);

  /* Allocate driver */

  priv = kmm_zalloc(sizeof(struct drv8323_priv_s));
  if (priv == NULL)
    {
      return -ENOMEM;
    }

  /* Register FOC device */

  ret = foc_register(path, dev);
  if (ret < 0)
    {
      kmm_free(priv);
      return ret;
    }

  /* Store board data */

  priv->ops = board->ops;
  priv->spi = board->spi;

  /* Store configuration snapshot */

  memcpy(&priv->cfg, board->cfg, sizeof(struct drv8323_cfg_s));

  /* Initialize FOC power stage */

  return focpwr_initialize(&priv->dev, board->devno, dev, &g_drv8323_ops);
}
