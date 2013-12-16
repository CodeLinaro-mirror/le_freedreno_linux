/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <mach/msm_iomap.h>
#include "clock.h"
#include "footswitch.h"

#define GEMINI_GFS_CTL_REG	0x01A0
#define GFX2D0_GFS_CTL_REG	0x0180
#define GFX2D1_GFS_CTL_REG	0x0184
#define GFX3D_GFS_CTL_REG	0x0188
#define MDP_GFS_CTL_REG		0x0190
#define ROT_GFS_CTL_REG		0x018C
#define VED_GFS_CTL_REG		0x0194
#define VFE_GFS_CTL_REG		0x0198
#define VPE_GFS_CTL_REG		0x019C
#define VCAP_GFS_CTL_REG	0x0254

#define CLAMP_BIT		BIT(5)
#define ENABLE_BIT		BIT(8)
#define RETENTION_BIT		BIT(9)

#define GFS_DELAY_CNT		31

#define RESET_DELAY_US		1
/* Clock rate to use if one has not previously been set. */
#define DEFAULT_RATE		27000000
#define MAX_CLKS		10

/*
 * Lock is only needed to protect against the first footswitch_enable()
 * call occuring concurrently with late_footswitch_init().
 */
static DEFINE_MUTEX(claim_lock);

struct footswitch {
	struct regulator_dev	*rdev;
	struct regulator_desc	desc;
	void			*gfs_ctl_reg;
	u32			gfs_ctl_reg_off;
	bool			is_enabled;
	bool			is_claimed;
	struct fs_clk_data	*clk_data;
	struct clk		*core_clk;
};

static int setup_clocks(struct footswitch *fs)
{
	int rc = 0;
	struct fs_clk_data *clock;
	long rate;

	/*
	 * Enable all clocks in the power domain. If a specific clock rate is
	 * required for reset timing, set that rate before enabling the clocks.
	 */
	for (clock = fs->clk_data; clock->clk; clock++) {
		clock->rate = clk_get_rate(clock->clk);
		if (!clock->rate || clock->reset_rate) {
			rate = clock->reset_rate ?
					clock->reset_rate : DEFAULT_RATE;
			rc = clk_set_rate(clock->clk, rate);
			if (rc && rc != -ENOSYS) {
				pr_err("Failed to set %s %s rate to %lu Hz.\n",
				       fs->desc.name, clock->name, clock->rate);
				for (clock--; clock >= fs->clk_data; clock--) {
					if (clock->enabled)
						clk_disable_unprepare(
								clock->clk);
					clk_set_rate(clock->clk, clock->rate);
				}
				return rc;
			}
		}
		/*
		 * Some clocks are for reset purposes only. These clocks will
		 * fail to enable. Ignore the failures but keep track of them so
		 * we don't try to disable them later and crash due to
		 * unbalanced calls.
		 */
		clock->enabled = !clk_prepare_enable(clock->clk);
	}

	return 0;
}

static void restore_clocks(struct footswitch *fs)
{
	struct fs_clk_data *clock;

	/* Restore clocks to their orignal states before setup_clocks(). */
	for (clock = fs->clk_data; clock->clk; clock++) {
		if (clock->enabled)
			clk_disable_unprepare(clock->clk);
		if (clock->rate && clk_set_rate(clock->clk, clock->rate))
			pr_err("Failed to restore %s %s rate to %lu Hz.\n",
			       fs->desc.name, clock->name, clock->rate);
	}
}

static int footswitch_is_enabled(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);

	return fs->is_enabled;
}

