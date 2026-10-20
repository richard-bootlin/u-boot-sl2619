// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Synaptics Incorporated
 *
 * Richard Genoud <richard.genoud@bootlin.com>
 *
 * Based on Linux Berlin/Synaptics drivers:
 *  - drivers/pinctrl/berlin/berlin.c:
 *    Copyright (C) 2014 Marvell Technology Group Ltd.
 *    Antoine Ténart <antoine.tenart@free-electrons.com>
 *
 *  - pinctrl/berlin/pinctrl-sl2610.c:
 *   Copyright (C) 2025 Synaptics Incorporated
 *   Author: Jisheng Zhang <jszhang@kernel.org>
 *
 */

#include <dm.h>
#include <dm/device_compat.h>
#include <dm/pinctrl.h>
#include <linux/bitfield.h>
#include <linux/pinctrl/pinctrl.h>
#include <regmap.h>
#include <syscon.h>

#define SYNA_PINCFG_DS	GENMASK(3, 0)	/* Drive strength */
#define SYNA_PINCFG_IE	BIT(4)		/* Input enable */
#define SYNA_PINCFG_PE	BIT(5)		/* Pull enable */
#define SYNA_PINCFG_PS	BIT(6)		/* Pull select (0:PD, 1:PU) */
#define SYNA_PINCFG_SL	BIT(7)		/* Slew rate (0:slow 1:fast) */
#define SYNA_PINCFG_SPU	BIT(8)		/* Strong pull-up */
#define SYNA_PINCFG_ST	BIT(9)		/* Schmitt trigger */

struct syna_desc_function {
	const char *name;
	u8 muxval;
};

struct syna_desc_group {
	const char *name;
	u16 offset;
	u8 bit_width;
	u8 lsb;
	u16 conf_offset;
	const struct syna_desc_function *functions;
};

struct syna_pinctrl_desc {
	const struct syna_desc_group *groups;
	unsigned int ngroups;
};

struct syna_pinctrl {
	struct regmap *regmap;
	const struct syna_pinctrl_desc *desc;
	const char **function_names;
	u32 nfunctions;
};

#define SYNA_PINCTRLCONF_GROUP(_name, _offset, _width, _lsb, _conf_offset, ...)	\
	{								\
		.name = _name,						\
		.offset = _offset,					\
		.bit_width = _width,					\
		.lsb = _lsb,						\
		.conf_offset = _conf_offset,				\
		.functions = (const struct syna_desc_function[]){	\
			__VA_ARGS__, { } },				\
	}

#define SYNA_PINCTRL_FUNCTION(_muxval, _name)		\
	{						\
		.name = _name,				\
		.muxval = _muxval,			\
	}

