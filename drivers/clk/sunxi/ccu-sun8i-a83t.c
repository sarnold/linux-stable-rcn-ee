/*
 * Copyright (c) 2016 Jean-Francois Moine <moinejf@free.fr>
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

#include <dt-bindings/clock/sun8i-a83t.h>
#include <dt-bindings/reset/sun8i-a83t.h>

#include "ccu.h"

static const char losc[32] = "osc32k";
static const char hosc[32] = "osc24M";

/* 2 * cpux */
/*	rate = 24MHz * n / p */
static struct ccu pll_c0cpux_clk = {
	CCU_REG(0x000),
	CCU_HW("pll-c0cpux", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 0),
	CCU_N(8, 8), .n_min = 12,
	CCU_P(16, 1),			/* only when rate < 288MHz */
	.features = CCU_FEATURE_N0,
};

static struct ccu pll_c1cpux_clk = {
	CCU_REG(0x004),
	CCU_HW("pll-c1cpux", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 1),
	CCU_N(8, 8), .n_min = 12,
	CCU_P(16, 1),			/* only when rate < 288MHz */
	.features = CCU_FEATURE_N0,
};

/* audio */
/*	rate = 24MHz * n / (d1 + 1) / (d2 + 1) / (p + 1) */
static struct ccu pll_audio_clk = {
	CCU_REG(0x008),
	CCU_HW("pll-audio", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 2),
	CCU_N(8, 8), .n_min = 12,
	CCU_D1(16, 1),
	CCU_D2(18, 1),
	CCU_M(0, 6),		/* p = divider */
	.features = CCU_FEATURE_N0,
};

/* video 0 */
/*	rate = 24MHz * n / (d1 + 1) >> p */
static struct ccu pll_video0_clk = {
	CCU_REG(0x010),
	CCU_HW("pll-video0", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 3),
	CCU_N(8, 8), .n_min = 12,
	CCU_D1(16, 1),
	CCU_P(0, 2),
	.features = CCU_FEATURE_N0,
};

/* video engine */
/*	rate = 24MHz * n / (d1 + 1) / (d2 + 1) */
static struct ccu pll_ve_clk = {
	CCU_REG(0x018),
	CCU_HW("pll-ve", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 4),
	CCU_N(8, 8), .n_min = 12,
	CCU_D1(16, 1),
	CCU_D2(18, 1),
	.features = CCU_FEATURE_N0,
};

/* ddr */
/*	rate = 24MHz * (n + 1) / (d1 + 1) / (d2 + 1)
 *	bit 21: DDR_CLOCK = PLL_DDR / PLL_PERIPH (default DDR)
 */
static struct ccu pll_ddr_clk = {
	CCU_REG(0x020),
	CCU_HW("pll-ddr", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 5),
	CCU_N(8, 6), .n_min = 12,
	CCU_D1(16, 1),
	CCU_D2(18, 1),
	CCU_UPD(30),
};

/* periph */
/*	rate = 24MHz * n / (d1 + 1) / (d2 + 1) */
static struct ccu pll_periph_clk = {
	CCU_REG(0x028),
	CCU_HW("pll-periph", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 6),
	CCU_N(8, 8), .n_min = 12,
	CCU_D1(16, 1),
	CCU_D2(18, 1),
	.features = CCU_FEATURE_N0,
};

/* gpu */
/*	rate = 24MHz * n / (d1 + 1) / (d2 + 1) */
static struct ccu pll_gpu_clk = {
	CCU_REG(0x038),
	CCU_HW("pll-gpu", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 7),
	CCU_N(8, 8), .n_min = 12,
	CCU_D1(16, 1),
	CCU_D2(18, 1),
	.features = CCU_FEATURE_N0,
};

