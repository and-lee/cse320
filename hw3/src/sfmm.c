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

        //initialize free lists: list is empty
        for(int i=0; i<NUM_FREE_LISTS; i++) {
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }

        sf_mem_grow();
        // create prologue: header = [al: 1, sz:      64, pal: 1][PROLOGUE]
        sf_block *bp = (sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer)))); //48
        bp->header = M & BLOCK_SIZE_MASK; // set block_size to 64
        bp->header |= THIS_BLOCK_ALLOCATED; // alloc = 1
        bp->header |= PREV_BLOCK_ALLOCATED; //prev_alloc = 1

        // wilderness block
        sf_block *wbp = (sf_block *)(sf_mem_start()+((M-(sizeof(sf_header)+sizeof(sf_footer)))+M)); //112
        wbp->prev_footer = bp->header; // prologue footer = same as header
        wbp->header = (PAGE_SZ-((M-(sizeof(sf_header)+sizeof(sf_footer)))+M)-(2*sizeof(sf_header))) & BLOCK_SIZE_MASK; // set block_size to 3968
        //prologue header, epilogue header
        wbp->header |= PREV_BLOCK_ALLOCATED; //prev_alloc = 1
        //wbp->body.links.next = (sf_block *)(sf_free_list_heads[NUM_FREE_LISTS].body.links.next);
        printf("%s\n", "HULLO");
        printf("%p\n", (sf_block *)(sf_free_list_heads[NUM_FREE_LISTS].body.links.next));

        // create epilogue
        bp = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
        //bp->header = 0 & BLOCK_SIZE_MASK; // set block_size to 0
        bp->header |= THIS_BLOCK_ALLOCATED; // alloc = 1
        // empty/free: prev_allo = 0
        bp->prev_footer = wbp->header;




    }
    // if free lists is empty
    for(int i=0; i<NUM_FREE_LISTS; i++) {
        if((sf_free_list_heads[i].body.links.next != &sf_free_list_heads[i]) && (sf_free_list_heads[i].body.links.prev != &sf_free_list_heads[i])){
            //sf_free_list_heads[i].body.links.next = ; // points to first node in the list
            //sf_free_list_heads[i].body.links.prev = ; // points to last node in the list
        }
        //non empty
    }


    //extend heap




    sf_show_heap();
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
