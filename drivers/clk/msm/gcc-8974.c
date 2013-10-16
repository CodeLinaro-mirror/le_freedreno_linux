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

#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk-provider.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

#include <dt-bindings/clk/msm-gcc-8974.h>
#include <dt-bindings/reset/msm-gcc-8974.h>

#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-branch.h"
#include "reset.h"

#define P_XO	0
#define P_GPLL0	1
#define P_GPLL1	1

static const u8 gcc_xo_gpll0_map[] = {
	[P_XO]		= 0,
	[P_GPLL0]	= 1,
};

static const char *gcc_xo_gpll0[] = {
	"xo",
	"gpll0_vote",
};

#define F(f, s, h, m, n) { (f), (s), (2 * (h) - 1), (m), (n) }

static struct clk_pll gpll0 = {
	.l_reg = 0x0004,
	.m_reg = 0x0008,
	.n_reg = 0x000c,
	.config_reg = 0x0014,
	.mode_reg = 0x0000,
	.status_reg = 0x001c,
	.status_bit = 17,
	.hw.init = &(struct clk_init_data){
		.name = "gpll0",
		.parent_names = (const char *[]){ "xo" },
		.num_parents = 1,
		.ops = &clk_pll_ops,
	},
};

static struct clk_hw gpll0_vote = {
	.enable_reg = 0x1480,
	.enable_mask = BIT(0),
	.init = &(struct clk_init_data){
		.name = "gpll0_vote",
		.parent_names = (const char *[]){ "gpll0" },
		.num_parents = 1,
		.ops = &clk_pll_vote_ops,
	},
};

