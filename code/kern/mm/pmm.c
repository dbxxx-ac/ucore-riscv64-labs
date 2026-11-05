#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <../sync/sync.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 请补充你在lab2中的代码
static void page_init(void) {
    extern char end[];
    uint64_t base = get_memory_base();
    uint64_t size = get_memory_size();
    const uint64_t address_max = ~(uint64_t)0;

    if (size == 0 || base > address_max - size) {
        panic("page_init: invalid DTB memory range");
    }
    // The boot mapping and descriptor indices assume this single DRAM base.
    if (base != DRAM_BASE) {
        panic("page_init: unsupported DRAM base 0x%016lx", base);
    }
    uint64_t detected_end = base + size;
    cprintf("page_init: detected RAM [0x%016lx, 0x%016lx)\n",
            base, detected_end);
    const uintptr_t kernel_begin_pa = KERNBASE - PHYSICAL_MEMORY_OFFSET;
    const uintptr_t physical_memory_end = KERNTOP - PHYSICAL_MEMORY_OFFSET;
    uintptr_t managed_end = ROUNDDOWN(
        detected_end < physical_memory_end ? detected_end : physical_memory_end,
        PGSIZE);
    if (managed_end <= kernel_begin_pa) {
        panic("page_init: RAM does not cover the kernel");
    }

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    uintptr_t kernel_end = (uintptr_t)end;
    if (kernel_end < KERNBASE || kernel_end > address_max - (PGSIZE - 1)) {
        panic("page_init: invalid kernel end address");
    }
    uintptr_t descriptors_va = ROUNDUP(kernel_end, PGSIZE);
    uintptr_t descriptors_pa = PADDR(descriptors_va);
    npage = managed_end / PGSIZE;
    size_t count = npage - nbase;
    if (descriptors_pa >= managed_end ||
        count > (managed_end - descriptors_pa) / sizeof(struct Page)) {
        panic("page_init: RAM cannot hold page descriptors");
    }
    // The preceding division also bounds the multiplication and addition.
    uintptr_t descriptors_end = descriptors_pa + count * sizeof(struct Page);
    if (descriptors_end > address_max - (PGSIZE - 1)) {
        panic("page_init: page alignment overflow");
    }
    uintptr_t free_begin = ROUNDUP(descriptors_end, PGSIZE);
    if (free_begin >= managed_end) {
        panic("page_init: no complete free pages");
    }

    pages = (struct Page *)descriptors_va;
    for (size_t i = 0; i < count; i++) {
        memset(&pages[i], 0, sizeof(struct Page));
        SetPageReserved(&pages[i]);
    }
    size_t free_count = (managed_end - free_begin) / PGSIZE;
    init_memmap(pa2page(free_begin), free_count);
    cprintf("page_init: managed end 0x%016lx, npage %lu, descriptors %lu\n",
            managed_end, npage, count);
    cprintf("page_init: descriptors VA 0x%016lx, PA [0x%016lx, 0x%016lx)\n",
            descriptors_va, descriptors_pa, descriptors_end);
    cprintf("page_init: free RAM [0x%016lx, 0x%016lx), %lu pages\n",
            free_begin, managed_end, free_count);
}


/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
