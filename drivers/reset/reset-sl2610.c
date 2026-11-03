// SPDX-License-Identifier: GPL-2.0
/*
 * Synaptics SL2610 reset driver
 *
 * Copyright (C) 2026 Synaptics Incorporated
 * Author: Richard Genoud <richard.genoud@bootlin.com>
 *
 * Based on Linux driver reset-sl2610.c:
 *	Author: Jisheng Zhang <jszhang@kernel.org>
 *	Copyright (C) 2026 Synaptics Incorporated
 *
 *  There's two kind of reset bits here:
 *  - the so-called sticky bits that can be asserted and de-asserted, and
 *    are active low.
 *  - the non-sticky bits (aka pulse bits) that generate a 640ns pulse and
 *    are active high.
 *
 *  rst_assert(), rst_deassert() and rst_reset() are implemented for sticky
 *  bits.
 *  But for pulse bits, rst_assert()/rst_deassert() don't fit, because we
 *  can't keep them asserted, or de-assert them.
 *  So, only rst_assert() is implemented for pulse bits, and delay is
 *  ignored.
 *
 */

#include <dm.h>
#include <dm/device_compat.h>
#include <dt-bindings/reset/syna,sl2610-reset.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <reset-uclass.h>
#include <syscon.h>

/*
 * sticky reset bits are active low, whereas
 * non-sticky resets bits are active high.
 */

/* SL2610 ACPU subsystem */
#define SL2610_GBL_PERIFRESET			0x350
#define SL2610_GBL_PERIFSTICKYRESET		0x360
#define SL2610_GBL_APBPERIFRESET		0x370
#define SL2610_GBL_TOPSTICKYRESET		0x378
#define SL2610_GBL_AVIOSTICKYRESET		0x37c

/* SL2610 system subsystem */
#define SL2610_SMGBL_STICKYRST0			0x400

#define SL2610_RESET(id, reg, bit, sticky)[id] = { reg, BIT(bit), sticky }

struct sl2610_reset_cfg {
	u32 reg;
	u32 bit;
	bool sticky;
};

struct sl2610_reset_data {
	const struct sl2610_reset_cfg *cfgs;
	size_t num;
};

struct sl2610_reset_priv {
	struct regmap *regmap;
	const struct sl2610_reset_data *data;
};

static const struct sl2610_reset_cfg sl2610_reset_cfgs[] = {
	SL2610_RESET(RST_SOC_SDIO0, SL2610_GBL_PERIFRESET, 2, false),
	SL2610_RESET(RST_SOC_USB0, SL2610_GBL_PERIFRESET, 4, false),
	SL2610_RESET(RST_SOC_EMMC, SL2610_GBL_PERIFRESET, 5, false),
	SL2610_RESET(RST_SOC_GETH0, SL2610_GBL_PERIFRESET, 6, false),
	SL2610_RESET(RST_SOC_SDIO1, SL2610_GBL_PERIFRESET, 7, false),
	SL2610_RESET(RST_SOC_USB1, SL2610_GBL_PERIFRESET, 8, false),
	SL2610_RESET(RST_SOC_GETH1, SL2610_GBL_PERIFRESET, 9, false),
	SL2610_RESET(RST_SOC_USB0PHY, SL2610_GBL_PERIFSTICKYRESET, 0, true),
	SL2610_RESET(RST_SOC_USB0CORE, SL2610_GBL_PERIFSTICKYRESET, 1, true),
	SL2610_RESET(RST_SOC_USB0MAHB, SL2610_GBL_PERIFSTICKYRESET, 2, true),
	SL2610_RESET(RST_SOC_USB1PHY, SL2610_GBL_PERIFSTICKYRESET, 3, true),
	SL2610_RESET(RST_SOC_USB1CORE, SL2610_GBL_PERIFSTICKYRESET, 4, true),
	SL2610_RESET(RST_SOC_USB1MAHB, SL2610_GBL_PERIFSTICKYRESET, 5, true),
	SL2610_RESET(RST_SOC_UART0, SL2610_GBL_APBPERIFRESET, 0, false),
	SL2610_RESET(RST_SOC_UART1, SL2610_GBL_APBPERIFRESET, 1, false),
	SL2610_RESET(RST_SOC_UART2, SL2610_GBL_APBPERIFRESET, 2, false),
	SL2610_RESET(RST_SOC_UART3, SL2610_GBL_APBPERIFRESET, 3, false),
	SL2610_RESET(RST_SOC_I2C0, SL2610_GBL_APBPERIFRESET, 4, false),
	SL2610_RESET(RST_SOC_I2C1, SL2610_GBL_APBPERIFRESET, 5, false),
	SL2610_RESET(RST_SOC_SPI0, SL2610_GBL_APBPERIFRESET, 6, false),
	SL2610_RESET(RST_SOC_SPI1, SL2610_GBL_APBPERIFRESET, 7, false),
	SL2610_RESET(RST_SOC_SPI2, SL2610_GBL_APBPERIFRESET, 8, false),
	SL2610_RESET(RST_SOC_SPI3, SL2610_GBL_APBPERIFRESET, 9, false),
	SL2610_RESET(RST_SOC_APBTIMERS, SL2610_GBL_APBPERIFRESET, 10, false),
	SL2610_RESET(RST_SOC_APBSYSCNT, SL2610_GBL_APBPERIFRESET, 11, false),
	SL2610_RESET(RST_SOC_APBWDT, SL2610_GBL_APBPERIFRESET, 12, false),
	SL2610_RESET(RST_SOC_APBGPIO, SL2610_GBL_APBPERIFRESET, 13, false),
	SL2610_RESET(RST_SOC_APBDMA, SL2610_GBL_APBPERIFRESET, 14, false),
	SL2610_RESET(RST_SOC_GPUCORE, SL2610_GBL_TOPSTICKYRESET, 0, true),
	SL2610_RESET(RST_SOC_NPUCORE, SL2610_GBL_TOPSTICKYRESET, 1, true),
	SL2610_RESET(RST_SOC_AVIOAIOG, SL2610_GBL_AVIOSTICKYRESET, 0, true),
	SL2610_RESET(RST_SOC_AVIOVPPG, SL2610_GBL_AVIOSTICKYRESET, 1, true),
	SL2610_RESET(RST_SOC_AVIOVIPG, SL2610_GBL_AVIOSTICKYRESET, 2, true),
};

