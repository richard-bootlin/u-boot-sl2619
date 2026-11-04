// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Synaptics Incorporated
 *
 * Richard Genoud <richard.genoud@bootlin.com>
 */

#include <dm.h>
#include <dm/device_compat.h>
#include <reset.h>
#include <sdhci.h>
#include <clk.h>
#include <mux.h>
#include <mux-internal.h>
#include <linux/delay.h>

#include <reset.h>
#include <sdhci.h>
#include <clk.h>
#include <env.h>

#define SDHCI_TUNING_LOOP_COUNT	128

/* Host Controller */
#define SDHCI_P_VENDOR_SPECIFIC_AREA	0xe8
#define SDHCI_P_VENDOR_SPECIFIC_AREA_MASK 0xfff
#define SDHCI_EMMC_CTRL_OFFSET			0x2c
#define CARD_IS_EMMC BIT(0)
#define EMMC_RST_N BIT(2)
#define EMMC_RST_N_OE BIT(3)

/* DWCMSHC uses a non-standard Host Control 2 value for HS400 */
#define DWCMSHC_CTRL_HS400	0x7

#define PHY_AT_CTRL_R_REG_OFFSET 0x40
#define AT_EN BIT(0)
#define CI_EN BIT(1)
#define SWIN_TH_EN BIT(2)
#define RPT_TUNE_ERR BIT(3)
#define TUNE_CLK_STOP_EN BIT(16)
#define PRE_CHANGE_DLY_SFT 17
#define PRE_CHANGE_DLY_MSK	(0x3 << PRE_CHANGE_DLY_SFT)
#define POST_CHANGE_DLY_SFT 19
#define POST_CHANGE_DLY_MSK (0x3 << POST_CHANGE_DLY_SFT)

/* PHY */
#define PHY_REG_BASE 0x300
#define PHY_REG(off) (PHY_REG_BASE + (off))

#define PHY_CNFG_REG PHY_REG(0)
#define PHY_RSTN BIT(0)
#define PHY_PWRGOOD BIT(1)

/* PHY PAD */
#define PHY_CMDPAD_CNFG_REG PHY_REG(0x4)
#define PHY_DATPAD_CNFG_REG PHY_REG(0x6)
#define PHY_CLKPAD_CNFG_REG PHY_REG(0x8)
#define PHY_STBPAD_CNFG_REG PHY_REG(0xa)
#define PHY_RSTNPAD_CNFG_REG PHY_REG(0xc)

/*PHY PAD GENERAL modes */
#define PAD_SP_8    0x8
#define PAD_SP_9    0x9
#define PAD_SN_8    0x8

/* PHY RX SEL modes */
#define RXSELOFF        0x0
#define SCHMITT1P8      0x1
#define SCHMITT3P3      0x2
#define SCHMITT1P2      0x3
#define COMPARATOR1P8   0x4
#define COMPARATOR1P2   0x5
#define COMPARATOR1P82  0x6
#define INTERNALLPBK    0x7

/* PHY WEAKPULL_EN modes */
#define WPE_DISABLE  0x0
#define WPE_PULLUP   0x1
#define WPE_PULLDOWN 0x2

/* PHY TXSLEW_CTRL_P modes */
#define TX_SLEW_P_0    0x0
#define TX_SLEW_P_2    0x2
#define TX_SLEW_P_3    0x3

/* PHY TXSLEW_CTRL_N modes */
#define TX_SLEW_N_2    0x2
#define TX_SLEW_N_3    0x3

#define PHY_COMMDL_CNFG_REG PHY_REG(0x1c)
#define DLSTEP_SEL BIT(0)
#define DLOUT_EN BIT(1)

#define PHY_SDCLKDL_CNFG_REG PHY_REG(0x1d)
#define EXTDLY_EN BIT(0)
#define BYPASS_EN BIT(1)
#define INPSEL_CNFG_SFT 2
#define INPSEL_CNFG_MSK (0x3 << INPSEL_CNFG_SFT)
#define UPDATE_DC BIT(4)

#define PHY_SDCLKDL_DC_REG PHY_REG(0x1e)
#define CCKDL_DC_SFT 0
#define CCKDL_DC_MSK (0x7f << CCKDL_DC_SFT)

