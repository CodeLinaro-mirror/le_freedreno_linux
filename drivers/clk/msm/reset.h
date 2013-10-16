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

#ifndef __MSM_CLK_RESET_H__
#define __MSM_CLK_RESET_H__

#include <linux/reset-controller.h>

struct msm_reset_map {
	unsigned int reg;
	u8 bit;
};

struct regmap;

struct msm_reset_controller {
	const struct msm_reset_map *reset_map;
	struct regmap *regmap;
	struct reset_controller_dev rcdev;
};

#define to_msm_reset_controller(r) \
	container_of(r, struct msm_reset_controller, rcdev);

extern struct reset_control_ops msm_reset_ops;

#endif
