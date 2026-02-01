#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include "heap_allocator.h"
#include "debug_utilities.h"

//gcc allocator.c -o allocator -Wall

/*
    ------ PARAMETRIC TESTS FOR THE ALLOCATOR --------
    Advanced test suite with parametric options for comprehensive allocator testing
    
    Available tests:
    - mmap_threshold: Test allocation size thresholds for mmap vs sbrk
    - alignment: Test memory alignment for various sizes
    - split_reuse: Test block splitting and reuse patterns
    - coalescing: Test merging of adjacent free blocks
    - fragmentation: Test allocator behavior under fragmentation patterns
    - stress_small: Stress test with many small allocations
    - large_blocks: Test multiple large allocations via mmap
    
    Usage:
        ./allocator <test1> [params...]
        ./allocator <test1> <test2> ...
    
    When specifying a single test, parameters can be customized.
    When specifying multiple tests, default parameters are used.
*/

/* ==================== PARAMETER DEFINITIONS ==================== */

typedef struct {
    size_t size_below_threshold;
    size_t size_at_threshold;
    size_t size_above_threshold;
} MmapThresholdParams;

typedef struct {
    size_t sizes[50];
    int num_sizes;
} AlignmentParams;

typedef struct {
    size_t initial_size;
    size_t realloc_size;
} SplitReuseParams;

typedef struct {
    size_t block_size;
    int num_blocks;
    int free_order;  // 0=FIFO, 1=LIFO, 2=alternating
} CoalescingParams;

typedef struct {
    size_t large_size;
    size_t small_size;
    size_t medium_size;
    int pattern_iterations;
} FragmentationParams;

typedef struct {
    size_t small_alloc_size;
    int num_allocations;
    int free_percentage;
} StressSmallParams;

typedef struct {
    size_t block_sizes[5];
    int num_blocks;
    int free_order;  // 0=FIFO, 1=LIFO, 2=random
} LargeBlocksParams;

/* ==================== DEFAULT PARAMETERS ==================== */

static bool verbose_mode = false;

MmapThresholdParams default_mmap_params = {
    .size_below_threshold = 64 * 1024,    // 64 KB
    .size_at_threshold = 128 * 1024,      // 128 KB (threshold)
    .size_above_threshold = 256 * 1024    // 256 KB
};

AlignmentParams default_alignment_params = {
    .sizes = {1, 2, 3, 7, 8, 15, 16, 24, 64, 256},
    .num_sizes = 10
};

SplitReuseParams default_split_params = {
    .initial_size = 256,
    .realloc_size = 96
};

CoalescingParams default_coalescing_params = {
    .block_size = 32,
    .num_blocks = 5,
    .free_order = 1  // LIFO
};

FragmentationParams default_fragmentation_params = {
    .large_size = 512,
    .small_size = 64,
    .medium_size = 256,
    .pattern_iterations = 10
};

StressSmallParams default_stress_params = {
    .small_alloc_size = 32,
    .num_allocations = 200,
    .free_percentage = 50
};

LargeBlocksParams default_large_blocks_params = {
    .block_sizes = {256 * 1024, 512 * 1024, 1024 * 1024, 2048 * 1024, 512 * 1024},
    .num_blocks = 5,
    .free_order = 1  // LIFO
};

/* ==================== HELPER FUNCTION ==================== */

