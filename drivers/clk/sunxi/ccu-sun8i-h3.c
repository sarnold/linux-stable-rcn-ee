/*
 * Copyright (c) 2016 Jean-Francois Moine <moinejf@free.fr>
 * Based on 'sunxi-ng' from
 * Copyright (c) 2016 Maxime Ripard. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clock/sun8i-h3.h>
#include <dt-bindings/reset/sun8i-h3.h>

#include "ccu.h"

static const char losc[32] = "osc32k";
static const char hosc[32] = "osc24M";

/*
 * Force the pll-audio-1x post-divider to 4
 * CCU_M(16, 4)
 */
static int ccu_h3_pll_audio_prepare(struct clk_hw *hw)
{
	struct ccu *ccu = hw2ccu(hw);

	ccu_set_field(ccu, ccu->reg, GENMASK(19, 16), (4 - 1) << 16);

	return 0;
}

static const struct clk_ops ccu_h3_pll_audio_ops = {
	.prepare = ccu_h3_pll_audio_prepare,

	.determine_rate = ccu_fixed_factor_determine_rate,
	.recalc_rate = ccu_fixed_factor_recalc_rate,
	.set_rate = ccu_fixed_factor_set_rate,
};

/* cpux */
/*	rate = 24MHz * (n - 1) * (k - 1) / (m - 1) >> p */
static struct ccu pll_cpux_clk = {
	CCU_REG(0x000),
	CCU_HW("pll-cpux", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x000, 28),
	CCU_N(8, 5),
	CCU_K(4, 2),
	CCU_M(0, 2),
	CCU_P(16, 2),		/* only if rate < 288MHz */
	.features = CCU_FEATURE_FLAT_FACTORS,
};

/* audio */
/*	rate = 24MHz * (n - 1) / (m - 1) */
static struct ccu pll_audio_base_clk = {
	CCU_REG(0x008),
	CCU_HW("pll-audio-base", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x008, 28),
	CCU_N(8, 7),
	CCU_M(0, 5),
};

/* We hardcode the divider (16,4) to 4 for now */
static struct ccu pll_audio_clk = {
	CCU_HW("pll-audio", "pll-audio-base", &ccu_h3_pll_audio_ops,
						CLK_SET_RATE_PARENT),
	CCU_FIXED(1, 4),
};
static struct ccu pll_audio_2x_clk = {
	CCU_HW("pll-audio-2x", "pll-audio_base", &ccu_fixed_factor_ops,
						CLK_SET_RATE_PARENT),
	CCU_FIXED(1, 2),
};
static struct ccu pll_audio_4x_clk = {
	CCU_HW("pll-audio-4x", "pll-audio_base", &ccu_fixed_factor_ops,
						CLK_SET_RATE_PARENT),
	CCU_FIXED(1, 1),
};
static struct ccu pll_audio_8x_clk = {
	CCU_HW("pll-audio-8x", "pll-audio_base", &ccu_fixed_factor_ops,
						CLK_SET_RATE_PARENT),
	CCU_FIXED(2, 1),
};

/* video */
/*	rate = 24MHz * (n - 1) / (m - 1) */
#define M_MASK_0_3 0x0f
static const struct frac video_fracs[] = {
/*	   rate		mask			    val */
	{270000000, M_MASK_0_3 | BIT(24) | BIT(25), 0},
	{297000000, M_MASK_0_3 | BIT(24) | BIT(25), BIT(25)},
	{0,			 BIT(24) | BIT(25), BIT(24)},
};
static const struct ccu_extra video_extra = {
	CCU_EXTRA_FRAC(video_fracs),
};
static struct ccu pll_video_clk = {
	CCU_REG(0x010),
	CCU_HW("pll-video", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x010, 28),
	CCU_N(8, 7),
	CCU_M(0, 4),
	.extra = &video_extra,
};

/* video engine */
/*	rate = 24MHz * (n - 1) / (m - 1) */
static struct ccu pll_ve_clk = {
	CCU_REG(0x018),
	CCU_HW("pll-ve", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x018, 28),
	CCU_N(8, 7),
	CCU_M(0, 4),
	.extra = &video_extra,
};

/* ddr */
/*	rate = 24MHz * (n - 1) * (k - 1) / (m - 1)
 *	bit 21: DDR_CLOCK = PLL_DDR / PLL_PERIPH (default DDR)
 */
