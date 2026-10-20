// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2026 Synaptics Incorporated
 */

#ifndef _SYNAPTICS_SOC_H_
#define _SYNAPTICS_SOC_H_

#include <linux/bitfield.h>
#include <linux/bitops.h>

#define MEMMAP_CHIP_CTRL_REG_BASE 0xf7e10000
#define BOOT_MODE_REGISTER	(MEMMAP_CHIP_CTRL_REG_BASE + 0xc)
#define BOOT_MODE_MASK		0x00030000
#define BOOT_MODE(x)		field_prep(BOOT_MODE_MASK, x)
#define BOOT_MODE_USB2		0x0
#define BOOT_MODE_XSPI		0x1
#define BOOT_MODE_EMMC		0x2

#endif /* _SYNAPTICS_SOC_H_ */
