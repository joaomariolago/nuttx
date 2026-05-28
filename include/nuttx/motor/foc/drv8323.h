/****************************************************************************
 * include/nuttx/motor/foc/drv8323.h
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

#ifndef __INCLUDE_NUTTX_MOTOR_FOC_DRV8323_H
#define __INCLUDE_NUTTX_MOTOR_FOC_DRV8323_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/fs/ioctl.h>
#include <nuttx/motor/foc/foc_pwr.h>
#include <nuttx/motor/motor_ioctl.h>

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Registers (4-bit addresses, MSB of 11-bit data is bit 10) */

#define DRV8323_REG_FAULT_STATUS1       (0x00)    /* Fault Status 1 (R) */
#define DRV8323_REG_VGS_STATUS2         (0x01)    /* VGS Status 2 (R) */
#define DRV8323_REG_DRIVER_CONTROL      (0x02)    /* Driver Control (R/W) */
#define DRV8323_REG_GATE_DRIVE_HS       (0x03)    /* Gate Drive HS (R/W) */
#define DRV8323_REG_GATE_DRIVE_LS       (0x04)    /* Gate Drive LS (R/W) */
#define DRV8323_REG_OCP_CONTROL         (0x05)    /* OCP Control (R/W) */
#define DRV8323_REG_CSA_CONTROL         (0x06)    /* CSA Control (R/W) */

/* SPI frame helpers (16-bit: W[15] | A[14:11] | D[10:0]) */

#define DRV8323_SPI_W_READ              (1u << 15)
#define DRV8323_SPI_W_WRITE             (0u << 15)
#define DRV8323_SPI_ADDR_SHIFT          (11)
#define DRV8323_SPI_ADDR_MASK           (0xfu << DRV8323_SPI_ADDR_SHIFT)
#define DRV8323_SPI_DATA_MASK           (0x7ffu)

/* Fault Status Register 1 (0x00, read-only) */

#define DRV8323_FS1_VDS_LC              (1u << 0)  /* VDS OC fault on C low-side  */
#define DRV8323_FS1_VDS_HC              (1u << 1)  /* VDS OC fault on C high-side */
#define DRV8323_FS1_VDS_LB              (1u << 2)  /* VDS OC fault on B low-side  */
#define DRV8323_FS1_VDS_HB              (1u << 3)  /* VDS OC fault on B high-side */
#define DRV8323_FS1_VDS_LA              (1u << 4)  /* VDS OC fault on A low-side  */
#define DRV8323_FS1_VDS_HA              (1u << 5)  /* VDS OC fault on A high-side */
#define DRV8323_FS1_OTSD                (1u << 6)  /* Overtemperature shutdown */
#define DRV8323_FS1_UVLO                (1u << 7)  /* VM undervoltage lockout */
#define DRV8323_FS1_GDF                 (1u << 8)  /* Gate drive fault */
#define DRV8323_FS1_VDS_OCP             (1u << 9)  /* VDS OC fault (any) */
#define DRV8323_FS1_FAULT               (1u << 10) /* OR of all fault bits */

/* VGS Status Register 2 (0x01, read-only) */

#define DRV8323_FS2_VGS_LC              (1u << 0)  /* Gate fault C low-side  */
#define DRV8323_FS2_VGS_HC              (1u << 1)  /* Gate fault C high-side */
#define DRV8323_FS2_VGS_LB              (1u << 2)  /* Gate fault B low-side  */
#define DRV8323_FS2_VGS_HB              (1u << 3)  /* Gate fault B high-side */
#define DRV8323_FS2_VGS_LA              (1u << 4)  /* Gate fault A low-side  */
#define DRV8323_FS2_VGS_HA              (1u << 5)  /* Gate fault A high-side */
#define DRV8323_FS2_CPUV                (1u << 6)  /* Charge-pump undervoltage */
#define DRV8323_FS2_OTW                 (1u << 7)  /* Overtemperature warning */
#define DRV8323_FS2_SC_OC               (1u << 8)  /* Sense OC on phase C */
#define DRV8323_FS2_SB_OC               (1u << 9)  /* Sense OC on phase B */
#define DRV8323_FS2_SA_OC               (1u << 10) /* Sense OC on phase A */

