// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2026 Synaptics Incorporated
 */

#include <asm/armv8/mmu.h>
#include <fdtdec.h>

#define SYNAPTICS_MEM_MAP_LEN ((CONFIG_NR_DRAM_BANKS) + 2)
/*
 * Force synaptics_mem_map initialization since this code runs
 * before BSS is cleared.
 * The last entry is the sentinel
 */
__section(".data") static struct mm_region synaptics_mem_map[SYNAPTICS_MEM_MAP_LEN] = { 0 };
struct mm_region *mem_map = synaptics_mem_map;

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	struct mm_region *reg_mem = mem_map + CONFIG_NR_DRAM_BANKS;
	int ret;

	ret = fdtdec_setup_memory_banksize();
	if (ret)
		return ret;

	ret = mem_map_from_dram_banks(0, SYNAPTICS_MEM_MAP_LEN,
				      PTE_BLOCK_MEMTYPE(MT_NORMAL) |
					     PTE_BLOCK_INNER_SHARE);
	if (ret)
		return ret;

	reg_mem->virt = CONFIG_SYNAPTICS_REGMEM_MAP_BASE;
	reg_mem->phys = CONFIG_SYNAPTICS_REGMEM_MAP_BASE;
	reg_mem->size = CONFIG_SYNAPTICS_REGMEM_MAP_SIZE;
	reg_mem->attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
		PTE_BLOCK_NON_SHARE | PTE_BLOCK_PXN | PTE_BLOCK_UXN;

	return 0;
}