static int footswitch_enable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	mutex_lock(&claim_lock);
	fs->is_claimed = true;
	mutex_unlock(&claim_lock);

	/* Return early if already enabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & (ENABLE_BIT | CLAMP_BIT)) == ENABLE_BIT)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/*
	 * (Re-)Assert resets for all clocks in the clock domain, since
	 * footswitch_enable() is first called before footswitch_disable()
	 * and resets should be asserted before power is restored.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(RESET_DELAY_US);

	/* Enable the power rail at the footswitch. */
	regval |= ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);
	/* Wait for the rail to fully charge. */
	mb();
	udelay(1);

	/* Un-clamp the I/O ports. */
	regval &= ~CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Deassert resets for all clocks in the power domain. */
	for (clock = fs->clk_data; clock->clk; clock++)
		clk_reset(clock->clk, CLK_RESET_DEASSERT);
	/* Toggle core reset again after first power-on (required for GFX3D). */
	if (fs->desc.id == FS_GFX3D) {
		clk_reset(fs->core_clk, CLK_RESET_ASSERT);
		udelay(RESET_DELAY_US);
		clk_reset(fs->core_clk, CLK_RESET_DEASSERT);
		udelay(RESET_DELAY_US);
	}

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = true;
	return 0;
}

static int footswitch_disable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	/* Return early if already disabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & ENABLE_BIT) == 0)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/*
	 * Assert resets for all clocks in the clock domain so that
	 * outputs settle prior to clamping.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(RESET_DELAY_US);

	/*
	 * Return clocks to their state before this function. For robustness
	 * if memory-retention across collapses is required, clocks should
	 * be disabled before asserting the clamps. Assuming clocks were off
	 * before entering footswitch_disable(), this will be true.
	 */
	restore_clocks(fs);

	/*
	 * Clamp the I/O ports of the core to ensure the values
	 * remain fixed while the core is collapsed.
	 */
	regval |= CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Collapse the power rail at the footswitch. */
	regval &= ~ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	fs->is_enabled = false;
	return 0;
}

static int gfx2d_footswitch_enable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	mutex_lock(&claim_lock);
	fs->is_claimed = true;
	mutex_unlock(&claim_lock);

	/* Return early if already enabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & (ENABLE_BIT | CLAMP_BIT)) == ENABLE_BIT)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/* Disable core clock. */
	clk_disable_unprepare(fs->core_clk);

	/*
	 * (Re-)Assert resets for all clocks in the clock domain, since
	 * footswitch_enable() is first called before footswitch_disable()
	 * and resets should be asserted before power is restored.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(RESET_DELAY_US);

	/* Enable the power rail at the footswitch. */
	regval |= ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);
	mb();
	udelay(1);

	/* Un-clamp the I/O ports. */
	regval &= ~CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Deassert resets for all clocks in the power domain. */
	for (clock = fs->clk_data; clock->clk; clock++)
		clk_reset(clock->clk, CLK_RESET_DEASSERT);
	udelay(RESET_DELAY_US);

	/* Re-enable core clock. */
	clk_prepare_enable(fs->core_clk);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = true;
	return 0;
}

static int gfx2d_footswitch_disable(struct regulator_dev *rdev)
{
	struct footswitch *fs = rdev_get_drvdata(rdev);
	struct fs_clk_data *clock;
	uint32_t regval, rc = 0;

	/* Return early if already disabled. */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	if ((regval & ENABLE_BIT) == 0)
		return 0;

	/* Make sure required clocks are on at the correct rates. */
	rc = setup_clocks(fs);
	if (rc)
		return rc;

	/* Disable core clock. */
	clk_disable_unprepare(fs->core_clk);

	/*
	 * Assert resets for all clocks in the clock domain so that
	 * outputs settle prior to clamping.
	 */
	for (clock = fs->clk_data; clock->clk; clock++)
		; /* Do nothing */
	for (clock--; clock >= fs->clk_data; clock--)
		clk_reset(clock->clk, CLK_RESET_ASSERT);
	/* Wait for synchronous resets to propagate. */
	udelay(5);

	/*
	 * Clamp the I/O ports of the core to ensure the values
	 * remain fixed while the core is collapsed.
	 */
	regval |= CLAMP_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Collapse the power rail at the footswitch. */
	regval &= ~ENABLE_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	/* Re-enable core clock. */
	clk_prepare_enable(fs->core_clk);

	/* Return clocks to their state before this function. */
	restore_clocks(fs);

	fs->is_enabled = false;
	return 0;
}

static struct regulator_ops standard_fs_ops = {
	.is_enabled = footswitch_is_enabled,
	.enable = footswitch_enable,
	.disable = footswitch_disable,
};

