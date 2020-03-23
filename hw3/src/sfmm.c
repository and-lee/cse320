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
#include <errno.h>

#define M 64 // minimum size in bytes

//helper functions
void initialize_free_lists() {
    for(int i=0; i<NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}
void insert_free_list(void * free_list, sf_block *block) {
    sf_block *prev_block = (sf_block *)free_list;
    block->body.links.next = prev_block;
    prev_block->body.links.next = block;
    block->body.links.prev = prev_block;
    block->body.links.next->body.links.prev = block;
}
void remove_free_list(void * free_list, sf_block *block) {
    sf_block *prev_block = (sf_block *)free_list;
    block->body.links.next = prev_block;
    prev_block->body.links.next = block;
    block->body.links.prev = prev_block;
    block->body.links.next->body.links.prev = block;
}
sf_block *get_next_block(sf_block *block) {
    return (sf_block *)((char *)(block)+((block->header)&BLOCK_SIZE_MASK));
}

void *sf_malloc(size_t size) {
    if(size==0) {
        return NULL;
    } // else size !=0


    // if heap is empty, initialize heap
    if(sf_mem_start()==sf_mem_end()) { // initial call
        //initialize free lists: list is empty
        initialize_free_lists();

        sf_block *ptr = sf_mem_grow();
        if(ptr == NULL) { // no more space to allocate
            sf_errno = ENOMEM;
            return NULL;
        }
        // create prologue. no access to prev_footer
        sf_block *prologue = (sf_block *)(sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer)))); //48
        prologue->header = M & BLOCK_SIZE_MASK; // set block_size to 64
        prologue->header |= THIS_BLOCK_ALLOCATED; // alloc = 1
        prologue->header |= PREV_BLOCK_ALLOCATED; //prev_alloc = 1

        // wilderness block
        sf_block *wbp = (sf_block *)((char *)(prologue)+M); //112
        wbp->prev_footer = prologue->header; // prologue footer = same as header
        wbp->header = (PAGE_SZ-((M-(sizeof(sf_header)+sizeof(sf_footer)))+M)-(2*sizeof(sf_header))) & BLOCK_SIZE_MASK; // set block_size to 3968 //prologue header, epilogue header
        wbp->header &= ~(THIS_BLOCK_ALLOCATED); //alloc = 0
        wbp->header |= PREV_BLOCK_ALLOCATED; //prev_alloc = 1
        // insert into free list
        insert_free_list(&sf_free_list_heads[NUM_FREE_LISTS-1], wbp);

        // create epilogue. only need header, prev_footer
        sf_block *epilogue = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
        epilogue->header = 0 & BLOCK_SIZE_MASK; // set block size to 0
        epilogue->header |= THIS_BLOCK_ALLOCATED; // alloc = 1
        epilogue->header &= ~(PREV_BLOCK_ALLOCATED); //prev_alloc = 0
        epilogue->prev_footer = wbp->header;
    }

    //determine size of block to be allocated
    int block_size = size + 8; //add header size +8
    int padding = M-(block_size%M); //add padding = multiple of 64 alignment
    block_size += padding;

    //search for smallest free list to satisfy request size->end-2
    //fib range
    //(block_size/M)-1
    for(int i = 0; i<NUM_FREE_LISTS; i++) {
        sf_block *found_block = ((sf_block *)(&sf_free_list_heads[i]))->body.links.next; //head = sentinel
        while(found_block != &sf_free_list_heads[i]) { // search link list
            if((found_block->header & BLOCK_SIZE_MASK) == block_size) {
                return found_block->body.payload;
            }
            if((found_block->header & BLOCK_SIZE_MASK) > block_size) { // block found
                //split without creating a splinter (<=64 bytes)

                //split : upper part = remainder
                sf_block *free_block = (sf_block *)((char *)(found_block)+block_size);
                free_block->header = (((found_block->header)&BLOCK_SIZE_MASK) - block_size) & BLOCK_SIZE_MASK;
                free_block->header |= PREV_BLOCK_ALLOCATED; //prev_alloc = 1
                free_block->header &= ~(THIS_BLOCK_ALLOCATED); //alloc = 0

                //split : lower part = allocation request
                sf_block *data_block = (sf_block *)(char *)(found_block);
                data_block->header = block_size & BLOCK_SIZE_MASK;
                //data_block->header |= THIS_BLOCK_ALLOCATED; //alloc = 1
                data_block->header |= (found_block->header|PREV_BLOCK_ALLOCATED); //prev_alloc
                data_block->prev_footer= found_block->prev_footer;

                free_block->prev_footer = data_block->header;
                get_next_block(free_block)->prev_footer = free_block->header;
                //insert remainer back into its size class
                remove_free_list(&sf_free_list_heads[NUM_FREE_LISTS-1], data_block);//remove allocated block from free list
                insert_free_list(&sf_free_list_heads[NUM_FREE_LISTS-2], free_block);

                //success return pointer to of request to payload
                //return data_block->body.payload;
            }

            found_block = found_block->body.links.next; // move to next block in the free list
        } //if no block found, continue to next size class
    }

    //if block not found when end is reached- use wilderness block.
        //if wilderness block too small/does not exist<<<<<<<
            // does not exist = initialize heap
            //extend heap
                //call sf_mem_grow. call umltiple times if size is multiple pages
                // after EACH call, COALESCE
                    // coalesce newly allocated page with wildreness block preceding
                    // insert new wilderness block to the beginning of the last free list
                //if no more space - sf_mem_grow returns NULL:: return NULL, sf_errno=ENOMEM





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