static struct ccu pll_ddr_clk = {
	CCU_REG(0x020),
	CCU_HW("pll-ddr", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x020, 28),
	CCU_N(8, 5),
	CCU_K(4, 2),
	CCU_M(0, 2),
	CCU_UPD(20),
};

/* periph0 */
/*	rate = 24MHz * (n - 1) * (k - 1) / 2 */
static const struct ccu_extra periph_extra = {
	CCU_EXTRA_POST_DIV(2),
};
static struct ccu pll_periph0_clk = {
	CCU_REG(0x028),
	CCU_HW("pll-periph0", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x028, 28),
	CCU_N(8, 5),
	CCU_K(4, 2),
	.features = CCU_FEATURE_FIXED_POSTDIV,
	.extra = &periph_extra,
};

static struct ccu pll_periph0_2x_clk = {
	CCU_HW("pll-periph0-2x", "pll-periph0", &ccu_fixed_factor_ops, 0),
	CCU_FIXED(1, 2),
};

/* gpu */
/*	rate = 24MHz * (n - 1) / (m - 1) */
static struct ccu pll_gpu_clk = {
	CCU_REG(0x038),
	CCU_HW("pll-gpu", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x038, 28),
	CCU_N(8, 7),
	CCU_M(0, 4),
	.extra = &video_extra,
};

/* periph1 */
/*	rate = 24MHz * (n - 1) * (k - 1) / 2 */
static struct ccu pll_periph1_clk = {
	CCU_REG(0x044),
	CCU_HW("pll-periph1", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x044, 28),
	CCU_N(8, 5),
	CCU_K(4, 2),
	.features = CCU_FEATURE_FIXED_POSTDIV,
	.extra = &periph_extra,
};

/* display engine */
/*	rate = 24MHz * (n - 1) / (m - 1) */
static struct ccu pll_de_clk = {
	CCU_REG(0x048),
	CCU_HW("pll-de", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x048, 28),
	CCU_N(8, 7),
	CCU_M(0, 4),
	.extra = &video_extra,
};

static const char *cpux_parents[] = { losc, hosc,
					"pll-cpux", "pll-cpux" };
static struct ccu cpux_clk = {
	CCU_REG(0x050),
	CCU_HW_PARENTS("cpux", cpux_parents, &ccu_periph_ops,
							CLK_IS_CRITICAL),
	CCU_MUX(16, 2),
};

static struct ccu axi_clk = {
	CCU_REG(0x050),
	CCU_HW("axi", "cpux", &ccu_periph_ops, 0),
	CCU_M(0, 2),
};

static const char *ahb1_parents[] = { losc, hosc,
					"axi", "pll-periph0" };
static const struct ccu_extra ahb1_extra = {
	.variable_prediv = { .index = 3, .shift = 6, .width = 2 },
};
static struct ccu ahb1_clk = {
	CCU_REG(0x054),
	CCU_HW_PARENTS("ahb1", ahb1_parents, &ccu_periph_ops, 0),
	CCU_MUX(12, 2),
	CCU_P(4, 2),
	.features = CCU_FEATURE_MUX_VARIABLE_PREDIV,
	.extra = &ahb1_extra,
};

static const struct ccu_extra apb1_extra = {
	.m_table = { 2, 2, 4, 8 },
};
static struct ccu apb1_clk = {
	CCU_REG(0x054),
	CCU_HW("apb1", "ahb1", &ccu_periph_ops, 0),
	CCU_M(8, 2),
	.features = CCU_FEATURE_M_TABLE,
	.extra = &apb1_extra,
};

static const char *apb2_parents[] = { losc, hosc,
						"pll-periph0", "pll-periph0" };