#define PHY_SMPLDL_CNFG_REG PHY_REG(0x20)
#define SEXTDLY_EN BIT(0)
#define SBYPASS_EN BIT(1)
#define SINPSEL_CNFG_SFT 2
#define SINPSEL_CNFG_MSK (0x3 << SINPSEL_CNFG_SFT)
#define SINPSEL_OVERRIDE BIT(4)

#define PHY_ATDL_CNFG_REG PHY_REG(0x21)
#define AEXTDLY_EN BIT(0)
#define ABYPASS_EN BIT(1)
#define AINPSEL_CNFG_SFT 2
#define AINPSEL_CNFG_MSK (0x3 << AINPSEL_CNFG_SFT)

#define PHY_DLL_CTRL_REG PHY_REG(0x24)
#define DLL_EN BIT(0)

#define PHY_DLL_CNFG1_REG PHY_REG(0x25)
#define WAITCYCLE_SFT 0
#define SLVDLY_SFT 4
#define WAITCYCLE_MSK (0x3 << WAITCYCLE_SFT)
#define SLVDLY_MSK (0x3 << SLVDLY_SFT)

#define PHY_DLL_CNFG2_REG PHY_REG(0x26)

#define PHY_DLLDL_CNFG_REG PHY_REG(0x28)
#define SLV_INPSEL_SFT 5
#define SLV_INPSEL_MSK (0x3 << SLV_INPSEL_SFT)

#define PHY_DLLLBT_CNFG_REG PHY_REG(0x2c)

#define PHY_DLL_STATUS_REG PHY_REG(0x2e)
#define LOCK_STS BIT(0)
#define ERROR_STS BIT(1)

#define PHY_DLLDBG_MLKDC_REG PHY_REG(0x30)
#define PHY_DLLDBG_SLKDC_REG PHY_REG(0x32)

#define PHY_GEN_SETTING(pad_sp, pad_sn) \
	 ((((pad_sp) & 0x0f) << 16) | \
	 (((pad_sn) & 0x0f) << 20))
#define PHY_GEN_MASK PHY_GEN_SETTING(0xf, 0xf)

struct phy_gen_setting {
	u32 reg;
	u32 mask;
	u32 setting;
};

#define PHY_PAD_SETTING(rxsel, weakpull_en, txslew_ctrl_p, txslew_ctrl_n) \
	(((rxsel) & 0x3) | \
	(((weakpull_en) & 0x3) << 3) | \
	(((txslew_ctrl_p) & 0xf) << 5) | \
	((txslew_ctrl_n) & 0xf) << 9)
#define PHY_PAD_MASK PHY_PAD_SETTING(0x3, 0x3, 0xf, 0xf)
#define PHY_PAD_SETTING_NUM 5

struct phy_pad_setting {
	u32 reg;
	u16 mask;
	u16 setting;
};

struct dwcmshc_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
	struct reset_ctl_bulk reset_ctl;
	u32 mmc_type;
	u32 sdclkdl_dc;
	u32 pad_sp;
	u32 pad_sn;
	u32 dll_delay_offset;
	enum mmc_voltage phy_voltage;
	int fixed_voltage;
	int skip_phy;
};

struct phy_gen_setting gen_setting_1v8 = {
	PHY_CNFG_REG, PHY_GEN_MASK, PHY_GEN_SETTING(PAD_SP_8, PAD_SN_8)};
struct phy_pad_setting pad_setting_1v8[PHY_PAD_SETTING_NUM] = {
	{
		PHY_CMDPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(SCHMITT1P8, WPE_PULLUP, TX_SLEW_P_0, TX_SLEW_N_3)
	},
	{
		PHY_DATPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(SCHMITT1P8, WPE_PULLUP, TX_SLEW_P_0, TX_SLEW_N_3)
	},
	{
		PHY_CLKPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(RXSELOFF, WPE_DISABLE, TX_SLEW_P_0, TX_SLEW_N_3)
	},
	{
		PHY_STBPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(SCHMITT1P8, WPE_PULLDOWN, TX_SLEW_P_0, TX_SLEW_N_3)
	},
	{
		PHY_RSTNPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(SCHMITT1P8, WPE_PULLUP, TX_SLEW_P_0, TX_SLEW_N_3)
	}
};

struct phy_gen_setting gen_setting_3v3 = {
	PHY_CNFG_REG, PHY_GEN_MASK, PHY_GEN_SETTING(PAD_SP_9, PAD_SN_8)};
