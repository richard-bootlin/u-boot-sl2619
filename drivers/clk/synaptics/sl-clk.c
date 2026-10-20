// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Synaptics Incorporated
 *
 * From Linux drivers/clk/berlin/sl-clk.c:
 *    Copyright (C) 2018 Synaptics Incorporated
 *    Copyright (c) 2015 Marvell Technology Group Ltd.
 *    Author: Jisheng Zhang <jszhang@kernel.org>
 */

#include <dm/device_compat.h>
#include <dm/devres.h>
#include <dm/uclass.h>

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/clk-provider.h>
#include <linux/compat.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/string.h>

#define SL_CLK_EN		BIT(0)
#define SL_CLK_PLL_SEL		GENMASK(3, 1)
#define SL_CLK_PLL_SWITCH	BIT(4)
#define SL_CLK_DIV_SWITCH	BIT(5)
#define SL_CLK_DIV3_SWITCH	BIT(6)
#define SL_CLK_DIV_SEL		GENMASK(9, 7)
#define SL_MAX_PARENTS	6

struct sl_clk {
	struct clk hw;
	void __iomem *reg;
	spinlock_t *lock;
	struct udevice *dev;
	unsigned int num_parents;
	const char *const *parent_names;
};

#define to_sl_clk(_hw) container_of(_hw, struct sl_clk, hw)

static const unsigned int sl_clk_values[] = { 1, 2, 3, 4, 6, 8, 12 };

static unsigned int sl_clk_value(u32 val)
{
	static const u8 div_table[] = { 0, 2, 4, 6, 8, 12, 0, 0 };

	if (val & SL_CLK_DIV3_SWITCH)
		return 3;
	if (!(val & SL_CLK_DIV_SWITCH))
		return 1;

	return div_table[FIELD_GET(SL_CLK_DIV_SEL, val)];
}

static ulong sl_clk_get_rate(struct clk *hw)
{
	struct sl_clk *clk = to_sl_clk(hw);
	unsigned int divisor = sl_clk_value(readl_relaxed(clk->reg));
	unsigned long parent_rate = clk_get_parent_rate(&clk->hw);

	if (WARN_ON_ONCE(!divisor))
		return 0;

	return parent_rate / divisor;
}

static unsigned int sl_clk_best(unsigned long parent_rate,
				unsigned long rate)
{
	unsigned long best_diff = ULONG_MAX;
	unsigned int best = 1, i;

	for (i = 0; i < ARRAY_SIZE(sl_clk_values); i++) {
		unsigned long candidate = parent_rate / sl_clk_values[i];
		unsigned long diff = candidate > rate ? candidate - rate :
							     rate - candidate;

		if (diff < best_diff) {
			best_diff = diff;
			best = sl_clk_values[i];
		}
	}

	return best;
}

static ulong sl_clk_set_rate(struct clk *hw, unsigned long rate)
{
	struct sl_clk *clk = to_sl_clk(hw);
	unsigned long parent_rate = clk_get_parent_rate(&clk->hw);
	unsigned int divisor, select;
	unsigned long flags;
	u32 val;

	divisor = sl_clk_best(parent_rate, rate);
	if (parent_rate / divisor != rate)
		return -EINVAL;

	spin_lock_irqsave(clk->lock, flags);
	val = readl_relaxed(clk->reg);
	val &= ~(SL_CLK_DIV3_SWITCH | SL_CLK_DIV_SWITCH | SL_CLK_DIV_SEL);
	if (divisor == 3) {
		val |= SL_CLK_DIV3_SWITCH;
	} else if (divisor != 1) {
		switch (divisor) {
		case 2:
			select = 1;
			break;
		case 4:
			select = 2;
			break;
		case 6:
			select = 3;
			break;
		case 8:
			select = 4;
			break;
		case 12:
			select = 5;
			break;
		default:
			spin_unlock_irqrestore(clk->lock, flags);
			return -EINVAL;
		}
		val |= SL_CLK_DIV_SWITCH |
			FIELD_PREP(SL_CLK_DIV_SEL, select);
	}
	writel_relaxed(val, clk->reg);
	spin_unlock_irqrestore(clk->lock, flags);

	return parent_rate / divisor;
}

