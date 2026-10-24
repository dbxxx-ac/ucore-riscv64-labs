#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__
#include <defs.h>

// Single-threaded boot-time allocator; no interrupt/concurrent use.
// Initialize after pmm_init(). Repeated initialization preserves allocations.
void slub_init(void);
// Returns uninitialized storage aligned to 16 bytes. NULL for zero size,
// overflow, unsupported size, uninitialized allocator or page exhaustion.
void *slub_alloc(size_t size);
// NULL is a no-op. Otherwise requires a live, exact slub_alloc return value.
// Invalid/interior/double frees are programming errors, not recoverable errors.
void slub_free(void *ptr);
// Independent boot-time behavior tests; releases all test allocations.
void slub_check(void);
#endif
