/*
 * Copyright (C) 2016 Jean-Francois Moine <moinejf@free.fr>
 * Based on the H3 version from
 * Copyright (C) 2016 Maxime Ripard <maxime.ripard@free-electrons.com>
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 *  a) This file is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of the
 *     License, or (at your option) any later version.
 *
 *     This file is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 * Or, alternatively,
 *
 *  b) Permission is hereby granted, free of charge, to any person
 *     obtaining a copy of this software and associated documentation
 *     files (the "Software"), to deal in the Software without
 *     restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or
 *     sell copies of the Software, and to permit persons to whom the
 *     Software is furnished to do so, subject to the following
 *     conditions:
 *
 *     The above copyright notice and this permission notice shall be
 *     included in all copies or substantial portions of the Software.
 *
 *     THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *     EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *     OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *     NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *     HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *     WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *     FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *     OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _DT_BINDINGS_CLK_SUN8I_A83T_H_
#define _DT_BINDINGS_CLK_SUN8I_A83T_H_

#define CLK_BUS_DMA		0
#define CLK_BUS_EHCI0		1
#define CLK_BUS_EHCI1		2
#define CLK_BUS_MMC0		3
#define CLK_BUS_MMC1		4
#define CLK_BUS_MMC2		5
#define CLK_BUS_OHCI0		6
#define CLK_BUS_PIO		7
#define CLK_BUS_UART0		8
#define CLK_BUS_UART1		9
#define CLK_BUS_UART2		10
#define CLK_BUS_UART3		11
#define CLK_BUS_UART4		12
#define CLK_BUS_USBDRD		13
#define CLK_DAUDIO0		14
#define CLK_DAUDIO1		15
#define CLK_DAUDIO2		16
#define CLK_HDMI		17
#define CLK_HDMI_DDC		18
#define CLK_MMC0		19
#define CLK_MMC0_SAMPLE		20
#define CLK_MMC0_OUTPUT		21
#define CLK_MMC1		22
#define CLK_MMC1_SAMPLE		23
#define CLK_MMC1_OUTPUT		24
#define CLK_MMC2		25
#define CLK_MMC2_SAMPLE		26
#define CLK_MMC2_OUTPUT		27
#define CLK_OHCI0		28
#define CLK_OSC12M		29
#define CLK_PLL_AUDIO		30
#define CLK_PLL_DE		31
#define CLK_PLL_GPU		32
#define CLK_PLL_HSIC		33
#define CLK_PLL_PERIPH		34
#define CLK_PLL_VE		35
#define CLK_PLL_VIDEO0		36
#define CLK_PLL_VIDEO1		37
#define CLK_SPDIF		38
#define CLK_SPI0		39
#define CLK_SPI1		40
#define CLK_TCON0		41
#define CLK_TCON1		42
#define CLK_TDM			43
#define CLK_USB_PHY0		44
#define CLK_USB_PHY1		45
#define CLK_USB_HSIC		46
#define CLK_VE			47

#endif /* _DT_BINDINGS_CLK_SUN8I_A83T_H_ */