/* Driver Control Register (0x02, R/W) */

#define DRV8323_DRVCTL_CLR_FLT          (1u << 0)  /* Clear latched fault bits */
#define DRV8323_DRVCTL_BRAKE            (1u << 1)  /* Brake (1x PWM mode) */
#define DRV8323_DRVCTL_COAST            (1u << 2)  /* Hi-Z all MOSFETs */
#define DRV8323_DRVCTL_1PWM_DIR         (1u << 3)  /* 1x PWM direction */
#define DRV8323_DRVCTL_1PWM_COM         (1u << 4)  /* 1x PWM async rectification */
#define DRV8323_DRVCTL_PWMMODE_SHIFT    (5)
#define DRV8323_DRVCTL_PWMMODE_MASK     (0x3u << DRV8323_DRVCTL_PWMMODE_SHIFT)
#  define DRV8323_DRVCTL_PWMMODE_6X     (0x0u << DRV8323_DRVCTL_PWMMODE_SHIFT)
#  define DRV8323_DRVCTL_PWMMODE_3X     (0x1u << DRV8323_DRVCTL_PWMMODE_SHIFT)
#  define DRV8323_DRVCTL_PWMMODE_1X     (0x2u << DRV8323_DRVCTL_PWMMODE_SHIFT)
#  define DRV8323_DRVCTL_PWMMODE_IND    (0x3u << DRV8323_DRVCTL_PWMMODE_SHIFT)
#define DRV8323_DRVCTL_PWMMODE(x)       (((x) << DRV8323_DRVCTL_PWMMODE_SHIFT) & \
                                         DRV8323_DRVCTL_PWMMODE_MASK)
#define DRV8323_DRVCTL_OTW_REP          (1u << 7)  /* Report OTW on nFAULT */
#define DRV8323_DRVCTL_DIS_GDF          (1u << 8)  /* Disable gate-drive fault */
#define DRV8323_DRVCTL_DIS_CPUV         (1u << 9)  /* Disable charge-pump UVLO */
/* bit 10 reserved */

/* Gate Drive HS Register (0x03, R/W) */

#define DRV8323_GDHS_IDRIVEN_HS_SHIFT   (0)
#define DRV8323_GDHS_IDRIVEN_HS_MASK    (0xfu << DRV8323_GDHS_IDRIVEN_HS_SHIFT)
#define DRV8323_GDHS_IDRIVEN_HS(x)      (((x) << DRV8323_GDHS_IDRIVEN_HS_SHIFT) & \
                                         DRV8323_GDHS_IDRIVEN_HS_MASK)
#define DRV8323_GDHS_IDRIVEP_HS_SHIFT   (4)
#define DRV8323_GDHS_IDRIVEP_HS_MASK    (0xfu << DRV8323_GDHS_IDRIVEP_HS_SHIFT)
#define DRV8323_GDHS_IDRIVEP_HS(x)      (((x) << DRV8323_GDHS_IDRIVEP_HS_SHIFT) & \
                                         DRV8323_GDHS_IDRIVEP_HS_MASK)
#define DRV8323_GDHS_LOCK_SHIFT         (8)
#define DRV8323_GDHS_LOCK_MASK          (0x7u << DRV8323_GDHS_LOCK_SHIFT)
#  define DRV8323_GDHS_LOCK_UNLOCKED    (0x3u << DRV8323_GDHS_LOCK_SHIFT)
#  define DRV8323_GDHS_LOCK_LOCKED      (0x6u << DRV8323_GDHS_LOCK_SHIFT)

/* Gate Drive LS Register (0x04, R/W) */

#define DRV8323_GDLS_IDRIVEN_LS_SHIFT   (0)
#define DRV8323_GDLS_IDRIVEN_LS_MASK    (0xfu << DRV8323_GDLS_IDRIVEN_LS_SHIFT)
#define DRV8323_GDLS_IDRIVEN_LS(x)      (((x) << DRV8323_GDLS_IDRIVEN_LS_SHIFT) & \
                                         DRV8323_GDLS_IDRIVEN_LS_MASK)
