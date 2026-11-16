// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2026 Synaptics Incorporated
 *
 * Author: Richard Genoud <richard.genoud@bootlin.com>
 *
 * From Linux: drivers/soc/synaptics/sl2610-syscon.c
 *    Copyright (C) 2026 Synaptics Incorporated
 *    Author: Jisheng Zhang <jszhang@kernel.org>
 */

#include <dm.h>
#include <dm/device-internal.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <linux/kernel.h>
#include <soc/synaptics/sl2610-syscon.h>

/**
 * struct sl2610_syscon_cell - description of a system controller sub-device
 * @drv_name: name of the U-Boot driver to bind the sub-device to
 * @block: register block the sub-device belongs to, passed as driver data
 */
struct sl2610_syscon_cell {
	const char *drv_name;
	enum sl2610_syscon_block block;
};

struct sl2610_syscon_data {
	const struct sl2610_syscon_cell *cells;
	unsigned int num_cells;
};

static const struct sl2610_syscon_cell sl2610_soc_cells[] = {
	{ "syna-sl2610-soc-clk",	SL2610_SYSCON_BLOCK_SOC },
	{ "sl2610-reset",		SL2610_SYSCON_BLOCK_SOC },
	{ "syna-pinctrl",		SL2610_SYSCON_BLOCK_SOC },
};

static const struct sl2610_syscon_cell sl2610_mcu_cells[] = {
	{ "syna-sl2610-system-clk",	SL2610_SYSCON_BLOCK_MCU },
	{ "sl2610-reset",		SL2610_SYSCON_BLOCK_MCU },
	{ "syna-pinctrl",		SL2610_SYSCON_BLOCK_MCU },
};

static const struct sl2610_syscon_cell sl2610_avio_cells[] = {
	{ "syna-sl2610-aio-clk",	SL2610_SYSCON_BLOCK_AVIO },
};

static const struct sl2610_syscon_cell sl2610_cpu_cells[] = {
	{ "syna-sl2610-cpu-clk",	SL2610_SYSCON_BLOCK_CPU },
};

static const struct sl2610_syscon_data sl2610_soc_data = {
	.cells = sl2610_soc_cells,
	.num_cells = ARRAY_SIZE(sl2610_soc_cells),
};

static const struct sl2610_syscon_data sl2610_mcu_data = {
	.cells = sl2610_mcu_cells,
	.num_cells = ARRAY_SIZE(sl2610_mcu_cells),
};

static const struct sl2610_syscon_data sl2610_avio_data = {
	.cells = sl2610_avio_cells,
	.num_cells = ARRAY_SIZE(sl2610_avio_cells),
};

static const struct sl2610_syscon_data sl2610_cpu_data = {
	.cells = sl2610_cpu_cells,
	.num_cells = ARRAY_SIZE(sl2610_cpu_cells),
};

static int sl2610_syscon_bind(struct udevice *dev)
{
	const struct sl2610_syscon_data *data =
		(const struct sl2610_syscon_data *)dev_get_driver_data(dev);
	const struct sl2610_syscon_cell *cell;
	struct driver *drv;
	int ret;
	int i;

	for (i = 0; i < data->num_cells; i++) {
		cell = &data->cells[i];

		drv = lists_driver_lookup_name(cell->drv_name);
		if (!drv) {
			dev_err(dev, "cannot find driver '%s'\n", cell->drv_name);
			return -ENOENT;
		}

		ret = device_bind_with_driver_data(dev, drv, cell->drv_name,
						   cell->block, dev_ofnode(dev),
						   NULL);
		if (ret) {
			dev_err(dev, "cannot bind '%s': %d\n",
				cell->drv_name, ret);
			return ret;
		}
	}

	return 0;
}

static const struct udevice_id sl2610_syscon_of_match[] = {
	{ .compatible = "syna,sl2610-soc-syscon", .data = (ulong)&sl2610_soc_data },
	{ .compatible = "syna,sl2610-mcu-syscon", .data = (ulong)&sl2610_mcu_data },
	{ .compatible = "syna,sl2610-avio-syscon", .data = (ulong)&sl2610_avio_data },
	{ .compatible = "syna,sl2610-cpu-syscon", .data = (ulong)&sl2610_cpu_data },
	{ }
};

U_BOOT_DRIVER(sl2610_syscon) = {
	.name		= "sl2610-syscon",
	.id		= UCLASS_NOP,
	.of_match	= sl2610_syscon_of_match,
	.bind		= sl2610_syscon_bind,
};