static struct regulator_ops gfx2d_fs_ops = {
	.is_enabled = footswitch_is_enabled,
	.enable = gfx2d_footswitch_enable,
	.disable = gfx2d_footswitch_disable,
};

#define FOOTSWITCH(_id, _name, _ops, _gfs_ctl_reg_off) \
	[(_id)] = { \
		.desc = { \
			.id = (_id), \
			.name = (_name), \
			.ops = (_ops), \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
		.gfs_ctl_reg_off = (_gfs_ctl_reg_off), \
	}
static struct footswitch footswitches[] = {
	FOOTSWITCH(FS_GFX2D0, "fs_gfx2d0", &gfx2d_fs_ops, GFX2D0_GFS_CTL_REG),
	FOOTSWITCH(FS_GFX2D1, "fs_gfx2d1", &gfx2d_fs_ops, GFX2D1_GFS_CTL_REG),
	FOOTSWITCH(FS_GFX3D,  "fs_gfx3d", &standard_fs_ops, GFX3D_GFS_CTL_REG),
	FOOTSWITCH(FS_IJPEG,  "fs_ijpeg", &standard_fs_ops, GEMINI_GFS_CTL_REG),
	FOOTSWITCH(FS_MDP,    "fs_mdp",   &standard_fs_ops, MDP_GFS_CTL_REG),
	FOOTSWITCH(FS_ROT,    "fs_rot",   &standard_fs_ops, ROT_GFS_CTL_REG),
	FOOTSWITCH(FS_VED,    "fs_ved",   &standard_fs_ops, VED_GFS_CTL_REG),
	FOOTSWITCH(FS_VFE,    "fs_vfe",   &standard_fs_ops, VFE_GFS_CTL_REG),
	FOOTSWITCH(FS_VPE,    "fs_vpe",   &standard_fs_ops, VPE_GFS_CTL_REG),
	FOOTSWITCH(FS_VCAP,   "fs_vcap",  &standard_fs_ops, VCAP_GFS_CTL_REG),
};

struct fs_init_data {
	struct regulator_init_data init_data;
	struct regulator_consumer_supply consumer_supply;
	struct fs_clk_data clk_data[10];
};

static int footswitch_id(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	u32 id;
	int rc;

	rc = of_property_read_u32(np, "cell-index", &id);
	if (rc) {
		dev_err(&pdev->dev, "could not get id: %d\n", rc);
		return rc;
	}

	return id;
}

static int footswitch_probe(struct platform_device *pdev)
{
	struct footswitch *fs;
	struct fs_init_data *driver_data = NULL;
	struct regulator_init_data *init_data;
	struct regulator_config cfg;
	struct fs_clk_data *clock;
	struct device_node *child, *np = pdev->dev.of_node;
	struct resource *res;
	void __iomem *io;
	uint32_t regval, rc = 0;
	int i, id;

	if (!(pdev && pdev->dev.of_node))
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get memory resource\n");
		return -EINVAL;
	}

	io = devm_ioremap_nocache(&pdev->dev, res->start, resource_size(res));
	if (!io) {
		dev_err(&pdev->dev, "failed to ioremap\n");
		return -ENOMEM;
	}

	id = footswitch_id(pdev);
	if (id < 0)
		return id;

	if (id >= MAX_FS)
		return -ENODEV;

	pr_info("initializing footswitch[%d]\n", id);

	/* real quick/dirty hack conversion to DT: */
	driver_data = kzalloc(sizeof(*driver_data), GFP_KERNEL);
	if (!driver_data) {
		rc = -ENOMEM;
		goto err;
	}
	init_data = &driver_data->init_data;
	init_data->constraints.valid_modes_mask = REGULATOR_MODE_NORMAL;
	init_data->constraints.valid_ops_mask = REGULATOR_CHANGE_STATUS;
	init_data->num_consumer_supplies = 1;
	init_data->consumer_supplies = &driver_data->consumer_supply;
	rc = of_property_read_string(np, "qcom,footswitch-name",
			&init_data->consumer_supplies[0].supply);
	if (rc) {
		dev_err(&pdev->dev, "could not get supply name: %d\n", rc);
		goto err;
	}
	rc = of_property_read_string(np, "qcom,footswitch-consumer",
			&init_data->consumer_supplies[0].dev_name);
	if (rc) {
		dev_err(&pdev->dev, "could not get consumer name: %d\n", rc);
		goto err;
	}

	/* read the clk names/rates: */
	i = 0;
	for_each_child_of_node(np, child) {
		if (of_device_is_compatible(child, "qcom,footswitch-clks")) {
			struct device_node *clk;
			for_each_child_of_node(child, clk) {
				struct fs_clk_data *clk_data = &driver_data->clk_data[i++];
				u32 val;

				if (WARN_ON(i >= ARRAY_SIZE(driver_data->clk_data))) {
					rc = -EINVAL;
					goto err;
				}

				rc = of_property_read_string(clk, "qcom,clk-name",
						&clk_data->name);
				if (rc) {
					dev_err(&pdev->dev, "could not get clk name: %d\n", rc);
					goto err;
				}

				if (!of_property_read_u32(clk, "reset-rate", &val))
					clk_data->reset_rate = val;
			}
		}
	}

	fs = &footswitches[id];
	fs->clk_data = driver_data->clk_data;

	for (clock = fs->clk_data; clock->name; clock++) {
		clock->clk = devm_clk_get(&pdev->dev, clock->name);
		if (IS_ERR(clock->clk)) {
			rc = PTR_ERR(clock->clk);

			// hack.. if mmcc is not probed yet, we get -ENOENT instead of
			// -EPROBE_DEFER.. which seems like a bug in of_clk_get_from_provider()
			if (rc == -ENOENT)
				rc = -EPROBE_DEFER;

			pr_err("%s clk_get(%s) failed\n", fs->desc.name,
			       clock->name);
			goto err;
		}
		if (!strncmp(clock->name, "core_clk", 8))
			fs->core_clk = clock->clk;
	}

	fs->gfs_ctl_reg = io + fs->gfs_ctl_reg_off;

	/*
	 * Set number of AHB_CLK cycles to delay the assertion of gfs_en_all
	 * after enabling the footswitch.  Also ensure the retention bit is
	 * clear so disabling the footswitch will power-collapse the core.
	 */
	regval = readl_relaxed(fs->gfs_ctl_reg);
	regval |= GFS_DELAY_CNT;
	regval &= ~RETENTION_BIT;
	writel_relaxed(regval, fs->gfs_ctl_reg);

	memset(&cfg, 0, sizeof(cfg));
	cfg.dev = &pdev->dev;
	cfg.init_data = init_data;
	cfg.driver_data = fs;
	cfg.of_node = np;

	fs->rdev = regulator_register(&fs->desc, &cfg);
	if (IS_ERR(footswitches[id].rdev)) {
		pr_err("regulator_register(\"%s\") failed\n",
			fs->desc.name);
		rc = PTR_ERR(footswitches[id].rdev);
		goto err;
	}

	return 0;

err:
	kfree(driver_data);
	return rc;
}

static int footswitch_remove(struct platform_device *pdev)
{
	struct footswitch *fs = &footswitches[footswitch_id(pdev)];
	regulator_unregister(fs->rdev);
	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,footswitch-8x60" },
	{}
};
MODULE_DEVICE_TABLE(of, dt_match);

static struct platform_driver footswitch_driver = {
	.probe		= footswitch_probe,
	.remove		= footswitch_remove,
	.driver		= {
		.name		= "footswitch-8x60",
		.owner		= THIS_MODULE,
		.of_match_table = dt_match,
	},
};

static int __init footswitch_init(void)
{
	return platform_driver_register(&footswitch_driver);
}
subsys_initcall(footswitch_init);

static void __exit footswitch_exit(void)
{
	platform_driver_unregister(&footswitch_driver);
}
module_exit(footswitch_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM8x60 rail footswitch");
MODULE_ALIAS("platform:footswitch-msm8x60");
