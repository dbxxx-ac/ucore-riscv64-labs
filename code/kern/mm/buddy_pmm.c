#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>

// lab2 Buddy System 2513513
// The fixed managed RAM window contains at most 2^15 pages.
#define BUDDY_MAX_ORDER 15
#define BUDDY_ALLOC_HEAD (1UL << 2)
static list_entry_t free_lists[BUDDY_MAX_ORDER + 1];
static size_t free_count;

static size_t block_pages(unsigned order) {
    return (size_t)1 << order;
}

static unsigned request_order(size_t n) {
    unsigned order = 0;
    while (order < BUDDY_MAX_ORDER && block_pages(order) < n) {
        order++;
    }
    return order;
}

static void buddy_init(void) {
    assert((PHYSICAL_MEMORY_END - DRAM_BASE) / PGSIZE <=
           block_pages(BUDDY_MAX_ORDER));
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&free_lists[order]);
    }
    free_count = 0;
}

// Insert a free block, recursively absorbing only its same-order buddy.
// Accounting is done by the caller, not by splitting or coalescing.
static void insert_block(struct Page *base, unsigned order) {
    size_t index = base - pages;
    while (order < BUDDY_MAX_ORDER) {
        size_t other = index ^ block_pages(order);
        if (other >= npage - nbase ||
            block_pages(order) > npage - nbase - other) {
            break;
        }
        struct Page *buddy = pages + other;
        if (!PageProperty(buddy) || buddy->property != order) {
            break;
        }
        list_del_init(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (other < index) {
            index = other;
        }
        order++;
    }
    base = pages + index;
    base->property = order;
    SetPageProperty(base);
    list_add(&free_lists[order], &base->page_link);
}

static void buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0 && base >= pages && base < pages + (npage - nbase));
    assert(n <= (size_t)(pages + (npage - nbase) - base));
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(base + i));
    }
    for (size_t i = 0; i < n; i++) {
        base[i].flags = base[i].property = 0;
        set_page_ref(base + i, 0);
        list_init(&base[i].page_link);
    }
    free_count += n;
    while (n != 0) {
        size_t index = base - pages;
        unsigned order = 0;
        while (order < BUDDY_MAX_ORDER &&
               block_pages(order + 1) <= n &&
               (index & (block_pages(order + 1) - 1)) == 0) {
            order++;
        }
        size_t length = block_pages(order);
        insert_block(base, order);
        base += length;
        n -= length;
    }
}

static struct Page *buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > block_pages(BUDDY_MAX_ORDER)) {
        return NULL;
    }
    unsigned wanted = request_order(n), order = wanted;
    if (block_pages(wanted) > free_count) {
        return NULL;
    }
    while (order <= BUDDY_MAX_ORDER && list_empty(&free_lists[order])) {
        order++;
    }
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }
    struct Page *base = le2page(list_next(&free_lists[order]), page_link);
    list_del_init(&base->page_link);
    ClearPageProperty(base);
    while (order > wanted) {
        order--;
        struct Page *right = base + block_pages(order);
        right->property = order;
        SetPageProperty(right);
        list_add(&free_lists[order], &right->page_link);
    }
    base->flags = BUDDY_ALLOC_HEAD;
    base->property = n; // Original request, not order, for matching free(n).
    free_count -= block_pages(wanted);
    return base;
}

static void buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0 && n <= block_pages(BUDDY_MAX_ORDER));
    assert(base >= pages && base < pages + (npage - nbase));
    assert(base->flags == BUDDY_ALLOC_HEAD && base->property == n);
    unsigned order = request_order(n);
    size_t length = block_pages(order);
    assert(((size_t)(base - pages) & (length - 1)) == 0);
    assert(length <= (size_t)(pages + (npage - nbase) - base));
    for (size_t i = 0; i < length; i++) {
        assert(!PageReserved(base + i) && !PageProperty(base + i));
        base[i].flags = base[i].property = 0;
        set_page_ref(base + i, 0);
    }
    free_count += length;
    insert_block(base, order);
}