struct phy_pad_setting pad_setting_3v3[PHY_PAD_SETTING_NUM] = {
	{
		PHY_CMDPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(SCHMITT3P3, WPE_PULLUP, TX_SLEW_P_3, TX_SLEW_N_2)
	},
	{
		PHY_DATPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(SCHMITT3P3, WPE_PULLUP, TX_SLEW_P_3, TX_SLEW_N_2)
	},
	{
		PHY_CLKPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(RXSELOFF, WPE_DISABLE, TX_SLEW_P_3, TX_SLEW_N_2)
	},
	{
		PHY_STBPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(SCHMITT3P3, WPE_PULLDOWN, TX_SLEW_P_3, TX_SLEW_N_2)
	},
	{
		PHY_RSTNPAD_CNFG_REG,
		PHY_PAD_MASK,
		PHY_PAD_SETTING(SCHMITT3P3, WPE_PULLUP, TX_SLEW_P_3, TX_SLEW_N_2)
	}
};

u8 phy_delay[MMC_MODES_END] = {
	80,  /* MMC_LEGACY */
	127, /* SD_LEGACY */ /* TODO: remove that! */
	80,  /* MMC_HS */
	35,  /* SD_HS */
	80,  /* MMC_HS_52 */
	40,  /* MMC_DDR_52 */
	100, /* UHS_SDR12 */
	100, /* UHS_SDR25 */
	100, /* UHS_SDR50 */
	43,  /* UHS_DDR50 */
	43,  /* UHS_SDR104 */
	40,  /* MMC_HS_200 */
	20,  /* MMC_HS_400 */
	0,   /* MMC_HS_400_ES */
};


static int dwcmshc_reset_phy(struct sdhci_host *host, int rst)
{
	volatile u16 valw;
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(host->mmc->dev);

	if (plat->skip_phy)
		return 0;

	valw = sdhci_readw(host, PHY_CNFG_REG);
	valw &= ~PHY_RSTN;
	valw |= rst;
	sdhci_writew(host, valw, PHY_CNFG_REG);

	return 0;
}

static void dwcmshc_reset_device(struct sdhci_host *host)
{
	u32 offset, val;

	offset = sdhci_readl(host, SDHCI_P_VENDOR_SPECIFIC_AREA);
	offset &= SDHCI_P_VENDOR_SPECIFIC_AREA_MASK;
	offset += SDHCI_EMMC_CTRL_OFFSET;
	val = sdhci_readl(host, offset);
	val |= CARD_IS_EMMC;
	val |= EMMC_RST_N_OE;
	val &= ~EMMC_RST_N;
	sdhci_writel(host, val, offset);
	mdelay(1);
	val = sdhci_readl(host, offset);
	val |= EMMC_RST_N;
	sdhci_writel(host, val, offset);
	mdelay(1);
	val = sdhci_readl(host, offset);
}

static void dwcmshc_reset_host(struct dwcmshc_sdhci_plat *plat)
{
	reset_reset_bulk(&plat->reset_ctl, 10);
}

static int dwcmshc_setup_phy_datapath(struct sdhci_host *host)
{
	u8 valb;
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(host->mmc->dev);

	if (plat->skip_phy)
		return 0;

	valb = sdhci_readb(host, PHY_COMMDL_CNFG_REG);
	valb &= ~DLSTEP_SEL;
	valb &= ~DLOUT_EN;
	sdhci_writeb(host, valb, PHY_COMMDL_CNFG_REG);

	valb = sdhci_readb(host, PHY_SDCLKDL_CNFG_REG);
	valb &= ~EXTDLY_EN;
	valb &= ~BYPASS_EN;
	valb &= ~UPDATE_DC;
	valb &= ~INPSEL_CNFG_MSK;
	sdhci_writeb(host, valb, PHY_SDCLKDL_CNFG_REG);

	valb = sdhci_readb(host, PHY_SMPLDL_CNFG_REG);
	valb &= ~SEXTDLY_EN;
	valb &= ~SBYPASS_EN;
	valb &= ~SINPSEL_OVERRIDE;
	valb &= ~SINPSEL_CNFG_MSK;
	valb |= (3 << SINPSEL_CNFG_SFT);
	sdhci_writeb(host, valb, PHY_SMPLDL_CNFG_REG);

	valb = sdhci_readb(host, PHY_ATDL_CNFG_REG);
	valb &= ~AEXTDLY_EN;
	valb &= ~ABYPASS_EN;
	valb &= ~AINPSEL_CNFG_MSK;
	valb |= (3 << AINPSEL_CNFG_SFT);
	sdhci_writeb(host, valb, PHY_ATDL_CNFG_REG);

	return 0;
}

