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

#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>

#include <asm/div64.h>

#include "clk-pll.h"

#define PLL_OUTCTRL	BIT(0)
#define PLL_BYPASSNL	BIT(1)
#define PLL_RESET_N	BIT(2)

static int clk_pll_enable(struct clk_hw *hw)
{
	struct clk_pll *pll = to_clk_pll(hw);
	int ret;

	/* Disable PLL bypass mode. */
	ret = regmap_update_bits(hw->regmap, pll->mode_reg, PLL_BYPASSNL,
				 PLL_BYPASSNL);
	if (ret)
		return ret;

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Delay 10us just to be safe.
	 */
	udelay(10);

	/* De-assert active-low PLL reset. */
	ret = regmap_update_bits(hw->regmap, pll->mode_reg, PLL_RESET_N,
				 PLL_RESET_N);
	if (ret)
		return ret;

	/* Wait until PLL is locked. */
	udelay(50);

	/* Enable PLL output. */
	ret = regmap_update_bits(hw->regmap, pll->mode_reg, PLL_OUTCTRL,
				 PLL_OUTCTRL);
	if (ret)
		return ret;

	return 0;
}

static void clk_pll_disable(struct clk_hw *hw)
{
	struct clk_pll *pll = to_clk_pll(hw);
	u32 mask;

	mask = PLL_OUTCTRL | PLL_RESET_N | PLL_BYPASSNL;
	regmap_update_bits(hw->regmap, pll->mode_reg, mask, 0);
}

static unsigned long
clk_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_pll *pll = to_clk_pll(hw);
	u32 l, m, n;
	unsigned long rate;
	u64 tmp;

	regmap_read(hw->regmap, pll->l_reg, &l);
	regmap_read(hw->regmap, pll->m_reg, &m);
	regmap_read(hw->regmap, pll->n_reg, &n);

	l &= 0x3ff;
	m &= 0x7ffff;
	n &= 0x7ffff;

	rate = parent_rate * l;
	if (n) {
		tmp = parent_rate;
		tmp *= m;
		do_div(tmp, n);
		rate += tmp;
	}
	return rate;
}

const struct clk_ops clk_pll_ops = {
	.enable = clk_pll_enable,
	.disable = clk_pll_disable,
	.recalc_rate = clk_pll_recalc_rate,
};
EXPORT_SYMBOL_GPL(clk_pll_ops);

static int wait_for_pll(struct clk_pll *pll)
{
	u32 val;
	int count;
	int ret;
	const char *name = __clk_get_name(pll->hw.clk);

	/* Wait for pll to enable. */
	for (count = 200; count > 0; count--) {
		ret = regmap_read(pll->hw.regmap, pll->status_reg, &val);
		if (ret)
			return ret;
		if (val & BIT(pll->status_bit))
			return 0;
		udelay(1);
	}

	WARN(1, "%s didn't enable after voting for it!\n", name);
	return -ETIMEDOUT;
}

static int clk_pll_vote_enable(struct clk_hw *hw)
{
	int ret;
	struct clk_pll *p = to_clk_pll(__clk_get_hw(__clk_get_parent(hw->clk)));

	ret = clk_enable_regmap(hw);
	if (ret)
		return ret;

	return wait_for_pll(p);
}

const struct clk_ops clk_pll_vote_ops = {
	.enable = clk_pll_vote_enable,
	.disable = clk_disable_regmap,
};
EXPORT_SYMBOL_GPL(clk_pll_vote_ops);