void print_help() {
    printf("=== Allocator Parametric Test Suite ===\n\n");
    printf("USAGE:\n");
    printf("  ./allocator [test_name] [param1=value] [param2=value] ... [verbose]\n");
    printf("  ./allocator test1 test2 test3 ... [verbose] (uses defaults for all)\n\n");
    printf("Global option:\n");
    printf("  verbose   Print allocator memory state at key steps\n\n");
    
    printf("AVAILABLE TESTS:\n\n");
    
    printf("1. mmap_threshold\n");
    printf("   Tests allocation thresholds for mmap vs sbrk\n");
    printf("   Parameters:\n");
    printf("     size_below=<bytes>    (default: %zu)\n", default_mmap_params.size_below_threshold);
    printf("     size_at=<bytes>       (default: %zu)\n", default_mmap_params.size_at_threshold);
    printf("     size_above=<bytes>    (default: %zu)\n\n", default_mmap_params.size_above_threshold);
    
    printf("2. alignment\n");
    printf("   Tests memory alignment for various allocation sizes\n");
    printf("   Parameters:\n");
    printf("     sizes=<size1,size2,...>  (default: 1,2,3,7,8,15,16,24,64,256)\n");
    printf("     Example: ./allocator alignment sizes=4,10,32,57,100\n\n");
    
    printf("3. split_reuse\n");
    printf("   Tests block splitting and reuse patterns\n");
    printf("   Parameters:\n");
    printf("     initial=<bytes>       (default: %zu)\n", default_split_params.initial_size);
    printf("     realloc=<bytes>       (default: %zu)\n\n", default_split_params.realloc_size);
    
    printf("4. coalescing\n");
    printf("   Tests merging of adjacent free blocks\n");
    printf("   Parameters:\n");
    printf("     block_size=<bytes>    (default: %zu)\n", default_coalescing_params.block_size);
    printf("     num_blocks=<count>    (default: %d)\n", default_coalescing_params.num_blocks);
    printf("     order=<0|1|2>         (0=FIFO, 1=LIFO, 2=alternating, default: %d)\n\n", default_coalescing_params.free_order);
    
    printf("5. fragmentation\n");
    printf("   Tests allocator behavior under fragmentation patterns\n");
    printf("   Parameters:\n");
    printf("     large=<bytes>         (default: %zu)\n", default_fragmentation_params.large_size);
    printf("     small=<bytes>         (default: %zu)\n", default_fragmentation_params.small_size);
    printf("     medium=<bytes>        (default: %zu)\n", default_fragmentation_params.medium_size);
    printf("     iterations=<count>    (default: %d)\n\n", default_fragmentation_params.pattern_iterations);
    
    printf("6. stress_small\n");
    printf("   Stress test with many small allocations\n");
    printf("   Parameters:\n");
    printf("     size=<bytes>          (default: %zu)\n", default_stress_params.small_alloc_size);
    printf("     count=<number>        (default: %d)\n", default_stress_params.num_allocations);
    printf("     free_pct=<percent>    (default: %d%%)\n\n", default_stress_params.free_percentage);
    
    printf("7. large_blocks\n");
    printf("   Tests multiple large allocations via mmap\n");
    printf("   Parameters:\n");
    printf("     num=<count>           (default: %d)\n", default_large_blocks_params.num_blocks);
    printf("     order=<0|1|2>         (0=FIFO, 1=LIFO, 2=random, default: %d)\n\n", default_large_blocks_params.free_order);
    
    printf("EXAMPLES:\n");
    printf("  ./allocator mmap_threshold\n");
    printf("  ./allocator mmap_threshold size_below=32768 size_above=524288\n");
    printf("  ./allocator coalescing block_size=64 num_blocks=10\n");
    printf("  ./allocator mmap_threshold coalescing stress_small\n");
    printf("\n");
}

/* ==================== PARAMETER PARSING ==================== */

int is_test_name(const char *arg) {
    return strcmp(arg, "mmap_threshold") == 0 ||
           strcmp(arg, "alignment") == 0 ||
           strcmp(arg, "split_reuse") == 0 ||
           strcmp(arg, "coalescing") == 0 ||
           strcmp(arg, "fragmentation") == 0 ||
           strcmp(arg, "stress_small") == 0 ||
           strcmp(arg, "large_blocks") == 0;
}

int is_parameter(const char *arg) {
    return strchr(arg, '=') != NULL;
}

void parse_mmap_params(MmapThresholdParams *params, int argc, char *argv[], int start_idx, int *end_idx) {
    *end_idx = start_idx;
    for (int i = start_idx; i < argc; i++) {
        if (is_test_name(argv[i])) break;
        if (!is_parameter(argv[i])) break;
        
        char *key = strtok(argv[i], "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            if (strcmp(key, "size_below") == 0) {
                params->size_below_threshold = atol(value);
            } else if (strcmp(key, "size_at") == 0) {
                params->size_at_threshold = atol(value);
            } else if (strcmp(key, "size_above") == 0) {
                params->size_above_threshold = atol(value);
            }
        }
        *end_idx = i + 1;
    }
}

