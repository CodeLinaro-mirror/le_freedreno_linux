/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CLK_PLL_H__
#define __MSM_CLK_PLL_H__

#include <linux/clk-provider.h>

/**
 * struct clk_pll - phase locked loop (PLL)
 * @l_reg: L register
 * @m_reg: M register
 * @n_reg: N register
 * @config_reg: config register
 * @mode_reg: mode register
 * @status_reg: status register
 * @status_bit: ANDed with @status_reg to determine if PLL is enabled
 * @hw: handle between common and hardware-specific interfaces
 */
struct clk_pll {
	u32	l_reg;
	u32	m_reg;
	u32	n_reg;
	u32	config_reg;
	u32	mode_reg;
	u32	status_reg;
	u8	status_bit;

	struct clk_hw	hw;
};

extern const struct clk_ops clk_pll_ops;
extern const struct clk_ops clk_pll_vote_ops;

#define to_clk_pll(_hw) container_of(_hw, struct clk_pll, hw)

#endif
