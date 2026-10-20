/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SL2610_PLL_H
#define __SL2610_PLL_H

#include <linux/io.h>

struct udevice;

int sl2610_pll_register(struct udevice *dev, void __iomem *ctrl,
			const char *out, const char *outf);

#endif /* __SL2610_PLL_H */