void parse_alignment_params(AlignmentParams *params, int argc, char *argv[], int start_idx, int *end_idx) {
    *end_idx = start_idx;
    for (int i = start_idx; i < argc; i++) {
        if (is_test_name(argv[i])) break;
        if (!is_parameter(argv[i])) break;
        
        char *key = strtok(argv[i], "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            if (strcmp(key, "sizes") == 0) {
                // Parse comma-separated list of sizes
                params->num_sizes = 0;
                char *token = strtok(value, ",");
                while (token != NULL && params->num_sizes < 50) {
                    params->sizes[params->num_sizes++] = atol(token);
                    token = strtok(NULL, ",");
                }
            }
        }
        *end_idx = i + 1;
    }
}

void parse_split_params(SplitReuseParams *params, int argc, char *argv[], int start_idx, int *end_idx) {
    *end_idx = start_idx;
    for (int i = start_idx; i < argc; i++) {
        if (is_test_name(argv[i])) break;
        if (!is_parameter(argv[i])) break;
        
        char *key = strtok(argv[i], "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            if (strcmp(key, "initial") == 0) {
                params->initial_size = atol(value);
            } else if (strcmp(key, "realloc") == 0) {
                params->realloc_size = atol(value);
            }
        }
        *end_idx = i + 1;
    }
}

void parse_coalescing_params(CoalescingParams *params, int argc, char *argv[], int start_idx, int *end_idx) {
    *end_idx = start_idx;
    for (int i = start_idx; i < argc; i++) {
        if (is_test_name(argv[i])) break;
        if (!is_parameter(argv[i])) break;
        
        char *key = strtok(argv[i], "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            if (strcmp(key, "block_size") == 0) {
                params->block_size = atol(value);
            } else if (strcmp(key, "num_blocks") == 0) {
                params->num_blocks = atoi(value);
            } else if (strcmp(key, "order") == 0) {
                params->free_order = atoi(value);
            }
        }
        *end_idx = i + 1;
    }
}

void parse_fragmentation_params(FragmentationParams *params, int argc, char *argv[], int start_idx, int *end_idx) {
    *end_idx = start_idx;
    for (int i = start_idx; i < argc; i++) {
        if (is_test_name(argv[i])) break;
        if (!is_parameter(argv[i])) break;
        
        char *key = strtok(argv[i], "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            if (strcmp(key, "large") == 0) {
                params->large_size = atol(value);
            } else if (strcmp(key, "small") == 0) {
                params->small_size = atol(value);
            } else if (strcmp(key, "medium") == 0) {
                params->medium_size = atol(value);
            } else if (strcmp(key, "iterations") == 0) {
                params->pattern_iterations = atoi(value);
            }
        }
        *end_idx = i + 1;
    }
}

void parse_stress_params(StressSmallParams *params, int argc, char *argv[], int start_idx, int *end_idx) {
    *end_idx = start_idx;
    for (int i = start_idx; i < argc; i++) {
        if (is_test_name(argv[i])) break;
        if (!is_parameter(argv[i])) break;
        
        char *key = strtok(argv[i], "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            if (strcmp(key, "size") == 0) {
                params->small_alloc_size = atol(value);
            } else if (strcmp(key, "count") == 0) {
                params->num_allocations = atoi(value);
            } else if (strcmp(key, "free_pct") == 0) {
                params->free_percentage = atoi(value);
            }
        }
        *end_idx = i + 1;
    }
}