static struct ccu apb2_clk = {
	CCU_REG(0x058),
	CCU_HW_PARENTS("apb2", apb2_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_M(0, 5),
	CCU_P(16, 2),
};

static const char *ahb2_parents[] = { "ahb1", "pll-periph0" };
static const struct ccu_extra ahb2_extra = {
	.fixed_div = { 1, 2 },
};
static struct ccu ahb2_clk = {
	CCU_REG(0x05c),
	CCU_HW_PARENTS("ahb2", ahb2_parents, &ccu_periph_ops, 0),
	CCU_MUX(0, 2),
	.features = CCU_FEATURE_MUX_FIXED_PREDIV,
	.extra = &ahb2_extra,
};

static struct ccu bus_ce_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ce", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(5),
};
static struct ccu bus_dma_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-dma", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(6),
};
static struct ccu bus_mmc0_clk = {
//	CCU_REG(0x060),
	CCU_HW("bus-mmc0", "ahb1", &ccu_periph_ops, 0),
//	CCU_GATE(8),
//	CCU_RESET(0x2c0, 8),
};
static struct ccu bus_mmc1_clk = {
//	CCU_REG(0x060),
	CCU_HW("bus-mmc1", "ahb1", &ccu_periph_ops, 0),
//	CCU_GATE(9),
//	CCU_RESET(0x2c0, 9),
};
static struct ccu bus_mmc2_clk = {
//	CCU_REG(0x060),
	CCU_HW("bus-mmc2", "ahb1", &ccu_periph_ops, 0),
//	CCU_GATE(10),
//	CCU_RESET(0x2c0, 10),
};
static struct ccu bus_nand_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-nand", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(13),
};
static struct ccu bus_dram_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-dram", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(14),
};
static struct ccu bus_emac_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-emac", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(17),
};
static struct ccu bus_ts_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ts", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(18),
};
static struct ccu bus_hstimer_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-hstimer", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(19),
};
static struct ccu bus_spi0_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-spi0", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(20),
};
static struct ccu bus_spi1_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-spi1", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(21),
};
static struct ccu bus_otg_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-otg", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(23),
};
static struct ccu bus_ehci0_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ehci0", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(24),
};
static struct ccu bus_ehci1_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ehci1", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(25),
};
static struct ccu bus_ehci2_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ehci2", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(26),
};
static struct ccu bus_ehci3_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ehci3", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(27),
};
static struct ccu bus_ohci0_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ohci0", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(28),
};
static struct ccu bus_ohci1_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ohci1", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(29),
};
static struct ccu bus_ohci2_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ohci2", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(30),
};
static struct ccu bus_ohci3_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ohci3", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(31),
};

static struct ccu bus_ve_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-ve", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(0),
};
static struct ccu bus_deinterlace_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-deinterlace", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(5),
};
static struct ccu bus_csi_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-csi", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(8),
};
static struct ccu bus_tve_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-tve", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(9),
};
static struct ccu bus_gpu_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-gpu", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(20),
};
static struct ccu bus_msgbox_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-msgbox", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(21),
};
static struct ccu bus_spinlock_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-spinlock", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(22),
};

static struct ccu bus_codec_clk = {
	CCU_REG(0x068),
	CCU_HW("bus-codec", "apb1", &ccu_periph_ops, 0),
	CCU_GATE(0),
};
static struct ccu bus_spdif_clk = {
	CCU_REG(0x068),
	CCU_HW("bus-spdif", "apb1", &ccu_periph_ops, 0),
	CCU_GATE(1),
};
static struct ccu bus_pio_clk = {
	CCU_REG(0x068),
	CCU_HW("bus-pio", "apb1", &ccu_periph_ops, 0),
	CCU_GATE(5),
};
//static struct ccu bus_ths_clk = {
//	CCU_REG(0x068),
//	CCU_HW("bus-ths", "apb1", &ccu_periph_ops, 0),
//	CCU_GATE(8),
//};

static struct ccu bus_i2c0_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-i2c0", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(0),
};
static struct ccu bus_i2c1_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-i2c1", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(1),
};
static struct ccu bus_i2c2_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-i2c2", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(2),
};
static struct ccu bus_uart0_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-uart0", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(16),
};
static struct ccu bus_uart1_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-uart1", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(17),
};
static struct ccu bus_uart2_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-uart2", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(18),
};
static struct ccu bus_uart3_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-uart3", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(19),
};
static struct ccu bus_scr_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-scr", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(20),
};

