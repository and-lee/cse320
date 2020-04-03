#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"

void assert_free_block_count(size_t size, int count);
void assert_free_list_block_count(size_t size, int count);

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & BLOCK_SIZE_MASK))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

/*
 * Assert that the free list with a specified index has the specified number of
 * blocks in it.
 */
void assert_free_list_size(int index, int size) {
    int cnt = 0;
    sf_block *bp = sf_free_list_heads[index].body.links.next;
    while(bp != &sf_free_list_heads[index]) {
	cnt++;
	bp = bp->body.links.next;
    }
    cr_assert_eq(cnt, size, "Free list %d has wrong number of free blocks (exp=%d, found=%d)",
		 index, size, cnt);
}

Test(sf_memsuite_student, malloc_an_int, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	int *x = sf_malloc(sizeof(int));

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;
	//printf("%s\n", "malloc an int");
	//sf_show_heap();

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 1);
	assert_free_block_count(3904, 1);
	assert_free_list_size(NUM_FREE_LISTS-1, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sf_memsuite_student, malloc_three_pages, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	// We want to allocate up to exactly three pages.
	void *x = sf_malloc(3 * PAGE_SZ - ((1 << 6) - sizeof(sf_header)) - 64 - 2*sizeof(sf_header));
	//printf("%s\n", "malloc three pages");
	//sf_show_heap();

	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sf_memsuite_student, malloc_too_large, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(PAGE_SZ << 16);
	//printf("%s\n", "malloc too large");
	//sf_show_heap();

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 1);
	assert_free_block_count(65408, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sf_memsuite_student, free_quick, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(32);
	/* void *z = */ sf_malloc(1);

	sf_free(y);
	//printf("%s\n", "free quick");
	//sf_show_heap();

	assert_free_block_count(0, 2);
	assert_free_block_count(64, 1);
	assert_free_block_count(3776, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_no_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(1);

	sf_free(y);
	//printf("%s\n", "free no coalesce");
	//sf_show_heap();

	assert_free_block_count(0, 2);
	assert_free_block_count(256, 1);
	assert_free_block_count(3584, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_coalesce, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	/* void *w = */ sf_malloc(8);
	void *x = sf_malloc(200);
	void *y = sf_malloc(300);
	/* void *z = */ sf_malloc(4);

	sf_free(y);
	sf_free(x);
	//printf("%s\n", "free coalesce");
	//sf_show_heap();

	assert_free_block_count(0, 2);
	assert_free_block_count(576, 1);
	assert_free_block_count(3264, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, freelist, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *u = sf_malloc(200);
	/* void *v = */ sf_malloc(300);
	void *w = sf_malloc(200);
	/* void *x = */ sf_malloc(500);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(700);

	sf_free(u);
	sf_free(w);
	sf_free(y);
	//printf("%s\n", "free list");
	//sf_show_heap();

	assert_free_block_count(0, 4);
	assert_free_block_count(256, 3);
	assert_free_block_count(1600, 1);
	assert_free_list_size(3, 3);
	assert_free_list_size(NUM_FREE_LISTS-1, 1);

	// First block in list should be the most recently freed block.
	int i = 3;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - 2*sizeof(sf_header),
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - 2*sizeof(sf_header));
}

Test(sf_memsuite_student, realloc_larger_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int));
	/* void *y = */ sf_malloc(10);
	x = sf_realloc(x, sizeof(int) * 20);
	//printf("%s\n", "realloc larger block");
	//sf_show_heap();

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 128, "Realloc'ed block size not what was expected!");

	assert_free_block_count(0, 2);
	assert_free_block_count(64, 1);
	assert_free_block_count(3712, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_splinter, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(int) * 20);
	void *y = sf_realloc(x, sizeof(int) * 16);
	//printf("%s\n", "realloc smaller block splinter");
	//sf_show_heap();

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 128, "Block size not what was expected!");

	// There should be only one free block of size 3840.
	assert_free_block_count(0, 1);
	assert_free_block_count(3840, 1);
}

