// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2026 Synaptics Incorporated
 */

#ifndef _SYNAPTICS_SOC_H_
#define _SYNAPTICS_SOC_H_

#include <linux/bitfield.h>
#include <linux/bitops.h>

/* Bootstrap register (chip control) */
#define BOOTSTRAP_REG		0xc
#define BOOT_MODE_MASK		GENMASK(17,16)
#define BOOT_MODE(x)		field_prep(BOOT_MODE_MASK, x)
#define BOOT_MODE_USB2		0x0
#define BOOT_MODE_SPI_SD	0x1
#define BOOT_MODE_EMMC		0x2
#define BOOT_MODE_SPI_NAND	0x3

#endif /* _SYNAPTICS_SOC_H_ */
