#ifndef DEBUG_UTILITIES_H
#define DEBUG_UTILITIES_H

#include "data_structure.h"
#include "utils.h"
#include "mmap_allocator.h"

/* ------------- DEBUG UTILITY ---------------------
    Includes functions useful to analyze and debug the allocator.

    - Print memory: print the current state of the allocated blocks
    and information about memory spaces (static heap, mmap, ecc.).
    The function print memory state in this way:
    1. Heap:
        a. Heap pointers info
        b. Gap info between static heap and sbrk memory info
        c. Blocks in static memory
        d. Blocks allocated in sbrk extended area
    2. Mmap allocated blocks
    3. Segregated free lists
*/

static void print_memory() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                      MEMORY STATE DUMP                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════════╝\n\n");
    
    // Print heap pointers info
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ HEAP POINTERS                                                   │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│ heap_start: %p                                      │\n", (void*)heap_start);
    printf("│ heap_top:   %p                                      │\n", (void*)heap_top);
    printf("│ heap_end:   %p                                      │\n", (void*)heap_end);
    printf("│ Static heap size: %zu bytes                                    │\n", (size_t)HEAP_TOTAL_SIZE);
    printf("│ Used in static heap: %ld bytes                                  │\n", 
           (long)(heap_top - (unsigned char*)heap_start));
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    // Print gap info
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ GAP INFO (between static heap and sbrk memory)                  │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    if (gap_start != NULL && gap_end != NULL) {
        printf("│ gap_start:  %p                                      │\n", (void*)gap_start);
        printf("│ gap_end:    %p                                      │\n", (void*)gap_end);
        printf("│ Gap size:   %ld bytes                                        │\n", 
               (long)(gap_end - gap_start));
    } else {
        printf("│ No gap exists (sbrk not used or memory is contiguous)         │\n");
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    // Print blocks in static heap
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ BLOCKS IN MEMORY                                                │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    
    unsigned char *current = (unsigned char*)heap_start;
    int block_num = 0;
    
    // Determine the end of the static heap region
    unsigned char *static_heap_end = (gap_start != NULL) ? gap_start : heap_top;
    
    // If heap_top is still within static heap bounds
    if (heap_top <= heap + HEAP_TOTAL_SIZE) {
        static_heap_end = heap_top;
    }
    
    printf("│                                                                 │\n");
    printf("│ === STATIC HEAP REGION ===                                      │\n");
    
    // Traverse static heap
    while (current < static_heap_end && current < heap + HEAP_TOTAL_SIZE) {
        Block *block = (Block*)current;
        size_t size = get_size(block);
        
        if (size == 0) {
            printf("│ [!] Invalid block at %p (size=0), stopping traversal    │\n", (void*)block);
            break;
        }
        
        bool used = is_used(block);
        Footer *footer = get_footer(block);
        size_t payload_size = size - sizeof(size_t) - sizeof(Footer);
        
        printf("│ Block #%d:                                                      │\n", block_num);
        printf("│   Address:      %p                                  │\n", (void*)block);
        printf("│   Total size:   %zu bytes                                      │\n", size);
        printf("│   Payload size: %zu bytes                                      │\n", payload_size);
        printf("│   Status:       %s                                          │\n", used ? "USED" : "FREE");
        printf("│   Header:       0x%lx                                          │\n", (unsigned long)block->header);
        printf("│   Footer:       0x%lx                                          │\n", (unsigned long)*footer);
        printf("│   Payload addr: %p                                  │\n", (void*)block->payload);
        
        if (!used) {
            printf("│   next_free:    %p                                  │\n", (void*)block->next_free);
            printf("│   prev_free:    %p                                  │\n", (void*)block->prev_free);
        }
        printf("│                                                                 │\n");
        
        current += size;
        block_num++;
    }
    
    // Print gap visualization if exists
    if (gap_start != NULL && gap_end != NULL) {
        printf("│ === MEMORY GAP ===                                              │\n");
        printf("│   From: %p                                          │\n", (void*)gap_start);
        printf("│   To:   %p                                          │\n", (void*)gap_end);
        printf("│   Size: %ld bytes (UNUSABLE)                                 │\n", 
               (long)(gap_end - gap_start));
        printf("│                                                                 │\n");
        
        // Traverse sbrk-allocated region
        printf("│ === SBRK ALLOCATED REGION ===                                   │\n");
        
        current = gap_end;
        while (current < heap_top) {
            Block *block = (Block*)current;
            size_t size = get_size(block);
            
            if (size == 0) {
                printf("│ [!] Invalid block at %p (size=0), stopping traversal    │\n", (void*)block);
                break;
            }
            
            bool used = is_used(block);
            Footer *footer = get_footer(block);
            size_t payload_size = size - sizeof(size_t) - sizeof(Footer);
            
            printf("│ Block #%d (sbrk):                                              │\n", block_num);
            printf("│   Address:      %p                                  │\n", (void*)block);
            printf("│   Total size:   %zu bytes                                      │\n", size);
            printf("│   Payload size: %zu bytes                                      │\n", payload_size);
            printf("│   Status:       %s                                          │\n", used ? "USED" : "FREE");
            printf("│   Header:       0x%lx                                          │\n", (unsigned long)block->header);
            printf("│   Footer:       0x%lx                                          │\n", (unsigned long)*footer);
            printf("│   Payload addr: %p                                  │\n", (void*)block->payload);
            
            if (!used) {
                printf("│   next_free:    %p                                  │\n", (void*)block->next_free);
                printf("│   prev_free:    %p                                  │\n", (void*)block->prev_free);
            }
            printf("│                                                                 │\n");
            
            current += size;
            block_num++;
        }
    }
    
    printf("│ Total blocks: %d                                                 │\n", block_num);
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    // Print mmap allocated blocks
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ MMAP ALLOCATED BLOCKS                                           │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");

    // mmap blocks do NOT live inside the custom heap regions, so we print
    // the tracked list built in mmap_allocator.h
    int mmap_count = 0;
    MmapTrackNode *mcur = mmap_tracked_head;
    while (mcur != NULL) {
        Block *block = mcur->block;
        size_t size = get_size(block);

        printf("│ Mmap Block #%d:                                                  │\n", mmap_count);
        printf("│   Address:      %p                                  │\n", (void*)block);
        printf("│   Mapped size:  %zu bytes                                      │\n", size);
        printf("│   Status:       %s                                          │\n",
               is_used(block) ? "USED" : "FREE");
        printf("│   Header:       0x%lx                                          │\n", (unsigned long)block->header);
        printf("│   Payload addr: %p                                  │\n", (void*)block->payload);
        printf("│                                                                 │\n");

        mmap_count++;
        mcur = mcur->next;
    }

    if (mmap_count == 0) {
        printf("│ No mmap blocks allocated                                        │\n");
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
    
    // Print segregated free lists
    printf("┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ SEGREGATED FREE LISTS                                           │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    
    const char *list_ranges[] = {
        "0-32 bytes",
        "33-64 bytes", 
        "65-128 bytes",
        "129-256 bytes",
        "257-512 bytes",
        ">512 bytes"
    };
    
    for (int i = 0; i < NUM_LISTS; i++) {
        printf("│ List[%d] (%s):                                       │\n", i, list_ranges[i]);
        
        Block *curr = segregatedLists[i];
        if (curr == NULL) {
            printf("│   (empty)                                                       │\n");
        } else {
            int count = 0;
            while (curr != NULL && count < 10) {  // Limit to prevent infinite loops
                printf("│   -> %p (size: %zu)                             │\n", 
                       (void*)curr, get_size(curr));
                curr = curr->next_free;
                count++;
            }
            if (curr != NULL) {
                printf("│   ... (more blocks)                                             │\n");
            }
        }
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n\n");
}

#endif