static const struct sl2610_reset_cfg sl2610_system_reset_cfgs[] = {
	SL2610_RESET(RST_SM_ADCCORE, SL2610_SMGBL_STICKYRST0, 0, true),
	SL2610_RESET(RST_SM_ADCPRST, SL2610_SMGBL_STICKYRST0, 1, true),
	SL2610_RESET(RST_SM_CAN0PRST, SL2610_SMGBL_STICKYRST0, 2, true),
	SL2610_RESET(RST_SM_CAN0SRST, SL2610_SMGBL_STICKYRST0, 3, true),
	SL2610_RESET(RST_SM_CAN1PRST, SL2610_SMGBL_STICKYRST0, 4, true),
	SL2610_RESET(RST_SM_CAN1SRST, SL2610_SMGBL_STICKYRST0, 5, true),
	SL2610_RESET(RST_SM_GPIOPRST, SL2610_SMGBL_STICKYRST0, 6, true),
	SL2610_RESET(RST_SM_GPIOSRST, SL2610_SMGBL_STICKYRST0, 7, true),
	SL2610_RESET(RST_SM_I2CM0PRST, SL2610_SMGBL_STICKYRST0, 8, true),
	SL2610_RESET(RST_SM_I2CM0SRST, SL2610_SMGBL_STICKYRST0, 9, true),
	SL2610_RESET(RST_SM_I2CM1PRST, SL2610_SMGBL_STICKYRST0, 10, true),
	SL2610_RESET(RST_SM_I2CM1SRST, SL2610_SMGBL_STICKYRST0, 11, true),
	SL2610_RESET(RST_SM_I3CPRST, SL2610_SMGBL_STICKYRST0, 12, true),
	SL2610_RESET(RST_SM_I3CSRST, SL2610_SMGBL_STICKYRST0, 13, true),
	SL2610_RESET(RST_SM_PDMPRST, SL2610_SMGBL_STICKYRST0, 14, true),
	SL2610_RESET(RST_SM_PDMSRST, SL2610_SMGBL_STICKYRST0, 15, true),
	SL2610_RESET(RST_SM_PVTHSRST, SL2610_SMGBL_STICKYRST0, 16, true),
	SL2610_RESET(RST_SM_PVTPRST, SL2610_SMGBL_STICKYRST0, 17, true),
	SL2610_RESET(RST_SM_PWMPERIRST, SL2610_SMGBL_STICKYRST0, 18, true),
	SL2610_RESET(RST_SM_PWMPRST, SL2610_SMGBL_STICKYRST0, 19, true),
	SL2610_RESET(RST_SM_SPIMPRST, SL2610_SMGBL_STICKYRST0, 20, true),
	SL2610_RESET(RST_SM_SPIMSRST, SL2610_SMGBL_STICKYRST0, 21, true),
	SL2610_RESET(RST_SM_SPISPRST, SL2610_SMGBL_STICKYRST0, 22, true),
	SL2610_RESET(RST_SM_SPISSRST, SL2610_SMGBL_STICKYRST0, 23, true),
	SL2610_RESET(RST_SM_UART0PRST, SL2610_SMGBL_STICKYRST0, 24, true),
	SL2610_RESET(RST_SM_UART0SRST, SL2610_SMGBL_STICKYRST0, 25, true),
	SL2610_RESET(RST_SM_UART1PRST, SL2610_SMGBL_STICKYRST0, 26, true),
	SL2610_RESET(RST_SM_UART1SRST, SL2610_SMGBL_STICKYRST0, 27, true),
	SL2610_RESET(RST_SM_UART2PRST, SL2610_SMGBL_STICKYRST0, 28, true),
	SL2610_RESET(RST_SM_UART2SRST, SL2610_SMGBL_STICKYRST0, 29, true),
	SL2610_RESET(RST_SM_UART3PRST, SL2610_SMGBL_STICKYRST0, 30, true),
	SL2610_RESET(RST_SM_UART3SRST, SL2610_SMGBL_STICKYRST0, 31, true),
};