void parse_large_blocks_params(LargeBlocksParams *params, int argc, char *argv[], int start_idx, int *end_idx) {
    *end_idx = start_idx;
    for (int i = start_idx; i < argc; i++) {
        if (is_test_name(argv[i])) break;
        if (!is_parameter(argv[i])) break;
        
        char *key = strtok(argv[i], "=");
        char *value = strtok(NULL, "=");
        
        if (key && value) {
            if (strcmp(key, "num") == 0) {
                params->num_blocks = atoi(value);
            } else if (strcmp(key, "order") == 0) {
                params->free_order = atoi(value);
            }
        }
        *end_idx = i + 1;
    }
}

/* ==================== TEST IMPLEMENTATIONS ==================== */

void test_mmap_threshold(MmapThresholdParams params) {
    printf("=== Test: mmap_threshold ===\n");
    printf("Parameters: size_below=%zu, size_at=%zu, size_above=%zu\n\n",
           params.size_below_threshold, params.size_at_threshold, params.size_above_threshold);
    
    printf("Allocating size below threshold (%zu bytes)...\n", params.size_below_threshold);
    void *p1 = my_malloc(params.size_below_threshold);
    assert(p1 != NULL);
    memset(p1, 'A', params.size_below_threshold);
    printf("  Success: %p\n", p1);
    if (verbose_mode) print_memory();
    
    printf("Allocating size at threshold (%zu bytes)...\n", params.size_at_threshold);
    void *p2 = my_malloc(params.size_at_threshold);
    assert(p2 != NULL);
    memset(p2, 'B', params.size_at_threshold);
    printf("  Success: %p\n", p2);
    if (verbose_mode) print_memory();
    
    printf("Allocating size above threshold (%zu bytes)...\n", params.size_above_threshold);
    void *p3 = my_malloc(params.size_above_threshold);
    assert(p3 != NULL);
    memset(p3, 'C', params.size_above_threshold);
    printf("  Success: %p\n", p3);
    if (verbose_mode) print_memory();
    
    my_free(p1);
    my_free(p2);
    my_free(p3);
    if (verbose_mode) print_memory();
    printf("Test PASSED\n\n");
}

void test_alignment(AlignmentParams params) {
    printf("=== Test: alignment ===\n");
    printf("Parameters: testing %d sizes\n\n", params.num_sizes);
    
    void *ptrs[params.num_sizes];
    
    // Detect actual alignment from first allocation
    void *test = my_malloc(1);
    uintptr_t test_addr = (uintptr_t)test;
    size_t actual_alignment = 1;
    for (size_t pow = 1; pow <= 256; pow *= 2) {
        if (test_addr % pow == 0) {
            actual_alignment = pow;
        }
    }
    my_free(test);
    if (verbose_mode) print_memory();
    
    printf("Detected allocator alignment: %zu bytes\n\n", actual_alignment);
    
    for (int i = 0; i < params.num_sizes; i++) {
        ptrs[i] = my_malloc(params.sizes[i]);
        assert(ptrs[i] != NULL);
        
        uintptr_t addr = (uintptr_t)ptrs[i];
        int aligned = (addr % actual_alignment == 0);
        
        printf("Size %zu: %p (aligned to %zu: %s)\n", params.sizes[i], ptrs[i], actual_alignment, aligned ? "YES" : "NO");
        assert(aligned);
        
        memset(ptrs[i], 'A' + i, params.sizes[i]);
    }
    if (verbose_mode) print_memory();
    
    for (int i = 0; i < params.num_sizes; i++) {
        my_free(ptrs[i]);
    }
    if (verbose_mode) print_memory();
    printf("Test PASSED\n\n");
}

void test_split_reuse(SplitReuseParams params) {
    printf("=== Test: split_reuse ===\n");
    printf("Parameters: initial=%zu, realloc=%zu\n\n",
           params.initial_size, params.realloc_size);
    
    printf("Step 1: Allocate initial block (%zu bytes)...\n", params.initial_size);
    void *p1 = my_malloc(params.initial_size);
    assert(p1 != NULL);
    memset(p1, 'A', params.initial_size);
    printf("  Allocated at %p\n", p1);
    
    printf("Step 2: Free the block...\n");
    my_free(p1);
    if (verbose_mode) print_memory();
    
    printf("Step 3: Allocate smaller block (%zu bytes)...\n", params.realloc_size);
    void *p2 = my_malloc(params.realloc_size);
    assert(p2 != NULL);
    memset(p2, 'B', params.realloc_size);
    printf("  Allocated at %p\n", p2);
    
    if (p2 == p1) {
        printf("SUCCESS: Reuse detected (same address)\n");
    } else {
        printf("INFO: Different address (allocator may have different policy)\n");
    }

    if (verbose_mode) print_memory();
    
    my_free(p2);
    printf("Test PASSED\n\n");
}

