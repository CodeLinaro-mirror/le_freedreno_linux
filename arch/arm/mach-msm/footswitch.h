/* Copyright (c) 2010-2012 Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MSM_FOOTSWITCH__
#define __MSM_FOOTSWITCH__

#include <linux/regulator/machine.h>

/* Device IDs */
#define FS_GFX2D0	0
#define FS_GFX2D1	1
#define FS_GFX3D	2
#define FS_IJPEG	3
#define FS_MDP		4
#define FS_MFC		5
#define FS_ROT		6
#define FS_VED		7
#define FS_VFE		8
#define FS_VPE		9
#define FS_VCAP		10
#define MAX_FS		11

struct fs_clk_data {
	const char *name;
	struct clk *clk;
	unsigned long rate;
	unsigned long reset_rate;
	bool enabled;
};

#endif