#define DRV8323_GDLS_IDRIVEP_LS_SHIFT   (4)
#define DRV8323_GDLS_IDRIVEP_LS_MASK    (0xfu << DRV8323_GDLS_IDRIVEP_LS_SHIFT)
#define DRV8323_GDLS_IDRIVEP_LS(x)      (((x) << DRV8323_GDLS_IDRIVEP_LS_SHIFT) & \
                                         DRV8323_GDLS_IDRIVEP_LS_MASK)
#define DRV8323_GDLS_TDRIVE_SHIFT       (8)
#define DRV8323_GDLS_TDRIVE_MASK        (0x3u << DRV8323_GDLS_TDRIVE_SHIFT)
#  define DRV8323_GDLS_TDRIVE_500NS     (0x0u << DRV8323_GDLS_TDRIVE_SHIFT)
#  define DRV8323_GDLS_TDRIVE_1US       (0x1u << DRV8323_GDLS_TDRIVE_SHIFT)
#  define DRV8323_GDLS_TDRIVE_2US       (0x2u << DRV8323_GDLS_TDRIVE_SHIFT)
#  define DRV8323_GDLS_TDRIVE_4US       (0x3u << DRV8323_GDLS_TDRIVE_SHIFT)
#define DRV8323_GDLS_TDRIVE(x)          (((x) << DRV8323_GDLS_TDRIVE_SHIFT) & \
                                         DRV8323_GDLS_TDRIVE_MASK)
#define DRV8323_GDLS_CBC                (1u << 10) /* Cycle-by-cycle OC retry */

/* OCP Control Register (0x05, R/W) */

#define DRV8323_OCP_VDSLVL_SHIFT        (0)
#define DRV8323_OCP_VDSLVL_MASK         (0xfu << DRV8323_OCP_VDSLVL_SHIFT)
#  define DRV8323_OCP_VDSLVL_0V06       (0x0u)
#  define DRV8323_OCP_VDSLVL_0V13       (0x1u)
#  define DRV8323_OCP_VDSLVL_0V20       (0x2u)
#  define DRV8323_OCP_VDSLVL_0V26       (0x3u)
#  define DRV8323_OCP_VDSLVL_0V31       (0x4u)
#  define DRV8323_OCP_VDSLVL_0V45       (0x5u)
#  define DRV8323_OCP_VDSLVL_0V53       (0x6u)
#  define DRV8323_OCP_VDSLVL_0V60       (0x7u)
#  define DRV8323_OCP_VDSLVL_0V68       (0x8u)
#  define DRV8323_OCP_VDSLVL_0V75       (0x9u)
#  define DRV8323_OCP_VDSLVL_0V94       (0xau)
#  define DRV8323_OCP_VDSLVL_1V13       (0xbu)
#  define DRV8323_OCP_VDSLVL_1V30       (0xcu)
#  define DRV8323_OCP_VDSLVL_1V50       (0xdu)
#  define DRV8323_OCP_VDSLVL_1V70       (0xeu)
#  define DRV8323_OCP_VDSLVL_1V88       (0xfu)
#define DRV8323_OCP_VDSLVL(x)           (((x) << DRV8323_OCP_VDSLVL_SHIFT) & \
                                         DRV8323_OCP_VDSLVL_MASK)
#define DRV8323_OCP_OCPDEG_SHIFT        (4)
#define DRV8323_OCP_OCPDEG_MASK         (0x3u << DRV8323_OCP_OCPDEG_SHIFT)
#  define DRV8323_OCP_OCPDEG_2US        (0x0u << DRV8323_OCP_OCPDEG_SHIFT)
#  define DRV8323_OCP_OCPDEG_4US        (0x1u << DRV8323_OCP_OCPDEG_SHIFT)
#  define DRV8323_OCP_OCPDEG_6US        (0x2u << DRV8323_OCP_OCPDEG_SHIFT)
#  define DRV8323_OCP_OCPDEG_8US        (0x3u << DRV8323_OCP_OCPDEG_SHIFT)
#define DRV8323_OCP_OCPDEG(x)           (((x) << DRV8323_OCP_OCPDEG_SHIFT) & \
                                         DRV8323_OCP_OCPDEG_MASK)