static int dwcmshc_setup_phy_delayline(struct sdhci_host *host, u8 delay)
{
	u8 valb;
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(host->mmc->dev);

	if (plat->skip_phy)
		return 0;

	valb = sdhci_readb(host, PHY_SDCLKDL_CNFG_REG);
	valb |= UPDATE_DC;
	sdhci_writeb(host, valb, PHY_SDCLKDL_CNFG_REG);

	valb = sdhci_readb(host, PHY_SDCLKDL_DC_REG);
	valb &= ~CCKDL_DC_MSK;
	valb |= delay << CCKDL_DC_SFT;
	sdhci_writeb(host, valb, PHY_SDCLKDL_DC_REG);

	valb = sdhci_readb(host, PHY_SDCLKDL_CNFG_REG);
	valb &= ~UPDATE_DC;
	sdhci_writeb(host, valb, PHY_SDCLKDL_CNFG_REG);

	return 0;
}

static int dwcmshc_setup_phy_tuning(struct sdhci_host *host)
{
	u32 offset, val;
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(host->mmc->dev);

	if (plat->skip_phy)
		return 0;

	offset = sdhci_readl(host, SDHCI_P_VENDOR_SPECIFIC_AREA);
	offset &= SDHCI_P_VENDOR_SPECIFIC_AREA_MASK;
	offset += PHY_AT_CTRL_R_REG_OFFSET;

	val = sdhci_readl(host, offset);
	val &= ~TUNE_CLK_STOP_EN;
	val &= ~POST_CHANGE_DLY_MSK;
	val &= ~PRE_CHANGE_DLY_MSK;
	val |= TUNE_CLK_STOP_EN | (3 << POST_CHANGE_DLY_SFT) | (3 << PRE_CHANGE_DLY_SFT);
	sdhci_writel(host, val, offset);

	return 0;
}

static int dwcmshc_setup_phy_configure(struct sdhci_host *host)
{
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(host->mmc->dev);
	struct phy_gen_setting *gen_setting = &gen_setting_1v8;
	struct phy_pad_setting *pad_setting = pad_setting_1v8;

	int i;
	volatile u16 valw;
	volatile u32 val;
	int timeout = 100;

	if (plat->skip_phy)
		return 0;

	if (host->mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		gen_setting = &gen_setting_3v3;
		pad_setting = pad_setting_3v3;
	}

	val = sdhci_readl(host, gen_setting->reg);
	val &= ~(gen_setting->mask);
	val |= gen_setting->setting;
	if (plat->pad_sp) {
		val &= ~(0xf << 16);
		val |= (plat->pad_sp & 0xFF) << 16;
	}
	if (plat->pad_sn) {
		val &= ~(0xf << 20);
		val |= (plat->pad_sn & 0xFF) << 20;
	}
	sdhci_writel(host, val, gen_setting->reg);

	for (i = 0; i < PHY_PAD_SETTING_NUM; i++) {
		valw = sdhci_readw(host, pad_setting[i].reg);
		valw &= ~(pad_setting[i].mask);
		valw |= pad_setting[i].setting;
		sdhci_writew(host, valw, pad_setting[i].reg);
	}

	do {
		mdelay(1);
		val = sdhci_readl(host, PHY_CNFG_REG);

		if (!timeout) {
			printf("EMMC PHY's PowerGood status is not ready !\n");
			return -ETIMEDOUT;
		}
	} while (!(val & PHY_PWRGOOD) && timeout--);

	plat->phy_voltage = host->mmc->signal_voltage;

	return 0;
}