void test_coalescing(CoalescingParams params) {
    printf("=== Test: coalescing ===\n");
    printf("Parameters: block_size=%zu, num_blocks=%d, order=%d\n\n",
           params.block_size, params.num_blocks, params.free_order);
    
    void *blocks[params.num_blocks];
    
    printf("Step 1: Allocating %d blocks of %zu bytes...\n", params.num_blocks, params.block_size);
    for (int i = 0; i < params.num_blocks; i++) {
        blocks[i] = my_malloc(params.block_size);
        assert(blocks[i] != NULL);
        memset(blocks[i], 'A' + i, params.block_size);
        printf("  Block %d: %p\n", i, blocks[i]);
    }
    if (verbose_mode) print_memory();
    
    printf("Step 2: Freeing blocks in order %d...\n", params.free_order);
    if (params.free_order == 0) {  // FIFO
        for (int i = 0; i < params.num_blocks; i++) {
            my_free(blocks[i]);
        }
        printf("  Freed in FIFO order\n");
    } else if (params.free_order == 1) {  // LIFO
        for (int i = params.num_blocks - 1; i >= 0; i--) {
            my_free(blocks[i]);
        }
        printf("  Freed in LIFO order\n");
    } else {  // Alternating
        for (int i = 0; i < params.num_blocks; i += 2) {
            my_free(blocks[i]);
        }
        for (int i = 1; i < params.num_blocks; i += 2) {
            my_free(blocks[i]);
        }
        printf("  Freed in alternating order\n");
    }
    if (verbose_mode) print_memory();
    
    printf("Step 3: Allocating merged block of %zu bytes...\n", (size_t)params.block_size * params.num_blocks);
    void *merged = my_malloc((size_t)params.block_size * params.num_blocks);
    assert(merged != NULL);
    memset(merged, 'Z', (size_t)params.block_size * params.num_blocks);
    printf("  Merged block allocated at %p\n", merged);
    
    if (verbose_mode) print_memory();

    my_free(merged);
    printf("Test PASSED\n\n");
}

void test_fragmentation(FragmentationParams params) {
    printf("=== Test: fragmentation ===\n");
    printf("Parameters: large=%zu, small=%zu, medium=%zu, iterations=%d\n\n",
           params.large_size, params.small_size, params.medium_size, params.pattern_iterations);
    
    void **large_blocks = malloc(sizeof(void*) * params.pattern_iterations);
    void **small_blocks = malloc(sizeof(void*) * params.pattern_iterations);
    
    for (int iter = 0; iter < params.pattern_iterations; iter++) {
        printf("Iteration %d: ", iter + 1);
        
        // Allocate large block
        large_blocks[iter] = my_malloc(params.large_size);
        assert(large_blocks[iter] != NULL);
        memset(large_blocks[iter], 'L', params.large_size);
        
        // Allocate small blocks
        small_blocks[iter] = my_malloc(params.small_size);
        assert(small_blocks[iter] != NULL);
        memset(small_blocks[iter], 'S', params.small_size);
        
        // Allocate and free medium block to create fragmentation
        void *medium = my_malloc(params.medium_size);
        assert(medium != NULL);
        memset(medium, 'M', params.medium_size);
        my_free(medium);
        if (verbose_mode) print_memory();
        
        printf("Large: %p, Small: %p\n", large_blocks[iter], small_blocks[iter]);
    }
    
    printf("Freeing large blocks to create fragmentation gaps...\n");
    for (int iter = 0; iter < params.pattern_iterations; iter++) {
        my_free(large_blocks[iter]);
    }
    if (verbose_mode) print_memory();
    
    printf("Attempting to allocate medium blocks in fragmented space...\n");
    for (int iter = 0; iter < params.pattern_iterations; iter++) {
        void *medium = my_malloc(params.medium_size);
        assert(medium != NULL);
        memset(medium, 'M', params.medium_size);
        my_free(medium);
    }
    if (verbose_mode) print_memory();
    
    for (int iter = 0; iter < params.pattern_iterations; iter++) {
        my_free(small_blocks[iter]);
    }
    if (verbose_mode) print_memory();
    
    free(large_blocks);
    free(small_blocks);
    printf("Test PASSED\n\n");
}