static struct ccu bus_ephy_clk = {
	CCU_REG(0x070),
	CCU_HW("bus-ephy", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(0),
};
static struct ccu bus_dbg_clk = {
	CCU_REG(0x070),
	CCU_HW("bus-dbg", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(7),
};

static const struct ccu_extra ths_extra = {
	.m_table = { 1, 2, 4, 6 },
};
static struct ccu ths_clk = {
	CCU_REG(0x074),
	CCU_HW("ths", hosc, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_BUS(0x068, 8),
	CCU_GATE(31),
	CCU_M(0, 2),
	.features = CCU_FEATURE_M_TABLE,
	.extra = &ths_extra,
};

static const char *mmc_parents[] = { hosc, "pll-periph0",
					"pll-periph1" };
static struct ccu nand_clk = {
	CCU_REG(0x080),
	CCU_HW_PARENTS("nand", mmc_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
};

static struct ccu mmc0_clk = {
	CCU_REG(0x088),
	CCU_HW_PARENTS("mmc0", mmc_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_RESET(0x2c0, 8),
	CCU_BUS(0x060, 8),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
	.features = CCU_FEATURE_SET_RATE_GATE,
};
static struct ccu mmc0_sample_clk = {
	CCU_REG(0x088),
	CCU_HW("mmc0_sample", "mmc0", &ccu_phase_ops, 0),
	CCU_PHASE(20, 3),
};
static struct ccu mmc0_output_clk = {
	CCU_REG(0x088),
	CCU_HW("mmc0_output", "mmc0", &ccu_phase_ops, 0),
	CCU_PHASE(8, 3),
};

static const struct ccu_extra mmc_extra = {
	.mode_select.rate = 100000000,
	.mode_select.bit = 30,
};
static struct ccu mmc1_clk = {
	CCU_REG(0x08c),
	CCU_HW_PARENTS("mmc1", mmc_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_RESET(0x2c0, 9),
	CCU_BUS(0x060, 9),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
	.features = CCU_FEATURE_SET_RATE_GATE |
			CCU_FEATURE_MODE_SELECT,
/*			| CCU_FEATURE_MUX_FIXED_PREDIV,	dynamically set */
	.extra = &mmc_extra,
};
static struct ccu mmc1_sample_clk = {
	CCU_REG(0x08c),
	CCU_HW("mmc1_sample", "mmc1", &ccu_phase_ops, 0),
	CCU_PHASE(20, 3),
};
static struct ccu mmc1_output_clk = {
	CCU_REG(0x08c),
	CCU_HW("mmc1_output", "mmc1", &ccu_phase_ops, 0),
	CCU_PHASE(8, 3),
};

static struct ccu mmc2_clk = {
	CCU_REG(0x090),
	CCU_HW_PARENTS("mmc2", mmc_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_RESET(0x2c0, 10),
	CCU_BUS(0x060, 10),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
	.features = CCU_FEATURE_SET_RATE_GATE |
			CCU_FEATURE_MODE_SELECT,
/*			| CCU_FEATURE_MUX_FIXED_PREDIV,	dynamically set */
	.extra = &mmc_extra,
};
static struct ccu mmc2_sample_clk = {
	CCU_REG(0x090),
	CCU_HW("mmc2_sample", "mmc2", &ccu_phase_ops, 0),
	CCU_PHASE(20, 3),
};
static struct ccu mmc2_output_clk = {
	CCU_REG(0x090),
	CCU_HW("mmc2_output", "mmc2", &ccu_phase_ops, 0),
	CCU_PHASE(8, 3),
};

static const char *ts_parents[] = { hosc, "pll-periph0", };
static struct ccu ts_clk = {
	CCU_REG(0x098),
	CCU_HW_PARENTS("ts", ts_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
};

static struct ccu ce_clk = {
	CCU_REG(0x09c),
	CCU_HW_PARENTS("ce", mmc_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
};

static struct ccu spi0_clk = {
	CCU_REG(0x0a0),
	CCU_HW_PARENTS("spi0", mmc_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
};

static struct ccu spi1_clk = {
	CCU_REG(0x0a4),
	CCU_HW_PARENTS("spi1", mmc_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
};

static const char *i2s_parents[] = { "pll-audio-8x", "pll_audio-4x",
					"pll-audio-2x", "pll-audio" };
static struct ccu i2s0_clk = {
	CCU_REG(0x0b0),
	CCU_HW_PARENTS("i2s0", i2s_parents, &ccu_periph_ops, 0),
	CCU_MUX(16, 2),
	CCU_BUS(0x068, 12),
	CCU_GATE(31),
};
static struct ccu i2s1_clk = {
	CCU_REG(0x0b4),
	CCU_HW_PARENTS("i2s1", i2s_parents, &ccu_periph_ops, 0),
	CCU_MUX(16, 2),
	CCU_BUS(0x068, 13),
	CCU_GATE(31),
};

static struct ccu i2s2_clk = {
	CCU_REG(0x0b8),
	CCU_HW_PARENTS("i2s2", i2s_parents, &ccu_periph_ops, 0),
	CCU_MUX(16, 2),
	CCU_BUS(0x068, 14),
	CCU_GATE(31),
};

static struct ccu spdif_clk = {
	CCU_REG(0x0c0),
	CCU_HW("spdif", "pll-audio", &ccu_periph_ops, 0),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static struct ccu usb_phy0_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-phy0", hosc, &ccu_periph_ops, 0),
	CCU_GATE(8),
};

static struct ccu usb_phy1_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-phy1", hosc, &ccu_periph_ops, 0),
	CCU_GATE(9),
};

static struct ccu usb_phy2_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-phy2", hosc, &ccu_periph_ops, 0),
	CCU_GATE(10),
};

static struct ccu usb_phy3_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-phy3", hosc, &ccu_periph_ops, 0),
	CCU_GATE(11),
};

static struct ccu usb_ohci0_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-ohci0", hosc, &ccu_periph_ops, 0),
	CCU_GATE(16),
};

static struct ccu usb_ohci1_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-ohci1", hosc, &ccu_periph_ops, 0),
	CCU_GATE(17),
};

static struct ccu usb_ohci2_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-ohci2", hosc, &ccu_periph_ops, 0),
	CCU_GATE(18),
};

static struct ccu usb_ohci3_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-ohci3", hosc, &ccu_periph_ops, 0),
	CCU_GATE(19),
};

static struct ccu dram_clk = {
	CCU_REG(0x0f4),
	CCU_HW("dram", "pll-ddr", &ccu_periph_ops, CLK_IS_CRITICAL),
	CCU_MUX(20, 2),
	CCU_M(0, 4),
	CCU_UPD(16),
};

static struct ccu dram_ve_clk = {
	CCU_REG(0x0100),
	CCU_HW("dram-ve", "dram", &ccu_periph_ops, 0),
	CCU_GATE(0),
};

static struct ccu dram_csi_clk = {
	CCU_REG(0x100),
	CCU_HW("dram-csi", "dram", &ccu_periph_ops, 0),
	CCU_GATE(1),
};

static struct ccu dram_deinterlace_clk = {
	CCU_REG(0x0100),
	CCU_HW("dram-deinterlace", "dram", &ccu_periph_ops, 0),
	CCU_GATE(2),
};

static struct ccu dram_ts_clk = {
	CCU_REG(0x0100),
	CCU_HW("dram-ts", "dram", &ccu_periph_ops, 0),
	CCU_GATE(3),
};

static const char *de_parents[] = { "pll-periph0-2x", "pll-de" };
static struct ccu de_clk = {
	CCU_REG(0x104),
	CCU_HW_PARENTS("de", de_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 3),
	CCU_RESET(0x2c4, 12),
	CCU_BUS(0x064, 12),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static struct ccu tcon0_clk = {
	CCU_REG(0x118),
	CCU_HW("tcon0", "pll-video", &ccu_periph_ops, 0),
	CCU_MUX(24, 3),
//	CCU_RESET(0x2c4, 4),	// tcon1
//	CCU_BUS(0x064, 4),
	CCU_RESET(0x2c4, 3),
	CCU_BUS(0x064, 3),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static const char *tve_parents[] = { "pll-de", "pll-periph1" };
static struct ccu tve_clk = {
	CCU_REG(0x120),
	CCU_HW_PARENTS("tve", tve_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 3),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static const char *ph0_ph1[] = { "pll-periph0", "pll-periph1" };
static struct ccu deinterlace_clk = {
	CCU_REG(0x124),
	CCU_HW_PARENTS("tve", ph0_ph1, &ccu_periph_ops, 0),
	CCU_MUX(24, 3),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static struct ccu csi_misc_clk = {
	CCU_REG(0x0130),
	CCU_HW("csi-misc", hosc, &ccu_periph_ops, 0),
	CCU_GATE(16),
};

static struct ccu csi_sclk_clk = {
	CCU_REG(0x134),
	CCU_HW_PARENTS("csi-sclk", ph0_ph1, &ccu_periph_ops, 0),
	CCU_MUX(24, 3),
	CCU_GATE(31),
	CCU_M(16, 4),
};

static const char *csi_mclk_parents[] = { hosc, "pll-video", "pll-periph0" };
static struct ccu csi_mclk_clk = {
	CCU_REG(0x134),
	CCU_HW_PARENTS("csi-mclk", csi_mclk_parents, &ccu_periph_ops, 0),
	CCU_MUX(8, 3),
	CCU_GATE(15),
	CCU_M(0, 5),
};

static struct ccu ve_clk = {
	CCU_REG(0x13c),
	CCU_HW("ve", "pll-ve", &ccu_periph_ops, 0),
	CCU_GATE(31),
	CCU_M(16, 3),
};

static struct ccu ac_dig_clk = {
	CCU_REG(0x0140),
	CCU_HW("ac-dig", "pll-audio", &ccu_periph_ops, 0),
	CCU_GATE(31),
};

static struct ccu avs_clk = {
	CCU_REG(0x0144),
	CCU_HW("avs", hosc, &ccu_periph_ops, 0),
	CCU_GATE(31),
};

static struct ccu hdmi_clk = {
	CCU_REG(0x150),
	CCU_HW("hdmi", "pll-video", &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_GATE(31),
	CCU_BUS(0x064, 11),
	CCU_M(0, 4),
};

static struct ccu hdmi_ddc_clk = {
	CCU_REG(0x0154),
	CCU_HW("hdmi-ddc", hosc, &ccu_periph_ops, 0),
	CCU_GATE(31),
};

static const char *mbus_parents[] = { hosc, "pll-periph0-2x",
						"pll-ddr" };
static struct ccu mbus_clk = {
	CCU_REG(0x15c),
	CCU_HW_PARENTS("mbus", mbus_parents, &ccu_periph_ops, CLK_IS_CRITICAL),
	CCU_GATE(31),
	CCU_MUX(24, 2),
	CCU_M(0, 3),
};

static struct ccu gpu_clk = {
	CCU_REG(0x1a0),
	CCU_HW("gpu", "pll-gpu", &ccu_periph_ops, 0),
	CCU_GATE(31),
	CCU_M(0, 3),
};

static struct clk_hw_onecell_data sun8i_h3_ccu_data = {
	.num = 97,
	.hws = {
		[CLK_BUS_DMA]		= &bus_dma_clk.hw,
		[CLK_BUS_EHCI0]		= &bus_ehci0_clk.hw,
		[CLK_BUS_EHCI1]		= &bus_ehci1_clk.hw,
		[CLK_BUS_EHCI2]		= &bus_ehci2_clk.hw,
		[CLK_BUS_EHCI3]		= &bus_ehci3_clk.hw,
		[CLK_BUS_MMC0]		= &bus_mmc0_clk.hw,
		[CLK_BUS_MMC1]		= &bus_mmc1_clk.hw,
		[CLK_BUS_MMC2]		= &bus_mmc2_clk.hw,
		[CLK_BUS_OHCI0]		= &bus_ohci0_clk.hw,
		[CLK_BUS_OHCI1]		= &bus_ohci1_clk.hw,
		[CLK_BUS_OHCI2]		= &bus_ohci2_clk.hw,
		[CLK_BUS_OHCI3]		= &bus_ohci3_clk.hw,
		[CLK_BUS_PIO]		= &bus_pio_clk.hw,
		[CLK_BUS_UART0]		= &bus_uart0_clk.hw,
		[CLK_BUS_UART1]		= &bus_uart1_clk.hw,
		[CLK_BUS_UART2]		= &bus_uart2_clk.hw,
		[CLK_BUS_UART3]		= &bus_uart3_clk.hw,
		[CLK_DE]		= &de_clk.hw,
		[CLK_HDMI]		= &hdmi_clk.hw,
		[CLK_HDMI_DDC]		= &hdmi_ddc_clk.hw,
		[CLK_I2S0]		= &i2s0_clk.hw,
		[CLK_I2S1]		= &i2s1_clk.hw,
		[CLK_I2S2]		= &i2s2_clk.hw,
		[CLK_MMC0]		= &mmc0_clk.hw,
		[CLK_MMC0_SAMPLE]	= &mmc0_sample_clk.hw,
		[CLK_MMC0_OUTPUT]	= &mmc0_output_clk.hw,
		[CLK_MMC1]		= &mmc1_clk.hw,
		[CLK_MMC1_SAMPLE]	= &mmc1_sample_clk.hw,
		[CLK_MMC1_OUTPUT]	= &mmc1_output_clk.hw,
		[CLK_MMC2]		= &mmc2_clk.hw,
		[CLK_MMC2_SAMPLE]	= &mmc2_sample_clk.hw,
		[CLK_MMC2_OUTPUT]	= &mmc2_output_clk.hw,
		[CLK_PLL_AUDIO]		= &pll_audio_clk.hw,
		[CLK_PLL_DE]		= &pll_de_clk.hw,
		[CLK_PLL_GPU]		= &pll_gpu_clk.hw,
		[CLK_PLL_PERIPH0]	= &pll_periph0_clk.hw,
		[CLK_PLL_PERIPH1]	= &pll_periph1_clk.hw,
		[CLK_PLL_VE]		= &pll_ve_clk.hw,
		[CLK_PLL_VIDEO]		= &pll_video_clk.hw,
		[CLK_SPDIF]		= &spdif_clk.hw,
		[CLK_SPI0]		= &spi0_clk.hw,
		[CLK_SPI1]		= &spi1_clk.hw,
		[CLK_TCON0]		= &tcon0_clk.hw,
		[CLK_THS]		= &ths_clk.hw,
		[CLK_TVE]		= &tve_clk.hw,
		[CLK_USB_OHCI0]		= &usb_ohci0_clk.hw,
		[CLK_USB_OHCI1]		= &usb_ohci1_clk.hw,
		[CLK_USB_OHCI2]		= &usb_ohci2_clk.hw,
		[CLK_USB_OHCI3]		= &usb_ohci3_clk.hw,
		[CLK_USB_PHY0]		= &usb_phy0_clk.hw,
		[CLK_USB_PHY1]		= &usb_phy1_clk.hw,
		[CLK_USB_PHY2]		= &usb_phy2_clk.hw,
		[CLK_USB_PHY3]		= &usb_phy3_clk.hw,
		[CLK_VE]		= &ve_clk.hw,
		&pll_cpux_clk.hw,
		&pll_audio_base_clk.hw,
		&pll_audio_2x_clk.hw,
		&pll_audio_4x_clk.hw,
		&pll_audio_8x_clk.hw,
		&pll_periph0_2x_clk.hw,
		&pll_ddr_clk.hw,			/* 60 */
		&cpux_clk.hw,
		&axi_clk.hw,
		&ahb1_clk.hw,
		&apb1_clk.hw,
		&apb2_clk.hw,
		&ahb2_clk.hw,
		&bus_ce_clk.hw,
		&bus_nand_clk.hw,
		&bus_dram_clk.hw,
		[CLK_BUS_EMAC]		= &bus_emac_clk.hw,	/* 70 */
		&bus_ts_clk.hw,
		&bus_hstimer_clk.hw,
		&bus_spi0_clk.hw,
		&bus_spi1_clk.hw,
		&bus_otg_clk.hw,
		&bus_ve_clk.hw,
		&bus_deinterlace_clk.hw,
		&bus_csi_clk.hw,
		&bus_tve_clk.hw,
		&bus_gpu_clk.hw,			/* 80 */
		&bus_msgbox_clk.hw,
		&bus_spinlock_clk.hw,
		&bus_codec_clk.hw,
		&bus_spdif_clk.hw,
		&bus_i2c0_clk.hw,
		&bus_i2c1_clk.hw,
		&bus_i2c2_clk.hw,
		&bus_scr_clk.hw,
		[CLK_BUS_EPHY]		= &bus_ephy_clk.hw,
		&bus_dbg_clk.hw,			/* 90 */
		&nand_clk.hw,
		&ts_clk.hw,
		&ce_clk.hw,
		&dram_clk.hw,
		&dram_ve_clk.hw,
		&dram_csi_clk.hw,
		&dram_deinterlace_clk.hw,
		&dram_ts_clk.hw,
		&deinterlace_clk.hw,
		&csi_misc_clk.hw,			/* 100 */
		&csi_sclk_clk.hw,
		&csi_mclk_clk.hw,
		&ac_dig_clk.hw,
		&avs_clk.hw,
		&mbus_clk.hw,
		&gpu_clk.hw,
	},
};

static struct ccu_reset_map sun8i_h3_ccu_resets[] = {
	[RST_USB_PHY0]	= { 0x0cc, 0 },
	[RST_USB_PHY1]	= { 0x0cc, 1 },
	[RST_USB_PHY2]	= { 0x0cc, 2 },
	[RST_USB_PHY3]	= { 0x0cc, 3 },

	[RST_MBUS]	= { 0x0fc, 31 },

	[RST_CE]	= { 0x2c0, 5 },
	[RST_DMA]	= { 0x2c0, 6 },
	[RST_NAND]	= { 0x2c0, 13 },
	[RST_DRAM]	= { 0x2c0, 14 },
	[RST_EMAC]	= { 0x2c0, 17 },
	[RST_TS]	= { 0x2c0, 18 },
	[RST_HSTIMER]	= { 0x2c0, 19 },
	[RST_SPI0]	= { 0x2c0, 20 },
	[RST_SPI1]	= { 0x2c0, 21 },
	[RST_OTG]	= { 0x2c0, 23 },
	[RST_EHCI0]	= { 0x2c0, 24 },
	[RST_EHCI1]	= { 0x2c0, 25 },
	[RST_EHCI2]	= { 0x2c0, 26 },
	[RST_EHCI3]	= { 0x2c0, 27 },
	[RST_OHCI0]	= { 0x2c0, 28 },
	[RST_OHCI1]	= { 0x2c0, 29 },
	[RST_OHCI2]	= { 0x2c0, 30 },
	[RST_OHCI3]	= { 0x2c0, 31 },

	[RST_VE]	= { 0x2c4, 0 },
	[RST_DEINTERLACE] = { 0x2c4, 5 },
	[RST_CSI]	= { 0x2c4, 8 },
	[RST_TVE]	= { 0x2c4, 9 },
	[RST_HDMI0]	= { 0x2c4, 10 },
	[RST_HDMI1]	= { 0x2c4, 11 },
	[RST_GPU]	= { 0x2c4, 20 },
	[RST_MSGBOX]	= { 0x2c4, 21 },
	[RST_SPINLOCK]	= { 0x2c4, 22 },
	[RST_DBG]	= { 0x2c4, 31 },

	[RST_EPHY]	= { 0x2c8, 2 },

	[RST_CODEC]	= { 0x2d0, 0 },
	[RST_SPDIF]	= { 0x2d0, 1 },
	[RST_THS]	= { 0x2d0, 8 },
	[RST_I2S0]	= { 0x2d0, 12 },
	[RST_I2S1]	= { 0x2d0, 13 },
	[RST_I2S2]	= { 0x2d0, 14 },

	[RST_I2C0]	= { 0x2d4, 0 },
	[RST_I2C1]	= { 0x2d4, 1 },
	[RST_I2C2]	= { 0x2d4, 2 },
	[RST_UART0]	= { 0x2d4, 16 },
	[RST_UART1]	= { 0x2d4, 17 },
	[RST_UART2]	= { 0x2d4, 18 },
	[RST_UART3]	= { 0x2d4, 19 },
	[RST_SCR]	= { 0x2d4, 20 },
};

static struct ccu_reset sun8i_h3_resets = {
	.rcdev.ops = &ccu_reset_ops,
	.rcdev.owner = THIS_MODULE,
	.rcdev.nr_resets = ARRAY_SIZE(sun8i_h3_ccu_resets),
	.reset_map = sun8i_h3_ccu_resets,
};

static void __init sun8i_h3_ccu_setup(struct device_node *node)
{
	ccu_probe(node, &sun8i_h3_ccu_data,
			&sun8i_h3_resets);
}
CLK_OF_DECLARE(sun8i_h3_ccu, "allwinner,sun8i-h3-ccu",
	       sun8i_h3_ccu_setup);