static u8 sl_clk_get_parent(struct sl_clk *clk)
{
	u32 val = readl_relaxed(clk->reg);
	u8 selector;
	unsigned int index;

	if (!(val & SL_CLK_PLL_SWITCH))
		return 0;

	selector = FIELD_GET(SL_CLK_PLL_SEL, val);
	index = selector + 1;
	if (index < clk->num_parents)
		return index;

	dev_warn(clk->dev, "%s: unsupported parent selector %u\n",
		 clk_hw_get_name(&clk->hw), selector);
	return 0;
}

static int sl_clk_set_parent(struct clk *hw, struct clk *parent)
{
	struct sl_clk *clk = to_sl_clk(hw);
	unsigned long flags;
	u32 val;
	int index;

	for (index = 0; index < clk->num_parents; index++)
		if (!strcmp(clk->parent_names[index], parent->dev->name))
			break;

	if (index == clk->num_parents || index >= SL_MAX_PARENTS)
		return -EINVAL;

	spin_lock_irqsave(clk->lock, flags);
	val = readl_relaxed(clk->reg);
	if (!index) {
		val &= ~SL_CLK_PLL_SWITCH;
	} else {
		val &= ~SL_CLK_PLL_SEL;
		val |= SL_CLK_PLL_SWITCH |
			FIELD_PREP(SL_CLK_PLL_SEL, index - 1);
	}
	writel_relaxed(val, clk->reg);
	spin_unlock_irqrestore(clk->lock, flags);

	return 0;
}

static int sl_clk_enable(struct clk *hw)
{
	struct sl_clk *clk = to_sl_clk(hw);
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(clk->lock, flags);
	val = readl_relaxed(clk->reg);
	writel_relaxed(val | SL_CLK_EN, clk->reg);
	spin_unlock_irqrestore(clk->lock, flags);

	return 0;
}

static int sl_clk_disable(struct clk *hw)
{
	struct sl_clk *clk = to_sl_clk(hw);
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(clk->lock, flags);
	val = readl_relaxed(clk->reg);
	writel_relaxed(val & ~SL_CLK_EN, clk->reg);
	spin_unlock_irqrestore(clk->lock, flags);

	return 0;
}

static const struct clk_ops sl_clk_ops = {
	.get_rate = sl_clk_get_rate,
	.set_rate = sl_clk_set_rate,
	.set_parent = sl_clk_set_parent,
	.enable = sl_clk_enable,
	.disable = sl_clk_disable,
};

struct clk *sl_clk_register(struct udevice *dev, void __iomem *reg,
			    const char *name, const char * const *parent_names,
			    unsigned int num_parents, unsigned long flags)
{
	struct sl_clk *clk;
	struct clk *hw;
	unsigned int i;
	u32 val;
	int ret;

	if (!dev || !reg || !name || !parent_names || !num_parents ||
	    num_parents > SL_MAX_PARENTS)
		return ERR_PTR(-EINVAL);

	for (i = 0; i < num_parents; i++)
		if (!parent_names[i])
			return ERR_PTR(-EINVAL);

	clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return ERR_PTR(-ENOMEM);

	clk->dev = dev;
	clk->reg = reg;
	clk->parent_names = parent_names;
	clk->num_parents = num_parents;

	hw = &clk->hw;
	hw->flags = flags;

	val = readl(reg);
	ret = clk_register(hw, "sl261x_sl_clk", name,
			   parent_names[sl_clk_get_parent(clk)]);
	if (ret) {
		kfree(clk);
		return ERR_PTR(ret);
	}

	return hw;
}

U_BOOT_DRIVER(sl261x_sl_clk) = {
	.name = "sl261x_sl_clk",
	.id = UCLASS_CLK,
	.ops = &sl_clk_ops,
};

