#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include <stddef.h>
#include <stdbool.h>
#include "data_structure.h"
#include "utils.h"
#include <unistd.h>
#include <sys/mman.h>


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
    //Check if it's valid: if the next block crosses the boundaries of the allocated heap, 
    // it would definitely be invalid. We check against heap_top because
    // we only want to coalesce with blocks that are actually allocated/in use.
    bool next_is_free = (unsigned char*)next_block < heap_top && !is_used(next_block);

    // ---- Check and get the PREVIOUS block ----
    Block *prev_block = NULL;
    bool prev_is_free = false;

    //To avoid errors from accessing memory zones that could lead to a crash, we first
    // should check if the current block that we are freeing is not the first of the heap
    if (block != heap_start) {
        //Now we check if the address of the footer is valid. This is done to make sure
        // we are not taking the previous block from a zone between the old heap and
        // the new space allocated by sbrk
        Footer* prev_footer_addr = (Footer*)((unsigned char*)block - sizeof(Footer));

        //If the address of the footer is in the heap memory space, then it's a valid footer 
        if ((unsigned char*)prev_footer_addr >= (unsigned char*)heap_start) {
            prev_block = get_prev_physical_block(block);
            
            //Then we check if the entire block is valid and not in use
            if ((unsigned char*)prev_block >= (unsigned char*)heap_start && !is_used(prev_block)) {
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
               set by sbrk. This can happen when the heap is not completely filled
               and therefore there would be a gap between heap_top and the returned sbrk address.
               In this case, we have to create a block of the size of the hole.
    */                     
    if (request != heap_end) {        
        // Recover the remaining space between the current used heap and the end of the
        // previously allocated heap
        size_t remaining = heap_end - heap_top;
        size_t needed_for_free_block = sizeof(Block) + sizeof(Footer);
        
        // If there is enough space for a block, we allocate a block equal to the gap
        if (remaining >= needed_for_free_block) {
            Block *rest = (Block*)heap_top;
            
            setup_block(rest, remaining, false);

            insert_into_free_list(rest);
        }

        // The heap pointers are moved to fit the new memory space
        heap_top = (unsigned char *)request;
        // Note: sbrk returns the first address that can be used to allocate.
        // Because of this, we have to move the end by the size that we 
        // allocated with sbrk.
        heap_end = (unsigned char *)request + sbrk_size;
    } else {
        // In the case where the address that sbrk returns is the same as the
        // end of the current heap, we just update the end of the heap.
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