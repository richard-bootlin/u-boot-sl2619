// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2026 Synaptics Incorporated
 */

#include <asm/io.h>
#include <dm/ofnode.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/string.h>

/* UART Software Reset Register */
#define SYNA_UART_SRR		0x88
/* Reset bits */
#define SYNA_UART_RST		BIT(0)
#define SYNA_UART_RST_FIFO_RCVR	BIT(1)
#define SYNA_UART_RST_FIFO_XMIT	BIT(2)
#define SYNA_UART_RST_ALL	(SYNA_UART_RST | SYNA_UART_RST_FIFO_RCVR | \
				 SYNA_UART_RST_FIFO_XMIT)

/*
 * ROM boot code leaves the UART console in a non default state,
 * reset it before probing the serial driver.
 */
static int reset_console(void)
{
	struct resource res;
	const char *str;
	ofnode node;
	int ret;

	str = ofnode_read_chosen_string("stdout-path");
	if (!str)
		return -ENOENT;

	if (str[0] == '/') {
		node = ofnode_path(str);
	} else {
		char alias[32]; /* max recommended length in DT */
		const char *p;
		int namelen;

		p = strchr(str, ':');
		namelen = p ? p - str : strlen(str);
		if (namelen >= sizeof(alias))
			return -ENAMETOOLONG;
		memcpy(alias, str, namelen);
		alias[namelen] = '\0';
		node = ofnode_get_aliases_node(alias);
	}

	if (!ofnode_valid(node))
		return -ENOENT;

	ret = ofnode_read_resource(node, 0, &res);
	if (ret)
		return ret;

	if (!ofnode_device_is_compatible(node, "syna,sl261x-uart"))
		return -ENODEV;

	writel(SYNA_UART_RST_ALL, (uintptr_t)(res.start + SYNA_UART_SRR));

	return 0;
}

int board_early_init_f(void)
{
	reset_console();

	return 0;
}
