#ifndef UTILS_H
#define UTILS_H

#include "data_structure.h"
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>

/*
    ------ UTILITY FUNCTIONS USED IN THE PROGRAM -------- 
    Here some utility functions used in the algorithms
    and functions are defined. They can be divided into 3 main categories:
    - List manipulation
    - Block manipulation (all of them performed by bitmask operations)
    - Footer related
    - Gap check utilities
*/

// -------- Block manipulation utilities -----------

static inline size_t get_size(Block *b) {
    //The operation is an AND between the header and a
    // literal that represents a negated 3 (unsigned long)
    return b->header & ~3UL;
}

static inline bool is_used(Block *b) {
    //The is_used flag is in the last bit of the header,
    // the operation is performed between the header and the literal 1
    return b->header & 1;
}

static inline void set_size(Block *b, size_t size) {
    //On the left side of the OR, the size is taken and the last three bits
    // are set to 0. On the right side of the OR, only the last 3 bits
    // (the flags) of the header are taken. The OR operation
    // merges the clean size with the clean flags.
    b->header = (size & SIZE_MASK) | (b->header & 3);
}

static inline void set_used(Block *b, bool used) {
    if (used) {
        b->header |= 1;
    } else {
        b->header &= ~1UL;
    }
}

static inline void set_header(Block *b, size_t size, bool used) {
    // Similar to set_size() with the difference that the last flag
    // is chosen on the spot
    b->header = (size & SIZE_MASK) | (used ? 1 : 0);
}

// -------- Gap checking utility -----------

static inline bool is_in_gap(void *addr) {
    if (gap_start == NULL || gap_end == NULL) {
        return false;  // No gap exists yet
    }
    unsigned char *ptr = (unsigned char *)addr;
    return (ptr >= gap_start && ptr < gap_end);
}

// Check if a block is in a valid heap region
static inline bool is_valid_heap_address(void *addr) {
    unsigned char *ptr = (unsigned char *)addr;
    
    // Must be within overall heap bounds
    if (ptr < (unsigned char *)heap_start || ptr >= heap_top) {
        return false;
    }
    
    if (is_in_gap(ptr)) {
        return false;
    }
    
    return true;
}

// ----------- List manipulation utilities ---------

// Get the appropriate bucket in the segregated list based on the size of the data
static inline int get_list_index(size_t size) {
    if (size <= 32) return 0;
    if (size <= 64) return 1;
    if (size <= 128) return 2;
    if (size <= 256) return 3;
    if (size <= 512) return 4;
    return 5; // > 512
}

static void remove_from_free_list(Block *block) {
    //If the block has a predecessor, the next of the predecessor
    // becomes the next of the current block
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        //If there is no predecessor, it means that the block
        // is the head of the list, so the next of the block becomes
        // the new head
        int idx = get_list_index(get_size(block));
        segregatedLists[idx] = block->next_free;
    }

    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    }

    // Clean the pointers
    block->next_free = NULL;
    block->prev_free = NULL;
}

static void insert_into_free_list(Block *block) {
    int idx = get_list_index(get_size(block));
    
    // Insert the block at the front of the list
    block->next_free = segregatedLists[idx];
    block->prev_free = NULL;
    
    //If the new block is not the first one to be placed into
    // the list, the predecessor of the former head becomes the new block
    if (segregatedLists[idx] != NULL) {
        segregatedLists[idx]->prev_free = block;
    }
    
    // The new block becomes the head of the list
    segregatedLists[idx] = block;
}

// ------------- Footer related utilities ---------------------

static inline Footer* get_footer(Block* b) {
    //To get the footer address, we first need to cast the block
    // as a byte pointer so arithmetic operations will be performed
    // byte by byte. Then, to get the footer, we just need to subtract
    // the size of the footer from the total size (which includes Header + Payload + Footer).
    return (Footer*)((unsigned char*)b + get_size(b) - sizeof(Footer));
}

static inline Block* get_prev_physical_block(Block* b) {
    Footer* prev_footer = (Footer*)((unsigned char*)b - sizeof(Footer));
    
    size_t prev_size = *prev_footer & SIZE_MASK;
    
    //Return the header of the block by subtracting the entire block size
    return (Block*)((unsigned char*)b - prev_size);
}

static inline Block* get_next_physical_block(Block* b) {
    return (Block*)((unsigned char*)b + get_size(b));
}

static inline Block* get_block_from_payload(void *ptr) {
    //offsetof is a macro that calculates the offset between
    // the start of the struct and the given member in memory.
    //Then, the pointer to the start of the payload is subtracted 
    // by the offset (in this implementation and on a 64-bit machine,
    // it will always be 8). Thanks to this, we can get the start address of the
    // block.
    return (Block*)((unsigned char*)ptr - offsetof(Block, payload));
}

// ----------------------------------------------------

// Set up the header and footer of the block
static inline void setup_block(Block* b, size_t size, bool used) {
    set_header(b, size, used);
    Footer* f = get_footer(b);
    *f = b->header;
}

// Get the current machine page size using the sysconf system call
static inline long get_page_size() {
    static long page_size = 0;
    if (page_size == 0) {
        page_size = sysconf(_SC_PAGESIZE);
        if (page_size < 0) {
            // Fallback in case of error
            return 4096;
        }
    }
    return page_size;
}

#endif