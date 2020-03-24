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
#include <limits.h>

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
void delete_free_list(void * free_list, sf_block *block) {
    block->body.links.next->body.links.prev = block->body.links.prev; //b->next->prev = b->prev
    block->body.links.prev->body.links.next = block->body.links.next; //b->prev->next = b->next
}
sf_block *get_next_block(sf_block *block) {
    return (sf_block *)((char *)(block)+((block->header)&BLOCK_SIZE_MASK));
}
sf_block *get_prev_block(sf_block *block) {
    return (sf_block *)((char *)(block)-((block->prev_footer)&BLOCK_SIZE_MASK));
}
long int get_block_size(sf_block *block){
    return block->header & BLOCK_SIZE_MASK;
}
void set_block_size(sf_block *block, size_t size){
    block->header = size & BLOCK_SIZE_MASK;
}
long int get_alloc_bit(sf_block *block) {
    return block->header & THIS_BLOCK_ALLOCATED;
}
long int get_prev_alloc_bit(sf_block *block) {
    return block->header & PREV_BLOCK_ALLOCATED;
}
sf_header create_header(size_t size, long int prev_alloc, long int alloc) {
    sf_header header = ((size&BLOCK_SIZE_MASK)|(prev_alloc)|(alloc));
    return header;
}
sf_block *get_free_list(long int size) {
    int fib[NUM_FREE_LISTS] = {0, M, 2*M, 3*M, 5*M, 8*M, 13*M, 21*M, 34*M, INT_MAX};
    for(int index=0; index<NUM_FREE_LISTS-1; index++) {
        if(size>fib[index] && size<=fib[index+1]) {
            return &sf_free_list_heads[index+1];
        }
    }
    return NULL;
}
sf_block *place_block(void * address, sf_header header) {
    sf_block *block = (sf_block*)(address);
    block->header = header;
    if(get_alloc_bit(block) == 0) { // add to free list
        insert_free_list(get_free_list(get_block_size(block)), block);
    }
    return block;
}
sf_block *split_block(sf_block *block, size_t size) {
    long int free_block_size = get_block_size(block) - size;

    //remove allocated block from free list
    delete_free_list(get_free_list(get_block_size(block)), block);

    // split without creating a splinter (<=64 bytes) : 'over alloc'
    // splinter (splinter size = < min size)
    // exact size. do not need to split. use entire block
    if((free_block_size < M) || (free_block_size == 0)) {
        block->header |= THIS_BLOCK_ALLOCATED; // switch to alloc
        return block;
    }

    //upper part = remainder [al: 0, sz:       get_block_size(block) - size, pal: 1]
    sf_block *free_block = place_block((char *)(block) + size, create_header(get_block_size(block) - size, PREV_BLOCK_ALLOCATED, 0));

    //lower part = allocation request [al: 1, sz:       size, pal: block.pal]
    sf_block *data_block = place_block((char *)(block), create_header(size, PREV_BLOCK_ALLOCATED, THIS_BLOCK_ALLOCATED));
    data_block->prev_footer= block->prev_footer;

    free_block->prev_footer = data_block->header;
    get_next_block(free_block)->prev_footer = free_block->header;

    return data_block;
}


void *sf_malloc(size_t size) {
    int pages = 0;
    if(size == 0) {
        return NULL;
    } // else size !=0

    // if heap is empty, initialize heap
    if(sf_mem_start()==sf_mem_end()) { // initial call
        //initialize free lists: list is empty
        initialize_free_lists();

        // extend heap
        sf_block *ptr = sf_mem_grow();
        if(ptr == NULL) { // no more space to allocate
            sf_errno = ENOMEM;
            return NULL;
        }

        // create prologue. (does not have prev_footer) [al: 1, sz:       64, pal: 1]
        sf_block *prologue = place_block(sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer))), create_header(M, PREV_BLOCK_ALLOCATED, THIS_BLOCK_ALLOCATED));

        // wilderness block 112 [al: 0, sz:       3968, pal: 1]
        sf_block *wilderness = place_block((char *)(prologue)+M, create_header(PAGE_SZ-((M-(sizeof(sf_header)+sizeof(sf_footer)))+M)-(2*sizeof(sf_header)), PREV_BLOCK_ALLOCATED, 0));
        wilderness->prev_footer = prologue->header; // prologue footer = same as header
        pages += 1;

        // create epilogue. (only need header, prev_footer) [al: 1, sz:       0, pal: 0]
        sf_block *epilogue = place_block(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)), create_header(0, 0, THIS_BLOCK_ALLOCATED));
        epilogue->prev_footer = wilderness->header;
    }

    //determine size of block to be allocated
    int block_size = size + 8; //add header size +8
    int padding = M-(block_size%M); //add padding = multiple of 64 alignment
    block_size += padding;

    //search for smallest free list to satisfy request size->end-2
    sf_block *free_list = get_free_list(block_size);
    while(free_list != &sf_free_list_heads[NUM_FREE_LISTS]) {
        sf_block *found_block = ((sf_block *)(free_list))->body.links.next; //head = sentinel
        while(found_block != free_list) { // search link list
            if(get_block_size(found_block) >= block_size) { // block found
                //success return pointer to of request to payload/valid regin of memory
                return split_block(found_block, block_size)->body.payload;
            }
            // move to next block in the free list
            found_block = found_block->body.links.next;
        } //if no block found, continue to next size class
        // move to the next free list
        free_list += 1;
    }
    // block not found - use wilderness block (if it exists)
    // wilderness block does not exist || wilderness block is not big enough > create more space in memory
    // old epilogue = header block
    sf_block *new_block = ((sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer))));
    // coalesce with previous wilderness block - if it exist
    if(get_prev_alloc_bit(new_block) == 0) {
        new_block = get_prev_block(new_block);
        // remove from free list
        delete_free_list(&sf_free_list_heads[NUM_FREE_LISTS-1], new_block); //delete_free_list(get_free_list(new_block), new_block);
    }

    pages = block_size/PAGE_SZ - pages;
    if(block_size%PAGE_SZ) { // need to alloc another page
        pages += 1;
    }
    for(int i=0; i<pages; i++) { // call multiple times if size needed is multiple pages
        sf_block *ptr = sf_mem_grow(); // extend heap
        if(ptr == NULL) { // no more space to allocate
            sf_errno = ENOMEM;
            return NULL;
        }
        sf_block *temp = (sf_block *)((char *)(ptr));
        temp->header = create_header(PAGE_SZ, 0, 0);

        // coalesce
        if(get_prev_alloc_bit(temp) == 0) {
            set_block_size(new_block, get_block_size(temp)+get_block_size(new_block));
        }

        // add to free list
        insert_free_list(&sf_free_list_heads[NUM_FREE_LISTS-1], new_block);

        // create new epilogue
        sf_block *new_epilogue = place_block(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)), create_header(0, 0, THIS_BLOCK_ALLOCATED));
        new_epilogue->prev_footer = new_block->header;

    }

    // insert new wilderness block to the beginning of the last free list (add to free list)
    return split_block(new_block, block_size)->body.payload; // return pointer to allocated block
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