static int dwcmshc_setup_hs400_phy_dll(struct sdhci_host *host)
{
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(host->mmc->dev);
	u32 delay_step, delay_code;
	u32 mstlkdc, slvlkdc;
	u16 valw;
	u8 valb;
	char *ep = NULL;

	if (plat->skip_phy)
		return 0;

	/* prepare DLL configuration */
	valw = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	valw &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, valw, SDHCI_CLOCK_CONTROL);

	sdhci_writew(host, 0x8000, PHY_DLLLBT_CNFG_REG);

	valb = sdhci_readb(host, PHY_DLL_CNFG1_REG);
	valb &= ~WAITCYCLE_MSK;
	valb |= (3 << WAITCYCLE_SFT);
	valb &= ~SLVDLY_MSK;
	valb |= (3 << SLVDLY_SFT);
	sdhci_writeb(host, valb, PHY_DLL_CNFG1_REG);

	sdhci_writeb(host, 0xa, PHY_DLL_CNFG2_REG);

	valb = sdhci_readb(host, PHY_DLLDL_CNFG_REG);
	valb &= ~SLV_INPSEL_MSK;
	valb = 0;
	valb |= (3 << SLV_INPSEL_SFT);
	sdhci_writeb(host, valb, PHY_DLLDL_CNFG_REG);

	valw = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	valw |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, valw, SDHCI_CLOCK_CONTROL);

	/* start DLL lock */
	valb = sdhci_readb(host, PHY_DLL_CTRL_REG);
	valb &= ~DLL_EN;
	sdhci_writeb(host, valb, PHY_DLL_CTRL_REG);
	valb |= DLL_EN;
	sdhci_writeb(host, valb, PHY_DLL_CTRL_REG);

	do {
		valb = sdhci_readb(host, PHY_DLL_STATUS_REG);
	} while (!(valb & LOCK_STS));

	if (valb & ERROR_STS) {
		printf("%s failed\n", __func__);
		return -1;
	}

	/* calibrate DLL */
	valb = sdhci_readb(host, PHY_DLLDBG_MLKDC_REG);
	mstlkdc = valb & 0x7F;
	valb = sdhci_readb(host, PHY_DLLDBG_SLKDC_REG);
	slvlkdc = valb & 0x7F;

	if (mstlkdc / slvlkdc == 4)
		delay_step = 5000 / mstlkdc;
	else if (mstlkdc / slvlkdc == 2)
		delay_step = 5000 / (2 * mstlkdc);
	else
		delay_step = 5000 / (4 * slvlkdc);

	ep = env_get("sdhci_dll_delay_offset");
	if (ep)
		plat->dll_delay_offset = (u32)simple_strtol(ep, NULL, 10);

	delay_code = plat->dll_delay_offset / delay_step;
	delay_code += 1;

	return delay_code;
}

static int dwcmshc_setup_phy(struct sdhci_host *host)
{
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(host->mmc->dev);

	if (plat->skip_phy)
		return 0;

	dwcmshc_setup_phy_datapath(host);
	dwcmshc_setup_phy_delayline(host,
				    plat->mmc_type == SD_VERSION_SD ?
				    phy_delay[SD_LEGACY] : phy_delay[MMC_LEGACY]);
	dwcmshc_setup_phy_tuning(host);
	dwcmshc_setup_phy_configure(host);

	return 0;
}

static void dwcmshc_sdhci_set_control_reg(struct sdhci_host *host)
{
	u16 ctrl;

	sdhci_set_control_reg(host);

	/*
	 * The generic core programs SDHCI_CTRL_HS400 (0x5), but DWCMSHC
	 * expects its own non-standard HS400 value.
	 */
	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	if ((ctrl & SDHCI_CTRL_UHS_MASK) == SDHCI_CTRL_HS400) {
		ctrl &= ~SDHCI_CTRL_UHS_MASK;
		ctrl |= DWCMSHC_CTRL_HS400;
		sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
	}
}

static int dwcmshc_sdhci_set_ios_post(struct sdhci_host *host)
{
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(host->mmc->dev);
	int delay = 0;
	char *ep = NULL;

	if (plat->phy_voltage != host->mmc->signal_voltage && !plat->fixed_voltage) {
		dwcmshc_reset_phy(host, 0);
		dwcmshc_setup_phy(host);
		dwcmshc_reset_phy(host, 1);
	}

	if (host->mmc->selected_mode == MMC_HS_400) {
		delay = dwcmshc_setup_hs400_phy_dll(host);
		if (delay > 0) {
			dwcmshc_setup_phy_delayline(host, (u8)delay);
			return 0;
		}

		printf("%s MMC_HS_400 PHY_DLL_CAL failed\n", __func__);

		ep = env_get("sdhci_sdclkdl_dc");
		if (ep)
			plat->sdclkdl_dc = (u32)simple_strtol(ep, NULL, 10);

		if (plat->sdclkdl_dc) {
			dwcmshc_setup_phy_delayline(host, plat->sdclkdl_dc);
			return 0;
		}
	}

	if (host->mmc->selected_mode == UHS_SDR104) {
		ep = env_get("sdhci_sdclkdl_dc");
		if (ep)
			plat->sdclkdl_dc = (u32)simple_strtol(ep, NULL, 10);

		if (plat->sdclkdl_dc) {
			dwcmshc_setup_phy_delayline(host, plat->sdclkdl_dc);
			return 0;
		}
	}

	if (host->mmc->selected_mode == MMC_LEGACY) {
		dwcmshc_setup_phy_delayline(host,
					    plat->mmc_type == SD_VERSION_SD ?
					    phy_delay[SD_LEGACY] : phy_delay[MMC_LEGACY]);
		return 0;
	}

	if (host->mmc->selected_mode < MMC_MODES_END && phy_delay[host->mmc->selected_mode])
		dwcmshc_setup_phy_delayline(host, phy_delay[host->mmc->selected_mode]);

	return 0;
}

