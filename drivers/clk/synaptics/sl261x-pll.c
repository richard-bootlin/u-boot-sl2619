// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Synaptics Incorporated
 *
 * Author: Richard Genoud <richard.genoud@bootlin.com>
 *
 * From Linux drivers/clk/berlin/sl261x-pll.c:
 *    Copyright (C) 2019-2021 Synaptics Incorporated
 *    Author: Jisheng Zhang <jszhang@kernel.org>
 */

#include <asm/io.h>
#include <clk.h>
#include <clk-uclass.h>
#include <div64.h>
#include <dm/device.h>
#include <linux/clk-provider.h>
#include <linux/kernel.h>

#define FRAC_BITS		24

#define CTRLC			0x08
#define DIVR_MASK		GENMASK(8, 0)
#define CTRLD			0x0c
#define DIVF_MASK		GENMASK(8, 0)
#define CTRLE			0x10
#define DIVFF_MASK		GENMASK(23, 0)
#define CTRLF			0x14
#define DIVQ_MASK		GENMASK(4, 0)
#define CTRLG			0x18
#define DIVQF_MASK		GENMASK(2, 0)

struct sl261x_pll {
	void __iomem	*ctrl;
	struct clk	pllout;
	struct clk	plloutf;
};

#define to_pllout(_clk)		container_of(_clk, struct sl261x_pll, pllout)
#define to_plloutf(_clk)	container_of(_clk, struct sl261x_pll, plloutf)

static unsigned long sl261x_pll_recalc_rate(struct sl261x_pll *pll,
					    unsigned long parent_rate, bool fractional_output)
{
	u32 dm, dn, frac, dp;
	u64 rate;

	dm = (readl_relaxed(pll->ctrl + CTRLC) & DIVR_MASK) + 1;
	dn = (readl_relaxed(pll->ctrl + CTRLD) & DIVF_MASK) + 1;
	frac = readl_relaxed(pll->ctrl + CTRLE) & DIVFF_MASK;

	if (fractional_output)
		dp = (readl_relaxed(pll->ctrl + CTRLG) & DIVQF_MASK) + 1;
	else
		dp = ((readl_relaxed(pll->ctrl + CTRLF) & DIVQ_MASK) + 1) * 2;

	rate = (u64)parent_rate * dn;
	rate += ((u64)parent_rate * frac + BIT_ULL(FRAC_BITS - 1)) >> FRAC_BITS;
	rate = DIV_ROUND_CLOSEST_ULL(rate * 4, (u64)dm * dp);

	return rate;
}

static unsigned long sl261x_pllout_recalc_rate(struct clk *clk)
{
	return sl261x_pll_recalc_rate(to_pllout(clk), clk_get_parent_rate(clk),
				      false);
}

static unsigned long sl261x_plloutf_recalc_rate(struct clk *clk)
{
	return sl261x_pll_recalc_rate(to_plloutf(clk), clk_get_parent_rate(clk),
				      true);
}

static const struct clk_ops sl261x_pllout_ops = {
	.get_rate = sl261x_pllout_recalc_rate,
};

static const struct clk_ops sl261x_plloutf_ops = {
	.get_rate = sl261x_plloutf_recalc_rate,
};

int sl261x_pll_register(struct udevice *dev, void __iomem *ctrl,
			       const char *out, const char *outf)
{
	struct sl261x_pll *pll;
	struct clk clk;
	int ret;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret)
		return ret;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return -ENOMEM;

	pll->ctrl = ctrl;
	pll->pllout.flags = CLK_GET_RATE_NOCACHE;
	pll->plloutf.flags = CLK_GET_RATE_NOCACHE;

	ret = clk_register(&pll->pllout, "sl261x_pll_out", out,
			   clk.dev->name);
	if (ret)
		goto err;

	ret = clk_register(&pll->plloutf, "sl261x_pll_outf", outf,
			   clk.dev->name);
	if (ret)
		goto err;

	return 0;

err:
	kfree(pll);
	return ret;
}

U_BOOT_DRIVER(sl261x_pll_out) = {
	.name = "sl261x_pll_out",
	.id = UCLASS_CLK,
	.ops = &sl261x_pllout_ops,
};

U_BOOT_DRIVER(sl261x_pll_outf) = {
	.name = "sl261x_pll_outf",
	.id = UCLASS_CLK,
	.ops = &sl261x_plloutf_ops,
};
