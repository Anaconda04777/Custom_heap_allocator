#ifndef MMAP_ALLOCATOR
#define MMAP_ALLOCATOR

#include "utils.h"
#include <sys/mman.h>

/*
    ------- FUNCTIONS THAT IMPLEMENT MMAP ALLOCATION ----------

    This header implements the functions needed to perform mmap
    allocation. This type of allocation is independent from the
    main mechanism and is performed only when larger allocations are
    requested.
*/

// Flag that indicates the mmap flag in the header,
// used to perform bitwise operations
#define MMAP_FLAG 2

// Check if a block is allocated with mmap using a bitwise operation
static inline bool is_mmap(Block *b) {
    return b->header & MMAP_FLAG;
}

// Set the block's mmap flag using a bitwise operation
static inline void set_mmap(Block *b, bool mmap_flag) {
    if (mmap_flag) {
        b->header |= MMAP_FLAG;
    } else {
        b->header &= ~MMAP_FLAG;
    }
}

static void* mmap_allocation(size_t size) {
    size_t page_size = (size_t)get_page_size();
    // Calculate total size (Header + payload). The footer is not
    // needed with mmap.
    size_t total_size = sizeof(Block) + size;
    
    // Round up the size of the block to the next page size multiple
    size_t mmap_size = (total_size + page_size - 1) & ~(page_size - 1);
    
    /*
        Allocation of the data through mmap
        - void *addr -> NULL: the OS chooses where to store the data
        - size_t length -> mmap_size: length of the memory block
        - int prot -> PROT_READ | PROT_WRITE: the program can read and write
            in this space of memory
        - int flags -> MAP_PRIVATE | MAP_ANONYMOUS: the memory is private to the
            process itself, which means that a child created with fork will copy
            it and it will not be shared. The MAP_ANONYMOUS flag indicates
            not to map to a file.
    */
    void *ptr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, 
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (ptr == MAP_FAILED) {
        return NULL;
    }
    
    Block *block = (Block*)ptr;
    
    // Set the block with the mmap flag true and the is_used flag true
    block->header = (mmap_size & SIZE_MASK) | 1 | MMAP_FLAG;
    
    return (void*)block->payload;
}

// Deallocate the block allocated with mmap
static void mmap_free(Block *block) {
    size_t size = get_size(block);
    munmap(block, size);
}

#endif