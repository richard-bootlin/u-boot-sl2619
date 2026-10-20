// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Synaptics Incorporated
 *
 */

#include <asm/io.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/fdtaddr.h>
#include <dm/pinctrl.h>

struct syna_desc_func {
	const char *name;
	u8 muxval;
};

struct syna_desc_group {
	const char *name;
	u8 offset;
	u8 bit_width;
	u8 lsb;
	struct syna_desc_func *funcs;
};

struct syna_pinctrl_desc {
	const struct syna_desc_group *groups;
	u32 ngroups;
};

struct syna_pinctrl_func {
	const char *name;
	const char **groups;
	u32 ngroups;
};

#define SYNA_PINCTRL_GROUP(_name, _offset, _width, _lsb, ...)		\
	{								\
		.name = _name,						\
		.offset = _offset,					\
		.bit_width = _width,					\
		.lsb = _lsb,						\
		.funcs = (struct syna_desc_func[]){		\
			__VA_ARGS__, { } },				\
	}

#define SYNA_PINCTRL_FUNC(_muxval, _name)		\
	{						\
		.name = _name,				\
		.muxval = _muxval,			\
	}

#define SYNA_PINCTRL_FUNC_UNKNOWN		{}

struct syna_pinctrl {
	fdt_addr_t base;
	struct udevice *dev;
	const struct syna_pinctrl_desc *desc;
	struct syna_pinctrl_func *funcs;
	u32 nfuncs;
};