#define DRV8323_OCP_OCPMODE_SHIFT       (6)
#define DRV8323_OCP_OCPMODE_MASK        (0x3u << DRV8323_OCP_OCPMODE_SHIFT)
#  define DRV8323_OCP_OCPMODE_LATCH     (0x0u << DRV8323_OCP_OCPMODE_SHIFT)
#  define DRV8323_OCP_OCPMODE_RETRY     (0x1u << DRV8323_OCP_OCPMODE_SHIFT)
#  define DRV8323_OCP_OCPMODE_REPORT    (0x2u << DRV8323_OCP_OCPMODE_SHIFT)
#  define DRV8323_OCP_OCPMODE_DISABLED  (0x3u << DRV8323_OCP_OCPMODE_SHIFT)
#define DRV8323_OCP_OCPMODE(x)          (((x) << DRV8323_OCP_OCPMODE_SHIFT) & \
                                         DRV8323_OCP_OCPMODE_MASK)
#define DRV8323_OCP_DEADTIME_SHIFT      (8)
#define DRV8323_OCP_DEADTIME_MASK       (0x3u << DRV8323_OCP_DEADTIME_SHIFT)
#  define DRV8323_OCP_DEADTIME_50NS     (0x0u << DRV8323_OCP_DEADTIME_SHIFT)
#  define DRV8323_OCP_DEADTIME_100NS    (0x1u << DRV8323_OCP_DEADTIME_SHIFT)
#  define DRV8323_OCP_DEADTIME_200NS    (0x2u << DRV8323_OCP_DEADTIME_SHIFT)
#  define DRV8323_OCP_DEADTIME_400NS    (0x3u << DRV8323_OCP_DEADTIME_SHIFT)
#define DRV8323_OCP_DEADTIME(x)         (((x) << DRV8323_OCP_DEADTIME_SHIFT) & \
                                         DRV8323_OCP_DEADTIME_MASK)
#define DRV8323_OCP_TRETRY              (1u << 10) /* 0 = 4 ms, 1 = 50 us */

/* CSA Control Register (0x06, R/W) */

#define DRV8323_CSACTL_SENLVL_SHIFT     (0)
#define DRV8323_CSACTL_SENLVL_MASK      (0x3u << DRV8323_CSACTL_SENLVL_SHIFT)
#  define DRV8323_CSACTL_SENLVL_0V25    (0x0u << DRV8323_CSACTL_SENLVL_SHIFT)
#  define DRV8323_CSACTL_SENLVL_0V50    (0x1u << DRV8323_CSACTL_SENLVL_SHIFT)
#  define DRV8323_CSACTL_SENLVL_0V75    (0x2u << DRV8323_CSACTL_SENLVL_SHIFT)
#  define DRV8323_CSACTL_SENLVL_1V00    (0x3u << DRV8323_CSACTL_SENLVL_SHIFT)
#define DRV8323_CSACTL_SENLVL(x)        (((x) << DRV8323_CSACTL_SENLVL_SHIFT) & \
                                         DRV8323_CSACTL_SENLVL_MASK)
#define DRV8323_CSACTL_CSA_CAL_C        (1u << 2)
#define DRV8323_CSACTL_CSA_CAL_B        (1u << 3)
#define DRV8323_CSACTL_CSA_CAL_A        (1u << 4)
#define DRV8323_CSACTL_CSA_CAL_ALL      (DRV8323_CSACTL_CSA_CAL_A | \
                                         DRV8323_CSACTL_CSA_CAL_B | \
                                         DRV8323_CSACTL_CSA_CAL_C)