static const struct sl2610_reset_data sl2610_reset_data = {
	.cfgs = sl2610_reset_cfgs,
	.num = ARRAY_SIZE(sl2610_reset_cfgs),
};

static const struct sl2610_reset_data sl2610_system_reset_data = {
	.cfgs = sl2610_system_reset_cfgs,
	.num = ARRAY_SIZE(sl2610_system_reset_cfgs),
};

static const
struct sl2610_reset_cfg *sl2610_get_cfg(struct sl2610_reset_priv *priv,
					unsigned long id)
{
	if (id >= priv->data->num)
		return NULL;

	return &priv->data->cfgs[id];
}

static int sl2610_reset_assert(struct reset_ctl *rst)
{
	struct sl2610_reset_priv *priv = dev_get_priv(rst->dev);
	const struct sl2610_reset_cfg *cfg = sl2610_get_cfg(priv, rst->id);

	if (!cfg)
		return -EINVAL;

	/*
	 * non-sticky bits are auto-deasserted, the reset is triggered for
	 * 640ns, and deasserted.
	 * So user should use rst_reset instead.
	 */
	if (!cfg->sticky)
		return -EINVAL;

	/* sticky reset bits are active low */
	return regmap_clear_bits(priv->regmap, cfg->reg, cfg->bit);
}

static int sl2610_reset_deassert(struct reset_ctl *rst)
{
	struct sl2610_reset_priv *priv = dev_get_priv(rst->dev);
	const struct sl2610_reset_cfg *cfg = sl2610_get_cfg(priv, rst->id);

	if (!cfg)
		return -EINVAL;

	if (!cfg->sticky)
		return -EINVAL;

	/* sticky reset bits are active low */
	return regmap_set_bits(priv->regmap, cfg->reg, cfg->bit);
}

static int sl2610_reset_reset(struct reset_ctl *rst, ulong delay_us)
{
	struct sl2610_reset_priv *priv = dev_get_priv(rst->dev);
	const struct sl2610_reset_cfg *cfg = sl2610_get_cfg(priv, rst->id);
	int err;

	if (!cfg)
		return -EINVAL;

	/*
	 * We should not use regmap_set_bits() for pulse-bits since it would
	 * reset again the devices behind every bits that where not cleared
	 * from a previous reset.
	 */
	if (!cfg->sticky)
		return regmap_write(priv->regmap, cfg->reg, cfg->bit);

	/* sticky reset bits are active low */
	err = regmap_clear_bits(priv->regmap, cfg->reg, cfg->bit);
	if (err)
		return err;

	udelay(delay_us);

	return regmap_set_bits(priv->regmap, cfg->reg, cfg->bit);
}

static int sl2610_reset_status(struct reset_ctl *rst)
{
	struct sl2610_reset_priv *priv = dev_get_priv(rst->dev);
	const struct sl2610_reset_cfg *cfg = sl2610_get_cfg(priv, rst->id);
	u32 val;
	int err;

	if (!cfg)
		return -EINVAL;

	/* reset from non sticky bits is self-deasserting */
	if (!cfg->sticky)
		return 0;

	err = regmap_read(priv->regmap, cfg->reg, &val);
	if (err)
		return err;

	/* sticky reset bits are active low */
	return !(val & cfg->bit);
}

static int sl2610_reset_probe(struct udevice *dev)
{
	struct sl2610_reset_priv *priv = dev_get_priv(dev);
	const struct sl2610_reset_data *data;

	priv->regmap = syscon_node_to_regmap(dev_ofnode(dev->parent));
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	data = (const struct sl2610_reset_data *)dev_get_driver_data(dev);
	if (!data)
		return -EINVAL;

	priv->data = data;

	return 0;
}

static const struct udevice_id sl2610_reset_ids[] = {
	{
		.compatible = "syna,sl2610-reset",
		.data = (ulong)&sl2610_reset_data
	},
	{
		.compatible = "syna,sl2610-system-reset",
		.data = (ulong)&sl2610_system_reset_data
	},
	{ /* sentinel */ }
};

static const struct reset_ops sl2610_reset_ops = {
	.rst_assert = sl2610_reset_assert,
	.rst_deassert = sl2610_reset_deassert,
	.rst_reset = sl2610_reset_reset,
	.rst_status = sl2610_reset_status,
};

U_BOOT_DRIVER(sl2610_reset) = {
	.name		= "sl2610-reset",
	.id		= UCLASS_RESET,
	.of_match	= sl2610_reset_ids,
	.probe		= sl2610_reset_probe,
	.ops		= &sl2610_reset_ops,
	.priv_auto	= sizeof(struct sl2610_reset_priv),
};