static const struct syna_desc_group sl261x_soc_pinctrl_groups[] = {
	SYNA_PINCTRL_GROUP("GPIO23", 0x0, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO23 */
			   SYNA_PINCTRL_FUNC(0x1, "tw2"), /* SCL */
			   SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* MDC */
			   SYNA_PINCTRL_FUNC(0x3, "key_row0"),
			   SYNA_PINCTRL_FUNC(0x5, "spi3"), /* SS2n */
			   SYNA_PINCTRL_FUNC(0x6, "key_col3"),
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT4 */
	SYNA_PINCTRL_GROUP("GPIO24", 0x0, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO24 */
			   SYNA_PINCTRL_FUNC(0x1, "tw2"), /* SDA */
			   SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* MDIO */
			   SYNA_PINCTRL_FUNC(0x3, "key_row1"),
			   SYNA_PINCTRL_FUNC(0x5, "spi3"), /* SS3n */
			   SYNA_PINCTRL_FUNC(0x6, "key_col2"),
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT5 */
	SYNA_PINCTRL_GROUP("GPIO25", 0x0, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO25 */
			   SYNA_PINCTRL_FUNC(0x1, "uart5"), /* RXD */
			   SYNA_PINCTRL_FUNC(0x2, "gpio_trig1"),
			   SYNA_PINCTRL_FUNC(0x3, "key_row2"),
			   SYNA_PINCTRL_FUNC(0x4, "sm_uart1"), /* RXD */
			   SYNA_PINCTRL_FUNC(0x6, "key_col1"),
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT6 */
	SYNA_PINCTRL_GROUP("GPIO26", 0x0, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO26 */
			   SYNA_PINCTRL_FUNC(0x1, "uart5"), /* TXD */
			   SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* PTP_PPS_O */
			   SYNA_PINCTRL_FUNC(0x4, "sm_uart1"), /* TXD */
			   SYNA_PINCTRL_FUNC(0x5, "usb2"), /* VBUS */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT7 */
	SYNA_PINCTRL_GROUP("GPIO27", 0x0, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO27 */
			   SYNA_PINCTRL_FUNC(0x1, "tw3"), /* SCL */
			   SYNA_PINCTRL_FUNC(0x2, "uart4"), /* TXD */
			   SYNA_PINCTRL_FUNC(0x4, "sm_uart1")), /* RTSn */
	SYNA_PINCTRL_GROUP("GPIO28", 0x0, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO28 */
			   SYNA_PINCTRL_FUNC(0x1, "tw3"), /* SDA */
			   SYNA_PINCTRL_FUNC(0x2, "uart4"), /* RXD */
			   SYNA_PINCTRL_FUNC(0x4, "sm_uart1")), /* CTSn */
	SYNA_PINCTRL_GROUP("GPIO29", 0x0, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO29 */
			   SYNA_PINCTRL_FUNC(0x2, "uart4"), /* DE */
			   SYNA_PINCTRL_FUNC(0x3, "key_row4"),
			   SYNA_PINCTRL_FUNC(0x4, "sm_uart1"), /* RXD */
			   SYNA_PINCTRL_FUNC(0x6, "key_col4"),
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO30", 0x0, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO30 */
			   SYNA_PINCTRL_FUNC(0x2, "uart4"), /* REn */
			   SYNA_PINCTRL_FUNC(0x3, "key_row5"),
			   SYNA_PINCTRL_FUNC(0x4, "sm_uart1"), /* TXD */
			   SYNA_PINCTRL_FUNC(0x6, "key_col5"),
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO31", 0x0, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO31 */
			   SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* MDC */
			   SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SS3n */
			   SYNA_PINCTRL_FUNC(0x6, "spi3"), /* SS2n */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS2n */
	SYNA_PINCTRL_GROUP("GPIO32", 0x0, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO32 */
			   SYNA_PINCTRL_FUNC(0x2, "rgmii"), /* MDIO */
			   SYNA_PINCTRL_FUNC(0x6, "spi3"), /* SS3n */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS3n */
	SYNA_PINCTRL_GROUP("GPIO33", 0x4, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO33 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TD0 */
			   SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* TXD0 */
			   SYNA_PINCTRL_FUNC(0x5, "can0"), /* TX */
			   SYNA_PINCTRL_FUNC(0x6, "spi3"), /* SS1n */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS1n */
	SYNA_PINCTRL_GROUP("GPIO34", 0x4, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO34 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TD1 */
			   SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* TXD1 */
			   SYNA_PINCTRL_FUNC(0x5, "can0")), /* RX */
	SYNA_PINCTRL_GROUP("GPIO35", 0x4, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO35 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TD2 */
			   SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* TXD0 */
			   SYNA_PINCTRL_FUNC(0x3, "key_col7"),
			   SYNA_PINCTRL_FUNC(0x4, "uart5"), /* RXD */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO36", 0x4, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO36 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TD3 */
			   SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* TXD1 */
			   SYNA_PINCTRL_FUNC(0x4, "uart5"), /* TXD */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO37", 0x4, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO37 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RD0 */
			   SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* RXD0 */
			   SYNA_PINCTRL_FUNC(0x5, "can1"), /* TX */
			   SYNA_PINCTRL_FUNC(0x7, "spi5")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO38", 0x4, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO38 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RD1 */
			   SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* RXD1 */
			   SYNA_PINCTRL_FUNC(0x5, "can1"), /* RX */
			   SYNA_PINCTRL_FUNC(0x7, "spi5")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO39", 0x4, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO39 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RD2 */
			   SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* RXD0 */
			   SYNA_PINCTRL_FUNC(0x3, "key_row6"),
			   SYNA_PINCTRL_FUNC(0x4, "uart6"), /* RXD */
			   SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SS1n*/
			   SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SS3n  */
	SYNA_PINCTRL_GROUP("GPIO40", 0x4, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO40 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RD3 */
			   SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* RXD1 */
			   SYNA_PINCTRL_FUNC(0x3, "key_row7"),
			   SYNA_PINCTRL_FUNC(0x4, "uart6"), /* TXD */
			   SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SS2n */
			   SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SS2n  */
	SYNA_PINCTRL_GROUP("GPIO41", 0x4, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO41 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RXC */
			   SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* CRSDV */
			   SYNA_PINCTRL_FUNC(0x7, "spi5")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO42", 0x4, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO42 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TXC */
			   SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* CRSDV */
			   SYNA_PINCTRL_FUNC(0x3, "key_row8"),
			   SYNA_PINCTRL_FUNC(0x4, "uart7"), /* RXD */
			   SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO43", 0x8, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO43 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* TXCTL */
			   SYNA_PINCTRL_FUNC(0x2, "rmii1"), /* TXEN */
			   SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO44", 0x8, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO44 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* RXCTL */
			   SYNA_PINCTRL_FUNC(0x2, "rmii2"), /* TXEN */
			   SYNA_PINCTRL_FUNC(0x3, "key_row9"),
			   SYNA_PINCTRL_FUNC(0x4, "uart7"), /* TXD */
			   SYNA_PINCTRL_FUNC(0x6, "spi4")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO45", 0x8, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO45 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii1"), /* CLKOUT */
			   SYNA_PINCTRL_FUNC(0x2, "rmii1")), /* REFCLK */
	SYNA_PINCTRL_GROUP("GPIO46", 0x8, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO46 */
			   SYNA_PINCTRL_FUNC(0x1, "sdio1"), /* CDn */
			   SYNA_PINCTRL_FUNC(0x2, "sdio2"), /* CDn */
			   SYNA_PINCTRL_FUNC(0x3, "key_col0"),
			   SYNA_PINCTRL_FUNC(0x5, "sm_uart1"), /* RTSn */
			   SYNA_PINCTRL_FUNC(0x6, "key_row9"),
			   SYNA_PINCTRL_FUNC(0x7, "dsi")), /* TE */
	SYNA_PINCTRL_GROUP("GPIO47", 0x8, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO47 */
			   SYNA_PINCTRL_FUNC(0x1, "sdio1"), /* WP */
			   SYNA_PINCTRL_FUNC(0x2, "sdio2"), /* WP */
			   SYNA_PINCTRL_FUNC(0x3, "key_col1"),
			   SYNA_PINCTRL_FUNC(0x4, "rmii2"), /* REFCLK */
			   SYNA_PINCTRL_FUNC(0x5, "sm_uart1"), /* CTSn */
			   SYNA_PINCTRL_FUNC(0x6, "key_row8")),
	SYNA_PINCTRL_GROUP("GPIO48", 0x8, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO48 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TD0 */
			   SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SS0n */
			   SYNA_PINCTRL_FUNC(0x6, "spi4"), /* SS2n */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS3n */
	SYNA_PINCTRL_GROUP("GPIO49", 0x8, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO49 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TD1 */
			   SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SDO */
			   SYNA_PINCTRL_FUNC(0x6, "spi4"), /* SS3n */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS2n */
	SYNA_PINCTRL_GROUP("GPIO50", 0x8, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO50 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TD2 */
			   SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SCLK */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS1n */
	SYNA_PINCTRL_GROUP("GPIO51", 0x8, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO51 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TD3 */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO52", 0x8, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO52 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RD0 */
			   SYNA_PINCTRL_FUNC(0x6, "spi4"), /* SS2n */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO53", 0xc, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO53 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RD1 */
			   SYNA_PINCTRL_FUNC(0x6, "spi4"), /* SS3n */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO54", 0xc, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO54 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RD2 */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO55", 0xc, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO55 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RD3 */
			   SYNA_PINCTRL_FUNC(0x5, "spi5"), /* SDI */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS1n */
	SYNA_PINCTRL_GROUP("GPIO56", 0xc, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO56 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RXC */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO57", 0xc, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO57 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TXC */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO58", 0xc, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO58 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* TXCTL */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO59", 0xc, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO59 */
			   SYNA_PINCTRL_FUNC(0x1, "rgmii2"), /* RXCTL */
			   SYNA_PINCTRL_FUNC(0x7, "spi4")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO0", 0xc, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO0 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* LRCK */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS0n */
	SYNA_PINCTRL_GROUP("GPIO1", 0xc, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO1 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* BCLK */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SCLK */
	SYNA_PINCTRL_GROUP("GPIO2", 0xc, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO2*/
			   SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* DO */
			   SYNA_PINCTRL_FUNC(0x4, "spdif"), /* O */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SDO */
	SYNA_PINCTRL_GROUP("GPIO3", 0x10, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO3 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* MCLK */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SS1n */
	SYNA_PINCTRL_GROUP("GPIO4", 0x10, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO4 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s1"), /* DI */
			   SYNA_PINCTRL_FUNC(0x4, "spdif"), /* I */
			   SYNA_PINCTRL_FUNC(0x7, "spi3")), /* SDI */
	SYNA_PINCTRL_GROUP("GPIO5", 0x10, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO5 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* LRCK */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* PIXCLK */
			   SYNA_PINCTRL_FUNC(0x3, "key_row0"),
			   SYNA_PINCTRL_FUNC(0x4, "spdif"), /* I */
			   SYNA_PINCTRL_FUNC(0x6, "key_col7")),
	SYNA_PINCTRL_GROUP("GPIO6", 0x10, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO6 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* BCLK */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* HSYNC */
			   SYNA_PINCTRL_FUNC(0x3, "key_row1"),
			   SYNA_PINCTRL_FUNC(0x4, "spdif"), /* O */
			   SYNA_PINCTRL_FUNC(0x6, "key_col6")),
	SYNA_PINCTRL_GROUP("GPIO7", 0x10, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO7 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* DO */
			   SYNA_PINCTRL_FUNC(0x3, "key_row2"),
			   SYNA_PINCTRL_FUNC(0x4, "spdif"), /* O */
			   SYNA_PINCTRL_FUNC(0x5, "sm_pdm"), /* CLKIO */
			   SYNA_PINCTRL_FUNC(0x6, "key_col5")),
	SYNA_PINCTRL_GROUP("GPIO8", 0x10, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO8 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* DI */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* VSYNC */
			   SYNA_PINCTRL_FUNC(0x3, "key_row3"),
			   SYNA_PINCTRL_FUNC(0x4, "spdif"), /* I */
			   SYNA_PINCTRL_FUNC(0x6, "key_col4")),
	SYNA_PINCTRL_GROUP("GPIO9", 0x10, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO9 */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA0 */
			   SYNA_PINCTRL_FUNC(0x4, "pdm")), /* DI1 */
	SYNA_PINCTRL_GROUP("GPIO10", 0x10, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO10 */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA1 */
			   SYNA_PINCTRL_FUNC(0x4, "pdm"), /* DI2 */
			   SYNA_PINCTRL_FUNC(0x5, "dsi")), /* TE */
	SYNA_PINCTRL_GROUP("GPIO11", 0x10, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO11 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s2"), /* MCLK */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA2 */
			   SYNA_PINCTRL_FUNC(0x4, "sm_pdm"), /* CLKIO */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* CLK */
	SYNA_PINCTRL_GROUP("GPIO12", 0x10, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO12 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s3"), /* LRCK */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA3 */
			   SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* WP */
			   SYNA_PINCTRL_FUNC(0x4, "sdio1"), /* WP */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT0 */
	SYNA_PINCTRL_GROUP("GPIO13", 0x14, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO13 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s3"), /* BCLK */
			   SYNA_PINCTRL_FUNC(0x2, "pdm"), /* DI1 */
			   SYNA_PINCTRL_FUNC(0x3, "rgmii"), /* PTP_PPS_O */
			   SYNA_PINCTRL_FUNC(0x4, "usb2"), /* VBUS */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT1 */
	SYNA_PINCTRL_GROUP("GPIO14", 0x14, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO14 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s3"), /* DO */
			   SYNA_PINCTRL_FUNC(0x4, "pdm"), /* DI3 */
			   SYNA_PINCTRL_FUNC(0x6, "spdif"), /* O */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT2 */
	SYNA_PINCTRL_GROUP("GPIO15", 0x14, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO15 */
			   SYNA_PINCTRL_FUNC(0x1, "i2s3"), /* DI */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA4 */
			   SYNA_PINCTRL_FUNC(0x4, "sm_pdm"), /* DI0 */
			   SYNA_PINCTRL_FUNC(0x6, "spdif"), /* I */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* OUT3 */
	SYNA_PINCTRL_GROUP("GPIO16", 0x14, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO16 */
			   SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SS0n */
			   SYNA_PINCTRL_FUNC(0x3, "sdio2")), /* DAT3 */
	SYNA_PINCTRL_GROUP("GPIO17", 0x14, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO17 */
			   SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SS1n */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA5 */
			   SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* DAT2 */
			   SYNA_PINCTRL_FUNC(0x5, "dsi")), /* TE */
	SYNA_PINCTRL_GROUP("GPIO18", 0x14, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO18 */
			   SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SS2n */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA6 */
			   SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* DAT1 */
			   SYNA_PINCTRL_FUNC(0x4, "pdm"), /* DI2 */
			   SYNA_PINCTRL_FUNC(0x5, "can1")), /* RX */
	SYNA_PINCTRL_GROUP("GPIO19", 0x14, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO19 */
			   SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SS3n */
			   SYNA_PINCTRL_FUNC(0x2, "cam"), /* DATA7 */
			   SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* DAT0 */
			   SYNA_PINCTRL_FUNC(0x4, "pdm"), /* DI3 */
			   SYNA_PINCTRL_FUNC(0x5, "can1")), /* TX */
	SYNA_PINCTRL_GROUP("GPIO20", 0x14, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO20 */
			   SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SDO */
			   SYNA_PINCTRL_FUNC(0x3, "sdio2")), /* CMD */
	SYNA_PINCTRL_GROUP("GPIO21", 0x14, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO21 */
			   SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SCLK */
			   SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* CLK */
			   SYNA_PINCTRL_FUNC(0x5, "clkout")),
	SYNA_PINCTRL_GROUP("GPIO22", 0x14, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* GPIO22 */
			   SYNA_PINCTRL_FUNC(0x1, "spi2"), /* SDI */
			   SYNA_PINCTRL_FUNC(0x3, "sdio2"), /* CDn */
			   SYNA_PINCTRL_FUNC(0x4, "sdio1")), /* CDn */
};

static const struct syna_desc_group sl261x_sysmgr_pinctrl_groups[] = {
	SYNA_PINCTRL_GROUP("SM_GPIO31", 0x0, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO31*/
			   SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM0*/
			   SYNA_PINCTRL_FUNC(0x2, "uart1"), /* SM UART1 RXD */
			   SYNA_PINCTRL_FUNC(0x3, "key_row7"),
			   SYNA_PINCTRL_FUNC(0x4, "pdm"), /* SM PDM DI0 */
			   SYNA_PINCTRL_FUNC(0x5, "uart0"), /* SM UART0 RXD */
			   SYNA_PINCTRL_FUNC(0x6, "can1")), /* SM CAN1 RX */
	SYNA_PINCTRL_GROUP("SM_GPIO32", 0x0, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO32 */
			   SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM1 */
			   SYNA_PINCTRL_FUNC(0x2, "uart1"), /* SM UART1 TXD */
			   SYNA_PINCTRL_FUNC(0x3, "key_col0"),
			   SYNA_PINCTRL_FUNC(0x4, "pdm"), /* SM PDM CLKIO */
			   SYNA_PINCTRL_FUNC(0x5, "uart0"), /* SM UART0 TXD */
			   SYNA_PINCTRL_FUNC(0x6, "can1")), /* SM CAN1 TX */
	SYNA_PINCTRL_GROUP("SM_GPIO33", 0x0, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO33 */
			   SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM2 */
			   SYNA_PINCTRL_FUNC(0x3, "uart2"), /* SM UART2 TXD */
			   SYNA_PINCTRL_FUNC(0x4, "uart3"), /* SM UART3 RTSn */
			   SYNA_PINCTRL_FUNC(0x5, "key_row5"),
			   SYNA_PINCTRL_FUNC(0x6, "uart3_de")), /* SM UART3 DE */
	SYNA_PINCTRL_GROUP("SM_GPIO34", 0x0, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO34 */
			   SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM3 */
			   SYNA_PINCTRL_FUNC(0x3, "uart2"), /* SM UART2 RXD */
			   SYNA_PINCTRL_FUNC(0x4, "uart3"), /* SM UART3 CTSn */
			   SYNA_PINCTRL_FUNC(0x5, "key_row4"),
			   SYNA_PINCTRL_FUNC(0x6, "uart3_ren")), /* SM UART3 REn */
	SYNA_PINCTRL_GROUP("SM_GPIO35", 0x0, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO35 */
			   SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM4 */
			   SYNA_PINCTRL_FUNC(0x2, "uart1"), /* SM UART1 RTSn */
			   SYNA_PINCTRL_FUNC(0x3, "uart3"), /* SM UART3 TXD */
			   SYNA_PINCTRL_FUNC(0x4, "uart2"), /* SM UART2 RTSn */
			   SYNA_PINCTRL_FUNC(0x5, "key_row3"),
			   SYNA_PINCTRL_FUNC(0x6, "uart0")), /* SM UART0 RTSn */
	SYNA_PINCTRL_GROUP("SM_GPIO36", 0x0, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO36 */
			   SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM5 */
			   SYNA_PINCTRL_FUNC(0x2, "uart1"), /* SM UART1 CTSn */
			   SYNA_PINCTRL_FUNC(0x3, "uart3"), /* SM UART3 RXD */
			   SYNA_PINCTRL_FUNC(0x4, "uart2"), /* SM UART2 CTSn */
			   SYNA_PINCTRL_FUNC(0x5, "key_row2"),
			   SYNA_PINCTRL_FUNC(0x6, "uart0")), /* SM UART0 CTSn */
	SYNA_PINCTRL_GROUP("SM_GPIO37", 0x0, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO37 */
			   SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM6 */
			   SYNA_PINCTRL_FUNC(0x3, "tw0"), /* SM TW0 SCL */
			   SYNA_PINCTRL_FUNC(0x4, "key_row6"),
			   SYNA_PINCTRL_FUNC(0x5, "pdm")), /* SM PDM CLKIO */
	SYNA_PINCTRL_GROUP("SM_GPIO38", 0x0, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO38 */
			   SYNA_PINCTRL_FUNC(0x1, "pwm"), /* SM PWM7 */
			   SYNA_PINCTRL_FUNC(0x3, "tw0"), /* SM TW0 SDA */
			   SYNA_PINCTRL_FUNC(0x5, "pdm")), /* SM PDM DI0 */
	SYNA_PINCTRL_GROUP("SM_GPIO3", 0x0, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO3 */
			   SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SS0n */
			   SYNA_PINCTRL_FUNC(0x2, "tw1"), /* SM TW1 SCL */
			   SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM8 */
			   SYNA_PINCTRL_FUNC(0x4, "i3c")), /* SM I3C MS SCL */
	SYNA_PINCTRL_GROUP("SM_GPIO4", 0x0, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO4 */
			   SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SS1n */
			   SYNA_PINCTRL_FUNC(0x2, "tw1"), /* SM TW1 SDA */
			   SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM0 */
			   SYNA_PINCTRL_FUNC(0x4, "i3c")), /* SM I3C MS SDA */
	SYNA_PINCTRL_GROUP("SM_GPIO5", 0x4, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO5 */
			   SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SS2n */
			   SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM1 */
	SYNA_PINCTRL_GROUP("SM_GPIO6", 0x4, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO6 */
			   SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SS3n */
			   SYNA_PINCTRL_FUNC(0x2, "spi1s"), /* SM SPI1S SSn */
			   SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM2 */
	SYNA_PINCTRL_GROUP("SM_GPIO9", 0x4, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO9 */
			   SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SDO */
			   SYNA_PINCTRL_FUNC(0x2, "spi1s"), /* SM SPI1S SDO */
			   SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM3 */
	SYNA_PINCTRL_GROUP("SM_GPIO10", 0x4, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO10 */
			   SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SCLK */
			   SYNA_PINCTRL_FUNC(0x2, "spi1s"), /* SM SPI1S SCLK */
			   SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM4 */
	SYNA_PINCTRL_GROUP("SM_GPIO11", 0x4, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO11 */
			   SYNA_PINCTRL_FUNC(0x1, "spi1"), /* SM SPI1 SDI */
			   SYNA_PINCTRL_FUNC(0x2, "spi1s"), /* SM SPI1S SDI */
			   SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM5 */
	SYNA_PINCTRL_GROUP("SM_GPIO30", 0x4, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO30 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DATA7 */
			   SYNA_PINCTRL_FUNC(0x3, "key_col6"),
			   SYNA_PINCTRL_FUNC(0x7, "sm_clkout")),
	SYNA_PINCTRL_GROUP("SM_GPIO29", 0x4, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO29 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DATA6 */
			   SYNA_PINCTRL_FUNC(0x2, "uart3"), /* SM UART3 RXD */
			   SYNA_PINCTRL_FUNC(0x3, "key_col5"),
			   SYNA_PINCTRL_FUNC(0x4, "uart2"), /* SM UART2 CTSn */
			   SYNA_PINCTRL_FUNC(0x5, "uart0"), /* SM UART0 CTSn */
			   SYNA_PINCTRL_FUNC(0x6, "key_row2"),
			   SYNA_PINCTRL_FUNC(0x7, "uart1")), /* SM UART1 CTSn */
	SYNA_PINCTRL_GROUP("SM_GPIO28", 0x4, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO28 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DATA5 */
			   SYNA_PINCTRL_FUNC(0x2, "uart3"), /* SM UART3 TXD */
			   SYNA_PINCTRL_FUNC(0x3, "key_col4"),
			   SYNA_PINCTRL_FUNC(0x4, "uart2"), /* SM UART2 RTSn */
			   SYNA_PINCTRL_FUNC(0x5, "uart0"), /* SM UART0 RTSn */
			   SYNA_PINCTRL_FUNC(0x6, "key_row3"),
			   SYNA_PINCTRL_FUNC(0x7, "uart1")), /* SM UART1 RTSn */
	SYNA_PINCTRL_GROUP("SM_GPIO27", 0x4, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO27 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DATA4 */
			   SYNA_PINCTRL_FUNC(0x2, "uart2"), /* SM UART2 RXD */
			   SYNA_PINCTRL_FUNC(0x3, "key_col3"),
			   SYNA_PINCTRL_FUNC(0x4, "uart3"), /* SM UART3 CTSn */
			   SYNA_PINCTRL_FUNC(0x5, "uart3_ren"), /* SM UART3 REn */
			   SYNA_PINCTRL_FUNC(0x6, "key_row4")),
	SYNA_PINCTRL_GROUP("SM_GPIO26", 0x4, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO26 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI CS1n */
			   SYNA_PINCTRL_FUNC(0x2, "uart2"), /* SM UART2 TXD */
			   SYNA_PINCTRL_FUNC(0x3, "key_col2"),
			   SYNA_PINCTRL_FUNC(0x4, "uart3"), /* SM UART3 RTSn */
			   SYNA_PINCTRL_FUNC(0x5, "uart3_de"), /* SM UART3 DE */
			   SYNA_PINCTRL_FUNC(0x6, "key_row5"),
			   SYNA_PINCTRL_FUNC(0x7, "sm_clkout")),
	SYNA_PINCTRL_GROUP("SM_GPIO25", 0x8, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO25 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi"), /* SM XSPI DQS */
			   SYNA_PINCTRL_FUNC(0x6, "key_row6")),
	SYNA_PINCTRL_GROUP("SM_GPIO24", 0x8, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO24 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI CLKn */
	SYNA_PINCTRL_GROUP("SM_GPIO23", 0x8, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO23 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI CLK */
	SYNA_PINCTRL_GROUP("SM_GPIO22", 0x8, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO22 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI DATA3 */
	SYNA_PINCTRL_GROUP("SM_GPIO21", 0x8, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO21 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI DATA2 */
	SYNA_PINCTRL_GROUP("SM_GPIO20", 0x8, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO20 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI DATA1 */
	SYNA_PINCTRL_GROUP("SM_GPIO19", 0x8, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO19 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI DATA0 */
	SYNA_PINCTRL_GROUP("SM_GPIO18", 0x8, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO18 */
			   SYNA_PINCTRL_FUNC(0x1, "xspi")), /* SM XSPI CS0n*/
	SYNA_PINCTRL_GROUP("SM_GPIO17", 0x8, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO17 */
			   SYNA_PINCTRL_FUNC(0x1, "uart1"), /* SM UART1 TXD */
			   SYNA_PINCTRL_FUNC(0x2, "can0"), /* SM CAN0 TX */
			   SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM8 */
			   SYNA_PINCTRL_FUNC(0x6, "uart0")), /* SM UART0 TXD */
	SYNA_PINCTRL_GROUP("SM_GPIO15", 0x8, 0x3, 0x1b,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO15 */
			   SYNA_PINCTRL_FUNC(0x1, "tw1"), /* SM TW1 SDA */
			   SYNA_PINCTRL_FUNC(0x2, "uart0"), /* SM UART0 RTSn */
			   SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM11 */
			   SYNA_PINCTRL_FUNC(0x4, "can0"), /* SM CAN0 TX */
			   SYNA_PINCTRL_FUNC(0x6, "uart1"), /* SM UART1 RTSn */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* DBG OUT */
	SYNA_PINCTRL_GROUP("SM_GPIO14", 0xc, 0x3, 0x00,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO14 */
			   SYNA_PINCTRL_FUNC(0x1, "tw1"), /* SM TW1 SCL */
			   SYNA_PINCTRL_FUNC(0x2, "uart0"), /* SM UART0 CTSn */
			   SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM10 */
			   SYNA_PINCTRL_FUNC(0x4, "can0"), /* SM CAN0 RX */
			   SYNA_PINCTRL_FUNC(0x6, "uart1"), /* SM UART1 CTSn */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* DBG OUT */
	SYNA_PINCTRL_GROUP("SM_GPIO16", 0xc, 0x3, 0x03,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO16 */
			   SYNA_PINCTRL_FUNC(0x1, "uart1"), /* SM UART1 RXD */
			   SYNA_PINCTRL_FUNC(0x2, "can0"), /* SM CAN0 RX */
			   SYNA_PINCTRL_FUNC(0x3, "pwm")), /* SM PWM7 */
	SYNA_PINCTRL_GROUP("SM_GPIO13", 0xc, 0x3, 0x06,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO13 */
			   SYNA_PINCTRL_FUNC(0x1, "tw0"), /* SM TW0 SDA */
			   SYNA_PINCTRL_FUNC(0x2, "i3c"), /* SM I3C MS SDA */
			   SYNA_PINCTRL_FUNC(0x3, "sm_clkout"),
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* DBG out */
	SYNA_PINCTRL_GROUP("SM_GPIO12", 0xc, 0x3, 0x09,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO12 */
			   SYNA_PINCTRL_FUNC(0x1, "tw0"), /* SM TW0 SCL */
			   SYNA_PINCTRL_FUNC(0x2, "i3c"), /* SM I3C MS SCL */
			   SYNA_PINCTRL_FUNC(0x3, "pwm"), /* SM PWM6 */
			   SYNA_PINCTRL_FUNC(0x7, "dbg")), /* DBG CLK */
	SYNA_PINCTRL_GROUP("SM_GPIO8", 0xc, 0x3, 0x0c,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO8 */
			   SYNA_PINCTRL_FUNC(0x1, "uart0"), /* SM UART0 TXD */
			   SYNA_PINCTRL_FUNC(0x2, "can0"), /* SM CAN0 TX */
			   SYNA_PINCTRL_FUNC(0x3, "sm_clkout"),
			   SYNA_PINCTRL_FUNC(0x6, "uart1")), /* SM UART0 TXD */
	SYNA_PINCTRL_GROUP("SM_GPIO7", 0xc, 0x3, 0x0f,
			   SYNA_PINCTRL_FUNC(0x0, "gpio"), /* SM GPIO7 */
			   SYNA_PINCTRL_FUNC(0x1, "uart0"), /* SM UART0 RXD */
			   SYNA_PINCTRL_FUNC(0x2, "can0"), /* SM CAN0 RX */
			   SYNA_PINCTRL_FUNC(0x3, "key_row6"),
			   SYNA_PINCTRL_FUNC(0x4, "gpio_trig"), /* TRIG2 */
			   SYNA_PINCTRL_FUNC(0x5, "pwm"), /* SM PWM9 */
			   SYNA_PINCTRL_FUNC(0x6, "uart1")), /* SM UART0 RXD */
	SYNA_PINCTRL_GROUP("SM_GPIO2", 0xc, 0x3, 0x12,
			   SYNA_PINCTRL_FUNC(0x0, "jtag"), /* TDO */
			   SYNA_PINCTRL_FUNC(0x1, "gpio"), /* SM GPIO2 */
			   SYNA_PINCTRL_FUNC(0x2, "pdm"), /* SM PDM CLKIO */
			   SYNA_PINCTRL_FUNC(0x3, "i2s2"), /* MCLK */
			   SYNA_PINCTRL_FUNC(0x6, "pwm")), /* SM PWM11 */
	SYNA_PINCTRL_GROUP("SM_GPIO1", 0xc, 0x3, 0x15,
			   SYNA_PINCTRL_FUNC(0x0, "jtag"), /* TDI */
			   SYNA_PINCTRL_FUNC(0x1, "gpio"), /* SM GPIO1 */
			   SYNA_PINCTRL_FUNC(0x2, "uart0"), /* SM UART0 RXD */
			   SYNA_PINCTRL_FUNC(0x3, "key_col1"),
			   SYNA_PINCTRL_FUNC(0x4, "gpio_trig"), /* TRIG0 */
			   SYNA_PINCTRL_FUNC(0x5, "pwm"), /* SM PWM9 */
			   SYNA_PINCTRL_FUNC(0x6, "key_row7")),
	SYNA_PINCTRL_GROUP("SM_GPIO0", 0xc, 0x3, 0x18,
			   SYNA_PINCTRL_FUNC(0x0, "jtag"), /* TMS */
			   SYNA_PINCTRL_FUNC(0x1, "gpio"), /* SM GPIO0 */
			   SYNA_PINCTRL_FUNC(0x2, "uart0"), /* SM UART0 TXD */
			   SYNA_PINCTRL_FUNC(0x3, "key_col0"),
			   SYNA_PINCTRL_FUNC(0x5, "pdm"), /* SM PDM DI0 */
			   SYNA_PINCTRL_FUNC(0x6, "pwm")), /* SM PWM10 */
};

static const struct syna_pinctrl_desc sl261x_soc_pinctrl_data = {
	.groups = sl261x_soc_pinctrl_groups,
	.ngroups = ARRAY_SIZE(sl261x_soc_pinctrl_groups),
};

static const struct syna_pinctrl_desc sl261x_sysmgr_pinctrl_data = {
	.groups = sl261x_sysmgr_pinctrl_groups,
	.ngroups = ARRAY_SIZE(sl261x_sysmgr_pinctrl_groups),
};

static int syna_pinctrl_get_groups_count(struct udevice *dev)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->desc->ngroups;
}

static const char *syna_pinctrl_get_group_name(struct udevice *dev,
					       u32 selector)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->desc->groups[selector].name;
}

static int synaptics_pinmux_get_functions_count(struct udevice *dev)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->nfuncs;
}

static const char *synaptics_pinmux_get_function_name(struct udevice *dev,
						      u32 selector)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);

	return pctrl->funcs[selector].name;
}

static struct syna_desc_func *
syna_pinctrl_find_function_by_name(struct syna_pinctrl *pctrl,
				   const struct syna_desc_group *group,
				     const char *fname)
{
	struct syna_desc_func *function = group->funcs;

	while (function->name) {
		if (!strcmp(function->name, fname))
			return function;

		function++;
	}

	return NULL;
}

static int synaptics_pinmux_group_set(struct udevice *dev,
				      u32 group_selector,
				   u32 func_selector)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);
	const struct syna_desc_group *group_desc = pctrl->desc->groups + group_selector;
	struct syna_pinctrl_func *func = pctrl->funcs + func_selector;
	struct syna_desc_func *function_desc =
		syna_pinctrl_find_function_by_name(pctrl, group_desc,
						   func->name);

	u32 mask, val;

	if (!function_desc)
		return -EINVAL;

	debug("%s: group=%d  func=%d\n", __func__, group_selector, func_selector);
	mask = GENMASK(group_desc->lsb + group_desc->bit_width - 1,
		       group_desc->lsb);
	val = function_desc->muxval << group_desc->lsb;

	fdt_addr_t addr = pctrl->base + group_desc->offset;

	debug("%s: base=%016llx  offset=%08x\n", __func__, pctrl->base, group_desc->offset);
	u32 data = readl(addr);

	debug("%s: readl  %016llx  %08x\n", __func__, addr, data);
	debug("%s: mask=%08x, val=%08x\n", __func__, mask, val);
	data &= ~mask;
	data |= val & mask;
	writel(data, addr);
	debug("%s: writel %016llx  %08x\n", __func__, addr, data);

	return 0;
}

static const struct pinctrl_ops syna_pinctrl_ops = {
	.get_groups_count = syna_pinctrl_get_groups_count,
	.get_group_name = syna_pinctrl_get_group_name,
	.get_functions_count = synaptics_pinmux_get_functions_count,
	.get_function_name = synaptics_pinmux_get_function_name,
	.pinmux_group_set = synaptics_pinmux_group_set,
	.set_state = pinctrl_generic_set_state,
};

static int syna_pinctrl_add_function(struct syna_pinctrl *pctrl,
				     const char *name)
{
	struct syna_pinctrl_func *function = pctrl->funcs;

	while (function->name) {
		if (!strcmp(function->name, name)) {
			function->ngroups++;
			return -EEXIST;
		}
		function++;
	}

	function->name = name;
	function->ngroups = 1;

	pctrl->nfuncs++;

	return 0;
}

static int syna_pinctrl_build_state(struct udevice *dev)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);
	struct syna_desc_group const *desc_group;
	struct syna_desc_func const *desc_function;
	int i, max_funcs = 0;

	pctrl->nfuncs = 0;

	for (i = 0; i < pctrl->desc->ngroups; i++) {
		desc_group = pctrl->desc->groups + i;
		/* compute the maxiumum number of functions a group can have */
		max_funcs += 1 << (desc_group->bit_width + 1);
	}

	/* we will reallocate later */
	pctrl->funcs = malloc(max_funcs * sizeof(*pctrl->funcs));
	if (!pctrl->funcs)
		return -ENOMEM;
	memset(pctrl->funcs, 0, max_funcs * sizeof(*pctrl->funcs));

	/* register all functions */
	for (i = 0; i < pctrl->desc->ngroups; i++) {
		desc_group = pctrl->desc->groups + i;
		desc_function = desc_group->funcs;

		while (desc_function->name) {
			syna_pinctrl_add_function(pctrl, desc_function->name);
			desc_function++;
		}
	}

	/* map functions to theirs groups */
	for (i = 0; i < pctrl->desc->ngroups; i++) {
		desc_group = pctrl->desc->groups + i;
		desc_function = desc_group->funcs;

		while (desc_function->name) {
			struct syna_pinctrl_func
				*function = pctrl->funcs;
			const char **groups;
			bool found = false;

			while (function->name) {
				if (!strcmp(desc_function->name, function->name)) {
					found = true;
					break;
				}
				function++;
			}

			if (!found)
				return -EINVAL;

			if (!function->groups) {
				function->groups =
					malloc(function->ngroups * sizeof(char *));
				if (!function->groups)
					return -ENOMEM;
				memset(function->groups, 0, function->ngroups * sizeof(char *));
			}

			groups = function->groups;
			while (*groups)
				groups++;
			*groups = desc_group->name;

			desc_function++;
		}
	}

	return 0;
}

static int syna_pinctrl_probe(struct udevice *dev)
{
	struct syna_pinctrl *pctrl = dev_get_priv(dev);
	int ret;

	pctrl->base = devfdt_get_addr(dev);
	if (pctrl->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	pctrl->dev = dev;
	pctrl->desc = (struct syna_pinctrl_desc *)dev_get_driver_data(dev);
	debug("%s:%s base=%016llx, desc=%p\n", __func__, dev->name, pctrl->base, pctrl->desc);
	ret = syna_pinctrl_build_state(dev);
	if (ret) {
		dev_err(dev, "cannot build driver state: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct udevice_id syna_pinctrl_match[] = {
	{
		.compatible = "syna,sl261x-soc-pinctrl",
		.data = (ulong)&sl261x_soc_pinctrl_data,
	},
	{
		.compatible = "syna,sl261x-system-pinctrl",
		.data = (ulong)&sl261x_sysmgr_pinctrl_data,
	},
	{}
};

U_BOOT_DRIVER(syna_pinctrl) = {
	.name		= "syna-pinctrl",
	.id		= UCLASS_PINCTRL,
	.of_match	= syna_pinctrl_match,
	.ops		= &syna_pinctrl_ops,
	.probe		= syna_pinctrl_probe,
	.priv_auto = sizeof(struct syna_pinctrl),
};