#define DRV8323_CSACTL_DIS_SEN          (1u << 5)
#define DRV8323_CSACTL_GAIN_SHIFT       (6)
#define DRV8323_CSACTL_GAIN_MASK        (0x3u << DRV8323_CSACTL_GAIN_SHIFT)
#  define DRV8323_CSACTL_GAIN_5         (0x0u << DRV8323_CSACTL_GAIN_SHIFT)
#  define DRV8323_CSACTL_GAIN_10        (0x1u << DRV8323_CSACTL_GAIN_SHIFT)
#  define DRV8323_CSACTL_GAIN_20        (0x2u << DRV8323_CSACTL_GAIN_SHIFT)
#  define DRV8323_CSACTL_GAIN_40        (0x3u << DRV8323_CSACTL_GAIN_SHIFT)
#define DRV8323_CSACTL_GAIN(x)          (((x) << DRV8323_CSACTL_GAIN_SHIFT) & \
                                         DRV8323_CSACTL_GAIN_MASK)
#define DRV8323_CSACTL_LSREF            (1u << 8)
#define DRV8323_CSACTL_VREFDIV          (1u << 9)
#define DRV8323_CSACTL_CSA_FET          (1u << 10)

/* DRV8323-specific IOCTL commands (offset from generic MTRIOC commands).
 * Values must not clash with motor_ioctl.h's MTRIOC_* (1..17).
 */

#define DRV8323_IOC_BASE                (100)
#define DRV8323IOC_SET_IDRIVE           _MTRIOC(DRV8323_IOC_BASE + 1)
#define DRV8323IOC_GET_IDRIVE           _MTRIOC(DRV8323_IOC_BASE + 2)
#define DRV8323IOC_SET_VDS_LVL          _MTRIOC(DRV8323_IOC_BASE + 3)
#define DRV8323IOC_GET_VDS_LVL          _MTRIOC(DRV8323_IOC_BASE + 4)
#define DRV8323IOC_SET_OCP_MODE         _MTRIOC(DRV8323_IOC_BASE + 5)
#define DRV8323IOC_GET_OCP_MODE         _MTRIOC(DRV8323_IOC_BASE + 6)
#define DRV8323IOC_CSA_PHASE_CAL        _MTRIOC(DRV8323_IOC_BASE + 7)
#define DRV8323IOC_GET_FAULTS           _MTRIOC(DRV8323_IOC_BASE + 8)
#define DRV8323IOC_CLR_FAULTS           _MTRIOC(DRV8323_IOC_BASE + 9)
#define DRV8323IOC_LOCK                 _MTRIOC(DRV8323_IOC_BASE + 10)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* PWM input mode (DRIVER_CONTROL.PWM_MODE) */

enum drv8323_pwm_mode_e
{
  DRV8323_PWM_6X          = 0,
  DRV8323_PWM_3X          = 1,
  DRV8323_PWM_1X          = 2,
  DRV8323_PWM_INDEPENDENT = 3,
};

/* Current sense amplifier gain (CSA_CONTROL.CSA_GAIN) */

enum drv8323_csa_gain_e
{
  DRV8323_GAIN_5  = 0,
  DRV8323_GAIN_10 = 1,
  DRV8323_GAIN_20 = 2,
  DRV8323_GAIN_40 = 3,
};

/* IDRIVE source levels (used for IDRIVEP_HS and IDRIVEP_LS) */

enum drv8323_idrivep_e
{
  DRV8323_IDRIVEP_10MA   = 0x0,
  DRV8323_IDRIVEP_30MA   = 0x1,
  DRV8323_IDRIVEP_60MA   = 0x2,
  DRV8323_IDRIVEP_80MA   = 0x3,
  DRV8323_IDRIVEP_120MA  = 0x4,
  DRV8323_IDRIVEP_140MA  = 0x5,
  DRV8323_IDRIVEP_170MA  = 0x6,
  DRV8323_IDRIVEP_190MA  = 0x7,
  DRV8323_IDRIVEP_260MA  = 0x8,
  DRV8323_IDRIVEP_330MA  = 0x9,
  DRV8323_IDRIVEP_370MA  = 0xa,
  DRV8323_IDRIVEP_440MA  = 0xb,
  DRV8323_IDRIVEP_570MA  = 0xc,
  DRV8323_IDRIVEP_680MA  = 0xd,
  DRV8323_IDRIVEP_820MA  = 0xe,
  DRV8323_IDRIVEP_1000MA = 0xf,
};

