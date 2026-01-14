#ifndef HEAP_ALLOCATOR_H
#define HEAP_ALLOCATOR_H

#include <stddef.h>
#include <stdbool.h>
#include "data_structure.h"
#include "utils.h"
#include "algorithms.h"
#include "mmap_allocator.h"

/*
    --------------- CUSTOM MALLOC AND FREE ----------------
    
    In this header, the two main functions that perform dynamic
    allocation are defined.

    - Malloc: it offers 3 ways to allocate data
        1. Standard allocation in a static heap:
            When the program starts, the heap offered to it
            is 4KB. Allocation in a static heap is more 
            efficient than using sbrk. 
            The blocks are created in the memory space of the
            static heap. Deallocated blocks are reused and chosen through
            the first-fit policy.
        2. Sbrk allocation: if the space in the heap runs out, the allocator
            uses the sbrk syscall to map more space in the process memory. 
            The heap memory is then extended and can be enlarged further
            through another sbrk allocation.
        3. Mmap allocation: if the data to allocate exceeds a certain threshold, 
            the allocator uses the mmap syscall to handle the large block independently.
    
    - Free: deallocates the data from the given blocks. To avoid external fragmentation,
        a coalesce operation is performed to merge two consecutive free blocks.
        If the block was allocated with mmap, it's deallocated with munmap.
*/

// Allocates data in dynamic memory
void* my_malloc(size_t size) {
    if (size == 0) return NULL;
    
    // Get the aligned size of the data
    size_t aligned_size = align(size);
    // Calculate total size which is Header + Payload + Footer
    size_t total_size = sizeof(size_t) + aligned_size + sizeof(size_t);
    
    //Note: size of block includes also the minimum space reserved for the pointers
    size_t min_block_size = sizeof(Block) + sizeof(size_t); 
    // The total size can't be less than the minimum size (header + footer)
    if (total_size < min_block_size) total_size = min_block_size;

    // ------------- (3) Mmap allocation --------------
    if (aligned_size >= MMAP_THRESHOLD) {
        return mmap_allocation(aligned_size);
    }

    // ------------- (1) Standard allocation ------------

    // A free appropriate block is found through first-fit policy
    Block *block = first_fit(total_size);
    
    // If a block was found...
    if (block != NULL) {
        remove_from_free_list(block);
        
        // Splitting of the block is performed to avoid internal fragmentation
        split_block(block, total_size);
        
        set_used(block, true);
        
        // Footer is updated
        Footer *footer = get_footer(block);
        *footer = block->header;

        return (void*)block->payload; 
    }

    // If there are no blocks available for that data, a new block is created
    // on top of the heap
    if (heap_top + total_size <= heap_end) {
        block = (Block*)heap_top;
        
        setup_block(block, total_size, true);

        // The top of the heap is moved
        heap_top += total_size;
        
        return (void*)block->payload;
    }

    // ------------ (2) Sbrk allocation ---------------
    // The payload pointer is directly returned from the allocation
    //printf("Start enlarge memory\n\n");
    return sbrk_allocation(total_size);
}

// Deallocates dynamic memory
void my_free(void* ptr) {
    if (!ptr) return;

    // Get the block from the payload
    Block *block = get_block_from_payload(ptr);
    
    // If the block was allocated with mmap, munmap is performed
    if (is_mmap(block)) {
        mmap_free(block);
        return;
    }
    
    set_used(block, false);
    
    // Update the footer before coalescing
    Footer *footer = get_footer(block);
    *footer = block->header;

    // Coalesce is performed and the new merged block is returned 
    block = coalesce(block); 

    // The new free block is inserted into the segregated list
    insert_into_free_list(block);
}

#endif