/* hsic */
/*	rate = 24MHz * n / (d1 + 1) / (d2 + 1) */
static struct ccu pll_hsic_clk = {
	CCU_REG(0x044),
	CCU_HW("pll-hsic", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 8),
	CCU_N(8, 8), .n_min = 12,
	CCU_D1(16, 1),
	CCU_D2(18, 1),
	.features = CCU_FEATURE_N0,
};

/* display engine */
/*	rate = 24MHz * n / (d1 + 1) / (d2 + 1) */
static struct ccu pll_de_clk = {
	CCU_REG(0x048),
	CCU_HW("pll-de", hosc, &ccu_pll_ops, 0),
	CCU_RESET(0x2c4, 12),
	CCU_BUS(0x64, 12),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 9),
	CCU_N(8, 8), .n_min = 12,
	CCU_D1(16, 1),
	CCU_D2(18, 1),
	.features = CCU_FEATURE_N0,
};

/* video 1 */
/*	rate = 24MHz * n / (d1 + 1) >> p */
static struct ccu pll_video1_clk = {
	CCU_REG(0x04c),
	CCU_HW("pll-video1", hosc, &ccu_pll_ops, 0),
	CCU_GATE(31),
	CCU_LOCK(0x20c, 10),
	CCU_N(8, 8), .n_min = 12,
	CCU_D1(16, 1),
	CCU_P(0, 2),
	.features = CCU_FEATURE_N0,
};

static const char *c0cpux_parents[] = { hosc, "pll-c0cpux" };
static struct ccu c0cpux_clk = {
	CCU_REG(0x050),
	CCU_HW_PARENTS("c0cpux", c0cpux_parents, &ccu_periph_ops,
							CLK_IS_CRITICAL),
	CCU_MUX(12, 1),
};

static struct ccu axi0_clk = {
	CCU_REG(0x050),
	CCU_HW("axi0", "c0cpux", &ccu_periph_ops, 0),
	CCU_M(0, 2),
};

static const char *c1cpux_parents[] = { hosc, "pll-c1cpux" };
static struct ccu c1cpux_clk = {
	CCU_REG(0x050),
	CCU_HW_PARENTS("c1cpux", c1cpux_parents, &ccu_periph_ops,
							CLK_IS_CRITICAL),
	CCU_MUX(28, 1),
};

static struct ccu axi1_clk = {
	CCU_REG(0x050),
	CCU_HW("axi1", "c1cpux", &ccu_periph_ops, 0),
	CCU_M(16, 2),
};

static const char *ahb1_parents[] = { losc, hosc,
					"pll-periph" };
static const struct ccu_extra ahb1_extra = {
	.variable_prediv = { .index = 2, .shift = 6, .width = 2 },
};
static struct ccu ahb1_clk = {
	CCU_REG(0x054),
	CCU_HW_PARENTS("ahb1", ahb1_parents, &ccu_periph_ops, 0),
	CCU_MUX(12, 2),
	CCU_P(4, 2),
	.features = CCU_FEATURE_MUX_VARIABLE_PREDIV,
	.extra = &ahb1_extra,
};

static struct ccu apb1_clk = {
	CCU_REG(0x054),
	CCU_HW("apb1", "ahb1", &ccu_periph_ops, 0),
	CCU_M(8, 2),
};

static const char *apb2_parents[] = { losc, hosc,
					"pll-periph",
					"pll-periph" };