static const struct syna_desc_group sl2610_soc_pinctrl_groups[] = {
	SYNA_PINCTRLCONF_GROUP("GPIO23", 0x8000, 0x3, 0x00, 0x8800,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO23 */
			       SYNA_PINCTRL_FUNCTION(0x1, "tw2"), /* SCL */
			       SYNA_PINCTRL_FUNCTION(0x2, "rgmii"), /* MDC */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row0"),
			       SYNA_PINCTRL_FUNCTION(0x5, "spi3"), /* SS2n */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col3"),
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* OUT4 */
	SYNA_PINCTRLCONF_GROUP("GPIO24", 0x8000, 0x3, 0x03, 0x8804,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO24 */
			       SYNA_PINCTRL_FUNCTION(0x1, "tw2"), /* SDA */
			       SYNA_PINCTRL_FUNCTION(0x2, "rgmii"), /* MDIO */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row1"),
			       SYNA_PINCTRL_FUNCTION(0x5, "spi3"), /* SS3n */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col2"),
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* OUT5 */
	SYNA_PINCTRLCONF_GROUP("GPIO25", 0x8000, 0x3, 0x06, 0x8808,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO25 */
			       SYNA_PINCTRL_FUNCTION(0x1, "uart5"), /* RXD */
			       SYNA_PINCTRL_FUNCTION(0x2, "gpio_trig1"),
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row2"),
			       SYNA_PINCTRL_FUNCTION(0x4, "sm_uart1"), /* RXD */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col1"),
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* OUT6 */
	SYNA_PINCTRLCONF_GROUP("GPIO26", 0x8000, 0x3, 0x09, 0x880c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO26 */
			       SYNA_PINCTRL_FUNCTION(0x1, "uart5"), /* TXD */
			       SYNA_PINCTRL_FUNCTION(0x2, "rgmii"), /* PTP_PPS_O */
			       SYNA_PINCTRL_FUNCTION(0x4, "sm_uart1"), /* TXD */
			       SYNA_PINCTRL_FUNCTION(0x5, "usb2"), /* VBUS */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* OUT7 */
	SYNA_PINCTRLCONF_GROUP("GPIO27", 0x8000, 0x3, 0x0c, 0x8810,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO27 */
			       SYNA_PINCTRL_FUNCTION(0x1, "tw3"), /* SCL */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart4"), /* TXD */
			       SYNA_PINCTRL_FUNCTION(0x4, "sm_uart1")), /* RTSn */
	SYNA_PINCTRLCONF_GROUP("GPIO28", 0x8000, 0x3, 0x0f, 0x8814,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO28 */
			       SYNA_PINCTRL_FUNCTION(0x1, "tw3"), /* SDA */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart4"), /* RXD */
			       SYNA_PINCTRL_FUNCTION(0x4, "sm_uart1")), /* CTSn */
	SYNA_PINCTRLCONF_GROUP("GPIO29", 0x8000, 0x3, 0x12, 0x8818,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO29 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart4"), /* DE */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row4"),
			       SYNA_PINCTRL_FUNCTION(0x4, "sm_uart1"), /* RXD */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col4"),
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SDI */
	SYNA_PINCTRLCONF_GROUP("GPIO30", 0x8000, 0x3, 0x15, 0x881c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO30 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart4"), /* REn */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row5"),
			       SYNA_PINCTRL_FUNCTION(0x4, "sm_uart1"), /* TXD */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col5"),
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SCLK */
	SYNA_PINCTRLCONF_GROUP("GPIO31", 0x8000, 0x3, 0x18, 0x8820,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO31 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rgmii"), /* MDC */
			       SYNA_PINCTRL_FUNCTION(0x5, "spi5"), /* SS3n */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi3"), /* SS2n */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SS2n */
	SYNA_PINCTRLCONF_GROUP("GPIO32", 0x8000, 0x3, 0x1b, 0x8824,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO32 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rgmii"), /* MDIO */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi3"), /* SS3n */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SS3n */
	SYNA_PINCTRLCONF_GROUP("GPIO33", 0x8004, 0x3, 0x00, 0x8828,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO33 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* TD0 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii1"), /* TXD0 */
			       SYNA_PINCTRL_FUNCTION(0x5, "can0"), /* TX */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi3"), /* SS1n */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SS1n */
	SYNA_PINCTRLCONF_GROUP("GPIO34", 0x8004, 0x3, 0x03, 0x882c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO34 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* TD1 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii1"), /* TXD1 */
			       SYNA_PINCTRL_FUNCTION(0x5, "can0")), /* RX */
	SYNA_PINCTRLCONF_GROUP("GPIO35", 0x8004, 0x3, 0x06, 0x8830,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO35 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* TD2 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii2"), /* TXD0 */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col7"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart5"), /* RXD */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SS0n */
	SYNA_PINCTRLCONF_GROUP("GPIO36", 0x8004, 0x3, 0x09, 0x8834,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO36 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* TD3 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii2"), /* TXD1 */
			       SYNA_PINCTRL_FUNCTION(0x4, "uart5"), /* TXD */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SDO */
	SYNA_PINCTRLCONF_GROUP("GPIO37", 0x8004, 0x3, 0x0c, 0x8838,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO37 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* RD0 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii1"), /* RXD0 */
			       SYNA_PINCTRL_FUNCTION(0x5, "can1"), /* TX */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi5")), /* SDI */
	SYNA_PINCTRLCONF_GROUP("GPIO38", 0x8004, 0x3, 0x0f, 0x883c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO38 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* RD1 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii1"), /* RXD1 */
			       SYNA_PINCTRL_FUNCTION(0x5, "can1"), /* RX */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi5")), /* SDO */
	SYNA_PINCTRLCONF_GROUP("GPIO39", 0x8004, 0x3, 0x12, 0x8840,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO39 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* RD2 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii2"), /* RXD0 */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row6"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart6"), /* RXD */
			       SYNA_PINCTRL_FUNCTION(0x5, "spi5"), /* SS1n*/
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4")), /* SS3n  */
	SYNA_PINCTRLCONF_GROUP("GPIO40", 0x8004, 0x3, 0x15, 0x8844,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO40 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* RD3 */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii2"), /* RXD1 */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row7"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart6"), /* TXD */
			       SYNA_PINCTRL_FUNCTION(0x5, "spi5"), /* SS2n */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4")), /* SS2n  */
	SYNA_PINCTRLCONF_GROUP("GPIO41", 0x8004, 0x3, 0x18, 0x8848,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO41 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* RXC */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii1"), /* CRSDV */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi5")), /* SCLK */
	SYNA_PINCTRLCONF_GROUP("GPIO42", 0x8004, 0x3, 0x1b, 0x884c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO42 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* TXC */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii2"), /* CRSDV */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row8"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart7"), /* RXD */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4")), /* SCLK */
	SYNA_PINCTRLCONF_GROUP("GPIO43", 0x8008, 0x3, 0x00, 0x8850,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO43 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* TXCTL */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii1"), /* TXEN */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4")), /* SS0n */
	SYNA_PINCTRLCONF_GROUP("GPIO44", 0x8008, 0x3, 0x03, 0x8854,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO44 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* RXCTL */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii2"), /* TXEN */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row9"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart7"), /* TXD */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4")), /* SDI */
	SYNA_PINCTRLCONF_GROUP("GPIO45", 0x8008, 0x3, 0x06, 0x8858,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO45 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii1"), /* CLKOUT */
			       SYNA_PINCTRL_FUNCTION(0x2, "rmii1")), /* REFCLK */
	SYNA_PINCTRLCONF_GROUP("GPIO46", 0x8008, 0x3, 0x09, 0x885c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO46 */
			       SYNA_PINCTRL_FUNCTION(0x1, "sdio1"), /* CDn */
			       SYNA_PINCTRL_FUNCTION(0x2, "sdio2"), /* CDn */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col0"),
			       SYNA_PINCTRL_FUNCTION(0x5, "sm_uart1"), /* RTSn */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_row9"),
			       SYNA_PINCTRL_FUNCTION(0x7, "dsi")), /* TE */
	SYNA_PINCTRLCONF_GROUP("GPIO47", 0x8008, 0x3, 0x0c, 0x8860,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO47 */
			       SYNA_PINCTRL_FUNCTION(0x1, "sdio1"), /* WP */
			       SYNA_PINCTRL_FUNCTION(0x2, "sdio2"), /* WP */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col1"),
			       SYNA_PINCTRL_FUNCTION(0x4, "rmii2"), /* REFCLK */
			       SYNA_PINCTRL_FUNCTION(0x5, "sm_uart1"), /* CTSn */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_row8")),
	SYNA_PINCTRLCONF_GROUP("GPIO48", 0x8008, 0x3, 0x0f, 0x8864,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO48 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* TD0 */
			       SYNA_PINCTRL_FUNCTION(0x5, "spi5"), /* SS0n */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4"), /* SS2n */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SS3n */
	SYNA_PINCTRLCONF_GROUP("GPIO49", 0x8008, 0x3, 0x12, 0x8868,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO49 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* TD1 */
			       SYNA_PINCTRL_FUNCTION(0x5, "spi5"), /* SDO */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4"), /* SS3n */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SS2n */
	SYNA_PINCTRLCONF_GROUP("GPIO50", 0x8008, 0x3, 0x15, 0x886c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO50 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* TD2 */
			       SYNA_PINCTRL_FUNCTION(0x5, "spi5"), /* SCLK */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SS1n */
	SYNA_PINCTRLCONF_GROUP("GPIO51", 0x8008, 0x3, 0x18, 0x8870,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO51 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* TD3 */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SS0n */
	SYNA_PINCTRLCONF_GROUP("GPIO52", 0x8008, 0x3, 0x1b, 0x8874,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO52 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* RD0 */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4"), /* SS2n */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SDO */
	SYNA_PINCTRLCONF_GROUP("GPIO53", 0x800c, 0x3, 0x00, 0x8878,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO53 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* RD1 */
			       SYNA_PINCTRL_FUNCTION(0x6, "spi4"), /* SS3n */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SCLK */
	SYNA_PINCTRLCONF_GROUP("GPIO54", 0x800c, 0x3, 0x03, 0x887c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO54 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* RD2 */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SDI */
	SYNA_PINCTRLCONF_GROUP("GPIO55", 0x800c, 0x3, 0x06, 0x8880,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO55 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* RD3 */
			       SYNA_PINCTRL_FUNCTION(0x5, "spi5"), /* SDI */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SS1n */
	SYNA_PINCTRLCONF_GROUP("GPIO56", 0x800c, 0x3, 0x09, 0x8884,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO56 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* RXC */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SS0n */
	SYNA_PINCTRLCONF_GROUP("GPIO57", 0x800c, 0x3, 0x0c, 0x8888,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO57 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* TXC */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SDO */
	SYNA_PINCTRLCONF_GROUP("GPIO58", 0x800c, 0x3, 0x0f, 0x888c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO58 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* TXCTL */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SCLK */
	SYNA_PINCTRLCONF_GROUP("GPIO59", 0x800c, 0x3, 0x12, 0x8890,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO59 */
			       SYNA_PINCTRL_FUNCTION(0x1, "rgmii2"), /* RXCTL */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi4")), /* SDI */
	SYNA_PINCTRLCONF_GROUP("GPIO0", 0x800c, 0x3, 0x15, 0x8894,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO0 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s1"), /* LRCK */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SS0n */
	SYNA_PINCTRLCONF_GROUP("GPIO1", 0x800c, 0x3, 0x18, 0x8898,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO1 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s1"), /* BCLK */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SCLK */
	SYNA_PINCTRLCONF_GROUP("GPIO2", 0x800c, 0x3, 0x1b, 0x889c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO2*/
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s1"), /* DO */
			       SYNA_PINCTRL_FUNCTION(0x4, "spdif"), /* O */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SDO */
	SYNA_PINCTRLCONF_GROUP("GPIO3", 0x8010, 0x3, 0x00, 0x88a0,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO3 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s1"), /* MCLK */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SS1n */
	SYNA_PINCTRLCONF_GROUP("GPIO4", 0x8010, 0x3, 0x03, 0x88a4,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO4 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s1"), /* DI */
			       SYNA_PINCTRL_FUNCTION(0x4, "spdif"), /* I */
			       SYNA_PINCTRL_FUNCTION(0x7, "spi3")), /* SDI */
	SYNA_PINCTRLCONF_GROUP("GPIO5", 0x8010, 0x3, 0x06, 0x88a8,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO5 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s2"), /* LRCK */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* PIXCLK */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row0"),
			       SYNA_PINCTRL_FUNCTION(0x4, "spdif"), /* I */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col7")),
	SYNA_PINCTRLCONF_GROUP("GPIO6", 0x8010, 0x3, 0x09, 0x88ac,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO6 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s2"), /* BCLK */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* HSYNC */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row1"),
			       SYNA_PINCTRL_FUNCTION(0x4, "spdif"), /* O */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col6")),
	SYNA_PINCTRLCONF_GROUP("GPIO7", 0x8010, 0x3, 0x0c, 0x88b0,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO7 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s2"), /* DO */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row2"),
			       SYNA_PINCTRL_FUNCTION(0x4, "spdif"), /* O */
			       SYNA_PINCTRL_FUNCTION(0x5, "sm_pdm"), /* CLKIO */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col5")),
	SYNA_PINCTRLCONF_GROUP("GPIO8", 0x8010, 0x3, 0x0f, 0x88b4,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO8 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s2"), /* DI */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* VSYNC */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row3"),
			       SYNA_PINCTRL_FUNCTION(0x4, "spdif"), /* I */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_col4")),
	SYNA_PINCTRLCONF_GROUP("GPIO9", 0x8010, 0x3, 0x12, 0x88b8,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO9 */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* DATA0 */
			       SYNA_PINCTRL_FUNCTION(0x4, "pdm")), /* DI1 */
	SYNA_PINCTRLCONF_GROUP("GPIO10", 0x8010, 0x3, 0x15, 0x88bc,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO10 */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* DATA1 */
			       SYNA_PINCTRL_FUNCTION(0x4, "pdm"), /* DI2 */
			       SYNA_PINCTRL_FUNCTION(0x5, "dsi")), /* TE */
	SYNA_PINCTRLCONF_GROUP("GPIO11", 0x8010, 0x3, 0x18, 0x88c0,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO11 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s2"), /* MCLK */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* DATA2 */
			       SYNA_PINCTRL_FUNCTION(0x4, "sm_pdm"), /* CLKIO */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* CLK */
	SYNA_PINCTRLCONF_GROUP("GPIO12", 0x8010, 0x3, 0x1b, 0x88c4,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO12 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s3"), /* LRCK */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* DATA3 */
			       SYNA_PINCTRL_FUNCTION(0x3, "sdio2"), /* WP */
			       SYNA_PINCTRL_FUNCTION(0x4, "sdio1"), /* WP */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* OUT0 */
	SYNA_PINCTRLCONF_GROUP("GPIO13", 0x8014, 0x3, 0x00, 0x88c8,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO13 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s3"), /* BCLK */
			       SYNA_PINCTRL_FUNCTION(0x2, "pdm"), /* DI1 */
			       SYNA_PINCTRL_FUNCTION(0x3, "rgmii"), /* PTP_PPS_O */
			       SYNA_PINCTRL_FUNCTION(0x4, "usb2"), /* VBUS */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* OUT1 */
	SYNA_PINCTRLCONF_GROUP("GPIO14", 0x8014, 0x3, 0x03, 0x88cc,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO14 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s3"), /* DO */
			       SYNA_PINCTRL_FUNCTION(0x4, "pdm"), /* DI3 */
			       SYNA_PINCTRL_FUNCTION(0x6, "spdif"), /* O */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* OUT2 */
	SYNA_PINCTRLCONF_GROUP("GPIO15", 0x8014, 0x3, 0x06, 0x88d0,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO15 */
			       SYNA_PINCTRL_FUNCTION(0x1, "i2s3"), /* DI */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* DATA4 */
			       SYNA_PINCTRL_FUNCTION(0x4, "sm_pdm"), /* DI0 */
			       SYNA_PINCTRL_FUNCTION(0x6, "spdif"), /* I */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* OUT3 */
	SYNA_PINCTRLCONF_GROUP("GPIO16", 0x8014, 0x3, 0x09, 0x88d4,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO16 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi2"), /* SS0n */
			       SYNA_PINCTRL_FUNCTION(0x3, "sdio2")), /* DAT3 */
	SYNA_PINCTRLCONF_GROUP("GPIO17", 0x8014, 0x3, 0x0c, 0x88d8,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO17 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi2"), /* SS1n */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* DATA5 */
			       SYNA_PINCTRL_FUNCTION(0x3, "sdio2"), /* DAT2 */
			       SYNA_PINCTRL_FUNCTION(0x5, "dsi")), /* TE */
	SYNA_PINCTRLCONF_GROUP("GPIO18", 0x8014, 0x3, 0x0f, 0x88dc,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO18 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi2"), /* SS2n */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* DATA6 */
			       SYNA_PINCTRL_FUNCTION(0x3, "sdio2"), /* DAT1 */
			       SYNA_PINCTRL_FUNCTION(0x4, "pdm"), /* DI2 */
			       SYNA_PINCTRL_FUNCTION(0x5, "can1")), /* RX */
	SYNA_PINCTRLCONF_GROUP("GPIO19", 0x8014, 0x3, 0x12, 0x88e0,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO19 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi2"), /* SS3n */
			       SYNA_PINCTRL_FUNCTION(0x2, "cam"), /* DATA7 */
			       SYNA_PINCTRL_FUNCTION(0x3, "sdio2"), /* DAT0 */
			       SYNA_PINCTRL_FUNCTION(0x4, "pdm"), /* DI3 */
			       SYNA_PINCTRL_FUNCTION(0x5, "can1")), /* TX */
	SYNA_PINCTRLCONF_GROUP("GPIO20", 0x8014, 0x3, 0x15, 0x88e4,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO20 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi2"), /* SDO */
			       SYNA_PINCTRL_FUNCTION(0x3, "sdio2")), /* CMD */
	SYNA_PINCTRLCONF_GROUP("GPIO21", 0x8014, 0x3, 0x18, 0x88e8,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO21 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi2"), /* SCLK */
			       SYNA_PINCTRL_FUNCTION(0x3, "sdio2"), /* CLK */
			       SYNA_PINCTRL_FUNCTION(0x5, "clkout")),
	SYNA_PINCTRLCONF_GROUP("GPIO22", 0x8014, 0x3, 0x1b, 0x88ec,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* GPIO22 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi2"), /* SDI */
			       SYNA_PINCTRL_FUNCTION(0x3, "sdio2"), /* CDn */
			       SYNA_PINCTRL_FUNCTION(0x4, "sdio1")), /* CDn */
};

static const struct syna_desc_group sl2610_sysmgr_pinctrl_groups[] = {
	SYNA_PINCTRLCONF_GROUP("SM_GPIO31", 0xb00, 0x3, 0x00, 0xc00,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO31*/
			       SYNA_PINCTRL_FUNCTION(0x1, "pwm"), /* SM PWM0*/
			       SYNA_PINCTRL_FUNCTION(0x2, "uart1"), /* SM UART1 RXD */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row7"),
			       SYNA_PINCTRL_FUNCTION(0x4, "pdm"), /* SM PDM DI0 */
			       SYNA_PINCTRL_FUNCTION(0x5, "uart0"), /* SM UART0 RXD */
			       SYNA_PINCTRL_FUNCTION(0x6, "can1")), /* SM CAN1 RX */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO32", 0xb00, 0x3, 0x03, 0xc04,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO32 */
			       SYNA_PINCTRL_FUNCTION(0x1, "pwm"), /* SM PWM1 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart1"), /* SM UART1 TXD */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col0"),
			       SYNA_PINCTRL_FUNCTION(0x4, "pdm"), /* SM PDM CLKIO */
			       SYNA_PINCTRL_FUNCTION(0x5, "uart0"), /* SM UART0 TXD */
			       SYNA_PINCTRL_FUNCTION(0x6, "can1")), /* SM CAN1 TX */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO33", 0xb00, 0x3, 0x06, 0xc08,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO33 */
			       SYNA_PINCTRL_FUNCTION(0x1, "pwm"), /* SM PWM2 */
			       SYNA_PINCTRL_FUNCTION(0x3, "uart2"), /* SM UART2 TXD */
			       SYNA_PINCTRL_FUNCTION(0x4, "uart3"), /* SM UART3 RTSn */
			       SYNA_PINCTRL_FUNCTION(0x5, "key_row5"),
			       SYNA_PINCTRL_FUNCTION(0x6, "uart3_de")), /* SM UART3 DE */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO34", 0xb00, 0x3, 0x09, 0xc0c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO34 */
			       SYNA_PINCTRL_FUNCTION(0x1, "pwm"), /* SM PWM3 */
			       SYNA_PINCTRL_FUNCTION(0x3, "uart2"), /* SM UART2 RXD */
			       SYNA_PINCTRL_FUNCTION(0x4, "uart3"), /* SM UART3 CTSn */
			       SYNA_PINCTRL_FUNCTION(0x5, "key_row4"),
			       SYNA_PINCTRL_FUNCTION(0x6, "uart3_ren")), /* SM UART3 REn */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO35", 0xb00, 0x3, 0x0c, 0xc10,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO35 */
			       SYNA_PINCTRL_FUNCTION(0x1, "pwm"), /* SM PWM4 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart1"), /* SM UART1 RTSn */
			       SYNA_PINCTRL_FUNCTION(0x3, "uart3"), /* SM UART3 TXD */
			       SYNA_PINCTRL_FUNCTION(0x4, "uart2"), /* SM UART2 RTSn */
			       SYNA_PINCTRL_FUNCTION(0x5, "key_row3"),
			       SYNA_PINCTRL_FUNCTION(0x6, "uart0")), /* SM UART0 RTSn */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO36", 0xb00, 0x3, 0x0f, 0xc14,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO36 */
			       SYNA_PINCTRL_FUNCTION(0x1, "pwm"), /* SM PWM5 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart1"), /* SM UART1 CTSn */
			       SYNA_PINCTRL_FUNCTION(0x3, "uart3"), /* SM UART3 RXD */
			       SYNA_PINCTRL_FUNCTION(0x4, "uart2"), /* SM UART2 CTSn */
			       SYNA_PINCTRL_FUNCTION(0x5, "key_row2"),
			       SYNA_PINCTRL_FUNCTION(0x6, "uart0")), /* SM UART0 CTSn */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO37", 0xb00, 0x3, 0x12, 0xc18,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO37 */
			       SYNA_PINCTRL_FUNCTION(0x1, "pwm"), /* SM PWM6 */
			       SYNA_PINCTRL_FUNCTION(0x3, "tw0"), /* SM TW0 SCL */
			       SYNA_PINCTRL_FUNCTION(0x4, "key_row6"),
			       SYNA_PINCTRL_FUNCTION(0x5, "pdm")), /* SM PDM CLKIO */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO38", 0xb00, 0x3, 0x15, 0xc1c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO38 */
			       SYNA_PINCTRL_FUNCTION(0x1, "pwm"), /* SM PWM7 */
			       SYNA_PINCTRL_FUNCTION(0x3, "tw0"), /* SM TW0 SDA */
			       SYNA_PINCTRL_FUNCTION(0x5, "pdm")), /* SM PDM DI0 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO3", 0xb00, 0x3, 0x18, 0xc20,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO3 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi1"), /* SM SPI1 SS0n */
			       SYNA_PINCTRL_FUNCTION(0x2, "tw1"), /* SM TW1 SCL */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm"), /* SM PWM8 */
			       SYNA_PINCTRL_FUNCTION(0x4, "i3c")), /* SM I3C MS SCL */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO4", 0xb00, 0x3, 0x1b, 0xc24,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO4 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi1"), /* SM SPI1 SS1n */
			       SYNA_PINCTRL_FUNCTION(0x2, "tw1"), /* SM TW1 SDA */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm"), /* SM PWM0 */
			       SYNA_PINCTRL_FUNCTION(0x4, "i3c")), /* SM I3C MS SDA */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO5", 0xb04, 0x3, 0x00, 0xc28,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO5 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi1"), /* SM SPI1 SS2n */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm")), /* SM PWM1 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO6", 0xb04, 0x3, 0x03, 0xc2c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO6 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi1"), /* SM SPI1 SS3n */
			       SYNA_PINCTRL_FUNCTION(0x2, "spi1s"), /* SM SPI1S SSn */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm")), /* SM PWM2 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO9", 0xb04, 0x3, 0x06, 0xc30,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO9 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi1"), /* SM SPI1 SDO */
			       SYNA_PINCTRL_FUNCTION(0x2, "spi1s"), /* SM SPI1S SDO */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm")), /* SM PWM3 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO10", 0xb04, 0x3, 0x09, 0xc34,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO10 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi1"), /* SM SPI1 SCLK */
			       SYNA_PINCTRL_FUNCTION(0x2, "spi1s"), /* SM SPI1S SCLK */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm")), /* SM PWM4 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO11", 0xb04, 0x3, 0x0c, 0xc38,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO11 */
			       SYNA_PINCTRL_FUNCTION(0x1, "spi1"), /* SM SPI1 SDI */
			       SYNA_PINCTRL_FUNCTION(0x2, "spi1s"), /* SM SPI1S SDI */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm")), /* SM PWM5 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO30", 0xb04, 0x3, 0x0f, 0xc44,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO30 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi"), /* SM XSPI DATA7 */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col6"),
			       SYNA_PINCTRL_FUNCTION(0x7, "sm_clkout")),
	SYNA_PINCTRLCONF_GROUP("SM_GPIO29", 0xb04, 0x3, 0x12, 0xc48,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO29 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi"), /* SM XSPI DATA6 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart3"), /* SM UART3 RXD */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col5"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart2"), /* SM UART2 CTSn */
			       SYNA_PINCTRL_FUNCTION(0x5, "uart0"), /* SM UART0 CTSn */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_row2"),
			       SYNA_PINCTRL_FUNCTION(0x7, "uart1")), /* SM UART1 CTSn */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO28", 0xb04, 0x3, 0x15, 0xc4c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO28 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi"), /* SM XSPI DATA5 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart3"), /* SM UART3 TXD */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col4"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart2"), /* SM UART2 RTSn */
			       SYNA_PINCTRL_FUNCTION(0x5, "uart0"), /* SM UART0 RTSn */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_row3"),
			       SYNA_PINCTRL_FUNCTION(0x7, "uart1")), /* SM UART1 RTSn */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO27", 0xb04, 0x3, 0x18, 0xc50,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO27 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi"), /* SM XSPI DATA4 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart2"), /* SM UART2 RXD */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col3"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart3"), /* SM UART3 CTSn */
			       SYNA_PINCTRL_FUNCTION(0x5, "uart3_ren"), /* SM UART3 REn */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_row4")),
	SYNA_PINCTRLCONF_GROUP("SM_GPIO26", 0xb04, 0x3, 0x1b, 0xc54,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO26 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi"), /* SM XSPI CS1n */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart2"), /* SM UART2 TXD */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col2"),
			       SYNA_PINCTRL_FUNCTION(0x4, "uart3"), /* SM UART3 RTSn */
			       SYNA_PINCTRL_FUNCTION(0x5, "uart3_de"), /* SM UART3 DE */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_row5"),
			       SYNA_PINCTRL_FUNCTION(0x7, "sm_clkout")),
	SYNA_PINCTRLCONF_GROUP("SM_GPIO25", 0xb08, 0x3, 0x00, 0xc58,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO25 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi"), /* SM XSPI DQS */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_row6")),
	SYNA_PINCTRLCONF_GROUP("SM_GPIO24", 0xb08, 0x3, 0x03, 0xc5c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO24 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi")), /* SM XSPI CLKn */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO23", 0xb08, 0x3, 0x06, 0xc60,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO23 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi")), /* SM XSPI CLK */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO22", 0xb08, 0x3, 0x09, 0xc64,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO22 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi")), /* SM XSPI DATA3 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO21", 0xb08, 0x3, 0x0c, 0xc68,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO21 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi")), /* SM XSPI DATA2 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO20", 0xb08, 0x3, 0x0f, 0xc6c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO20 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi")), /* SM XSPI DATA1 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO19", 0xb08, 0x3, 0x12, 0xc70,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO19 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi")), /* SM XSPI DATA0 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO18", 0xb08, 0x3, 0x15, 0xc74,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO18 */
			       SYNA_PINCTRL_FUNCTION(0x1, "xspi")), /* SM XSPI CS0n*/
	SYNA_PINCTRLCONF_GROUP("SM_GPIO17", 0xb08, 0x3, 0x18, 0xc78,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO17 */
			       SYNA_PINCTRL_FUNCTION(0x1, "uart1"), /* SM UART1 TXD */
			       SYNA_PINCTRL_FUNCTION(0x2, "can0"), /* SM CAN0 TX */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm"), /* SM PWM8 */
			       SYNA_PINCTRL_FUNCTION(0x6, "uart0")), /* SM UART0 TXD */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO15", 0xb08, 0x3, 0x1b, 0xc7c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO15 */
			       SYNA_PINCTRL_FUNCTION(0x1, "tw1"), /* SM TW1 SDA */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart0"), /* SM UART0 RTSn */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm"), /* SM PWM11 */
			       SYNA_PINCTRL_FUNCTION(0x4, "can0"), /* SM CAN0 TX */
			       SYNA_PINCTRL_FUNCTION(0x6, "uart1"), /* SM UART1 RTSn */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* DBG OUT */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO14", 0xb0c, 0x3, 0x00, 0xc80,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO14 */
			       SYNA_PINCTRL_FUNCTION(0x1, "tw1"), /* SM TW1 SCL */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart0"), /* SM UART0 CTSn */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm"), /* SM PWM10 */
			       SYNA_PINCTRL_FUNCTION(0x4, "can0"), /* SM CAN0 RX */
			       SYNA_PINCTRL_FUNCTION(0x6, "uart1"), /* SM UART1 CTSn */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* DBG OUT */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO16", 0xb0c, 0x3, 0x03, 0xc84,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO16 */
			       SYNA_PINCTRL_FUNCTION(0x1, "uart1"), /* SM UART1 RXD */
			       SYNA_PINCTRL_FUNCTION(0x2, "can0"), /* SM CAN0 RX */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm")), /* SM PWM7 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO13", 0xb0c, 0x3, 0x06, 0xc88,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO13 */
			       SYNA_PINCTRL_FUNCTION(0x1, "tw0"), /* SM TW0 SDA */
			       SYNA_PINCTRL_FUNCTION(0x2, "i3c"), /* SM I3C MS SDA */
			       SYNA_PINCTRL_FUNCTION(0x3, "sm_clkout"),
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* DBG out */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO12", 0xb0c, 0x3, 0x09, 0xc8c,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO12 */
			       SYNA_PINCTRL_FUNCTION(0x1, "tw0"), /* SM TW0 SCL */
			       SYNA_PINCTRL_FUNCTION(0x2, "i3c"), /* SM I3C MS SCL */
			       SYNA_PINCTRL_FUNCTION(0x3, "pwm"), /* SM PWM6 */
			       SYNA_PINCTRL_FUNCTION(0x7, "dbg")), /* DBG CLK */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO8", 0xb0c, 0x3, 0x0c, 0xc90,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO8 */
			       SYNA_PINCTRL_FUNCTION(0x1, "uart0"), /* SM UART0 TXD */
			       SYNA_PINCTRL_FUNCTION(0x2, "can0"), /* SM CAN0 TX */
			       SYNA_PINCTRL_FUNCTION(0x3, "sm_clkout"),
			       SYNA_PINCTRL_FUNCTION(0x6, "uart1")), /* SM UART1 TXD */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO7", 0xb0c, 0x3, 0x0f, 0xc94,
			       SYNA_PINCTRL_FUNCTION(0x0, "gpio"), /* SM GPIO7 */
			       SYNA_PINCTRL_FUNCTION(0x1, "uart0"), /* SM UART0 RXD */
			       SYNA_PINCTRL_FUNCTION(0x2, "can0"), /* SM CAN0 RX */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_row6"),
			       SYNA_PINCTRL_FUNCTION(0x4, "gpio_trig"), /* TRIG2 */
			       SYNA_PINCTRL_FUNCTION(0x5, "pwm"), /* SM PWM9 */
			       SYNA_PINCTRL_FUNCTION(0x6, "uart1")), /* SM UART1 RXD */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO2", 0xb0c, 0x3, 0x12, 0xc98,
			       SYNA_PINCTRL_FUNCTION(0x0, "jtag"), /* TDO */
			       SYNA_PINCTRL_FUNCTION(0x1, "gpio"), /* SM GPIO2 */
			       SYNA_PINCTRL_FUNCTION(0x2, "pdm"), /* SM PDM CLKIO */
			       SYNA_PINCTRL_FUNCTION(0x3, "i2s2"), /* MCLK */
			       SYNA_PINCTRL_FUNCTION(0x6, "pwm")), /* SM PWM11 */
	SYNA_PINCTRLCONF_GROUP("SM_GPIO1", 0xb0c, 0x3, 0x15, 0xc9c,
			       SYNA_PINCTRL_FUNCTION(0x0, "jtag"), /* TDI */
			       SYNA_PINCTRL_FUNCTION(0x1, "gpio"), /* SM GPIO1 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart0"), /* SM UART0 RXD */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col1"),
			       SYNA_PINCTRL_FUNCTION(0x4, "gpio_trig"), /* TRIG0 */
			       SYNA_PINCTRL_FUNCTION(0x5, "pwm"), /* SM PWM9 */
			       SYNA_PINCTRL_FUNCTION(0x6, "key_row7")),
	SYNA_PINCTRLCONF_GROUP("SM_GPIO0", 0xb0c, 0x3, 0x18, 0xca0,
			       SYNA_PINCTRL_FUNCTION(0x0, "jtag"), /* TMS */
			       SYNA_PINCTRL_FUNCTION(0x1, "gpio"), /* SM GPIO0 */
			       SYNA_PINCTRL_FUNCTION(0x2, "uart0"), /* SM UART0 TXD */
			       SYNA_PINCTRL_FUNCTION(0x3, "key_col0"),
			       SYNA_PINCTRL_FUNCTION(0x5, "pdm"), /* SM PDM DI0 */
			       SYNA_PINCTRL_FUNCTION(0x6, "pwm")), /* SM PWM10 */
};

static const struct syna_pinctrl_desc sl2610_soc_pinctrl_data = {
	.groups = sl2610_soc_pinctrl_groups,
	.ngroups = ARRAY_SIZE(sl2610_soc_pinctrl_groups),
};

static const struct syna_pinctrl_desc sl2610_sysmgr_pinctrl_data = {
	.groups = sl2610_sysmgr_pinctrl_groups,
	.ngroups = ARRAY_SIZE(sl2610_sysmgr_pinctrl_groups),
};

static int syna_pinctrl_get_groups_count(struct udevice *dev)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->desc->ngroups;
}

static const char *syna_pinctrl_get_group_name(struct udevice *dev,
					       unsigned int selector)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->desc->groups[selector].name;
}