void test_stress_small(StressSmallParams params) {
    printf("=== Test: stress_small ===\n");
    printf("Parameters: size=%zu, count=%d, free_pct=%d%%\n\n",
           params.small_alloc_size, params.num_allocations, params.free_percentage);
    
    void **ptrs = malloc(sizeof(void*) * params.num_allocations);
    assert(ptrs != NULL);
    
    printf("Allocating %d blocks of %zu bytes...\n", params.num_allocations, params.small_alloc_size);
    for (int i = 0; i < params.num_allocations; i++) {
        ptrs[i] = my_malloc(params.small_alloc_size);
        assert(ptrs[i] != NULL);
        memset(ptrs[i], (char)('A' + (i % 26)), params.small_alloc_size);
        
        if ((i + 1) % 50 == 0) {
            printf("  Allocated %d blocks\n", i + 1);
        }
    }
    if (verbose_mode) print_memory();
    
    printf("Freeing %d%% of allocated blocks...\n", params.free_percentage);
    int blocks_to_free = (params.num_allocations * params.free_percentage) / 100;
    for (int i = 0; i < blocks_to_free; i++) {
        int idx = (i * 2) % params.num_allocations;  // Free every other block
        if (ptrs[idx] != NULL) {
            my_free(ptrs[idx]);
            ptrs[idx] = NULL;
        }
    }
    if (verbose_mode) print_memory();
    
    printf("Re-allocating freed space...\n");
    for (int i = 0; i < params.num_allocations; i++) {
        if (ptrs[i] == NULL) {
            ptrs[i] = my_malloc(params.small_alloc_size);
            assert(ptrs[i] != NULL);
            memset(ptrs[i], (char)('A' + (i % 26)), params.small_alloc_size);
        }
    }
    if (verbose_mode) print_memory();
    
    printf("Freeing all remaining blocks...\n");
    for (int i = 0; i < params.num_allocations; i++) {
        if (ptrs[i] != NULL) {
            my_free(ptrs[i]);
        }
    }
    if (verbose_mode) print_memory();
    
    free(ptrs);
    printf("Test PASSED\n\n");
}

void test_large_blocks(LargeBlocksParams params) {
    printf("=== Test: large_blocks ===\n");
    printf("Parameters: num=%d, order=%d\n\n", params.num_blocks, params.free_order);
    
    void **blocks = malloc(sizeof(void*) * params.num_blocks);
    assert(blocks != NULL);
    
    printf("Allocating %d large blocks via mmap...\n", params.num_blocks);
    for (int i = 0; i < params.num_blocks; i++) {
        blocks[i] = my_malloc(params.block_sizes[i]);
        assert(blocks[i] != NULL);
        memset(blocks[i], 'A' + i, params.block_sizes[i]);
        printf("  Block %d (%zu bytes): %p\n", i, params.block_sizes[i], blocks[i]);
    }
    if (verbose_mode) print_memory();
    
    printf("Freeing blocks in order %d...\n", params.free_order);
    if (params.free_order == 0) {  // FIFO
        for (int i = 0; i < params.num_blocks; i++) {
            my_free(blocks[i]);
        }
        printf("  Freed in FIFO order\n");
    } else if (params.free_order == 1) {  // LIFO
        for (int i = params.num_blocks - 1; i >= 0; i--) {
            my_free(blocks[i]);
        }
        printf("  Freed in LIFO order\n");
    } else {  // Random-like pattern
        for (int i = 0; i < params.num_blocks; i += 2) {
            my_free(blocks[i]);
        }
        for (int i = 1; i < params.num_blocks; i += 2) {
            my_free(blocks[i]);
        }
        printf("  Freed in random-like order\n");
    }
    if (verbose_mode) print_memory();
    
    free(blocks);
    printf("Test PASSED\n\n");
}