static struct ccu apb2_clk = {
	CCU_REG(0x058),
	CCU_HW_PARENTS("apb2", apb2_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_M(0, 5),
	CCU_P(16, 2),
};

#if 1 // from bpi-m3 code
static struct ccu ahb2_clk = {
	CCU_HW("ahb2", "pll-periph", &ccu_fixed_factor_ops, 0),
	CCU_FIXED(1, 2),
};
#else // from doc
static const char *ahb2_parents[] = { "ahb1",
					"pll-periph" };
static const struct ccu_extra ahb2_extra = {
	.fixed_div = { 1, 2},
};
static struct ccu ahb2_clk = {
	CCU_REG(0x05c),
	CCU_HW_PARENTS("ahb2", ahb2_parents, &ccu_periph_ops, 0),
	CCU_MUX(0, 2),
	.features = CCU_FEATURE_MUX_FIXED_PREDIV,
	.extra = &ahb2_extra,
};
#endif

static struct ccu bus_mipi_dsi_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-mipi-dsi", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(1),
};
static struct ccu bus_ss_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ss", "ahb1", &ccu_periph_ops, 0),
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
//	CCU_RESET(0x2c0, 8),
//	CCU_GATE(8),
};
static struct ccu bus_mmc1_clk = {
//	CCU_REG(0x060),
	CCU_HW("bus-mmc1", "ahb1", &ccu_periph_ops, 0),
//	CCU_RESET(0x2c0, 9),
//	CCU_GATE(9),
};
static struct ccu bus_mmc2_clk = {
//	CCU_REG(0x060),
	CCU_HW("bus-mmc2", "ahb1", &ccu_periph_ops, 0),
//	CCU_RESET(0x2c0, 10),
//	CCU_GATE(10),
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
	CCU_HW("bus-emac", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(17),
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
static struct ccu bus_usbdrd_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-usbdrd", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(24),
};
static struct ccu bus_ehci0_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ehci0", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(26),
};
static struct ccu bus_ehci1_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ehci1", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(27),
};
static struct ccu bus_ohci0_clk = {
	CCU_REG(0x060),
	CCU_HW("bus-ohci0", "ahb2", &ccu_periph_ops, 0),
	CCU_GATE(29),
};

