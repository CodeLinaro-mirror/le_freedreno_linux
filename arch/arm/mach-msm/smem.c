/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/notifier.h>
#include <linux/stat.h>
#include <linux/of_address.h>
#include <linux/memblock.h>

#include <asm/mach/map.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smem.h>

#include "smem_private.h"

/**
 * OVERFLOW_ADD_UNSIGNED() - check for unsigned overflow
 *
 * @type: type to check for overflow
 * @a: left value to use
 * @b: right value to use
 * @returns: true if a + b will result in overflow; false otherwise
 */
#define OVERFLOW_ADD_UNSIGNED(type, a, b) \
	(((type)~0 - (a)) < (b) ? true : false)

#define MODEM_SBL_VERSION_INDEX 7
#define SMEM_VERSION_INFO_SIZE (32 * 4)
#define SMEM_VERSION 0x000B

enum {
	MSM_SMEM_DEBUG = 1U << 0,
	MSM_SMEM_INFO = 1U << 1,
};

static int msm_smem_debug_mask;
module_param_named(debug_mask, msm_smem_debug_mask,
			int, S_IRUGO | S_IWUSR | S_IWGRP);

#define SMEM_DBG(x...) do {                               \
		if (msm_smem_debug_mask & MSM_SMEM_DEBUG) \
			pr_debug(x);                      \
	} while (0)

remote_spinlock_t remote_spinlock;
int spinlocks_initialized;
uint32_t num_smem_areas;
struct smem_area *smem_areas;
struct ramdump_segment *smem_ramdump_segments;

static DEFINE_MUTEX(spinlock_init_lock);
static DEFINE_SPINLOCK(smem_init_check_lock);

static struct smem_area smem_block = {
#ifdef MSM_SHARED_RAM_PHYS
	.phys_addr = MSM_SHARED_RAM_PHYS,
#else
	.phys_addr = 0x00100000,
#endif
	.size = MSM_SHARED_RAM_SIZE,
	.virt_addr = MSM_SHARED_RAM_BASE,
};

/**
 * smem_phys_to_virt() - Convert a physical base and offset to virtual address
 *
 * @base: physical base address to check
 * @offset: offset from the base to get the final address
 * @returns: virtual SMEM address; NULL for failure
 *
 * Takes a physical address and an offset and checks if the resulting physical
 * address would fit into one of the smem regions.  If so, returns the
 * corresponding virtual address.  Otherwise returns NULL.
 */
static void *smem_phys_to_virt(phys_addr_t base, unsigned offset)
{
	int i;
	phys_addr_t phys_addr;
	resource_size_t size;

	if (OVERFLOW_ADD_UNSIGNED(phys_addr_t, base, offset))
		return NULL;

	if (!smem_areas) {
		/*
		 * Early boot - no area configuration yet, so default
		 * to using the main memory region.
		 *
		 * To remove the MSM_SHARED_RAM_BASE and the static
		 * mapping of SMEM in the future, add dump_stack()
		 * to identify the early callers of smem_get_entry()
		 * (which calls this function) and replace those calls
		 * with a new function that knows how to lookup the
		 * SMEM base address before SMEM has been probed.
		 */
		phys_addr = smem_block.phys_addr;
		size = smem_block.size;

		if (base >= phys_addr && base + offset < phys_addr + size) {
			if (OVERFLOW_ADD_UNSIGNED(uintptr_t,
				(uintptr_t)smem_block.virt_addr, offset)) {
				pr_err("%s: overflow %p %x\n", __func__,
					smem_block.virt_addr, offset);
				return NULL;
			}

			return smem_block.virt_addr + offset;
		} else {
			return NULL;
		}
	}
	for (i = 0; i < num_smem_areas; ++i) {
		phys_addr = smem_areas[i].phys_addr;
		size = smem_areas[i].size;

		if (base < phys_addr || base + offset >= phys_addr + size)
			continue;

		if (OVERFLOW_ADD_UNSIGNED(uintptr_t,
				(uintptr_t)smem_areas[i].virt_addr, offset)) {
			pr_err("%s: overflow %p %x\n", __func__,
				smem_areas[i].virt_addr, offset);
			return NULL;
		}

		return smem_areas[i].virt_addr + offset;
	}

	return NULL;
}

/**
 * smem_virt_to_phys() - Convert SMEM address to physical address.
 *
 * @smem_address: Address of SMEM item (returned by smem_alloc(), etc)
 * @returns: Physical address (or NULL if there is a failure)
 *
 * This function should only be used if an SMEM item needs to be handed
 * off to a DMA engine.
 */
