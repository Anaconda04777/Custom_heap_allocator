#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#include <stdint.h>
#include <stddef.h>

/*
    -------- DATA STRUCTURES USED IN THE DYNAMIC ALLOCATOR ----------

    To allocate memory dynamically, we use a hybrid strategy that combines
    3 different methods:
        1. Allocation in the memory space of an array that represents our heap
        2. sbrk used only to reserve more space for the static heap
        3. mmap that handles larger allocations
    
    Additionally, we use a segregated list to keep track of the 
    free blocks.
    To implement the allocator this way, we need 3 main data structures:
        - Block: it's the base of our implementation, we use it to allocate
        data in the heap. Each data item is stored in a block which resides
        in the heap. In this implementation, a block has 5 attributes:
        size, is_used flag, pointers to the previous and next block in the 
        doubly linked list (for the segregated list), and footer.
        As we can see, the actual object, as defined, has only the header 
        and pointers for the doubly linked list.
        The reason is that the size and is_used flag are compressed in 
        the header through a bitmask in which the last three bits are
        used as flags to indicate whether the block is in use and whether 
        the block was allocated with mmap.
        Furthermore, the block uses a special C data structure called union:
        the union allows storing two different data types in the same memory space,
        overlapping them. When a block is free (i.e., it doesn't store any data, 
        and therefore has to be placed in the segregated list), the memory zone will store
        the two pointers needed by the segregated list (which, remember, is a doubly
        linked list). When the block contains some data, it will just store
        the payload in that area (a pointer to the memory area where the data are stored).
        The size of this section is 16 bytes (8 + 8) because the compiler reserves
        space based on the largest data type it could store.
        Finally, we have the footer, which as we can see is not stored in the struct because 
        we cannot know in advance where it will be stored in memory (given that it's after the payload).
        Its purpose is to make it easy to find the previous adjacent block's header
        in the heap. We need this information to implement coalescing in O(1).

                    |-----------------------------------|
                    |           header (8 bytes)        |  
                    |-----------------------------------|
                    |                                   |
                    |                                   | 
                    |              payload              | 
                    |               or                  |
                    |        pointer next/prev          |
                    |                                   |
                    |-----------------------------------|
                    |           footer (8 bytes)        |  <-- It doesn't belong to the struct, 
                    |                                   |        but in memory is next to it
                    |-----------------------------------|
        - Heap: is an array of bytes that represents the address space of our
        heap. To operate on the heap, we need to know at which address it starts (heap_start),
        the current top (heap_top) since we need to know where the unallocated memory starts,
        and at which address it ends (heap_end) since we need to know when to extend the heap space
        through sbrk.
        - Segregated list: it's an array of doubly linked lists that keeps track of the current
        free blocks. The purpose of this method is to make searching through the
        free blocks more efficient, to find one of the right size for allocation.
*/

// Heap starts with 4 KB of memory
#define HEAP_TOTAL_SIZE 4096
// Number of buckets for the segregated list
#define NUM_LISTS 6
// Threshold to use mmap instead of the heap (in this case 128KB)
#define MMAP_THRESHOLD (128 * 1024)

// Define the size of a word and the size of a header
// to make the code clearer
typedef intptr_t word_t;
typedef size_t Footer;

// Mask to get just the last three bits from the header.
// It works in the following way: it gets the size of a word from
// the system (on 64-bit it is 8 bytes), then subtracts 1
// (so it will be 7), then converts the integer to binary 
// (0x07 = 0000...0111) and then negates it (so,
// = 1111...1000). With this mask, we can easily eliminate 
// the last 3 bits of a header to get just the size.
#define SIZE_MASK (~(sizeof(word_t) - 1))

// Definition of the Block structure
typedef struct Block {
    size_t header; 

    union {
        struct {
            struct Block *next_free;
            struct Block *prev_free;
        };
        unsigned char payload[0]; 
    };
} Block;

// Definition of the heap, which is an array of bytes (unsigned char)
static unsigned char heap[HEAP_TOTAL_SIZE];

// Pointers to handle the heap

// Points to the top of the allocated portion of the heap
static unsigned char *heap_top = heap; 
//Pointer to the end of the heap array
static unsigned char *heap_end = heap + HEAP_TOTAL_SIZE;

// Pointer to the first block, which starts at the beginning of the heap
static Block* heap_start = (Block *)heap;

// Array of segregated free lists
static Block *segregatedLists[NUM_LISTS] = { NULL };

#endif