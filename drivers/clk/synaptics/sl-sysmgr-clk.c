// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Synaptics Incorporated
 *
 * From Linux: drivers/clk/berlin/sl-sysmgr-clk.c
 *    Copyright (C) 2026 Synaptics Incorporated
 *    Author: Jisheng Zhang <jszhang@kernel.org>
 *
 */

#include <dm/device.h>
#include <dm/read.h>

#include <linux/clk-provider.h>

#include "sl-sysmgr-clk.h"

#define SL_SYSMGR_DIV_SHIFT	2

int sl_sysmgr_clk_setup(struct udevice *dev,
			const struct sl_sysmgr_fixedclk_desc *fdescs,
			int fclk_n,
			const struct sl_sysmgr_gateclk_desc *gdescs,
			int gclk_n,
			u32 gclk_offset,
			const struct sl_sysmgr_clk_desc *descs,
			int clk_n)
{
	void __iomem *base;
	struct udevice *parent;
	struct clk *clk;
	int i;

	parent = dev_get_parent(dev);
	if (!parent)
		return -ENODEV;

	base = dev_read_addr_ptr(parent);
	if (!base)
		return -ENODEV;

	for (i = 0; i < fclk_n; i++) {
		clk = clk_register_fixed_rate(NULL, fdescs[i].name,
					      fdescs[i].rate);
		if (IS_ERR(clk))
			return PTR_ERR(clk);

		dev_clk_dm(dev, fdescs[i].id, clk);
	}

	for (i = 0; i < gclk_n; i++) {
		clk = clk_register_gate(dev, gdescs[i].name,
					gdescs[i].parent_name, 0,
					base + gclk_offset, gdescs[i].bit_idx,
					0, NULL);
		if (IS_ERR(clk))
			return PTR_ERR(clk);

		dev_clk_dm(dev, gdescs[i].id, clk);
	}

	for (i = 0; i < clk_n; i++) {
		clk = clk_register_divider(dev, descs[i].name,
					   descs[i].parent_name, 0,
					   base + descs[i].offset,
					   SL_SYSMGR_DIV_SHIFT,
					   descs[i].div_bits, 0);
		if (IS_ERR(clk))
			return PTR_ERR(clk);

		dev_clk_dm(dev, descs[i].id, clk);
	}

	return 0;
}