phys_addr_t smem_virt_to_phys(void *smem_address)
{
	phys_addr_t phys_addr = 0;
	int i;
	void *vend;

	if (!smem_areas)
		return phys_addr;

	for (i = 0; i < num_smem_areas; ++i) {
		vend = (void *)(smem_areas[i].virt_addr + smem_areas[i].size);

		if (smem_address >= smem_areas[i].virt_addr &&
				smem_address < vend) {
			phys_addr = smem_address - smem_areas[i].virt_addr;
			phys_addr +=  smem_areas[i].phys_addr;
			break;
		}
	}

	return phys_addr;
}
EXPORT_SYMBOL(smem_virt_to_phys);

/* smem_alloc returns the pointer to smem item if it is already allocated.
 * Otherwise, it returns NULL.
 */
void *smem_alloc(unsigned id, unsigned size)
{
	return smem_find(id, size);
}
EXPORT_SYMBOL(smem_alloc);

static void *__smem_get_entry(unsigned id, unsigned *size, bool skip_init_check)
{
	struct smem_shared *shared = (void *) smem_block.virt_addr;
	struct smem_heap_entry *toc = shared->heap_toc;
	int use_spinlocks = spinlocks_initialized;
	void *ret = 0;
	unsigned long flags = 0;

	if (!skip_init_check && !smem_initialized_check())
		return ret;

	if (id >= SMEM_NUM_ITEMS)
		return ret;

	if (use_spinlocks)
		remote_spin_lock_irqsave(&remote_spinlock, flags);
	/* toc is in device memory and cannot be speculatively accessed */
	if (toc[id].allocated) {
		phys_addr_t phys_base;

		*size = toc[id].size;
		barrier();

		phys_base = toc[id].reserved & BASE_ADDR_MASK;
		if (!phys_base)
			phys_base = smem_block.phys_addr;
		ret = smem_phys_to_virt(phys_base, toc[id].offset);
	} else {
		*size = 0;
	}
	if (use_spinlocks)
		remote_spin_unlock_irqrestore(&remote_spinlock, flags);

	return ret;
}

static void *__smem_find(unsigned id, unsigned size_in, bool skip_init_check)
{
	unsigned size;
	void *ptr;

	ptr = __smem_get_entry(id, &size, skip_init_check);
	if (!ptr)
		return 0;

	size_in = ALIGN(size_in, 8);
	if (size_in != size) {
		pr_err("smem_find(%d, %d): wrong size %d\n",
			id, size_in, size);
		return 0;
	}

	return ptr;
}

void *smem_find(unsigned id, unsigned size_in)
{
	return __smem_find(id, size_in, false);
}
EXPORT_SYMBOL(smem_find);

/* smem_alloc2 returns the pointer to smem item.  If it is not allocated,
 * it allocates it and then returns the pointer to it.
 */
void *smem_alloc2(unsigned id, unsigned size_in)
{
	struct smem_shared *shared = (void *)smem_block.virt_addr;
	struct smem_heap_entry *toc = shared->heap_toc;
	unsigned long flags;
	void *ret = NULL;
	int rc;

	if (!smem_initialized_check())
		return NULL;

	if (id >= SMEM_NUM_ITEMS)
		return NULL;

	if (unlikely(!spinlocks_initialized)) {
		rc = init_smem_remote_spinlock();
		if (unlikely(rc)) {
			pr_err("%s: remote spinlock init failed %d\n",
								__func__, rc);
			return NULL;
		}
	}

	size_in = ALIGN(size_in, 8);
	remote_spin_lock_irqsave(&remote_spinlock, flags);
	if (toc[id].allocated) {
		SMEM_DBG("%s: %u already allocated\n", __func__, id);
		if (size_in != toc[id].size)
			pr_err("%s: wrong size %u (expected %u)\n",
				__func__, toc[id].size, size_in);
		else
			ret = (void *)(smem_block.virt_addr + toc[id].offset);
	} else if (id > SMEM_FIXED_ITEM_LAST) {
		SMEM_DBG("%s: allocating %u\n", __func__, id);
		if (shared->heap_info.heap_remaining >= size_in) {
			toc[id].offset = shared->heap_info.free_offset;
			toc[id].size = size_in;
			wmb();
			toc[id].allocated = 1;

			shared->heap_info.free_offset += size_in;
			shared->heap_info.heap_remaining -= size_in;
			ret = (void *)(smem_block.virt_addr + toc[id].offset);
		} else
			pr_err("%s: not enough memory %u (required %u)\n",
				__func__, shared->heap_info.heap_remaining,
				size_in);
	}
	wmb();
	remote_spin_unlock_irqrestore(&remote_spinlock, flags);
	return ret;
}
EXPORT_SYMBOL(smem_alloc2);

