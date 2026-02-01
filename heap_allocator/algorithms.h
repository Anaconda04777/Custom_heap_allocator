#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <stddef.h>
#include <stdbool.h>
#include "data_structure.h"
#include "utils.h"
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>


/*
    -------------- ALGORITHMS USED BY THE ALLOCATOR -------------
    In this header, all the algorithms used by my_malloc and my_free are defined.
    The algorithms are: align, coalesce, first_fit, split_block, and sbrk_allocation.
    
    - Align: aligns the block of memory based on the hardware architecture. In short,
    it adds padding to the size of the block so that it will be a multiple of 
    the machine word (on a x64 machine, they will be multiples of 8). By doing so, not only
    is aligned memory access more efficient, but we are also sure that the last 3 least
    significant bits are always free (since the number will be a multiple of 8).
    
    - Coalesce: is a technique that reduces memory fragmentation.
    Basically, the algorithm works by merging two adjacent blocks when we are freeing
    one of them.
    
    - First Fit: is the policy chosen to find an already-created block in the heap
    when new data is allocated. Among the search policies, first-fit is the simplest one.

    - Split Block: splits the block into two different blocks, one of the exact size
    that the allocation needs, and the other of the remaining size. This simple technique helps
    avoid internal fragmentation caused by first-fit when it chooses a block much larger 
    than what the allocation needs.

    - Sbrk Allocation: it's the algorithm that allows extension of our heap memory.
    The problem with allocating this way is that most of the time the memory
    reserved by sbrk will not be contiguous with our already-allocated heap. Therefore,
    the algorithm fills the gap by creating a new block between the last allocated block 
    in the heap and the new address.
    Note that since the heap is allocated in the BSS section, the new address returned by
    sbrk will always be higher than the end of the heap. This happens because the sbrk system call
    manages a kernel-level pointer called "program break" which indicates the end of the data
    zone managed by the operating system. When we call the sbrk() syscall, the program break
    will always grow toward higher addresses.

    Higher addresses   +-------------------+
                       |       Stack       |
                       |        ...        |
                       |        | |        |
                       |        V V        |
                       +-------------------+
                       |                   |
                       | (Free space)      |
                       |                   |
                       +-------------------+
                       |        ^ ^        |
                       |        | |        |
                       |        ...        |
                       |       Heap        | <-- sbrk allocates here
                       +-------------------+
                       | BSS               | <-- The allocated heap array is here
                       +-------------------+
                       | Data              |
                       +-------------------+
                       | Text (code)       |
      Lower addresses  +-------------------+

*/


// Align the block of memory based on the hardware architecture with a bit operation:
// This method ensures rounding up the number to the next closest multiple of the word size
static inline size_t align(size_t n) {
    return (n + sizeof(word_t) - 1) & ~(sizeof(word_t) - 1);
}


static Block* coalesce(Block* block) {
    /*
    Step 1) Check if the adjacent blocks are valid and not in use.
        Besides checking if the blocks are not in use, we have to verify that the 
        next block we are calculating is valid (i.e., it doesn't extend
        beyond the current heap or before it). Since the heap array is stored
        in the BSS section, it can happen that after the heap is extended and the program
        break has moved forward, some data from other BSS-allocated variables can be present
        between the old heap and the new space. In this case, the physical calculation of
        the blocks through the footer will fail if we don't properly validate
        the memory zone we calculated.
    */
    // ---- Check and get the NEXT block ----
    Block *next_block = get_next_physical_block(block);

    bool next_is_free = false;

    // Check if next block is valid:
    // 1. Must be within heap bounds (before heap_top)
    // 2. Must NOT be in the gap between static heap and sbrk memory
    // 3. Must not be used
    if (is_valid_heap_address(next_block)) {
        next_is_free = !is_used(next_block);
    }

    // ---- Check and get the PREVIOUS block ----
    Block *prev_block = NULL;
    bool prev_is_free = false;

    //Check if we're not at the start of the static heap or at the start of the
    //  new allocated memory by sbrk
    // We need to check both: not at heap_start AND not at gap_end (start of sbrk region)
    bool at_region_start = ((unsigned char*)block == (unsigned char*)heap_start) ||
                           (gap_end != NULL && (unsigned char*)block == gap_end);

    if (!at_region_start) {
        Footer* prev_footer_addr = (Footer*)((unsigned char*)block - sizeof(Footer));
        
        // Check also if the footer is in a valid heap memory (not in the gap)
        if (is_valid_heap_address(prev_footer_addr)) {
            
            prev_block = get_prev_physical_block(block);
            
            // Verify the previous block is also in valid memory and not used
            if (is_valid_heap_address(prev_block) && !is_used(prev_block)) {
                prev_is_free = true;
            }
        }
    }

    // Step 2) Generate the new block
    size_t new_size = get_size(block);

    // --- Case 1: Merge with next ---
    if (next_is_free) {
        remove_from_free_list(next_block);
        // Increase the size of the current block by the size of the next block 
        new_size += get_size(next_block);
    }

    // --- Case 2: Merge with previous ---
    if (prev_is_free) {
        remove_from_free_list(prev_block);
        // Increase the size of the current block by the size of the previous block 
        new_size += get_size(prev_block);
        // The address of the new block is the one of the previous block since
        // it comes first in terms of addresses
        block = prev_block; 
    }
    
    setup_block(block, new_size, false);

    return block;
}