static struct clk_rcg2 config_noc_clk_src = {
	.cmd_rcgr = 0x0150,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.hw.init = &(struct clk_init_data){
		.name = "config_noc_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 periph_noc_clk_src = {
	.cmd_rcgr = 0x0190,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.hw.init = &(struct clk_init_data){
		.name = "periph_noc_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 system_noc_clk_src = {
	.cmd_rcgr = 0x0120,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.hw.init = &(struct clk_init_data){
		.name = "system_noc_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_pll gpll1 = {
	.l_reg = 0x0044,
	.m_reg = 0x0048,
	.n_reg = 0x004c,
	.config_reg = 0x0054,
	.mode_reg = 0x0040,
	.status_reg = 0x005c,
	.status_bit = 17,
	.hw.init = &(struct clk_init_data){
		.name = "gpll1",
		.parent_names = (const char *[]){ "xo" },
		.num_parents = 1,
		.ops = &clk_pll_ops,
	},
};

static struct clk_hw gpll1_vote = {
	.enable_reg = 0x1480,
	.enable_mask = BIT(1),
	.init = &(struct clk_init_data){
		.name = "gpll1_vote",
		.parent_names = (const char *[]){ "gpll1" },
		.num_parents = 1,
		.ops = &clk_pll_vote_ops,
	},
};

static const struct freq_tbl ftbl_gcc_usb30_master_clk[] = {
	F(125000000, P_GPLL0, 1, 5, 24),
	{ }
};

static struct clk_rcg2 usb30_master_clk_src = {
	.cmd_rcgr = 0x03d4,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_usb30_master_clk,
	.hw.init = &(struct clk_init_data){
		.name = "usb30_master_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk[] = {
	F(19200000, P_XO, 1, 0, 0),
	F(37500000, P_GPLL0, 16, 0, 0),
	F(50000000, P_GPLL0, 12, 0, 0),
	{ }
};

static struct clk_rcg2 blsp1_qup1_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0660,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup1_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk[] = {
	F(960000, P_XO, 10, 1, 2),
	F(4800000, P_XO, 4, 0, 0),
	F(9600000, P_XO, 2, 0, 0),
	F(15000000, P_GPLL0, 10, 1, 4),
	F(19200000, P_XO, 1, 0, 0),
	F(25000000, P_GPLL0, 12, 1, 2),
	F(50000000, P_GPLL0, 12, 0, 0),
	{ }
};

static struct clk_rcg2 blsp1_qup1_spi_apps_clk_src = {
	.cmd_rcgr = 0x064c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup1_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup2_i2c_apps_clk_src = {
	.cmd_rcgr = 0x06e0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup2_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup2_spi_apps_clk_src = {
	.cmd_rcgr = 0x06cc,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup2_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup3_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0760,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup3_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup3_spi_apps_clk_src = {
	.cmd_rcgr = 0x074c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup3_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup4_i2c_apps_clk_src = {
	.cmd_rcgr = 0x07e0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup4_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup4_spi_apps_clk_src = {
	.cmd_rcgr = 0x07cc,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup4_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup5_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0860,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup5_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup5_spi_apps_clk_src = {
	.cmd_rcgr = 0x084c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup5_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup6_i2c_apps_clk_src = {
	.cmd_rcgr = 0x08e0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup6_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_qup6_spi_apps_clk_src = {
	.cmd_rcgr = 0x08cc,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_qup6_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_blsp1_2_uart1_6_apps_clk[] = {
	F(3686400, P_GPLL0, 1, 96, 15625),
	F(7372800, P_GPLL0, 1, 192, 15625),
	F(14745600, P_GPLL0, 1, 384, 15625),
	F(16000000, P_GPLL0, 5, 2, 15),
	F(19200000, P_XO, 1, 0, 0),
	F(24000000, P_GPLL0, 5, 1, 5),
	F(32000000, P_GPLL0, 1, 4, 75),
	F(40000000, P_GPLL0, 15, 0, 0),
	F(46400000, P_GPLL0, 1, 29, 375),
	F(48000000, P_GPLL0, 12.5, 0, 0),
	F(51200000, P_GPLL0, 1, 32, 375),
	F(56000000, P_GPLL0, 1, 7, 75),
	F(58982400, P_GPLL0, 1, 1536, 15625),
	F(60000000, P_GPLL0, 10, 0, 0),
	F(63160000, P_GPLL0, 9.5, 0, 0),
	{ }
};

static struct clk_rcg2 blsp1_uart1_apps_clk_src = {
	.cmd_rcgr = 0x068c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_uart1_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_uart2_apps_clk_src = {
	.cmd_rcgr = 0x070c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_uart2_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_uart3_apps_clk_src = {
	.cmd_rcgr = 0x078c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_uart3_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_uart4_apps_clk_src = {
	.cmd_rcgr = 0x080c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_uart4_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_uart5_apps_clk_src = {
	.cmd_rcgr = 0x088c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_uart5_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp1_uart6_apps_clk_src = {
	.cmd_rcgr = 0x090c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp1_uart6_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup1_i2c_apps_clk_src = {
	.cmd_rcgr = 0x09a0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup1_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup1_spi_apps_clk_src = {
	.cmd_rcgr = 0x098c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup1_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup2_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0a20,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup2_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup2_spi_apps_clk_src = {
	.cmd_rcgr = 0x0a0c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup2_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup3_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0aa0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup3_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup3_spi_apps_clk_src = {
	.cmd_rcgr = 0x0a8c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup3_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup4_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0b20,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup4_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup4_spi_apps_clk_src = {
	.cmd_rcgr = 0x0b0c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup4_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup5_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0ba0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup5_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup5_spi_apps_clk_src = {
	.cmd_rcgr = 0x0b8c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup5_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup6_i2c_apps_clk_src = {
	.cmd_rcgr = 0x0c20,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_i2c_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup6_i2c_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_qup6_spi_apps_clk_src = {
	.cmd_rcgr = 0x0c0c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_qup1_6_spi_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_qup6_spi_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_uart1_apps_clk_src = {
	.cmd_rcgr = 0x09cc,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_uart1_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_uart2_apps_clk_src = {
	.cmd_rcgr = 0x0a4c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_uart2_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_uart3_apps_clk_src = {
	.cmd_rcgr = 0x0acc,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_uart3_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_uart4_apps_clk_src = {
	.cmd_rcgr = 0x0b4c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_uart4_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_uart5_apps_clk_src = {
	.cmd_rcgr = 0x0bcc,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_uart5_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 blsp2_uart6_apps_clk_src = {
	.cmd_rcgr = 0x0c4c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_blsp1_2_uart1_6_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "blsp2_uart6_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_ce1_clk[] = {
	F(50000000, P_GPLL0, 12, 0, 0),
	F(75000000, P_GPLL0, 8, 0, 0),
	F(100000000, P_GPLL0, 6, 0, 0),
	F(150000000, P_GPLL0, 4, 0, 0),
	{ }
};

static struct clk_rcg2 ce1_clk_src = {
	.cmd_rcgr = 0x1050,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_ce1_clk,
	.hw.init = &(struct clk_init_data){
		.name = "ce1_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_ce2_clk[] = {
	F(50000000, P_GPLL0, 12, 0, 0),
	F(75000000, P_GPLL0, 8, 0, 0),
	F(100000000, P_GPLL0, 6, 0, 0),
	F(150000000, P_GPLL0, 4, 0, 0),
	{ }
};

static struct clk_rcg2 ce2_clk_src = {
	.cmd_rcgr = 0x1090,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_ce2_clk,
	.hw.init = &(struct clk_init_data){
		.name = "ce2_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_gp_clk[] = {
	F(4800000, P_XO, 4, 0, 0),
	F(6000000, P_GPLL0, 10, 1, 10),
	F(6750000, P_GPLL0, 1, 1, 89),
	F(8000000, P_GPLL0, 15, 1, 5),
	F(9600000, P_XO, 2, 0, 0),
	F(16000000, P_GPLL0, 1, 2, 75),
	F(19200000, P_XO, 1, 0, 0),
	F(24000000, P_GPLL0, 5, 1, 5),
	{ }
};


static struct clk_rcg2 gp1_clk_src = {
	.cmd_rcgr = 0x1904,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_gp_clk,
	.hw.init = &(struct clk_init_data){
		.name = "gp1_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 gp2_clk_src = {
	.cmd_rcgr = 0x1944,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_gp_clk,
	.hw.init = &(struct clk_init_data){
		.name = "gp2_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 gp3_clk_src = {
	.cmd_rcgr = 0x1984,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_gp_clk,
	.hw.init = &(struct clk_init_data){
		.name = "gp3_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_pdm2_clk[] = {
	F(60000000, P_GPLL0, 10, 0, 0),
	{ }
};

static struct clk_rcg2 pdm2_clk_src = {
	.cmd_rcgr = 0x0cd0,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_pdm2_clk,
	.hw.init = &(struct clk_init_data){
		.name = "pdm2_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_sdcc1_4_apps_clk[] = {
	F(144000, P_XO, 16, 3, 25),
	F(400000, P_XO, 12, 1, 4),
	F(20000000, P_GPLL0, 15, 1, 2),
	F(25000000, P_GPLL0, 12, 1, 2),
	F(50000000, P_GPLL0, 12, 0, 0),
	F(100000000, P_GPLL0, 6, 0, 0),
	F(200000000, P_GPLL0, 3, 0, 0),
	{ }
};

static struct clk_rcg2 sdcc1_apps_clk_src = {
	.cmd_rcgr = 0x04d0,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "sdcc1_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 sdcc2_apps_clk_src = {
	.cmd_rcgr = 0x0510,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "sdcc2_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 sdcc3_apps_clk_src = {
	.cmd_rcgr = 0x0550,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "sdcc3_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 sdcc4_apps_clk_src = {
	.cmd_rcgr = 0x0590,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_sdcc1_4_apps_clk,
	.hw.init = &(struct clk_init_data){
		.name = "sdcc4_apps_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_tsif_ref_clk[] = {
	F(105000, P_XO, 2, 1, 91),
	{ }
};

static struct clk_rcg2 tsif_ref_clk_src = {
	.cmd_rcgr = 0x0d90,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_tsif_ref_clk,
	.hw.init = &(struct clk_init_data){
		.name = "tsif_ref_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_usb30_mock_utmi_clk[] = {
	F(60000000, P_GPLL0, 10, 0, 0),
	{ }
};

static struct clk_rcg2 usb30_mock_utmi_clk_src = {
	.cmd_rcgr = 0x03e8,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_usb30_mock_utmi_clk,
	.hw.init = &(struct clk_init_data){
		.name = "usb30_mock_utmi_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_usb_hs_system_clk[] = {
	F(60000000, P_GPLL0, 10, 0, 0),
	F(75000000, P_GPLL0, 8, 0, 0),
	{ }
};

static struct clk_rcg2 usb_hs_system_clk_src = {
	.cmd_rcgr = 0x0490,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_usb_hs_system_clk,
	.hw.init = &(struct clk_init_data){
		.name = "usb_hs_system_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_usb_hsic_clk[] = {
	F(480000000, P_GPLL1, 1, 0, 0),
	{ }
};

static u8 usb_hsic_clk_src_map[] = {
	[P_XO]		= 0,
	[P_GPLL1]	= 4,
};

static struct clk_rcg2 usb_hsic_clk_src = {
	.cmd_rcgr = 0x0440,
	.hid_width = 5,
	.parent_map = usb_hsic_clk_src_map,
	.freq_tbl = ftbl_gcc_usb_hsic_clk,
	.hw.init = &(struct clk_init_data){
		.name = "usb_hsic_clk_src",
		.parent_names = (const char *[]){
			"xo",
			"gpll1_vote",
		},
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_usb_hsic_io_cal_clk[] = {
	F(9600000, P_XO, 2, 0, 0),
	{ }
};

static struct clk_rcg2 usb_hsic_io_cal_clk_src = {
	.cmd_rcgr = 0x0458,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_usb_hsic_io_cal_clk,
	.hw.init = &(struct clk_init_data){
		.name = "usb_hsic_io_cal_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 1,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_gcc_usb_hsic_system_clk[] = {
	F(60000000, P_GPLL0, 10, 0, 0),
	F(75000000, P_GPLL0, 8, 0, 0),
	{ }
};

static struct clk_rcg2 usb_hsic_system_clk_src = {
	.cmd_rcgr = 0x041c,
	.hid_width = 5,
	.parent_map = gcc_xo_gpll0_map,
	.freq_tbl = ftbl_gcc_usb_hsic_system_clk,
	.hw.init = &(struct clk_init_data){
		.name = "usb_hsic_system_clk_src",
		.parent_names = gcc_xo_gpll0,
		.num_parents = 2,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_branch gcc_bam_dma_ahb_clk = {
	.halt_reg = 0x0d44,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(12),
		.init = &(struct clk_init_data){
			.name = "gcc_bam_dma_ahb_clk",
			.parent_names = (const char *[]){
				"periph_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_ahb_clk = {
	.halt_reg = 0x05c4,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(17),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_ahb_clk",
			.parent_names = (const char *[]){
				"periph_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup1_i2c_apps_clk = {
	.halt_reg = 0x0648,
	.hw = {
		.enable_reg = 0x0648,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup1_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup1_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup1_spi_apps_clk = {
	.halt_reg = 0x0644,
	.hw = {
		.enable_reg = 0x0644,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup1_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup1_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup2_i2c_apps_clk = {
	.halt_reg = 0x06c8,
	.hw = {
		.enable_reg = 0x06c8,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup2_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup2_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup2_spi_apps_clk = {
	.halt_reg = 0x06c4,
	.hw = {
		.enable_reg = 0x06c4,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup2_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup2_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup3_i2c_apps_clk = {
	.halt_reg = 0x0748,
	.hw = {
		.enable_reg = 0x0748,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup3_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup3_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup3_spi_apps_clk = {
	.halt_reg = 0x0744,
	.hw = {
		.enable_reg = 0x0744,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup3_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup3_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup4_i2c_apps_clk = {
	.halt_reg = 0x07c8,
	.hw = {
		.enable_reg = 0x07c8,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup4_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup4_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup4_spi_apps_clk = {
	.halt_reg = 0x07c4,
	.hw = {
		.enable_reg = 0x07c4,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup4_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup4_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup5_i2c_apps_clk = {
	.halt_reg = 0x0848,
	.hw = {
		.enable_reg = 0x0848,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup5_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup5_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup5_spi_apps_clk = {
	.halt_reg = 0x0844,
	.hw = {
		.enable_reg = 0x0844,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup5_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup5_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup6_i2c_apps_clk = {
	.halt_reg = 0x08c8,
	.hw = {
		.enable_reg = 0x08c8,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup6_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup6_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_qup6_spi_apps_clk = {
	.halt_reg = 0x08c4,
	.hw = {
		.enable_reg = 0x08c4,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_qup6_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_qup6_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart1_apps_clk = {
	.halt_reg = 0x0684,
	.hw = {
		.enable_reg = 0x0684,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart1_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_uart1_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart2_apps_clk = {
	.halt_reg = 0x0704,
	.hw = {
		.enable_reg = 0x0704,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart2_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_uart2_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart3_apps_clk = {
	.halt_reg = 0x0784,
	.hw = {
		.enable_reg = 0x0784,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart3_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_uart3_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart4_apps_clk = {
	.halt_reg = 0x0804,
	.hw = {
		.enable_reg = 0x0804,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart4_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_uart4_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart5_apps_clk = {
	.halt_reg = 0x0884,
	.hw = {
		.enable_reg = 0x0884,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart5_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_uart5_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp1_uart6_apps_clk = {
	.halt_reg = 0x0904,
	.hw = {
		.enable_reg = 0x0904,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp1_uart6_apps_clk",
			.parent_names = (const char *[]){
				"blsp1_uart6_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_ahb_clk = {
	.halt_reg = 0x05c4,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(15),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_ahb_clk",
			.parent_names = (const char *[]){
				"periph_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup1_i2c_apps_clk = {
	.halt_reg = 0x0988,
	.hw = {
		.enable_reg = 0x0988,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup1_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup1_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup1_spi_apps_clk = {
	.halt_reg = 0x0984,
	.hw = {
		.enable_reg = 0x0984,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup1_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup1_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup2_i2c_apps_clk = {
	.halt_reg = 0x0a08,
	.hw = {
		.enable_reg = 0x0a08,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup2_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup2_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup2_spi_apps_clk = {
	.halt_reg = 0x0a04,
	.hw = {
		.enable_reg = 0x0a04,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup2_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup2_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup3_i2c_apps_clk = {
	.halt_reg = 0x0a88,
	.hw = {
		.enable_reg = 0x0a88,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup3_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup3_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup3_spi_apps_clk = {
	.halt_reg = 0x0a84,
	.hw = {
		.enable_reg = 0x0a84,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup3_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup3_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup4_i2c_apps_clk = {
	.halt_reg = 0x0b08,
	.hw = {
		.enable_reg = 0x0b08,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup4_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup4_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup4_spi_apps_clk = {
	.halt_reg = 0x0b04,
	.hw = {
		.enable_reg = 0x0b04,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup4_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup4_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup5_i2c_apps_clk = {
	.halt_reg = 0x0b88,
	.hw = {
		.enable_reg = 0x0b88,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup5_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup5_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup5_spi_apps_clk = {
	.halt_reg = 0x0b84,
	.hw = {
		.enable_reg = 0x0b84,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup5_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup5_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup6_i2c_apps_clk = {
	.halt_reg = 0x0c08,
	.hw = {
		.enable_reg = 0x0c08,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup6_i2c_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup6_i2c_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_qup6_spi_apps_clk = {
	.halt_reg = 0x0c04,
	.hw = {
		.enable_reg = 0x0c04,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_qup6_spi_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_qup6_spi_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_uart1_apps_clk = {
	.halt_reg = 0x09c4,
	.hw = {
		.enable_reg = 0x09c4,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_uart1_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_uart1_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_uart2_apps_clk = {
	.halt_reg = 0x0a44,
	.hw = {
		.enable_reg = 0x0a44,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_uart2_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_uart2_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_uart3_apps_clk = {
	.halt_reg = 0x0ac4,
	.hw = {
		.enable_reg = 0x0ac4,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_uart3_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_uart3_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_uart4_apps_clk = {
	.halt_reg = 0x0b44,
	.hw = {
		.enable_reg = 0x0b44,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_uart4_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_uart4_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_uart5_apps_clk = {
	.halt_reg = 0x0bc4,
	.hw = {
		.enable_reg = 0x0bc4,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_uart5_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_uart5_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_blsp2_uart6_apps_clk = {
	.halt_reg = 0x0c44,
	.hw = {
		.enable_reg = 0x0c44,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_blsp2_uart6_apps_clk",
			.parent_names = (const char *[]){
				"blsp2_uart6_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_boot_rom_ahb_clk = {
	.halt_reg = 0x0e04,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(10),
		.init = &(struct clk_init_data){
			.name = "gcc_boot_rom_ahb_clk",
			.parent_names = (const char *[]){
				"config_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_ahb_clk = {
	.halt_reg = 0x104c,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(3),
		.init = &(struct clk_init_data){
			.name = "gcc_ce1_ahb_clk",
			.parent_names = (const char *[]){
				"config_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_axi_clk = {
	.halt_reg = 0x1048,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(4),
		.init = &(struct clk_init_data){
			.name = "gcc_ce1_axi_clk",
			.parent_names = (const char *[]){
				"system_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce1_clk = {
	.halt_reg = 0x1050,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(5),
		.init = &(struct clk_init_data){
			.name = "gcc_ce1_clk",
			.parent_names = (const char *[]){
				"ce1_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce2_ahb_clk = {
	.halt_reg = 0x108c,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_ce2_ahb_clk",
			.parent_names = (const char *[]){
				"config_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce2_axi_clk = {
	.halt_reg = 0x1088,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(1),
		.init = &(struct clk_init_data){
			.name = "gcc_ce2_axi_clk",
			.parent_names = (const char *[]){
				"system_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_ce2_clk = {
	.halt_reg = 0x1090,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(2),
		.init = &(struct clk_init_data){
			.name = "gcc_ce2_clk",
			.parent_names = (const char *[]){
				"ce2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp1_clk = {
	.halt_reg = 0x1900,
	.hw = {
		.enable_reg = 0x1900,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_gp1_clk",
			.parent_names = (const char *[]){
				"gp1_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp2_clk = {
	.halt_reg = 0x1940,
	.hw = {
		.enable_reg = 0x1940,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_gp2_clk",
			.parent_names = (const char *[]){
				"gp2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_gp3_clk = {
	.halt_reg = 0x1980,
	.hw = {
		.enable_reg = 0x1980,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_gp3_clk",
			.parent_names = (const char *[]){
				"gp3_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_pdm2_clk = {
	.halt_reg = 0x0ccc,
	.hw = {
		.enable_reg = 0x0ccc,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_pdm2_clk",
			.parent_names = (const char *[]){
				"pdm2_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_prng_ahb_clk = {
	.halt_reg = 0x0d04,
	.halt_check = BRANCH_HALT_VOTED,
	.hw = {
		.enable_reg = 0x1484,
		.enable_mask = BIT(13),
		.init = &(struct clk_init_data){
			.name = "gcc_prng_ahb_clk",
			.parent_names = (const char *[]){
				"periph_noc_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc1_apps_clk = {
	.halt_reg = 0x04c4,
	.hw = {
		.enable_reg = 0x04c4,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_sdcc1_apps_clk",
			.parent_names = (const char *[]){
				"sdcc1_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc2_apps_clk = {
	.halt_reg = 0x0504,
	.hw = {
		.enable_reg = 0x0504,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_sdcc2_apps_clk",
			.parent_names = (const char *[]){
				"sdcc2_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc3_apps_clk = {
	.halt_reg = 0x0544,
	.hw = {
		.enable_reg = 0x0544,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_sdcc3_apps_clk",
			.parent_names = (const char *[]){
				"sdcc3_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sdcc4_apps_clk = {
	.halt_reg = 0x0584,
	.hw = {
		.enable_reg = 0x0584,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_sdcc4_apps_clk",
			.parent_names = (const char *[]){
				"sdcc4_apps_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_sys_noc_usb3_axi_clk = {
	.halt_reg = 0x0108,
	.hw = {
		.enable_reg = 0x0108,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_sys_noc_usb3_axi_clk",
			.parent_names = (const char *[]){
				"usb30_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_tsif_ref_clk = {
	.halt_reg = 0x0d88,
	.hw = {
		.enable_reg = 0x0d88,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_tsif_ref_clk",
			.parent_names = (const char *[]){
				"tsif_ref_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb2a_phy_sleep_clk = {
	.halt_reg = 0x04ac,
	.hw = {
		.enable_reg = 0x04ac,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb2a_phy_sleep_clk",
			.parent_names = (const char *[]){
				"gcc_sleep_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb2b_phy_sleep_clk = {
	.halt_reg = 0x04b4,
	.hw = {
		.enable_reg = 0x04b4,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb2b_phy_sleep_clk",
			.parent_names = (const char *[]){
				"gcc_sleep_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_master_clk = {
	.halt_reg = 0x03c8,
	.hw = {
		.enable_reg = 0x03c8,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb30_master_clk",
			.parent_names = (const char *[]){
				"usb30_master_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_mock_utmi_clk = {
	.halt_reg = 0x03d0,
	.hw = {
		.enable_reg = 0x03d0,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb30_mock_utmi_clk",
			.parent_names = (const char *[]){
				"usb30_mock_utmi_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb30_sleep_clk = {
	.halt_reg = 0x03cc,
	.hw = {
		.enable_reg = 0x03cc,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb30_sleep_clk",
			.parent_names = (const char *[]){
				"gcc_sleep_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_hs_system_clk = {
	.halt_reg = 0x0484,
	.hw = {
		.enable_reg = 0x0484,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb_hs_system_clk",
			.parent_names = (const char *[]){
				"usb_hs_system_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_hsic_clk = {
	.halt_reg = 0x0410,
	.hw = {
		.enable_reg = 0x0410,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb_hsic_clk",
			.parent_names = (const char *[]){
				"usb_hsic_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_hsic_io_cal_clk = {
	.halt_reg = 0x0414,
	.hw = {
		.enable_reg = 0x0414,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb_hsic_io_cal_clk",
			.parent_names = (const char *[]){
				"usb_hsic_io_cal_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_hsic_io_cal_sleep_clk = {
	.halt_reg = 0x0418,
	.hw = {
		.enable_reg = 0x0418,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb_hsic_io_cal_sleep_clk",
			.parent_names = (const char *[]){
				"gcc_sleep_clk_src",
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch gcc_usb_hsic_system_clk = {
	.halt_reg = 0x040c,
	.hw = {
		.enable_reg = 0x040c,
		.enable_mask = BIT(0),
		.init = &(struct clk_init_data){
			.name = "gcc_usb_hsic_system_clk",
			.parent_names = (const char *[]){
				"usb_hsic_system_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_hw *map[] = {
	[GPLL0] = &gpll0.hw,
	[GPLL0_VOTE] = &gpll0_vote,
	[CONFIG_NOC_CLK_SRC] = &config_noc_clk_src.hw,
	[PERIPH_NOC_CLK_SRC] = &periph_noc_clk_src.hw,
	[SYSTEM_NOC_CLK_SRC] = &system_noc_clk_src.hw,
	[GPLL1] = &gpll1.hw,
	[GPLL1_VOTE] = &gpll1_vote,
	[USB30_MASTER_CLK_SRC] = &usb30_master_clk_src.hw,
	[BLSP1_QUP1_I2C_APPS_CLK_SRC] = &blsp1_qup1_i2c_apps_clk_src.hw,
	[BLSP1_QUP1_SPI_APPS_CLK_SRC] = &blsp1_qup1_spi_apps_clk_src.hw,
	[BLSP1_QUP2_I2C_APPS_CLK_SRC] = &blsp1_qup2_i2c_apps_clk_src.hw,
	[BLSP1_QUP2_SPI_APPS_CLK_SRC] = &blsp1_qup2_spi_apps_clk_src.hw,
	[BLSP1_QUP3_I2C_APPS_CLK_SRC] = &blsp1_qup3_i2c_apps_clk_src.hw,
	[BLSP1_QUP3_SPI_APPS_CLK_SRC] = &blsp1_qup3_spi_apps_clk_src.hw,
	[BLSP1_QUP4_I2C_APPS_CLK_SRC] = &blsp1_qup4_i2c_apps_clk_src.hw,
	[BLSP1_QUP4_SPI_APPS_CLK_SRC] = &blsp1_qup4_spi_apps_clk_src.hw,
	[BLSP1_QUP5_I2C_APPS_CLK_SRC] = &blsp1_qup5_i2c_apps_clk_src.hw,
	[BLSP1_QUP5_SPI_APPS_CLK_SRC] = &blsp1_qup5_spi_apps_clk_src.hw,
	[BLSP1_QUP6_I2C_APPS_CLK_SRC] = &blsp1_qup6_i2c_apps_clk_src.hw,
	[BLSP1_QUP6_SPI_APPS_CLK_SRC] = &blsp1_qup6_spi_apps_clk_src.hw,
	[BLSP1_UART1_APPS_CLK_SRC] = &blsp1_uart1_apps_clk_src.hw,
	[BLSP1_UART2_APPS_CLK_SRC] = &blsp1_uart2_apps_clk_src.hw,
	[BLSP1_UART3_APPS_CLK_SRC] = &blsp1_uart3_apps_clk_src.hw,
	[BLSP1_UART4_APPS_CLK_SRC] = &blsp1_uart4_apps_clk_src.hw,
	[BLSP1_UART5_APPS_CLK_SRC] = &blsp1_uart5_apps_clk_src.hw,
	[BLSP1_UART6_APPS_CLK_SRC] = &blsp1_uart6_apps_clk_src.hw,
	[BLSP2_QUP1_I2C_APPS_CLK_SRC] = &blsp2_qup1_i2c_apps_clk_src.hw,
	[BLSP2_QUP1_SPI_APPS_CLK_SRC] = &blsp2_qup1_spi_apps_clk_src.hw,
	[BLSP2_QUP2_I2C_APPS_CLK_SRC] = &blsp2_qup2_i2c_apps_clk_src.hw,
	[BLSP2_QUP2_SPI_APPS_CLK_SRC] = &blsp2_qup2_spi_apps_clk_src.hw,
	[BLSP2_QUP3_I2C_APPS_CLK_SRC] = &blsp2_qup3_i2c_apps_clk_src.hw,
	[BLSP2_QUP3_SPI_APPS_CLK_SRC] = &blsp2_qup3_spi_apps_clk_src.hw,
	[BLSP2_QUP4_I2C_APPS_CLK_SRC] = &blsp2_qup4_i2c_apps_clk_src.hw,
	[BLSP2_QUP4_SPI_APPS_CLK_SRC] = &blsp2_qup4_spi_apps_clk_src.hw,
	[BLSP2_QUP5_I2C_APPS_CLK_SRC] = &blsp2_qup5_i2c_apps_clk_src.hw,
	[BLSP2_QUP5_SPI_APPS_CLK_SRC] = &blsp2_qup5_spi_apps_clk_src.hw,
	[BLSP2_QUP6_I2C_APPS_CLK_SRC] = &blsp2_qup6_i2c_apps_clk_src.hw,
	[BLSP2_QUP6_SPI_APPS_CLK_SRC] = &blsp2_qup6_spi_apps_clk_src.hw,
	[BLSP2_UART1_APPS_CLK_SRC] = &blsp2_uart1_apps_clk_src.hw,
	[BLSP2_UART2_APPS_CLK_SRC] = &blsp2_uart2_apps_clk_src.hw,
	[BLSP2_UART3_APPS_CLK_SRC] = &blsp2_uart3_apps_clk_src.hw,
	[BLSP2_UART4_APPS_CLK_SRC] = &blsp2_uart4_apps_clk_src.hw,
	[BLSP2_UART5_APPS_CLK_SRC] = &blsp2_uart5_apps_clk_src.hw,
	[BLSP2_UART6_APPS_CLK_SRC] = &blsp2_uart6_apps_clk_src.hw,
	[CE1_CLK_SRC] = &ce1_clk_src.hw,
	[CE2_CLK_SRC] = &ce2_clk_src.hw,
	[GP1_CLK_SRC] = &gp1_clk_src.hw,
	[GP2_CLK_SRC] = &gp2_clk_src.hw,
	[GP3_CLK_SRC] = &gp3_clk_src.hw,
	[PDM2_CLK_SRC] = &pdm2_clk_src.hw,
	[SDCC1_APPS_CLK_SRC] = &sdcc1_apps_clk_src.hw,
	[SDCC2_APPS_CLK_SRC] = &sdcc2_apps_clk_src.hw,
	[SDCC3_APPS_CLK_SRC] = &sdcc3_apps_clk_src.hw,
	[SDCC4_APPS_CLK_SRC] = &sdcc4_apps_clk_src.hw,
	[TSIF_REF_CLK_SRC] = &tsif_ref_clk_src.hw,
	[USB30_MOCK_UTMI_CLK_SRC] = &usb30_mock_utmi_clk_src.hw,
	[USB_HS_SYSTEM_CLK_SRC] = &usb_hs_system_clk_src.hw,
	[USB_HSIC_CLK_SRC] = &usb_hsic_clk_src.hw,
	[USB_HSIC_IO_CAL_CLK_SRC] = &usb_hsic_io_cal_clk_src.hw,
	[USB_HSIC_SYSTEM_CLK_SRC] = &usb_hsic_system_clk_src.hw,
	[GCC_BAM_DMA_AHB_CLK] = &gcc_bam_dma_ahb_clk.hw,
	[GCC_BLSP1_AHB_CLK] = &gcc_blsp1_ahb_clk.hw,
	[GCC_BLSP1_QUP1_I2C_APPS_CLK] = &gcc_blsp1_qup1_i2c_apps_clk.hw,
	[GCC_BLSP1_QUP1_SPI_APPS_CLK] = &gcc_blsp1_qup1_spi_apps_clk.hw,
	[GCC_BLSP1_QUP2_I2C_APPS_CLK] = &gcc_blsp1_qup2_i2c_apps_clk.hw,
	[GCC_BLSP1_QUP2_SPI_APPS_CLK] = &gcc_blsp1_qup2_spi_apps_clk.hw,
	[GCC_BLSP1_QUP3_I2C_APPS_CLK] = &gcc_blsp1_qup3_i2c_apps_clk.hw,
	[GCC_BLSP1_QUP3_SPI_APPS_CLK] = &gcc_blsp1_qup3_spi_apps_clk.hw,
	[GCC_BLSP1_QUP4_I2C_APPS_CLK] = &gcc_blsp1_qup4_i2c_apps_clk.hw,
	[GCC_BLSP1_QUP4_SPI_APPS_CLK] = &gcc_blsp1_qup4_spi_apps_clk.hw,
	[GCC_BLSP1_QUP5_I2C_APPS_CLK] = &gcc_blsp1_qup5_i2c_apps_clk.hw,
	[GCC_BLSP1_QUP5_SPI_APPS_CLK] = &gcc_blsp1_qup5_spi_apps_clk.hw,
	[GCC_BLSP1_QUP6_I2C_APPS_CLK] = &gcc_blsp1_qup6_i2c_apps_clk.hw,
	[GCC_BLSP1_QUP6_SPI_APPS_CLK] = &gcc_blsp1_qup6_spi_apps_clk.hw,
	[GCC_BLSP1_UART1_APPS_CLK] = &gcc_blsp1_uart1_apps_clk.hw,
	[GCC_BLSP1_UART2_APPS_CLK] = &gcc_blsp1_uart2_apps_clk.hw,
	[GCC_BLSP1_UART3_APPS_CLK] = &gcc_blsp1_uart3_apps_clk.hw,
	[GCC_BLSP1_UART4_APPS_CLK] = &gcc_blsp1_uart4_apps_clk.hw,
	[GCC_BLSP1_UART5_APPS_CLK] = &gcc_blsp1_uart5_apps_clk.hw,
	[GCC_BLSP1_UART6_APPS_CLK] = &gcc_blsp1_uart6_apps_clk.hw,
	[GCC_BLSP2_AHB_CLK] = &gcc_blsp2_ahb_clk.hw,
	[GCC_BLSP2_QUP1_I2C_APPS_CLK] = &gcc_blsp2_qup1_i2c_apps_clk.hw,
	[GCC_BLSP2_QUP1_SPI_APPS_CLK] = &gcc_blsp2_qup1_spi_apps_clk.hw,
	[GCC_BLSP2_QUP2_I2C_APPS_CLK] = &gcc_blsp2_qup2_i2c_apps_clk.hw,
	[GCC_BLSP2_QUP2_SPI_APPS_CLK] = &gcc_blsp2_qup2_spi_apps_clk.hw,
	[GCC_BLSP2_QUP3_I2C_APPS_CLK] = &gcc_blsp2_qup3_i2c_apps_clk.hw,
	[GCC_BLSP2_QUP3_SPI_APPS_CLK] = &gcc_blsp2_qup3_spi_apps_clk.hw,
	[GCC_BLSP2_QUP4_I2C_APPS_CLK] = &gcc_blsp2_qup4_i2c_apps_clk.hw,
	[GCC_BLSP2_QUP4_SPI_APPS_CLK] = &gcc_blsp2_qup4_spi_apps_clk.hw,
	[GCC_BLSP2_QUP5_I2C_APPS_CLK] = &gcc_blsp2_qup5_i2c_apps_clk.hw,
	[GCC_BLSP2_QUP5_SPI_APPS_CLK] = &gcc_blsp2_qup5_spi_apps_clk.hw,
	[GCC_BLSP2_QUP6_I2C_APPS_CLK] = &gcc_blsp2_qup6_i2c_apps_clk.hw,
	[GCC_BLSP2_QUP6_SPI_APPS_CLK] = &gcc_blsp2_qup6_spi_apps_clk.hw,
	[GCC_BLSP2_UART1_APPS_CLK] = &gcc_blsp2_uart1_apps_clk.hw,
	[GCC_BLSP2_UART2_APPS_CLK] = &gcc_blsp2_uart2_apps_clk.hw,
	[GCC_BLSP2_UART3_APPS_CLK] = &gcc_blsp2_uart3_apps_clk.hw,
	[GCC_BLSP2_UART4_APPS_CLK] = &gcc_blsp2_uart4_apps_clk.hw,
	[GCC_BLSP2_UART5_APPS_CLK] = &gcc_blsp2_uart5_apps_clk.hw,
	[GCC_BLSP2_UART6_APPS_CLK] = &gcc_blsp2_uart6_apps_clk.hw,
	[GCC_BOOT_ROM_AHB_CLK] = &gcc_boot_rom_ahb_clk.hw,
	[GCC_CE1_AHB_CLK] = &gcc_ce1_ahb_clk.hw,
	[GCC_CE1_AXI_CLK] = &gcc_ce1_axi_clk.hw,
	[GCC_CE1_CLK] = &gcc_ce1_clk.hw,
	[GCC_CE2_AHB_CLK] = &gcc_ce2_ahb_clk.hw,
	[GCC_CE2_AXI_CLK] = &gcc_ce2_axi_clk.hw,
	[GCC_CE2_CLK] = &gcc_ce2_clk.hw,
	[GCC_GP1_CLK] = &gcc_gp1_clk.hw,
	[GCC_GP2_CLK] = &gcc_gp2_clk.hw,
	[GCC_GP3_CLK] = &gcc_gp3_clk.hw,
	[GCC_PDM2_CLK] = &gcc_pdm2_clk.hw,
	[GCC_PRNG_AHB_CLK] = &gcc_prng_ahb_clk.hw,
	[GCC_SDCC1_APPS_CLK] = &gcc_sdcc1_apps_clk.hw,
	[GCC_SDCC2_APPS_CLK] = &gcc_sdcc2_apps_clk.hw,
	[GCC_SDCC3_APPS_CLK] = &gcc_sdcc3_apps_clk.hw,
	[GCC_SDCC4_APPS_CLK] = &gcc_sdcc4_apps_clk.hw,
	[GCC_SYS_NOC_USB3_AXI_CLK] = &gcc_sys_noc_usb3_axi_clk.hw,
	[GCC_TSIF_REF_CLK] = &gcc_tsif_ref_clk.hw,
	[GCC_USB2A_PHY_SLEEP_CLK] = &gcc_usb2a_phy_sleep_clk.hw,
	[GCC_USB2B_PHY_SLEEP_CLK] = &gcc_usb2b_phy_sleep_clk.hw,
	[GCC_USB30_MASTER_CLK] = &gcc_usb30_master_clk.hw,
	[GCC_USB30_MOCK_UTMI_CLK] = &gcc_usb30_mock_utmi_clk.hw,
	[GCC_USB30_SLEEP_CLK] = &gcc_usb30_sleep_clk.hw,
	[GCC_USB_HS_SYSTEM_CLK] = &gcc_usb_hs_system_clk.hw,
	[GCC_USB_HSIC_CLK] = &gcc_usb_hsic_clk.hw,
	[GCC_USB_HSIC_IO_CAL_CLK] = &gcc_usb_hsic_io_cal_clk.hw,
	[GCC_USB_HSIC_IO_CAL_SLEEP_CLK] = &gcc_usb_hsic_io_cal_sleep_clk.hw,
	[GCC_USB_HSIC_SYSTEM_CLK] = &gcc_usb_hsic_system_clk.hw,
};

static const struct msm_reset_map msm_gcc_8974_resets[] = {
	[GCC_SYSTEM_NOC_BCR] = { 0x0100 },
	[GCC_CONFIG_NOC_BCR] = { 0x0140 },
	[GCC_PERIPH_NOC_BCR] = { 0x0180 },
	[GCC_IMEM_BCR] = { 0x0200 },
	[GCC_MMSS_BCR] = { 0x0240 },
	[GCC_QDSS_BCR] = { 0x0300 },
	[GCC_USB_30_BCR] = { 0x03c0 },
	[GCC_USB3_PHY_BCR] = { 0x03fc },
	[GCC_USB_HS_HSIC_BCR] = { 0x0400 },
	[GCC_USB_HS_BCR] = { 0x0480 },
	[GCC_USB2A_PHY_BCR] = { 0x04a8 },
	[GCC_USB2B_PHY_BCR] = { 0x04b0 },
	[GCC_SDCC1_BCR] = { 0x04c0 },
	[GCC_SDCC2_BCR] = { 0x0500 },
	[GCC_SDCC3_BCR] = { 0x0540 },
	[GCC_SDCC4_BCR] = { 0x0580 },
	[GCC_BLSP1_BCR] = { 0x05c0 },
	[GCC_BLSP1_QUP1_BCR] = { 0x0640 },
	[GCC_BLSP1_UART1_BCR] = { 0x0680 },
	[GCC_BLSP1_QUP2_BCR] = { 0x06c0 },
	[GCC_BLSP1_UART2_BCR] = { 0x0700 },
	[GCC_BLSP1_QUP3_BCR] = { 0x0740 },
	[GCC_BLSP1_UART3_BCR] = { 0x0780 },
	[GCC_BLSP1_QUP4_BCR] = { 0x07c0 },
	[GCC_BLSP1_UART4_BCR] = { 0x0800 },
	[GCC_BLSP1_QUP5_BCR] = { 0x0840 },
	[GCC_BLSP1_UART5_BCR] = { 0x0880 },
	[GCC_BLSP1_QUP6_BCR] = { 0x08c0 },
	[GCC_BLSP1_UART6_BCR] = { 0x0900 },
	[GCC_BLSP2_BCR] = { 0x0940 },
	[GCC_BLSP2_QUP1_BCR] = { 0x0980 },
	[GCC_BLSP2_UART1_BCR] = { 0x09c0 },
	[GCC_BLSP2_QUP2_BCR] = { 0x0a00 },
	[GCC_BLSP2_UART2_BCR] = { 0x0a40 },
	[GCC_BLSP2_QUP3_BCR] = { 0x0a80 },
	[GCC_BLSP2_UART3_BCR] = { 0x0ac0 },
	[GCC_BLSP2_QUP4_BCR] = { 0x0b00 },
	[GCC_BLSP2_UART4_BCR] = { 0x0b40 },
	[GCC_BLSP2_QUP5_BCR] = { 0x0b80 },
	[GCC_BLSP2_UART5_BCR] = { 0x0bc0 },
	[GCC_BLSP2_QUP6_BCR] = { 0x0c00 },
	[GCC_BLSP2_UART6_BCR] = { 0x0c40 },
	[GCC_PDM_BCR] = { 0x0cc0 },
	[GCC_BAM_DMA_BCR] = { 0x0d40 },
	[GCC_TSIF_BCR] = { 0x0d80 },
	[GCC_TCSR_BCR] = { 0x0dc0 },
	[GCC_BOOT_ROM_BCR] = { 0x0e00 },
	[GCC_MSG_RAM_BCR] = { 0x0e40 },
	[GCC_TLMM_BCR] = { 0x0e80 },
	[GCC_MPM_BCR] = { 0x0ec0 },
	[GCC_SEC_CTRL_BCR] = { 0x0f40 },
	[GCC_SPMI_BCR] = { 0x0fc0 },
	[GCC_SPDM_BCR] = { 0x1000 },
	[GCC_CE1_BCR] = { 0x1040 },
	[GCC_CE2_BCR] = { 0x1080 },
	[GCC_BIMC_BCR] = { 0x1100 },
	[GCC_MPM_NON_AHB_RESET] = { 0x0ec4, 2 },
	[GCC_MPM_AHB_RESET] = {	0x0ec4, 1 },
	[GCC_SNOC_BUS_TIMEOUT0_BCR] = { 0x1240 },
	[GCC_SNOC_BUS_TIMEOUT2_BCR] = { 0x1248 },
	[GCC_PNOC_BUS_TIMEOUT0_BCR] = { 0x1280 },
	[GCC_PNOC_BUS_TIMEOUT1_BCR] = { 0x1288 },
	[GCC_PNOC_BUS_TIMEOUT2_BCR] = { 0x1290 },
	[GCC_PNOC_BUS_TIMEOUT3_BCR] = { 0x1298 },
	[GCC_PNOC_BUS_TIMEOUT4_BCR] = { 0x12a0 },
	[GCC_CNOC_BUS_TIMEOUT0_BCR] = { 0x12c0 },
	[GCC_CNOC_BUS_TIMEOUT1_BCR] = { 0x12c8 },
	[GCC_CNOC_BUS_TIMEOUT2_BCR] = { 0x12d0 },
	[GCC_CNOC_BUS_TIMEOUT3_BCR] = { 0x12d8 },
	[GCC_CNOC_BUS_TIMEOUT4_BCR] = { 0x12e0 },
	[GCC_CNOC_BUS_TIMEOUT5_BCR] = { 0x12e8 },
	[GCC_CNOC_BUS_TIMEOUT6_BCR] = { 0x12f0 },
	[GCC_DEHR_BCR] = { 0x1300 },
	[GCC_RBCPR_BCR] = { 0x1380 },
	[GCC_MSS_RESTART] = { 0x1680 },
	[GCC_LPASS_RESTART] = { 0x16c0 },
	[GCC_WCSS_RESTART] = { 0x1700 },
	[GCC_VENUS_RESTART] = { 0x1740 },
};

static const struct regmap_config msm_gcc_8974_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x1fc0,
	.fast_io	= true,
};

static const struct of_device_id msm_gcc_8974_match_table[] = {
	{ .compatible = "qcom,gcc-8974" },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_gcc_8974_match_table);

static int msm_gcc_8974_probe(struct platform_device *pdev)
{
	void __iomem *base;
	struct resource *res;
	int i, ret;
	struct device *dev = &pdev->dev;
	struct clk *clk;
	struct clk_onecell_data *data;
	struct clk **clks;
	struct regmap *regmap;
	size_t num_clks;
	struct msm_reset_controller *reset;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	regmap = devm_regmap_init_mmio(dev, base, &msm_gcc_8974_regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	num_clks = ARRAY_SIZE(map);
	data = devm_kzalloc(dev, sizeof(*data) + sizeof(*clks) * num_clks,
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	clks = (struct clk **)(data + 1);
	data->clks = clks;
	data->clk_num = num_clks;

	reset = devm_kzalloc(dev, sizeof(*reset), GFP_KERNEL);
	if (!reset)
		return -ENOMEM;
	platform_set_drvdata(pdev, &reset);

	/* Temporary until RPM clocks supported */
	clk = clk_register_fixed_rate(dev, "xo", NULL, CLK_IS_ROOT, 19200000);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	/* Should move to DT node? */
	clk = clk_register_fixed_rate(dev, "gcc_sleep_clk_src", NULL,
				      CLK_IS_ROOT, 32768);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	for (i = 0; i < num_clks; i++) {
		if (!map[i])
			continue;
		clk = devm_clk_register(dev, map[i]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);
		clks[i] = clk;
	}

	ret = of_clk_add_provider(dev->of_node, of_clk_src_onecell_get, data);
	if (ret)
		return ret;

	reset->rcdev.of_node = dev->of_node;
	reset->rcdev.ops = &msm_reset_ops,
	reset->rcdev.owner = THIS_MODULE,
	reset->rcdev.nr_resets = ARRAY_SIZE(msm_gcc_8974_resets),
	reset->regmap = regmap;
	reset->reset_map = msm_gcc_8974_resets,

	ret = reset_controller_register(&reset->rcdev);
	if (ret)
		of_clk_del_provider(dev->of_node);

	return ret;
}

static int msm_gcc_8974_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);
	reset_controller_unregister(platform_get_drvdata(pdev));
	return 0;
}

static struct platform_driver msm_gcc_8974_driver = {
	.probe		= msm_gcc_8974_probe,
	.remove		= msm_gcc_8974_remove,
	.driver		= {
		.name	= "msm-gcc-8974",
		.owner	= THIS_MODULE,
		.of_match_table = msm_gcc_8974_match_table,
	},
};

static int __init msm_gcc_8974_init(void)
{
	return platform_driver_register(&msm_gcc_8974_driver);
}
core_initcall(msm_gcc_8974_init);

static void __exit msm_gcc_8974_exit(void)
{
	platform_driver_unregister(&msm_gcc_8974_driver);
}
module_exit(msm_gcc_8974_exit);

MODULE_DESCRIPTION("MSM GCC 8974 Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:msm-gcc-8974");