void *smem_get_entry(unsigned id, unsigned *size)
{
	return __smem_get_entry(id, size, false);
}
EXPORT_SYMBOL(smem_get_entry);


/**
 * smem_get_remote_spinlock - Remote spinlock pointer for unit testing.
 *
 * @returns: pointer to SMEM remote spinlock
 */
remote_spinlock_t *smem_get_remote_spinlock(void)
{
	return &remote_spinlock;
}
EXPORT_SYMBOL(smem_get_remote_spinlock);

/**
 * init_smem_remote_spinlock - Reentrant remote spinlock initialization
 *
 * @returns: sucess or error code for failure
 */
int init_smem_remote_spinlock(void)
{
	int rc = 0;

	/*
	 * Optimistic locking.  Init only needs to be done once by the first
	 * caller.  After that, serializing inits between different callers
	 * is unnecessary.  The second check after the lock ensures init
	 * wasn't previously completed by someone else before the lock could
	 * be grabbed.
	 */
	if (!spinlocks_initialized) {
		mutex_lock(&spinlock_init_lock);
		if (!spinlocks_initialized) {
			rc = remote_spin_lock_init(&remote_spinlock,
						SMEM_SPINLOCK_SMEM_ALLOC);
			if (!rc)
				spinlocks_initialized = 1;
		}
		mutex_unlock(&spinlock_init_lock);
	}
	return rc;
}

#if defined(CONFIG_OF) && defined(CONFIG_OF_ADDRESS)
static struct of_device_id msm_smem_match_table[] = {
	{ .compatible = "qcom,smem" },
	{},
};

static int smem_of_init(struct smem_area *area)
{
	struct device_node *node;
	struct resource res;
	int idx;

	node = of_find_matching_node(NULL, msm_smem_match_table);
	if (node == NULL)
		return -EINVAL;

	idx = of_property_match_string(node, "reg-names", "smem");
	if (idx < 0)
		goto err;

	if (of_address_to_resource(node, idx, &res))
		goto err;

	if (of_get_property(node, "reserve-memory", NULL)) {
		if (memblock_remove(res.start, resource_size(&res)))
			goto err;
	}

	area->virt_addr = ioremap(res.start, resource_size(&res));
	if (area->virt_addr == NULL)
		goto err;

	area->phys_addr = res.start;
	area->size      = resource_size(&res);

	of_node_put(node);
	pr_info("%s: initialized successfully\n", __func__);
	return 0;
err:
	of_node_put(node);
	pr_info("%s: failed initialization\n", __func__);
	return -EINVAL;
}
#else
#define smem_of_init(ptr) 0
#endif
/**
 * smem_initialized_check - Reentrant check that smem has been initialized
 *
 * @returns: true if initialized, false if not.
 */
bool smem_initialized_check(void)
{
	static int checked;
	static int is_inited;
	unsigned long flags;
	struct smem_shared *smem;
	int *version_array;

	if (likely(checked)) {
		if (unlikely(!is_inited))
			pr_err("%s: smem not initialized\n", __func__);
		return is_inited;
	}

	spin_lock_irqsave(&smem_init_check_lock, flags);
	if (checked) {
		spin_unlock_irqrestore(&smem_init_check_lock, flags);
		if (unlikely(!is_inited))
			pr_err("%s: smem not initialized\n", __func__);
		return is_inited;
	}

	if (smem_of_init(&smem_block))
		goto failed;
	smem = (void *)smem_block.virt_addr;
	if (smem->heap_info.initialized != 1)
		goto failed;
	if (smem->heap_info.reserved != 0)
		goto failed;

	version_array = __smem_find(SMEM_VERSION_INFO, SMEM_VERSION_INFO_SIZE,
									true);
	if (version_array == NULL)
		goto failed;
	if (version_array[MODEM_SBL_VERSION_INDEX] != SMEM_VERSION << 16)
		goto failed;

	is_inited = 1;
	checked = 1;
	spin_unlock_irqrestore(&smem_init_check_lock, flags);
	return is_inited;

failed:
	is_inited = 0;
	checked = 1;
	spin_unlock_irqrestore(&smem_init_check_lock, flags);
	pr_err("%s: bootloader failure detected, shared memory not inited\n",
								__func__);
	return is_inited;
}
EXPORT_SYMBOL(smem_initialized_check);

int smem_init(struct smem_area *area)
{
	if (!smem_initialized_check())
		return -ENOMEM;
	if (area)
		*area = smem_block;
	return 0;
}
EXPORT_SYMBOL(smem_init);