/* ==================== MAIN ==================== */

int main(int argc, char *argv[]) {
    if (argc == 1) {
        print_help();
        return 0;
    }
    
    // Check if user asked for help
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_help();
        return 0;
    }
    
    // Detect verbose flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "verbose") == 0) {
            verbose_mode = true;
        }
    }

    // Count number of tests
    int test_count = 0;
    for (int i = 1; i < argc; i++) {
        if (is_test_name(argv[i])) {
            test_count++;
        }
    }
    
    if (test_count == 0) {
        fprintf(stderr, "Error: No valid test name specified\n");
        print_help();
        return 1;
    }
    
    printf("=== Dynamic Allocator - Parametric Test Suite ===\n\n");
    
    int i = 1;
    int single_test = (test_count == 1);
    
    while (i < argc) {
        if (strcmp(argv[i], "verbose") == 0) {
            verbose_mode = true;
            i++;
            continue;
        }
        if (!is_test_name(argv[i])) {
            i++;
            continue;
        }
        
        const char *test_name = argv[i];
        i++;
        
        int params_end = i;
        
        // Parse parameters only if single test
        if (single_test) {
            if (strcmp(test_name, "mmap_threshold") == 0) {
                MmapThresholdParams params = default_mmap_params;
                parse_mmap_params(&params, argc, argv, i, &params_end);
                test_mmap_threshold(params);
            } else if (strcmp(test_name, "alignment") == 0) {
                AlignmentParams params = default_alignment_params;
                parse_alignment_params(&params, argc, argv, i, &params_end);
                test_alignment(params);
            } else if (strcmp(test_name, "split_reuse") == 0) {
                SplitReuseParams params = default_split_params;
                parse_split_params(&params, argc, argv, i, &params_end);
                test_split_reuse(params);
            } else if (strcmp(test_name, "coalescing") == 0) {
                CoalescingParams params = default_coalescing_params;
                parse_coalescing_params(&params, argc, argv, i, &params_end);
                test_coalescing(params);
            } else if (strcmp(test_name, "fragmentation") == 0) {
                FragmentationParams params = default_fragmentation_params;
                parse_fragmentation_params(&params, argc, argv, i, &params_end);
                test_fragmentation(params);
            } else if (strcmp(test_name, "stress_small") == 0) {
                StressSmallParams params = default_stress_params;
                parse_stress_params(&params, argc, argv, i, &params_end);
                test_stress_small(params);
            } else if (strcmp(test_name, "large_blocks") == 0) {
                LargeBlocksParams params = default_large_blocks_params;
                parse_large_blocks_params(&params, argc, argv, i, &params_end);
                test_large_blocks(params);
            }
            i = params_end;
        } else {
            // Multiple tests: use defaults only
            if (strcmp(test_name, "mmap_threshold") == 0) {
                test_mmap_threshold(default_mmap_params);
            } else if (strcmp(test_name, "alignment") == 0) {
                test_alignment(default_alignment_params);
            } else if (strcmp(test_name, "split_reuse") == 0) {
                test_split_reuse(default_split_params);
            } else if (strcmp(test_name, "coalescing") == 0) {
                test_coalescing(default_coalescing_params);
            } else if (strcmp(test_name, "fragmentation") == 0) {
                test_fragmentation(default_fragmentation_params);
            } else if (strcmp(test_name, "stress_small") == 0) {
                test_stress_small(default_stress_params);
            } else if (strcmp(test_name, "large_blocks") == 0) {
                test_large_blocks(default_large_blocks_params);
            }
        }
    }
    
    printf("=== All tests completed successfully ===\n");
    return 0;
}