static int syna_pinctrl_get_pins_count(struct udevice *dev)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->desc->ngroups;
}

static const char *syna_pinctrl_get_pin_name(struct udevice *dev,
					     unsigned int selector)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->desc->groups[selector].name;
}

static int syna_pinctrl_get_pin_muxing(struct udevice *dev,
				       unsigned int selector, char *buf,
				       int size)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);
	const struct syna_desc_group *group = pctrl->desc->groups + selector;
	const struct syna_desc_function *function = group->functions;
	u32 mask = GENMASK(group->lsb + group->bit_width - 1, group->lsb);
	u32 muxval;
	int err;

	err = regmap_read(pctrl->regmap, group->offset, &muxval);
	if (err)
		return err;

	muxval = (muxval & mask) >> group->lsb;

	while (function->name) {
		if (function->muxval == muxval) {
			snprintf(buf, size, "%s", function->name);
			return 0;
		}
		function++;
	}

	snprintf(buf, size, "unknown (0x%x)", muxval);

	return 0;
}

static int syna_pinmux_get_functions_count(struct udevice *dev)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->nfunctions;
}

static const char *syna_pinmux_get_function_name(struct udevice *dev,
						 unsigned int selector)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->function_names[selector];
}

static const struct pinconf_param syna_conf_params[] = {
	{ "bias-disable", PIN_CONFIG_BIAS_DISABLE, 0 },
	{ "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 1 },
	{ "bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 1 },
	{ "drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 0 },
	{ "input-disable", PIN_CONFIG_INPUT_ENABLE, 0 },
	{ "input-enable", PIN_CONFIG_INPUT_ENABLE, 1 },
	{ "input-schmitt-enable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 1 },
	{ "input-schmitt-disable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 0 },
	{ "slew-rate", PIN_CONFIG_SLEW_RATE, 0 },
};

static int syna_pinconf_group_set(struct udevice *dev,
				  unsigned int group,
				  unsigned int param, unsigned int arg)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);
	const struct syna_desc_group *group_desc = pctrl->desc->groups + group;
	u32 mask = 0, val = 0;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
		mask |= (SYNA_PINCFG_PE | SYNA_PINCFG_PS | SYNA_PINCFG_SPU);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		mask |= (SYNA_PINCFG_PE | SYNA_PINCFG_PS | SYNA_PINCFG_SPU);
		val |= SYNA_PINCFG_PE;
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		mask |= (SYNA_PINCFG_PE | SYNA_PINCFG_PS | SYNA_PINCFG_SPU);
		val |= (SYNA_PINCFG_PE | SYNA_PINCFG_PS);
		switch (arg) {
		case 1:
			break;
		case 2:
			/* Hijacking 2Ohms value for strong pull-up feel hacky */
			val |= SYNA_PINCFG_SPU;
			break;
		default:
			return -EINVAL;
		}
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		mask |= SYNA_PINCFG_DS;
		if (arg > FIELD_MAX(mask))
			return -EINVAL;
		val |= FIELD_PREP(mask, arg);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		mask |= SYNA_PINCFG_IE;
		if (arg)
			val |= SYNA_PINCFG_IE;
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		mask |= SYNA_PINCFG_ST;
		if (arg)
			val |= SYNA_PINCFG_ST;
		break;
	case PIN_CONFIG_SLEW_RATE:
		mask |= SYNA_PINCFG_SL;
		if (arg)
			val |= SYNA_PINCFG_SL;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return regmap_update_bits(pctrl->regmap, group_desc->conf_offset, mask, val);
}

static const struct syna_desc_function *
syna_pinctrl_find_function_by_name(const struct syna_desc_group *group,
				   const char *fname)
{
	const struct syna_desc_function *function = group->functions;

	while (function->name) {
		if (!strcmp(function->name, fname))
			return function;

		function++;
	}

	return NULL;
}

static int syna_pinmux_group_set(struct udevice *dev, unsigned int group,
				 unsigned int function)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);
	const struct syna_desc_group *group_desc = pctrl->desc->groups + group;
	const char *fname = pctrl->function_names[function];
	const struct syna_desc_function *function_desc;
	u32 mask, val;

	function_desc = syna_pinctrl_find_function_by_name(group_desc, fname);

	if (!function_desc)
		return -EINVAL;

	mask = GENMASK(group_desc->lsb + group_desc->bit_width - 1,
		       group_desc->lsb);
	val = function_desc->muxval << group_desc->lsb;

	return regmap_update_bits(pctrl->regmap, group_desc->offset, mask, val);
}

static const struct pinctrl_ops syna_pinctrl_ops = {
	.get_pins_count = syna_pinctrl_get_pins_count,
	.get_pin_name = syna_pinctrl_get_pin_name,
	.get_groups_count = syna_pinctrl_get_groups_count,
	.get_group_name = syna_pinctrl_get_group_name,
	.get_functions_count = syna_pinmux_get_functions_count,
	.get_function_name = syna_pinmux_get_function_name,
	.pinmux_group_set = syna_pinmux_group_set,
	.pinconf_num_params = ARRAY_SIZE(syna_conf_params),
	.pinconf_params = syna_conf_params,
	.pinconf_group_set = syna_pinconf_group_set,
	.set_state = pinctrl_generic_set_state,
	.get_pin_muxing = syna_pinctrl_get_pin_muxing,
};

static void syna_pinctrl_add_function(struct syna_pinctrl *pctrl,
				      const char *name)
{
	const char **fname = pctrl->function_names;

	while (*fname) {
		if (!strcmp(*fname, name))
			/* name already exists */
			return;
		fname++;
	}

	*fname = name;

	pctrl->nfunctions++;
}

static int syna_pinctrl_build_state(struct udevice *dev)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);
	const struct syna_desc_group *desc_group;
	const struct syna_desc_function *desc_function;
	const char **new_function_names;
	int i, max_functions = 0;
	int ret;

	pctrl->nfunctions = 0;

	/* compute the theoretical maximum number of functions */
	for (i = 0; i < pctrl->desc->ngroups; i++) {
		desc_group = pctrl->desc->groups + i;
		max_functions += 1 << desc_group->bit_width;
	}

	/* allocate the function_names array, plus a sentinel */
	pctrl->function_names = calloc(max_functions + 1,
				       sizeof(*pctrl->function_names));
	if (!pctrl->function_names)
		return -ENOMEM;

	/* register all functions */
	for (i = 0; i < pctrl->desc->ngroups; i++) {
		desc_group = pctrl->desc->groups + i;
		desc_function = desc_group->functions;

		while (desc_function->name) {
			syna_pinctrl_add_function(pctrl, desc_function->name);
			desc_function++;
		}
	}

	if (pctrl->nfunctions == 0) {
		ret = -EINVAL;
		goto error;
	}

	/*
	 * reallocate the functions name pointer to the real value.
	 * NB: there's no need for a sentinel now.
	 */
	new_function_names = realloc(pctrl->function_names,
				     pctrl->nfunctions * sizeof(*pctrl->function_names));
	if (!new_function_names) {
		ret = -ENOMEM;
		goto error;
	}

	pctrl->function_names = new_function_names;

	return 0;
