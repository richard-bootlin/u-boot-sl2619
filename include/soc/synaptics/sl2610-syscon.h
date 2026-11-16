/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2026 Synaptics Incorporated
 */

#ifndef __SOC_SYNAPTICS_SL2610_SYSCON_H__
#define __SOC_SYNAPTICS_SL2610_SYSCON_H__

/*
 * The SL2610 SoC has several system controller register blocks. Each block
 * bundles a set of sub-devices (clock, reset, pin controller). The block
 * identity is passed as driver data when the system controller binds its
 * sub-devices, and lets each sub-device driver select the register layout it
 * has to use.
 */
enum sl2610_syscon_block {
	SL2610_SYSCON_BLOCK_SOC,
	SL2610_SYSCON_BLOCK_MCU,
	SL2610_SYSCON_BLOCK_AVIO,
	SL2610_SYSCON_BLOCK_CPU,
};

#endif /* __SOC_SYNAPTICS_SL2610_SYSCON_H__ */