static size_t buddy_nr_free_pages(void) {
    return free_count;
}

// Test storage is static: the boot stack is only two pages.
#define TEST_CAPACITY (1UL << BUDDY_MAX_ORDER)
static unsigned char seen[TEST_CAPACITY], baseline[TEST_CAPACITY];
static struct Page test_pages[64];
static list_entry_t saved_lists[BUDDY_MAX_ORDER + 1];

// Check coverage, links, alignment, accounting and maximal coalescing.
static void check_free_lists(void) {
    size_t total = 0, capacity = npage - nbase;
    assert(capacity <= TEST_CAPACITY);
    for (size_t i = 0; i < capacity; i++) {
        seen[i] = 0;
    }
    for (unsigned order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_entry_t *head = &free_lists[order];
        assert(list_next(list_prev(head)) == head);
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
            struct Page *p = le2page(le, page_link);
            size_t length = block_pages(order);
            assert(p >= pages && p < pages + capacity);
            size_t index = p - pages;
            assert(PageProperty(p) && !PageReserved(p) && p->property == order);
            assert((index & (length - 1)) == 0 && length <= capacity - index);
            assert(list_prev(list_next(le)) == le);
            assert(list_next(list_prev(le)) == le);
            for (size_t i = 0; i < length; i++) {
                assert(!seen[index + i]);
                assert(!PageReserved(p + i));
                assert(((p + i)->flags & BUDDY_ALLOC_HEAD) == 0);
                if (i != 0) {
                    assert(!PageProperty(p + i));
                }
                seen[index + i] = 1;
            }
            size_t other = index ^ length;
            if (order < BUDDY_MAX_ORDER && other < capacity) {
                assert(!PageProperty(pages + other) ||
                       pages[other].property != order);
            }
            total += length;
            assert(total <= capacity);
        }
    }
    for (size_t i = 0; i < capacity; i++) {
        assert(!PageProperty(pages + i) || seen[i]);
    }
    assert(total == free_count);
}

// Compare exact block boundaries and orders, not incidental list ordering.
static void layout_snapshot(bool save) {
    check_free_lists();
    for (size_t i = 0; i < npage - nbase; i++) {
        unsigned char value = PageProperty(pages + i) ? pages[i].property + 1 : 0;
        if (save) {
            baseline[i] = value;
        } else {
            assert(baseline[i] == value);
        }
    }
}

static void reset_test_arena(size_t first, size_t length) {
    buddy_init();
    for (size_t i = 0; i < 64; i++) {
        test_pages[i].flags = 0;
        SetPageReserved(test_pages + i);
        test_pages[i].property = 0;
        set_page_ref(test_pages + i, 0);
    }
    buddy_init_memmap(test_pages + first, length);
    check_free_lists();
}

