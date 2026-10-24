#include <slub.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>

// Simplified SLUB: per-class slab lists and an in-page allocation bitmap.
#define CLASS_COUNT 8
#define ALIGNMENT 16
#define BITMAP_WORDS 4
struct slab {
    struct slab *next, *prev;
    struct Page *page;
    size_t requested_pages; // Preserve Buddy's ORIGINAL request for free_pages.
    size_t object_size, capacity, used;
    uint64_t bitmap[BITMAP_WORDS];
};
#define DATA_OFFSET ROUNDUP(sizeof(struct slab), ALIGNMENT)
static const size_t sizes[CLASS_COUNT] = {16, 32, 64, 128, 256, 512, 1024, 2048};
static struct slab *classes[CLASS_COUNT], *large_objects;
static bool initialized;

static void link_slab(struct slab **head, struct slab *s) {
    s->prev = NULL;
    s->next = *head;
    if (*head != NULL) {
        (*head)->prev = s;
    }
    *head = s;
}

static void release_slab(struct slab **head, struct slab *s) {
    struct Page *page = s->page;
    size_t n = s->requested_pages;
    if (s->prev != NULL) {
        s->prev->next = s->next;
    } else {
        *head = s->next;
    }
    if (s->next != NULL) {
        s->next->prev = s->prev;
    }
    // Do not access s after returning its storage to Buddy.
    free_pages(page, n);
}

static struct slab *new_slab(size_t n) {
    struct Page *page = alloc_pages(n);
    if (page == NULL) {
        return NULL;
    }
    uintptr_t pa = page2pa(page);
    // entry.S maps this RAM window at PA + va_pa_offset; KADDR is disabled.
    assert(pa >= KERNEL_BEGIN_PADDR && pa < PHYSICAL_MEMORY_END);
    assert(PPN(pa) < npage && pa <= ~(uintptr_t)0 - va_pa_offset);
    struct slab *s = (struct slab *)(pa + va_pa_offset);
    memset(s, 0, sizeof(*s));
    s->page = page;
    s->requested_pages = n;
    return s;
}

void slub_init(void) {
    if (initialized) {
        return;
    }
    assert(pmm_manager != NULL && pages != NULL);
    assert(DATA_OFFSET < PGSIZE && DATA_OFFSET % ALIGNMENT == 0);
    assert((PGSIZE - DATA_OFFSET) / sizes[0] <= BITMAP_WORDS * 64);
    initialized = 1;
    cprintf("slub_init: initialized, small objects 16..2048 bytes, alignment 16\n");
}

void *slub_alloc(size_t size) {
    if (!initialized || size == 0) {
        return NULL;
    }
    if (size <= sizes[CLASS_COUNT - 1]) {
        unsigned c = 0;
        while (sizes[c] < size) {
            c++;
        }
        struct slab *s = classes[c];
        while (s != NULL && s->used == s->capacity) {
            s = s->next;
        }
        if (s == NULL) {
            s = new_slab(1);
            if (s == NULL) {
                return NULL;
            }
            s->object_size = sizes[c];
            s->capacity = (PGSIZE - DATA_OFFSET) / s->object_size;
            link_slab(&classes[c], s);
        }
        for (size_t i = 0; i < s->capacity; i++) {
            uint64_t mask = (uint64_t)1 << (i % 64);
            if (!(s->bitmap[i / 64] & mask)) {
                s->bitmap[i / 64] |= mask;
                s->used++;
                return (char *)s + DATA_OFFSET + i * s->object_size;
            }
        }
        panic("slub_alloc: inconsistent slab bitmap");
    }
    size_t max = ~(size_t)0;
    if (size > max - DATA_OFFSET) {
        return NULL;
    }
    size_t total = size + DATA_OFFSET;
    // Division plus remainder avoids an overflowing ROUNDUP(total, PGSIZE).
    size_t n = total / PGSIZE + (total % PGSIZE != 0);
    if (n > (PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE) {
        return NULL;
    }
    struct slab *s = new_slab(n);
    if (s == NULL) {
        return NULL;
    }
    s->object_size = size;
    s->capacity = s->used = 1;
    link_slab(&large_objects, s);
    return (char *)s + DATA_OFFSET;
}

void slub_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    assert(initialized);
    uintptr_t address = (uintptr_t)ptr;
    // Search owned pages before reading metadata; never dereference ptr itself.
    for (unsigned c = 0; c < CLASS_COUNT; c++) {
        for (struct slab *s = classes[c]; s != NULL; s = s->next) {
            uintptr_t start = (uintptr_t)s + DATA_OFFSET;
            if (address < start || address - start >= s->capacity * s->object_size) {
                continue;
            }
            size_t offset = address - start;
            assert(offset % s->object_size == 0);
            size_t i = offset / s->object_size;
            uint64_t mask = (uint64_t)1 << (i % 64);
            assert((s->bitmap[i / 64] & mask) != 0 && s->used > 0);
            s->bitmap[i / 64] &= ~mask;
            s->used--;
            if (s->used == 0) {
                release_slab(&classes[c], s);
            }
            return;
        }
    }
    for (struct slab *s = large_objects; s != NULL; s = s->next) {
        if (address == (uintptr_t)s + DATA_OFFSET) {
            release_slab(&large_objects, s);
            return;
        }
    }
    panic("slub_free: pointer is not a live SLUB object");
}