/* IDRIVE sink levels (used for IDRIVEN_HS and IDRIVEN_LS) */

enum drv8323_idriven_e
{
  DRV8323_IDRIVEN_20MA   = 0x0,
  DRV8323_IDRIVEN_60MA   = 0x1,
  DRV8323_IDRIVEN_120MA  = 0x2,
  DRV8323_IDRIVEN_160MA  = 0x3,
  DRV8323_IDRIVEN_240MA  = 0x4,
  DRV8323_IDRIVEN_280MA  = 0x5,
  DRV8323_IDRIVEN_340MA  = 0x6,
  DRV8323_IDRIVEN_380MA  = 0x7,
  DRV8323_IDRIVEN_520MA  = 0x8,
  DRV8323_IDRIVEN_660MA  = 0x9,
  DRV8323_IDRIVEN_740MA  = 0xa,
  DRV8323_IDRIVEN_880MA  = 0xb,
  DRV8323_IDRIVEN_1140MA = 0xc,
  DRV8323_IDRIVEN_1360MA = 0xd,
  DRV8323_IDRIVEN_1640MA = 0xe,
  DRV8323_IDRIVEN_2000MA = 0xf,
};

/* TDRIVE peak gate-current drive time */

enum drv8323_tdrive_e
{
  DRV8323_TDRIVE_500NS = 0,
  DRV8323_TDRIVE_1US   = 1,
  DRV8323_TDRIVE_2US   = 2,
  DRV8323_TDRIVE_4US   = 3,
};

/* Dead time between HS and LS transitions */

enum drv8323_dead_time_e
{
  DRV8323_DEAD_50NS  = 0,
  DRV8323_DEAD_100NS = 1,
  DRV8323_DEAD_200NS = 2,
  DRV8323_DEAD_400NS = 3,
};

/* OCP response mode */

enum drv8323_ocp_mode_e
{
  DRV8323_OCP_LATCH    = 0,
  DRV8323_OCP_RETRY    = 1,
  DRV8323_OCP_REPORT   = 2,
  DRV8323_OCP_DISABLED = 3,
};

/* OCP deglitch time */

enum drv8323_ocp_deg_e
{
  DRV8323_OCPDEG_2US = 0,
  DRV8323_OCPDEG_4US = 1,
  DRV8323_OCPDEG_6US = 2,
  DRV8323_OCPDEG_8US = 3,
};

/* VDS overcurrent trip level */

enum drv8323_vds_lvl_e
{
  DRV8323_VDSLVL_0V06 = 0x0,
  DRV8323_VDSLVL_0V13 = 0x1,
  DRV8323_VDSLVL_0V20 = 0x2,
  DRV8323_VDSLVL_0V26 = 0x3,
  DRV8323_VDSLVL_0V31 = 0x4,
  DRV8323_VDSLVL_0V45 = 0x5,
  DRV8323_VDSLVL_0V53 = 0x6,
  DRV8323_VDSLVL_0V60 = 0x7,
  DRV8323_VDSLVL_0V68 = 0x8,
  DRV8323_VDSLVL_0V75 = 0x9,
  DRV8323_VDSLVL_0V94 = 0xa,
  DRV8323_VDSLVL_1V13 = 0xb,
  DRV8323_VDSLVL_1V30 = 0xc,
  DRV8323_VDSLVL_1V50 = 0xd,
  DRV8323_VDSLVL_1V70 = 0xe,
  DRV8323_VDSLVL_1V88 = 0xf,
};

/* Sense overcurrent trip level (SP-pin) */

enum drv8323_sen_lvl_e
{
  DRV8323_SENLVL_0V25 = 0,
  DRV8323_SENLVL_0V50 = 1,
  DRV8323_SENLVL_0V75 = 2,
  DRV8323_SENLVL_1V00 = 3,
};

/* DRV8323 board ops (board-provided GPIO/IRQ glue) */

struct drv8323_ops_s
{
  CODE int  (*fault_attach)(FAR struct focpwr_dev_s *dev, xcpt_t isr,
                            FAR void *arg);
  CODE int  (*gate_enable)(FAR struct focpwr_dev_s *dev, bool enable);
  CODE int  (*configure)(FAR struct focpwr_dev_s *dev);
  CODE void (*fault_handle)(FAR struct focpwr_dev_s *dev);
};