static int dwcmshc_sdhci_execute_tuning(struct mmc *mmc, u8 opcode)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	struct sdhci_host *host = dev_get_priv(mmc->dev);
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(mmc->dev);
	int tuning_loop_counter = SDHCI_TUNING_LOOP_COUNT;
	u32 timeout = 100;
	u32 offset, val;
	u16 valw;
	int delay = 0;

	if (opcode == MMC_CMD_SEND_TUNING_BLOCK_HS200) {
		delay = dwcmshc_setup_hs400_phy_dll(host);
		if (delay > 0)
			dwcmshc_setup_phy_delayline(host, (u8)delay);
	}

	/* prepare tuning */
	valw = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	valw &= ~SDHCI_CTRL_TUNED_CLK;
	valw &= ~SDHCI_CTRL_EXEC_TUNING;
	sdhci_writew(host, valw, SDHCI_HOST_CONTROL2);

	offset = sdhci_readl(host, SDHCI_P_VENDOR_SPECIFIC_AREA);
	offset &= SDHCI_P_VENDOR_SPECIFIC_AREA_MASK;
	offset += PHY_AT_CTRL_R_REG_OFFSET;

	val = sdhci_readl(host, offset);
	val &= ~(RPT_TUNE_ERR | SWIN_TH_EN | AT_EN);
	if (plat->mmc_type == MMC_VERSION_MMC)
		val |= AT_EN;
	val |= TUNE_CLK_STOP_EN;
	val |= 0x3 << PRE_CHANGE_DLY_SFT;
	val |= 0x3 << POST_CHANGE_DLY_SFT;
	sdhci_writel(host, val, offset);

	/* reset DAT and CMD */
	sdhci_writeb(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA, SDHCI_SOFTWARE_RESET);
	while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & (SDHCI_RESET_CMD | SDHCI_RESET_DATA)) {
		if (timeout == 0) {
			printf("%s: Reset 0x%x never completed.\n",
			       __func__, (int)(SDHCI_RESET_CMD | SDHCI_RESET_DATA));
			return -1;
		}
		timeout--;
		udelay(1000);
	}

	/* start tuning */
	valw = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	valw |= SDHCI_CTRL_EXEC_TUNING;
	sdhci_writew(host, valw, SDHCI_HOST_CONTROL2);

	do {
		cmd.cmdidx = opcode;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;

		data.blocksize = 64;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;

		if (tuning_loop_counter-- == 0)
			break;

		if (cmd.cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200 &&
		    mmc->bus_width == 8)
			data.blocksize = 128;

		sdhci_writew(host, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,
						    data.blocksize),
			     SDHCI_BLOCK_SIZE);
		sdhci_writew(host, data.blocks, SDHCI_BLOCK_COUNT);
		sdhci_writew(host, SDHCI_TRNS_READ, SDHCI_TRANSFER_MODE);

		mmc_send_cmd(mmc, &cmd, NULL);
		valw = sdhci_readw(host, SDHCI_HOST_CONTROL2);

		if (cmd.cmdidx == MMC_CMD_SEND_TUNING_BLOCK)
			udelay(1);
	} while (valw & SDHCI_CTRL_EXEC_TUNING);

	if (tuning_loop_counter < 0) {
		valw &= ~SDHCI_CTRL_TUNED_CLK;
		sdhci_writew(host, valw, SDHCI_HOST_CONTROL2);
	}

	if (!(valw & SDHCI_CTRL_TUNED_CLK)) {
		printf("%s:Tuning failed\n", __func__);
		return -1;
	}

	return 0;
}