Test(sf_memsuite_student, realloc_smaller_block_free_block, .init = sf_mem_init, .fini = sf_mem_fini) {
	void *x = sf_malloc(sizeof(double) * 8);
	void *y = sf_realloc(x, sizeof(int));
	//printf("%s\n", "realloc smaller block free block");
	//sf_show_heap();

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char*)y - 2*sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & BLOCK_SIZE_MASK) == 64, "Realloc'ed block size not what was expected!");

	// After realloc'ing x, we can return a block of size 64 to the freelist.
	// This block will go into the main freelist and be coalesced.
	assert_free_block_count(0, 1);
	assert_free_block_count(3904, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

Test(sf_memsuite_student, memalign_not_aligned, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_memalign(sizeof(int), 1024);
	void *y = sf_memalign(sizeof(int), 512);
	//sf_show_heap();

	cr_assert_not_null(x, "x is NULL!");
	cr_assert_eq(((long int)x)%1024, 0, "Block not aligned!");
	cr_assert_not_null(y, "y is NULL!");
	cr_assert_eq(((long int)y)%512, 0, "Block not aligned!");

	// offset created, second memalign, y, has to have an inital portion that is freed
		// more than 1 free block, wilderness and free block betweem memaligned blocks
	assert_free_list_size(NUM_FREE_LISTS-1, 1); // wilderness block
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, memalign_already_aligned, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_memalign(500, 64);
	//sf_show_heap();

	// memalign with correct alignment is the same as malloc
	cr_assert_not_null(x, "x is NULL!");
	cr_assert_eq(((long int)x)%64, 0, "Block not aligned!");

	assert_free_block_count(0, 1);
	assert_free_block_count(3456, 1);
	assert_free_list_size(NUM_FREE_LISTS-1, 1);
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, free_coalesce_twice, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *x = sf_malloc(sizeof(int));
	void *y = sf_malloc(sizeof(int));
	void *z = sf_malloc(sizeof(int));
	sf_malloc(sizeof(int));
	sf_free(x);
	sf_free(z);

	sf_free(y);
	//sf_show_heap();

	// coalesce with 2 free blocks, before and after selected block to free
	assert_free_block_count(0, 2); // 3 lists, coalesce 2 lists, create 1 list. (3-2)+1 = 2
	assert_free_block_count(192, 1); // coalesce 64 above, 64 below, and block itself sz 64. 64*3 = 192
	assert_free_list_size(2, 1); // only 1 freelist of size 192
	assert_free_block_count(3712, 1);
	assert_free_list_size(NUM_FREE_LISTS-1, 1); // only 1 wilderness block
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sf_memsuite_student, malloc_correct_free_list, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *a = sf_malloc(sizeof(int)*20);
	sf_malloc(sizeof(int));
	void *b = sf_malloc(sizeof(int)*40);
	sf_malloc(sizeof(int));
	void *c = sf_malloc(sizeof(int)*60);
	sf_malloc(sizeof(int));
	sf_free(a);
	sf_free(b);
	sf_free(c);

	void *x = sf_malloc(sizeof(int)*40);
	sf_show_heap();

	// was sucessfully malloced
	cr_assert_not_null(x, "x is NULL!");

	// freelist of size 196 taken by malloc
	assert_free_list_size(2, 0); // no free list, was malloced
	assert_free_block_count(0, 3); // only 3 freelists, not 4
	assert_free_block_count(128, 1);
	assert_free_list_size(1, 1); // only 1 freelist of size 128
	assert_free_block_count(256, 1);
	assert_free_list_size(3, 1); // only 1 freelist of size 256
	assert_free_block_count(3200, 1);
	assert_free_list_size(NUM_FREE_LISTS-1, 1); // only 1 wilderness block
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}


Test(sf_memsuite_student, malloc_split, .init = sf_mem_init, .fini = sf_mem_fini) {
	sf_errno = 0;
	void *a = sf_malloc(sizeof(int)*80);
	sf_malloc(sizeof(int));
	sf_free(a);

	void *x = sf_malloc(sizeof(int)*20);
	//sf_show_heap();

	// was sucessfully malloced
	cr_assert_not_null(x, "x is NULL!");

	// free bigger block and malloc smaller block to create split. should create smaller free block.
	assert_free_block_count(0, 2); // 2 free lists
	assert_free_list_size(4, 0); // no free list, was split
	assert_free_list_size(3, 1); // free block created from split
	assert_free_block_count(256, 1);
	assert_free_block_count(3520, 1);
	assert_free_list_size(NUM_FREE_LISTS-1, 1); // only 1 wilderness block
	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}