static void buddy_check(void) {
    // Preserve the live allocator while testing synthetic descriptors.
    check_free_lists();
    struct Page *live_pages = pages;
    size_t live_npage = npage, live_free = free_count;
    for (unsigned i = 0; i <= BUDDY_MAX_ORDER; i++) {
        saved_lists[i] = free_lists[i];
    }
    pages = test_pages;
    npage = nbase + 64;

    reset_test_arena(1, 29);
    assert(free_count == 29);
    for (size_t i = 0; i < 64; i++) {
        assert(seen[i] == (i >= 1 && i < 30));
    }
    layout_snapshot(1);
    struct Page *single[32];
    for (size_t i = 0; i < 29; i++) {
        single[i] = buddy_alloc_pages(1);
        assert(single[i] >= pages + 1 && single[i] < pages + 30);
        for (size_t j = 0; j < i; j++) {
            assert(single[i] != single[j]);
        }
    }
    assert(free_count == 0 && buddy_alloc_pages(1) == NULL);
    for (size_t i = 29; i > 0; i--) {
        buddy_free_pages(single[i - 1], 1);
    }
    assert(free_count == 29);
    layout_snapshot(0);
    cprintf("buddy_test: unaligned 29-page initialization/exhaustion/restoration passed\n");

    for (unsigned order = 0; order < 5; order++) {
        for (unsigned reverse = 0; reverse < 2; reverse++) {
            reset_test_arena(0, 32);
            layout_snapshot(1);
            size_t length = block_pages(order);
            struct Page *a = buddy_alloc_pages(length);
            struct Page *b = buddy_alloc_pages(length);
            assert(a == pages && b == pages + length);
            assert(free_count == 32 - 2 * length);
            check_free_lists();
            buddy_free_pages(reverse ? b : a, length);
            check_free_lists();
            buddy_free_pages(reverse ? a : b, length);
            assert(free_count == 32);
            layout_snapshot(0);
        }
    }
    cprintf("buddy_test: orders 0..4 split and both buddy free orders passed\n");

    reset_test_arena(0, 32);
    layout_snapshot(1);
    for (size_t i = 0; i < 32; i++) {
        single[i] = buddy_alloc_pages(1);
        assert(single[i] == pages + i);
    }
    for (size_t i = 0; i < 32; i += 2) {
        buddy_free_pages(single[i], 1);
    }
    assert(free_count == 16 && buddy_alloc_pages(2) == NULL);
    assert(buddy_alloc_pages(TEST_CAPACITY + 1) == NULL);
    check_free_lists();
    for (size_t i = 1; i < 32; i += 2) {
        buddy_free_pages(single[i], 1);
    }
    assert(free_count == 32);
    layout_snapshot(0);
    cprintf("buddy_test: fragmented failure with 16 free pages passed\n");

    const size_t requests[] = {1, 2, 3, 5, 8, 9, 16, 17, 31, 32};
    reset_test_arena(0, 64);
    layout_snapshot(1);
    for (unsigned cycle = 0; cycle < 20; cycle++) {
        for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); i++) {
            size_t n = requests[i], actual = block_pages(request_order(n));
            struct Page *p = buddy_alloc_pages(n);
            assert(p != NULL && p->property == n);
            assert(free_count == 64 - actual);
            check_free_lists();
            buddy_free_pages(p, n);
            assert(free_count == 64);
            layout_snapshot(0);
        }
    }
    cprintf("buddy_test: rounding and 200 allocation/free cycles passed\n");

    // Restore original sentinel addresses as well as saved links.
    pages = live_pages;
    npage = live_npage;
    free_count = live_free;
    for (unsigned i = 0; i <= BUDDY_MAX_ORDER; i++) {
        free_lists[i] = saved_lists[i];
    }
    layout_snapshot(1);
    const size_t live_requests[] = {1, 3, 5, 9, 17, 33};
    struct Page *blocks[6];
    for (unsigned cycle = 0; cycle < 8; cycle++) {
        size_t used = 0;
        for (unsigned i = 0; i < 6; i++) {
            size_t length = block_pages(request_order(live_requests[i]));
            blocks[i] = buddy_alloc_pages(live_requests[i]);
            assert(blocks[i] != NULL);
            for (unsigned j = 0; j < i; j++) {
                size_t previous = block_pages(request_order(live_requests[j]));
                assert(blocks[i] + length <= blocks[j] ||
                       blocks[j] + previous <= blocks[i]);
            }
            used += length;
            assert(free_count == live_free - used);
            check_free_lists();
        }
        for (unsigned i = 6; i > 0; i--) {
            buddy_free_pages(blocks[i - 1], live_requests[i - 1]);
            check_free_lists();
        }
        assert(free_count == live_free);
        layout_snapshot(0);
    }
    cprintf("buddy_test: live non-overlap and 8 repeated batches passed\n");
    cprintf("buddy_check: behavioral checks passed, restored free pages %lu\n", free_count);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
