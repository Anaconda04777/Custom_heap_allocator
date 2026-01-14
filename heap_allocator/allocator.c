#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "heap_allocator.h"

//gcc allocator.c -o allocator -Wall

/*
    ------ TESTS FOR THE ALLOCATOR --------
    In this script we are going to test our allocator with 5
    simple test
    
    1. Test Basic Allocation: testing a simple allocate-write-deallocate flow
    
    2. Test Reuse: test if a block that is allocated and then freed would be 
        reused another time when a data of the same size is requested
    
    3. Test Coalescing: test if the key feature of the coalescing works
        properly. 3 blocks of the same type are allocated and then freed
        one after the other. The expected behave is to have then just single 
        bigger block in memory which is reused by the last bigger allocation
    
    4. Test Large Allocation: test if a large allocation, which would trigger the
        mmap allocator, works properly in the allocate-write-free flow
    
    5. Test Many Allocations: test the correct functioning of sbrk by allocating
        a huge number of blocks which total size would exceed the default size
        of the heap
    
    Bonus: in the folder you can find test_hashtable_official which is
        a real use case script that use my_malloc and my_free insted of the 
        standard ones
*/

void test_basic_allocation() {
    printf("=== Test 1: Base allocation ===\n");
    
    void *p1 = my_malloc(32);
    printf("p1 allocated (32 bytes): %p\n", p1);
    
    void *p2 = my_malloc(64);
    printf("p2 allocated (64 bytes): %p\n", p2);
    
    void *p3 = my_malloc(128);
    printf("p3 allocated (128 bytes): %p\n", p3);
    
    // Writing some stuff in the blocks with memset to check if they work
    memset(p1, 'A', 32);
    memset(p2, 'B', 64);
    memset(p3, 'C', 128);
    
    printf("Writing in blocks test passed \n\n");
    
    my_free(p1);
    my_free(p2);
    my_free(p3);

    printf("Deallocation test passed\n\n");
}

void test_reuse() {
    printf("=== Test 2: Reuse of free blocks ===\n");
    
    void *p1 = my_malloc(64);
    printf("First allocation (64 bytes): %p\n", p1);
    
    my_free(p1);
    printf("Free p1\n");
    
    void *p2 = my_malloc(64);
    printf("Second allocation (64 bytes): %p\n", p2);
    
    assert(p1 == p2);
    
    printf("SUCCESS: a block of the same reused correctly!\n\n");
    
    my_free(p2);
}

void test_coalescing() {
    printf("=== Test 3: Coalescing ===\n");
    
    void *p1 = my_malloc(sizeof(int));
    void *p2 = my_malloc(sizeof(int));
    void *p3 = my_malloc(sizeof(int));
    
    printf("Allocating 3 contiguous blocks\n");
    printf("p1: %p, p2: %p, p3: %p\n", p1, p2, p3);
    
    //It should merge p1+p2+p3
    my_free(p1);
    my_free(p3);
    my_free(p2);
    
    printf("Freed all of 3 blocks (coalescing should merge them)\n");
    
    //Allocating a big block that should use that space
    int* p4 = (int*) my_malloc(sizeof(int)*3);
    printf("Allocating a more bigger block (sizeof(int)*3): %p\n\n", p4);

    //Write on the coalesced block
    for (int i = 0; i < 3; i++) {
            p4[i] = (i + 1) * 10;
    }

    for (int i = 0; i < 3; i++) {
        printf("p4[%d] = %d\n", i, p4[i]);
    }

    my_free(p4);
}

void test_large_allocation() {
    printf("=== Test 4: Use of mmap on bigger allocations ===\n");
    
    // Allocation go over MMAP_THRESHOLD (128KB)
    size_t large_size = 256 * 1024; // 256 KB
    void *p = my_malloc(large_size);
    
    assert(p != NULL);
    
    printf("Allocating (256KB) with mmap: %p\n", p);
    
    // Writing on the memory to check
    memset(p, 'X', large_size);
    printf("Writing on the bigger block test passed\n");
    
    my_free(p);
    printf("Deallocating a bigger block test passed\n\n");
    
}

void test_many_allocations() {
    printf("=== Test 5: Test many allocations ===\n");
    
    //Allocate 50x100 = 5000 bytes => use of sbrk
    #define NUM_ALLOCS 70
    void *ptrs[NUM_ALLOCS];
    
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = my_malloc(100);
        if (ptrs[i] == NULL) {
            printf("Allocation %d failed (Could be OOM problem)\n", i);
        }
    }
    
    printf("Allocating %d blocks\n", NUM_ALLOCS);
    
    // Free every even block
    for (int i = 0; i < NUM_ALLOCS; i += 2) {
        my_free(ptrs[i]);
    }
    
    printf("Freed every even blocks\n");
    
    // Free the rest
    for (int i = 1; i < NUM_ALLOCS; i += 2) {
        if (ptrs[i] != NULL) {
            my_free(ptrs[i]);
        }
    }
    
    printf("Freed every odd blocks\n\n");
}

int main(void) {
    printf("=== Dynamic Allocator Test ===\n\n");
    
    test_basic_allocation();
    test_reuse();
    test_coalescing();
    test_large_allocation();
    test_many_allocations();
    
    printf("=== All tests passed successfully ===\n");
    
    return 0;
}