const struct sdhci_ops dwcmshc_ops = {
	.set_control_reg = &dwcmshc_sdhci_set_control_reg,
	.set_ios_post = &dwcmshc_sdhci_set_ios_post,
	.platform_execute_tuning = &dwcmshc_sdhci_execute_tuning,
};

static int dwcmshc_probe(struct udevice *dev)
{
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	int ret = 0;
	struct clk clk;
	unsigned long clock;
	struct mux_control *mux;
	unsigned int state;

	dwcmshc_reset_host(plat);

	mux = devm_mux_control_get(dev, "mux-0");
	if (!IS_ERR(mux)) {
		state = dev_read_u32_default(dev, "mux-0-value", 0);
		mux_control_select(mux, state);
	}

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret < 0) {
		dev_err(dev, "failed to get clock\n");
		return ret;
	}

	clock = clk_get_rate(&clk);
	if (IS_ERR_VALUE(clock)) {
		dev_err(dev, "failed to get rate\n");
		return clock;
	}

	host->max_clk = clock;
	host->mmc = &plat->mmc;
	host->mmc->dev = dev;
	host->mmc->signal_voltage = MMC_SIGNAL_VOLTAGE_180;

	if (dev_read_bool(dev, "3_3v-signalling"))
		host->mmc->signal_voltage = MMC_SIGNAL_VOLTAGE_330;

	if (dev_read_bool(dev, "fixed-voltage"))
		plat->fixed_voltage = 1;

	if (dev_read_bool(dev, "skip-phy"))
		plat->skip_phy = 1;

	if (dev_read_bool(dev, "sdhci-sdio"))
		plat->mmc_type = SD_VERSION_SD;
	else
		plat->mmc_type = MMC_VERSION_MMC;

	plat->sdclkdl_dc = dev_read_u32_default(dev, "sdclkdl-dc", 0);
	plat->pad_sn = dev_read_u32_default(dev, "pad_sn", 0);
	plat->pad_sp = dev_read_u32_default(dev, "pad_sp", 0);
	plat->dll_delay_offset = dev_read_u32_default(dev, "dll-delay-offset", 1000);

	ret = sdhci_setup_cfg(&plat->cfg, host, host->max_clk, 400000);
	if (ret)
		return ret;

	upriv->mmc = host->mmc;
	host->mmc->priv = host;

	ret = sdhci_probe(dev);
	if (ret)
		return ret;

	dwcmshc_reset_device(host);
	dwcmshc_reset_phy(host, 0);
	dwcmshc_setup_phy(host);
	dwcmshc_reset_phy(host, 1);

	return ret;
}

static int dwcmshc_of_to_plat(struct udevice *dev)
{
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(dev);
	struct sdhci_host *host = dev_get_priv(dev);
	struct mmc_config *cfg = &plat->cfg;
	int ret;

	if (reset_get_bulk(dev, &plat->reset_ctl)) {
		printf("%s: can't get reset index for sdhci-dwcmshc\n", __func__);
	//	return -ENODEV;
	}
	host->name = dev->name;
	host->ioaddr = (void *)dev_read_addr(dev);

	ret = mmc_of_parse(dev, cfg);
	if (ret)
		return ret;

	host->ops = &dwcmshc_ops;

	return 0;
}

static int dwcmshc_bind(struct udevice *dev)
{
	struct dwcmshc_sdhci_plat *plat = dev_get_plat(dev);
	int ret;

	ret = sdhci_bind(dev, &plat->mmc, &plat->cfg);
	if (ret)
		return ret;

	return 0;
}

static const struct udevice_id dwcmshc_sdhci_ids[] = {
	{ .compatible = "snps,dwcmshc-sdhci" },
	{ }
};

U_BOOT_DRIVER(dwcmshc_sdhci) = {
	.name	= "sdhci-dwcmshc",
	.id	= UCLASS_MMC,
	.of_match = dwcmshc_sdhci_ids,
	.of_to_plat = dwcmshc_of_to_plat,
	.ops	= &sdhci_ops,
	.bind	= dwcmshc_bind,
	.probe	= dwcmshc_probe,
	.priv_auto = sizeof(struct sdhci_host),
	.plat_auto = sizeof(struct dwcmshc_sdhci_plat),
};