static Block* first_fit(size_t size) {
    int start_idx = get_list_index(size);
    
    // Because of splitting and coalescing, blocks can 
    // "migrate" across different lists. For this reason, we
    // should iterate through all the lists after the target one

    for (int i = start_idx; i < NUM_LISTS; i++) {
        Block *current = segregatedLists[i];
        
        while (current != NULL) {
            // Return a block of sufficient size
            if (get_size(current) >= size) {
                return current;
            }
            current = current->next_free;
        }
    }
    
    return NULL;
}

static void split_block(Block *block, size_t needed_size) {
    size_t current_size = get_size(block);
    size_t min_block_size = sizeof(Block) + sizeof(Footer);
    
    // Check if the current size of the block we are analyzing 
    // is sufficient to contain the needed space for the data
    // we want to allocate + the basic information that a block needs.
    // Note: we also handle the edge case where 
    // current_size == needed_size + min_block_size. We do this
    // to avoid creating blocks of size == min_block_size, 
    // which would lead to internal fragmentation.
    if (current_size >= needed_size + min_block_size) {
        
        setup_block(block, needed_size, true);
        
        // Create a new block with the remaining space.
        // First, we calculate where the second block starts.
        // It will start at the end of the new block.
        Block *new_block = (Block*)((unsigned char*)block + needed_size);
        size_t new_size = current_size - needed_size;
        
        setup_block(new_block, new_size, false);
        
        insert_into_free_list(new_block);
    }
}

static void* sbrk_allocation(size_t total_size) {
    /* Step 1) Calculate how much to enlarge the heap
                Since sbrk works with pages, we will calculate 
                how many pages we need to enlarge our heap 
    */  
    // Calculate page size depending on the hardware
    size_t page_size = (size_t)get_page_size();

    // If the amount to add to the heap is less than a page, it will
    // enlarge by the size of a single page
    size_t size_to_alloc = total_size;
    if (size_to_alloc < page_size) {
        size_to_alloc = page_size;
    }
    
    // Round up to a multiple of a page using the technique also used in the align algorithm
    size_t sbrk_size = (size_to_alloc + page_size - 1) & ~(page_size - 1);

    // Step 2) Call sbrk
    void *request = sbrk(sbrk_size);
    if (request == (void*)-1) {
        return NULL; // if the request is -1, we have an Out Of Memory error
    }

    /* Step 3) There may be a hole between the current heap size and the program break
               set by sbrk. This can happen when some other data are stored in the BSS
               section after the end of the heap, and therefore there would be a gap
               between heap_top and the returned sbrk address.
    */                     
    if (request != heap_end) {
        //First call to extend the memory will probably have a gap
        assert(gap_start == NULL);
        //Create a free block with remaining static heap space if possible
        size_t remaining = heap_end - heap_top;
        size_t needed_for_free_block = sizeof(Block) + sizeof(Footer);
        
        if (remaining >= needed_for_free_block) {
            Block *rest = (Block*)heap_top;
            setup_block(rest, remaining, false);
            insert_into_free_list(rest);
            
            //Gap starts after this free block
            gap_start = heap_end;
        } else {
            //Not enough space for a block, gap starts at heap_top
            gap_start = heap_top;
        }
        
        gap_end = (unsigned char *)request;

        //Move the heap pointers to fit the new memory space
        heap_top = (unsigned char *)request;
        heap_end = (unsigned char *)request + sbrk_size;
    } else {
        //Memory is contiguous, just extend heap_end
        heap_end += sbrk_size;
    }

    // When we finish setting up the new space, we can allocate a new
    // block in it to store the requested data.
    Block* block = (Block*)heap_top;
    
    setup_block(block, total_size, true);

    heap_top += total_size;
    
    return (void*)block->payload;
}

#endif