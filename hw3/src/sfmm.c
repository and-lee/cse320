/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"

#define M 64 // minimum size in bytes

void *sf_malloc(size_t size) {
    if(size==0) {
        return NULL;
    }

    // else size !=0

    // if heap is empty
    if(sf_mem_start()==sf_mem_end()) { // initial call
        sf_mem_grow();
        // create prologue
        sf_block *bp = (sf_block *)(sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer))));
        //48
        // header = [al: 1, sz:      64, pal: 1][PROLOGUE]
        bp->header = M & BLOCK_SIZE_MASK; // set block_size to 64
        bp->header |= THIS_BLOCK_ALLOCATED; // alloc = 1
        bp->header |= PREV_BLOCK_ALLOCATED; //prev_alloc = 1

        // next block in the heap = free/alloc blocks
        sf_block *nbp = (sf_block *)(sf_mem_start()+((M-(sizeof(sf_header)+sizeof(sf_footer)))+M));
        // prologue footer = same as header
        nbp->prev_footer = bp->header;
        nbp->header = (PAGE_SZ-((M-(sizeof(sf_header)+sizeof(sf_footer)))+M)-(2*sizeof(sf_header))) & BLOCK_SIZE_MASK; // set block_size to 64
        //96
        nbp->header |= PREV_BLOCK_ALLOCATED; //prev_alloc = 1

        // create epilogue
        bp = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
        //bp->header = 0 & BLOCK_SIZE_MASK; // set block_size to 0
        bp->header |= THIS_BLOCK_ALLOCATED; // alloc = 1
        // empty/free: prev_allo = 0
        bp->prev_footer = nbp->header;



        // free lists


    }
    //extend heap

    sf_show_heap();
    printf("%s\n", "end malloc");
    return NULL;
}

void sf_free(void *pp) {
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    return NULL;
}