/* DRV8323 cold-start configuration.
 *
 * Each field maps directly to a bit-field of one of the R/W registers
 * 0x02..0x06 and is written verbatim by drv8323_setup().
 */

struct drv8323_cfg_s
{
  uint32_t freq;                /* SPI clock frequency [Hz] */

  /* DRIVER_CONTROL (0x02) */

  uint8_t  pwm_mode    : 2;     /* drv8323_pwm_mode_e */
  uint8_t  dis_cpuv    : 1;     /* 1 = disable VCP UVLO fault */
  uint8_t  dis_gdf     : 1;     /* 1 = disable gate-drive fault */
  uint8_t  otw_rep     : 1;     /* 1 = report OTW on nFAULT */
  uint8_t  onepwm_com  : 1;     /* 1 = 1xPWM asynchronous rectification */
  uint8_t  onepwm_dir  : 1;     /* 1xPWM commutation direction bit */

  /* GATE_DRIVE_HS / LS (0x03 / 0x04) */

  uint8_t  idrivep_hs  : 4;     /* drv8323_idrivep_e */
  uint8_t  idriven_hs  : 4;     /* drv8323_idriven_e */
  uint8_t  idrivep_ls  : 4;     /* drv8323_idrivep_e */
  uint8_t  idriven_ls  : 4;     /* drv8323_idriven_e */
  uint8_t  tdrive      : 2;     /* drv8323_tdrive_e */
  uint8_t  cbc         : 1;     /* 1 = cycle-by-cycle OC retry */

  /* OCP_CONTROL (0x05) */

  uint8_t  tretry      : 1;     /* 0 = 4 ms retry, 1 = 50 us */
  uint8_t  dead_time   : 2;     /* drv8323_dead_time_e */
  uint8_t  ocp_mode    : 2;     /* drv8323_ocp_mode_e */
  uint8_t  ocp_deg     : 2;     /* drv8323_ocp_deg_e */
  uint8_t  vds_lvl     : 4;     /* drv8323_vds_lvl_e */

  /* CSA_CONTROL (0x06) */

  uint8_t  csa_fet     : 1;     /* 1 = CSA+ on SHx (instead of SPx) */
  uint8_t  vref_div    : 1;     /* 1 = VREF/2 (bidirectional) */
  uint8_t  ls_ref      : 1;     /* 1 = LS VDS_OCP measured SHx-SNx */
  uint8_t  csa_gain    : 2;     /* drv8323_csa_gain_e */
  uint8_t  dis_sen     : 1;     /* 1 = disable sense OC fault */
  uint8_t  sen_lvl     : 2;     /* drv8323_sen_lvl_e */
};

/* DRV8323 board data */

struct drv8323_board_s
{
  FAR struct spi_dev_s     *spi;
  FAR struct drv8323_ops_s *ops;
  FAR struct drv8323_cfg_s *cfg;
  int                       devno;
};

/* IOCTL payloads */

struct drv8323_idrive_s
{
  uint8_t idrivep_hs;           /* drv8323_idrivep_e */
  uint8_t idriven_hs;           /* drv8323_idriven_e */
  uint8_t idrivep_ls;           /* drv8323_idrivep_e */
  uint8_t idriven_ls;           /* drv8323_idriven_e */
};

struct drv8323_csa_cal_s
{
  bool ch_a;                    /* 1 = short CSA A inputs for offset cal */
  bool ch_b;
  bool ch_c;
};

struct drv8323_faults_s
{
  uint16_t fs1;                 /* raw FAULT_STATUS1 (DRV8323_FS1_*) */
  uint16_t fs2;                 /* raw VGS_STATUS2   (DRV8323_FS2_*) */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int drv8323_register(FAR const char *path,
                     FAR struct foc_dev_s *dev,
                     FAR struct drv8323_board_s *board);

#endif /* __INCLUDE_NUTTX_MOTOR_FOC_DRV8323_H */