error:
	free(pctrl->function_names);
	pctrl->function_names = NULL;
	pctrl->nfunctions = 0;
	return ret;
}

static int syna_pinctrl_probe_regmap(struct udevice *dev,
				     const struct syna_pinctrl_desc *desc,
				     struct regmap *regmap)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);
	int ret;

	pctrl->desc = desc;
	pctrl->regmap = regmap;

	ret = syna_pinctrl_build_state(dev);
	if (ret) {
		dev_err(dev, "cannot build driver state: %d\n", ret);
		return ret;
	}

	return 0;
}

static int syna_pinctrl_probe(struct udevice *dev)
{
	struct regmap *regmap = syscon_node_to_regmap(dev_ofnode(dev->parent));
	const struct syna_pinctrl_desc *desc;

	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	desc = (const struct syna_pinctrl_desc *)dev_get_driver_data(dev);

	return syna_pinctrl_probe_regmap(dev, desc, regmap);
}

static const struct udevice_id syna_pinctrl_match[] = {
	{
		.compatible = "syna,sl2610-soc-pinctrl",
		.data = (ulong)&sl2610_soc_pinctrl_data,
	},
	{
		.compatible = "syna,sl2610-system-pinctrl",
		.data = (ulong)&sl2610_sysmgr_pinctrl_data,
	},
	{}
};

U_BOOT_DRIVER(syna_pinctrl) = {
	.name		= "syna-pinctrl",
	.id		= UCLASS_PINCTRL,
	.of_match	= syna_pinctrl_match,
	.ops		= &syna_pinctrl_ops,
	.probe		= syna_pinctrl_probe,
	.priv_auto	= sizeof(struct syna_pinctrl),
};
