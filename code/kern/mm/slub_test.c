#include <slub.h>
#include <pmm.h>
#include <stdio.h>

// Large test collections stay off the two-page boot stack.
static void *objects[600];
static const size_t boundaries[] = {
    1, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128,
    129, 255, 256, 257, 511, 512, 513, 1023, 1024,
    1025, 2047, 2048, 2049, 4096
};

static unsigned char pattern(size_t id, size_t offset) {
    return (unsigned char)(id * 37 + offset * 13 + (offset >> 8));
}

static void write_pattern(void *ptr, size_t size, size_t id) {
    assert(ptr != NULL && ((uintptr_t)ptr & 15) == 0);
    unsigned char *bytes = ptr;
    for (size_t i = 0; i < size; i++) {
        bytes[i] = pattern(id, i);
    }
}

static void read_pattern(void *ptr, size_t size, size_t id) {
    unsigned char *bytes = ptr;
    for (size_t i = 0; i < size; i++) {
        assert(bytes[i] == pattern(id, i));
    }
}

static void disjoint(void *a, size_t an, void *b, size_t bn) {
    uintptr_t av = (uintptr_t)a, bv = (uintptr_t)b;
    assert(av + an <= bv || bv + bn <= av);
}

static void boundary_check(void) {
    size_t before = nr_free_pages();
    size_t count = sizeof(boundaries) / sizeof(boundaries[0]);
    for (size_t i = 0; i < count; i++) {
        objects[i] = slub_alloc(boundaries[i]);
        write_pattern(objects[i], boundaries[i], i);
        for (size_t j = 0; j < i; j++) {
            disjoint(objects[i], boundaries[i], objects[j], boundaries[j]);
        }
    }
    // All patterns are read AFTER all objects have been written.
    for (size_t i = 0; i < count; i++) {
        read_pattern(objects[i], boundaries[i], i);
    }
    slub_init(); // Reinitialization must not discard live objects.
    for (size_t i = count; i > 0; i--) {
        read_pattern(objects[i - 1], boundaries[i - 1], i - 1);
        slub_free(objects[i - 1]);
    }
    assert(nr_free_pages() == before);
    cprintf("slub_test: size boundaries/alignment/live data isolation passed\n");
}

static void slab_reuse_check(void) {
    size_t before = nr_free_pages(), count = 0;
    uintptr_t first_page = 0;
    bool crossed = 0;
    // Discover usable capacity by observed addresses, without slab internals.
    for (; count < sizeof(objects) / sizeof(objects[0]); count++) {
        objects[count] = slub_alloc(16);
        write_pattern(objects[count], 16, count);
        uintptr_t page = ROUNDDOWN((uintptr_t)objects[count], PGSIZE);
        if (count == 0) {
            first_page = page;
        }
        for (size_t j = 0; j < count; j++) {
            disjoint(objects[count], 16, objects[j], 16);
        }
        if (page != first_page) {
            count++;
            crossed = 1;
            break;
        }
        assert(nr_free_pages() == before - 1);
    }
    assert(crossed && count > 2 && nr_free_pages() == before - 2);
    size_t capacity = count - 1;
    // Fill the second page too, so the first-page hole is the ONLY free slot.
    assert(2 * capacity <= sizeof(objects) / sizeof(objects[0]));
    for (; count < 2 * capacity; count++) {
        objects[count] = slub_alloc(16);
        write_pattern(objects[count], 16, count);
        assert(ROUNDDOWN((uintptr_t)objects[count], PGSIZE) ==
               ROUNDDOWN((uintptr_t)objects[capacity], PGSIZE));
        for (size_t j = 0; j < count; j++) {
            disjoint(objects[count], 16, objects[j], 16);
        }
        assert(nr_free_pages() == before - 2);
    }
    void *hole = objects[1];
    slub_free(hole);
    assert(nr_free_pages() == before - 2);
    objects[1] = slub_alloc(16);
    assert(objects[1] == hole && nr_free_pages() == before - 2);
    write_pattern(objects[1], 16, 1);
    for (size_t i = 0; i < count; i++) {
        read_pattern(objects[i], 16, i);
    }
    // Return every object on the second page before freeing the first page.
    for (size_t i = count; i > capacity; i--) {
        slub_free(objects[i - 1]);
        assert(nr_free_pages() == before - (i == capacity + 1 ? 1 : 2));
    }
    assert(nr_free_pages() == before - 1);
    for (size_t i = capacity; i > 0; i--) {
        slub_free(objects[i - 1]);
    }
    assert(nr_free_pages() == before);
    cprintf("slub_test: same-page slots/reuse/second-page/reclaim passed (%lu slots)\n",
            capacity);
}

static void large_check(void) {
    size_t before = nr_free_pages();
    const size_t size = 2 * PGSIZE + 123;
    void *ptr = slub_alloc(size);
    write_pattern(ptr, size, 77);
    size_t offset = (uintptr_t)ptr % PGSIZE;
    size_t requested = (offset + size + PGSIZE - 1) / PGSIZE;
    assert(requested == 3);
    assert(nr_free_pages() == before - 4);
    // Independent Buddy metadata confirms original request, not rounded size.
    struct Page *page = pa2page(PADDR((void *)ROUNDDOWN((uintptr_t)ptr, PGSIZE)));
    assert(page->property == requested);
    read_pattern(ptr, size, 77); // Includes first, last and every crossed page.
    slub_free(ptr); // Buddy itself asserts that free's n matches the request.
    assert(nr_free_pages() == before);
    cprintf("slub_test: large object request=3 pages, Buddy charge=4, reclaim passed\n");
}

static void failure_check(void) {
    size_t before = nr_free_pages();
    assert(slub_alloc(0) == NULL);
    assert(slub_alloc(~(size_t)0) == NULL);
    size_t window = PHYSICAL_MEMORY_END - DRAM_BASE;
    assert(slub_alloc(window) == NULL); // Header makes this exceed max pages.
    // Supported request of 32768 pages, but RAM reserved for kernel prevents it.
    assert(slub_alloc(window - PGSIZE) == NULL);
    slub_free(NULL);
    assert(nr_free_pages() == before);
    cprintf("slub_test: zero/overflow/unsupported/unavailable/NULL-free passed\n");
}

void slub_check(void) {
    size_t before = nr_free_pages();
    cprintf("slub_check: baseline free pages %lu\n", before);
    boundary_check();
    slab_reuse_check();
    large_check();
    failure_check();
    for (unsigned cycle = 0; cycle < 8; cycle++) {
        objects[0] = slub_alloc(17);
        objects[1] = slub_alloc(17);
        objects[2] = slub_alloc(3 * PGSIZE);
        write_pattern(objects[0], 17, cycle);
        write_pattern(objects[1], 17, cycle + 1);
        write_pattern(objects[2], 3 * PGSIZE, cycle + 2);
        disjoint(objects[0], 17, objects[1], 17);
        read_pattern(objects[0], 17, cycle);
        read_pattern(objects[1], 17, cycle + 1);
        read_pattern(objects[2], 3 * PGSIZE, cycle + 2);
        slub_free(objects[1]);
        slub_free(objects[2]);
        slub_free(objects[0]);
        assert(nr_free_pages() == before);
    }
    assert(nr_free_pages() == before);
    cprintf("slub_test: 8 mixed allocation/free cycles passed\n");
    cprintf("slub_check: behavioral tests passed, free pages %lu -> %lu\n",
            before, nr_free_pages());
}