static struct ccu bus_ve_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-ve", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(0),
};
static struct ccu bus_csi_clk = {
	CCU_REG(0x064),
	CCU_HW("bus-csi", "ahb1", &ccu_periph_ops, 0),
	CCU_GATE(8),
};
static struct ccu bus_gpu_clk= {
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
static struct ccu bus_uart4_clk = {
	CCU_REG(0x06c),
	CCU_HW("bus-uart4", "apb2", &ccu_periph_ops, 0),
	CCU_GATE(20),
};

static const char *cci400_parents[] = { hosc,
					"pll-periph",
					"pll-hsic" };
static struct ccu cci400_clk = {
	CCU_REG(0x078),
	CCU_HW_PARENTS("cci400", cci400_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_M(0, 2),
};

static const char *mmc_parents[] = { hosc, "pll-periph" };
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

static struct ccu mmc1_clk = {
	CCU_REG(0x08c),
	CCU_HW_PARENTS("mmc1", mmc_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_RESET(0x2c0, 9),
	CCU_BUS(0x060, 9),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
	.features = CCU_FEATURE_SET_RATE_GATE,
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

#if 0 // other version - not coded
static const struct ccu_extra mmc_extra = {
	.mode_select.rate = 104000000,
};
static struct ccu mmc2mod_clk = {
	CCU_REG(0x090),
	CCU_HW_PARENTS("mmc2mod", mmc_parents, &ccu_periph_ops, 0),
	CCU_M(30, 1),
	.features = CCU_FEATURE_MODE_SELECT,
	.extra = &mmc_extra,
};
static struct ccu mmc2_clk = {
	CCU_REG(0x090),
	CCU_HW("mmc2", "mmc2mod", &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_RESET(0x2c0, 10),
	CCU_BUS(0x060, 10),
	CCU_GATE(31),
	CCU_M(0, 4),
	CCU_P(16, 2),
	.features = CCU_FEATURE_SET_RATE_GATE,
};
#else // works fine enough
static const struct ccu_extra mmc_extra = {
	.mode_select.rate = 100000000,
	.mode_select.bit = 30,
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
#endif
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

static struct ccu ss_clk = {
	CCU_REG(0x09c),
	CCU_HW_PARENTS("ss", mmc_parents, &ccu_periph_ops, 0),
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

static struct ccu daudio0_clk = {
	CCU_REG(0x0b0),
	CCU_HW("daudio0", "pll-audio", &ccu_periph_ops, 0),
	CCU_BUS(0x068, 12),
	CCU_GATE(31),
	CCU_M(0, 4),
};
static struct ccu daudio1_clk = {
	CCU_REG(0x0b4),
	CCU_HW("daudio1", "pll-audio", &ccu_periph_ops, 0),
	CCU_GATE(31),
	CCU_BUS(0x068, 13),
	CCU_M(0, 4),
};

static struct ccu daudio2_clk = {
	CCU_REG(0x0b8),
	CCU_HW("daudio2", "pll-audio", &ccu_periph_ops, 0),
	CCU_GATE(31),
	CCU_BUS(0x068, 14),
	CCU_M(0, 4),
};

static struct ccu tdm_clk = {
	CCU_REG(0x0bc),
	CCU_HW("tdm", "pll-audio", &ccu_periph_ops, 0),
	CCU_BUS(0x068, 15),
	CCU_GATE(31),
	CCU_M(0, 4),
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

static struct ccu usb_hsic_clk = {
	CCU_REG(0x0cc),
	CCU_HW("usb-hsic", hosc, &ccu_periph_ops, 0),
	CCU_GATE(10),
};

static struct ccu osc12m_clk = {
	CCU_REG(0x0cc),
	CCU_HW("osc12M", hosc, &ccu_fixed_factor_ops, 0),
	CCU_GATE(11),
	CCU_FIXED(1, 2),
};

static struct ccu ohci0_clk = {
	CCU_REG(0x0cc),
	CCU_HW("ohci0", hosc, &ccu_periph_ops, 0),
	CCU_GATE(16),
};

static struct ccu dram_clk = {
	CCU_REG(0x0f4),
	CCU_HW("dram", "pll-ddr", &ccu_periph_ops, 0),
	CCU_M(0, 4),
	CCU_UPD(16),
};

/* pll_ddr config not done */

static struct ccu dram_ve_clk = {
	CCU_REG(0x0100),
	CCU_HW("dram-ve", "dram", &ccu_periph_ops, 0),
	CCU_GATE(0),
};

static struct ccu dram_csi_clk = {
	CCU_REG(0x0100),
	CCU_HW("dram-csi", "dram", &ccu_periph_ops, 0),
	CCU_GATE(1),
};

static struct ccu tcon0_clk = {
	CCU_REG(0x118),
	CCU_HW("tcon0", "pll-video0", &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_RESET(0x2c4, 4),
	CCU_BUS(0x064, 4),
	CCU_GATE(31),
};

static struct ccu tcon1_clk = {
	CCU_REG(0x11c),
	CCU_HW("tcon1", "pll-video1", &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_RESET(0x2c4, 5),
	CCU_BUS(0x064, 5),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static struct ccu csi_misc_clk = {
	CCU_REG(0x0130),
	CCU_HW("csi-misc", hosc, &ccu_periph_ops, 0),
	CCU_GATE(16),
};

static struct ccu mipi_csi_clk = {
	CCU_REG(0x0130),
	CCU_HW("mipi-csi", hosc, &ccu_periph_ops, 0),
	CCU_GATE(31),
};

/* hack: "losc" is used for no parent */
static const char *csi_sclk_parents[] = { "pll-periph",
					losc, losc, losc, losc,
					"pll-ve" };
static struct ccu csi_sclk_clk = {
	CCU_REG(0x134),
	CCU_HW_PARENTS("csi-sclk", csi_sclk_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 3),
	CCU_GATE(31),
	CCU_M(16, 4),
};

/* hack: "losc" is used for no parent */
static const char *csi_mclk_parents[] = { losc, losc, losc,
					"pll-periph",
					losc,
					hosc };
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

static struct ccu avs_clk = {
	CCU_REG(0x0144),
	CCU_HW("avs", hosc, &ccu_periph_ops, 0),
	CCU_GATE(31),
};

static struct ccu hdmi_clk = {
	CCU_REG(0x150),
	CCU_HW("hdmi", "pll-video1", &ccu_periph_ops, 0),
	CCU_MUX(24, 2),
	CCU_BUS(0x064, 11),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static struct ccu hdmi_ddc_clk = {
	CCU_REG(0x0154),
	CCU_HW("hdmi-ddc", hosc, &ccu_periph_ops, 0),
	CCU_GATE(31),
};

static const char *mbus_parents[] = { hosc, "pll-periph",
					"pll-ddr" };
static struct ccu mbus_clk = {
	CCU_REG(0x15c),
	CCU_HW_PARENTS("mbus", mbus_parents, &ccu_periph_ops, CLK_IS_CRITICAL),
	CCU_MUX(24, 2),
	CCU_GATE(31),
	CCU_M(0, 3),
};

static struct ccu mipi_dsi0_clk = {
	CCU_REG(0x168),
	CCU_HW("mipi-dsi0", "pll-video0", &ccu_periph_ops, 0),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static const char *mipi_dsi1_parents[] = { hosc,
//--fixme: declare null_clk "null" with fixed factor mul=0
/* hack: "losc" is used for no parent */
				losc, losc, losc, losc,
					losc, losc, losc, losc,
				"pll-video0" };
static struct ccu mipi_dsi1_clk = {
	CCU_REG(0x16c),
	CCU_HW_PARENTS("mipi-dsi1", mipi_dsi1_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 4),
	CCU_GATE(31),
	CCU_M(0, 4),
};

static struct ccu gpu_core_clk = {
	CCU_REG(0x1a0),
	CCU_HW("gpu-core", "pll-gpu", &ccu_periph_ops, 0),
	CCU_GATE(31),
	CCU_M(0, 3),
};

static const char *gpu_mem_parents[] = { "pll-gpu", "pll-periph" };
static struct ccu gpu_mem_clk = {
	CCU_REG(0x1a4),
	CCU_HW_PARENTS("gpu-mem", gpu_mem_parents, &ccu_periph_ops, 0),
	CCU_MUX(24, 1),
	CCU_GATE(31),
	CCU_M(0, 3),
};

static struct ccu gpu_hyd_clk = {
	CCU_REG(0x1a8),
	CCU_HW("gpu-hyd", "pll-gpu", &ccu_periph_ops, 0),
	CCU_GATE(31),
	CCU_M(0, 3),
};

static struct clk_hw_onecell_data sun8i_a83t_ccu_data = {
	.num = 93,
	.hws = {
		[CLK_BUS_DMA]		= (struct clk_hw *) &bus_dma_clk.hw,
		[CLK_BUS_EHCI0]		= &bus_ehci0_clk.hw,
		[CLK_BUS_EHCI1]		= &bus_ehci1_clk.hw,
		[CLK_BUS_MMC0]		= &bus_mmc0_clk.hw,
		[CLK_BUS_MMC1]		= &bus_mmc1_clk.hw,
		[CLK_BUS_MMC2]		= &bus_mmc2_clk.hw,
		[CLK_BUS_OHCI0]		= &bus_ohci0_clk.hw,
		[CLK_BUS_PIO]		= &bus_pio_clk.hw,
		[CLK_BUS_UART0]		= &bus_uart0_clk.hw,
		[CLK_BUS_UART1]		= &bus_uart1_clk.hw,
		[CLK_BUS_UART2]		= &bus_uart2_clk.hw,
		[CLK_BUS_UART3]		= &bus_uart3_clk.hw,
		[CLK_BUS_UART4]		= &bus_uart4_clk.hw,
		[CLK_BUS_USBDRD]	= &bus_usbdrd_clk.hw,
		[CLK_DAUDIO0]		= &daudio0_clk.hw,
		[CLK_DAUDIO1]		= &daudio1_clk.hw,
		[CLK_DAUDIO2]		= &daudio2_clk.hw,
		[CLK_HDMI]		= &hdmi_clk.hw,
		[CLK_HDMI_DDC]		= &hdmi_ddc_clk.hw,
		[CLK_MMC0]		= &mmc0_clk.hw,
		[CLK_MMC0_SAMPLE]	= &mmc0_sample_clk.hw,
		[CLK_MMC0_OUTPUT]	= &mmc0_output_clk.hw,
		[CLK_MMC1]		= &mmc1_clk.hw,
		[CLK_MMC1_SAMPLE]	= &mmc1_sample_clk.hw,
		[CLK_MMC1_OUTPUT]	= &mmc1_output_clk.hw,
		[CLK_MMC2]		= &mmc2_clk.hw,
		[CLK_MMC2_SAMPLE]	= &mmc2_sample_clk.hw,
		[CLK_MMC2_OUTPUT]	= &mmc2_output_clk.hw,
		[CLK_OHCI0]		= &ohci0_clk.hw,
		[CLK_OSC12M]		= &osc12m_clk.hw,
		[CLK_PLL_AUDIO]		= &pll_audio_clk.hw,
		[CLK_PLL_DE]		= &pll_de_clk.hw,
		[CLK_PLL_GPU]		= &pll_gpu_clk.hw,
		[CLK_PLL_HSIC]		= &pll_hsic_clk.hw,
		[CLK_PLL_PERIPH]	= &pll_periph_clk.hw,
		[CLK_PLL_VE]		= &pll_ve_clk.hw,
		[CLK_PLL_VIDEO0]	= &pll_video0_clk.hw,
		[CLK_PLL_VIDEO1]	= &pll_video1_clk.hw,
		[CLK_SPDIF]		= &spdif_clk.hw,
		[CLK_SPI0]		= &spi0_clk.hw,
		[CLK_SPI1]		= &spi1_clk.hw,
		[CLK_TCON0]		= &tcon0_clk.hw,
		[CLK_TCON1]		= &tcon1_clk.hw,
		[CLK_TDM]		= &tdm_clk.hw,
		[CLK_USB_PHY0]		= &usb_phy0_clk.hw,
		[CLK_USB_PHY1]		= &usb_phy1_clk.hw,
		[CLK_USB_HSIC]		= &usb_hsic_clk.hw,
		[CLK_VE]		= &ve_clk.hw,
		&pll_c0cpux_clk.hw,
		&pll_c1cpux_clk.hw,
		&pll_ddr_clk.hw,			/* 50 */
		&c0cpux_clk.hw,
		&axi0_clk.hw,
		&c1cpux_clk.hw,
		&axi1_clk.hw,
		&ahb1_clk.hw,
		&apb1_clk.hw,
		&apb2_clk.hw,
		&ahb2_clk.hw,
		&bus_mipi_dsi_clk.hw,
		&bus_ss_clk.hw,				/* 60 */
		&bus_nand_clk.hw,
		&bus_dram_clk.hw,
		&bus_emac_clk.hw,
		&bus_hstimer_clk.hw,
		&bus_spi0_clk.hw,
		&bus_spi1_clk.hw,
		&bus_ve_clk.hw,
		&bus_csi_clk.hw,
		&bus_gpu_clk.hw,
		&bus_msgbox_clk.hw,			/* 70 */
		&bus_spinlock_clk.hw,
		&bus_spdif_clk.hw,
		&bus_i2c0_clk.hw,
		&bus_i2c1_clk.hw,
		&bus_i2c2_clk.hw,
		&cci400_clk.hw,
		&nand_clk.hw,
		&ss_clk.hw,
		&dram_clk.hw,
		&dram_ve_clk.hw,			/* 80 */
		&dram_csi_clk.hw,
		&csi_misc_clk.hw,
		&mipi_csi_clk.hw,
		&csi_sclk_clk.hw,
		&csi_mclk_clk.hw,
		&avs_clk.hw,
		&mbus_clk.hw,
		&mipi_dsi0_clk.hw,
		&mipi_dsi1_clk.hw,
		&gpu_core_clk.hw,			/* 90 */
		&gpu_mem_clk.hw,
		&gpu_hyd_clk.hw,
	},
};

static struct ccu_reset_map sun8i_a83t_ccu_resets[] = {
	[RST_USB_PHY0]	=  { 0x0cc, 0 },
	[RST_USB_PHY1]	=  { 0x0cc, 1 },
	[RST_USB_HSIC]	=  { 0x0cc, 2 },

	[RST_DRAM_CTR]	=  { 0x0f4, 31 },
	[RST_MBUS]	=  { 0x0fc, 31 },

	[RST_MIPI_DSI]	=  { 0x2c0, 1 },
	[RST_CE]	=  { 0x2c0, 5 },
	[RST_DMA]	=  { 0x2c0, 6 },
	[RST_NAND]	=  { 0x2c0, 13 },
	[RST_DRAM]	=  { 0x2c0, 14 },
	[RST_EMAC]	=  { 0x2c0, 17 },
	[RST_HSTIMER]	=  { 0x2c0, 19 },
	[RST_SPI0]	=  { 0x2c0, 20 },
	[RST_SPI1]	=  { 0x2c0, 21 },
	[RST_USBDRD]	=  { 0x2c0, 24 },
	[RST_EHCI0]	=  { 0x2c0, 26 },
	[RST_EHCI1]	=  { 0x2c0, 27 },
	[RST_OHCI0]	=  { 0x2c0, 29 },

	[RST_VE]	=  { 0x2c4, 0 },
	[RST_CSI]	=  { 0x2c4, 8 },
	[RST_HDMI0]	=  { 0x2c4, 10 },
	[RST_HDMI1]	=  { 0x2c4, 11 },
	[RST_GPU]	=  { 0x2c4, 20 },
	[RST_MSGBOX]	=  { 0x2c4, 21 },
	[RST_SPINLOCK]	=  { 0x2c4, 22 },

	[RST_LVDS]	=  { 0x2c8, 0 },

	[RST_SPDIF]	=  { 0x2d0, 1 },
	[RST_DAUDIO0]	=  { 0x2d0, 12 },
	[RST_DAUDIO1]	=  { 0x2d0, 13 },
	[RST_DAUDIO2]	=  { 0x2d0, 14 },
	[RST_TDM]	=  { 0x2d0, 15 },

	[RST_I2C0]	=  { 0x2d8, 0 },
	[RST_I2C1]	=  { 0x2d8, 1 },
	[RST_I2C2]	=  { 0x2d8, 2 },
	[RST_UART0]	=  { 0x2d8, 16 },
	[RST_UART1]	=  { 0x2d8, 17 },
	[RST_UART2]	=  { 0x2d8, 18 },
	[RST_UART3]	=  { 0x2d8, 19 },
	[RST_UART4]	=  { 0x2d8, 20 },
};

static struct ccu_reset sun8i_a83t_resets = {
	.rcdev.ops = &ccu_reset_ops,
	.rcdev.owner = THIS_MODULE,
	.rcdev.nr_resets = ARRAY_SIZE(sun8i_a83t_ccu_resets),
	.reset_map = sun8i_a83t_ccu_resets,
};

static void __init sun8i_a83t_ccu_setup(struct device_node *node)
{
	ccu_probe(node, &sun8i_a83t_ccu_data,
			&sun8i_a83t_resets);
}
CLK_OF_DECLARE(sun8i_a83t_ccu, "allwinner,sun8i-a83t-ccu",
	       sun8i_a83t_ccu_setup);
