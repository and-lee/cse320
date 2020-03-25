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
void insert_free_list(sf_block *free_list, sf_block *block) {
    block = (sf_block*)(block);
    // add to front of the list
    sf_block *sentinel = (sf_block *)((char *)free_list);
    block->body.links.next = sentinel->body.links.next;
    sentinel->body.links.next = block;
    block->body.links.prev = sentinel;
    block->body.links.next->body.links.prev = block;
}
void delete_free_list(sf_block *block) {
    block = (sf_block*)(block);
    block->body.links.prev->body.links.next = block->body.links.next; //b->prev->next = b->next
    block->body.links.next->body.links.prev = block->body.links.prev; //b->next->prev = b->prev
}
sf_block *get_next_block(sf_block *block) {
    return (sf_block *)(((char *)block)+((block->header)&BLOCK_SIZE_MASK));
}
sf_block *get_prev_block(sf_block *block) {
    return (sf_block *)(((char *)block)-((block->prev_footer)&BLOCK_SIZE_MASK));
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
sf_block *get_free_list(sf_block *block) {
    block = (sf_block*)(block);
    sf_block *epilogue = (sf_block*)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
    if(get_next_block(block) == epilogue) { // if = wildernes block
        return &sf_free_list_heads[NUM_FREE_LISTS-1];
    }
    long int size = get_block_size(block);
    /*int fib[NUM_FREE_LISTS] = {0, M, 2*M, 3*M, 5*M, 8*M, 13*M, 21*M, 34*M, INT_MAX};
    for(int index=0; index<NUM_FREE_LISTS-1; index++) {
        if(size>fib[index] && size<=fib[index+1]) {
            return &sf_free_list_heads[index-1];
        }
    }
    */
    if(size>0 && size<=M) {
        return &sf_free_list_heads[0];
    } else if(size>M && size<=2*M) {
        return &sf_free_list_heads[1];
    } else if(size>2*M && size<=3*M) {
        return &sf_free_list_heads[2];
    } else if(size>3*M && size<=5*M) {
        return &sf_free_list_heads[3];
    } else if(size>5*M && size<=8*M) {
        return &sf_free_list_heads[4];
    } else if(size>8*M && size<=13*M) {
        return &sf_free_list_heads[5];
    } else if(size>13*M && size<=21*M) {
        return &sf_free_list_heads[6];
    } else if(size>21*M && size<=34*M) {
        return &sf_free_list_heads[7];
    } else if(size>34*M) {
        return &sf_free_list_heads[8];
    }
    return NULL;
}
sf_block *place(void *address, sf_header b_header) {
    // heap already exists and is initialized
    sf_block *block = (sf_block *)(((char *)address));
    //int to_delete_from_free_list = 0;
    //if(sf_mem_start() != sf_mem_end()) { // if block exists, heap non empty
        //to_delete_from_free_list = get_alloc_bit(block); // get old alloc bit
    //}
    int to_delete_from_free_list = get_alloc_bit(block); // get old alloc bit
    block->header = b_header;

    sf_block *next_block = get_next_block(block);
    sf_block *prev_block = get_prev_block(block);
    sf_block *epilogue = (sf_block*)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer))); ////
    if(get_prev_alloc_bit(block) == 0) { // free block, set footer
        block->prev_footer = prev_block->header;
    }

    if(get_alloc_bit(block)) { // alloc = 1
        // set next block's prev_alloc = 1
        next_block->header |= PREV_BLOCK_ALLOCATED;

        // delete from free list (if it exists)
        if((to_delete_from_free_list)) { // block is now allocated
            delete_free_list(block);
        }
    } else { // alloc = 0
        // set next block's prev_alloc = 0
        next_block->header &= ~PREV_BLOCK_ALLOCATED;
        next_block->prev_footer = block->header; // set block's footer

        // insert
        if(next_block == epilogue) {
            insert_free_list(&sf_free_list_heads[NUM_FREE_LISTS-1], block);
        } else {
            insert_free_list(get_free_list(block), block);
        }
    }
    return block;
}
sf_block *split_block(sf_block *block, size_t size) {
    block = (sf_block*)(block);
    long int free_block_size = get_block_size(block) - size;

    delete_free_list(block); // reassign

    // split without creating a splinter (<=64 bytes) : 'over alloc'
    // splinter (splinter size = < min size)
    // exact size. do not need to split. use entire block
    if((free_block_size < M) || (free_block_size == 0)) {
        // alloc = 1
        return place((char *)(block), create_header(get_block_size(block), get_prev_alloc_bit(block), 1));
    }

    //upper part = remainder [al: 0, sz:       get_block_size(block) - size, pal: 1]
    // checks if block is wilderness block
    place((char *)(block) + size, create_header(get_block_size(block) - size, PREV_BLOCK_ALLOCATED, 0));

    //lower part = allocation request [al: 1, sz:       size, p.al: block.pal]
    sf_block *data_block = place((char *)(block), create_header(size, PREV_BLOCK_ALLOCATED, THIS_BLOCK_ALLOCATED));
    return data_block;
}
sf_block *coalesce_block(sf_block *first, sf_block *second) {
    first = (sf_block *)(first);
    second = (sf_block *)(second);

    if(get_alloc_bit(second) == 0) { // delete combining block from the free list
        delete_free_list(second);
    }
    if(get_alloc_bit(first) == 0) {
        delete_free_list(first);
    }
    //combine into one block
    place(first, create_header(get_block_size(first)+(get_block_size(second)), get_prev_alloc_bit(first), get_alloc_bit(first)));

    return first;
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

        // create prologue [al: 1, sz:       64, pal: 1]
        //sf_block *prologue = place_block(sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer))), create_header(M, PREV_BLOCK_ALLOCATED, THIS_BLOCK_ALLOCATED));
        sf_block *prologue = (sf_block *)(sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer))));
        prologue->header = create_header(M, PREV_BLOCK_ALLOCATED, THIS_BLOCK_ALLOCATED);
        // no prev_footer

        // wilderness block 112 [al: 0, sz:       3968, pal: 1]
        sf_block *wilderness = (sf_block *)((char *)(prologue)+M);
        wilderness->header = create_header(PAGE_SZ-((M-(sizeof(sf_header)+sizeof(sf_footer)))+M)-(2*sizeof(sf_header)), PREV_BLOCK_ALLOCATED, 0);
        wilderness->prev_footer = prologue->header; // prologue footer = same as header
        // insert into free list
        insert_free_list(&sf_free_list_heads[NUM_FREE_LISTS-1] ,wilderness);
        // footer set in epilogue
        pages += 1;

        // create epilogue. (only need header, prev_footer) [al: 1, sz:       0, pal: 0]
        //sf_block *epilogue = place_block(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)), create_header(0, 0, THIS_BLOCK_ALLOCATED));
        sf_block *epilogue = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
        epilogue->header = create_header(0, 0, THIS_BLOCK_ALLOCATED);
        epilogue->prev_footer = wilderness->header;
        //end, has no next block
    }

    //determine size of block to be allocated
    int block_size = size + 8; //add header size +8
    int remainder = block_size%M;
    if(remainder != 0){
        remainder = M-(remainder); //add padding = multiple of 64 alignment
    }
    block_size += remainder;

    //search for smallest free list to satisfy request size->end ////
    // block not found - use wilderness block (if it exists)
    for(int i =0; i<NUM_FREE_LISTS; i++) {
        sf_block *free_list = &sf_free_list_heads[i]; ////
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
    }

    // wilderness block does not exist
    // wilderness block is not big enough > create more space in memory
    // old epilogue = header block
    sf_block *new_block = ((sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer))));
    // coalesce with previous wilderness block - if it exist
    if(get_prev_alloc_bit(new_block) == 0) {
        new_block = get_prev_block(new_block); // wilderness block
        // do not coalesce with prologue/header
        //set_block_size(new_block, get_block_size(new_block)+sizeof(sf_header)+sizeof(sf_footer));
        // update footer in epilogue (no need to set pal, same)
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
        //coalesce
        set_block_size(new_block, get_block_size(new_block)+PAGE_SZ);

        // create new epilogue
        //sf_block *new_epilogue = place_block(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)), create_header(0, 0, THIS_BLOCK_ALLOCATED));
        /*sf_block *new_epilogue = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
        new_epilogue->header = create_header(0, 0, THIS_BLOCK_ALLOCATED);
        new_epilogue->prev_footer = new_block->header;
        //end, has no next block*/
    }
    // create new epilogue
    //sf_block *new_epilogue = place_block(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)), create_header(0, 0, THIS_BLOCK_ALLOCATED));
    sf_block *new_epilogue = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
    new_epilogue->header = create_header(0, 0, THIS_BLOCK_ALLOCATED);
    new_epilogue->prev_footer = new_block->header;
    //end, has no next block

    // insert new wilderness block to the beginning of the last free list
    return split_block(new_block, block_size)->body.payload; // return pointer to allocated block
}


void sf_free(void *pp) {
    sf_block *block = (sf_block*)(((char *)pp)-(sizeof(sf_header)+sizeof(sf_footer))); // pp = payload pointer
    //sf_block *prologue = (sf_block*)(sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer)))); // pp = payload pointer
    //sf_block *epilogue = (sf_block*)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer))); // pp = payload pointer

    // verify pointer
    // invalid pointer
    if ((pp == NULL)
        || ((((long int)pp)%M) != 0)
        || (get_alloc_bit(block) == 0)
        ////////////////////////////////////////////////////////////////////////////////////////////////////
        //|| (((long int)&(block->header)) < ((long int)get_next_block(prologue))) || (((long int)&(block->header)) < ((long int)epilogue))
        || ((get_prev_alloc_bit(block) == 0) && (get_alloc_bit(get_prev_block(block)) != 0))
        ) {
        // pointer == NULL
        // pointer not alligned to 64-bytes
        // allocated bit in header = 0
        // header is before the end of the prologue || after the beginning of the epilogue
        // prev_alloc = 0 && alloc(previous block)!=0
        abort(); // exit program
    }

    /*
    // set alloc = 0
    block->header &= ~(THIS_BLOCK_ALLOCATED);

    // valid pointer
    // coalesce block with adjacent free blocks
    // determine size class for new coalesced free block
    sf_block *next_block = get_next_block(block);
    sf_block *prev_block = get_prev_block(block);
    if(get_prev_alloc_bit(block) == 0) {
        delete_free_list(prev_block);
        coalesce_block(prev_block, block);
    }
    if(get_alloc_bit(next_block) == 0) {
        delete_free_list(next_block);
        coalesce_block(block, next_block);
    }
    //update footer
    get_next_block(block)->prev_footer = block->header;

    next_block->header &= ~PREV_BLOCK_ALLOCATED;

    // insert block at the beginning of the free list of appropriate size class
    insert_free_list(get_free_list(block), block); // mainting free list
*/


    // valid pointer
    // set alloc = 0
    // added into the free list, footers updated
    block = place((char *)(block), create_header(get_block_size(block), get_prev_alloc_bit(block), 0));

    // coalesce block with adjacent free blocks
    // determine size class for new coalesced free block
    sf_block *next_block = get_next_block(block);
    sf_block *prev_block = get_prev_block(block);
    if(get_prev_alloc_bit(block) == 0) {
        coalesce_block(prev_block, block);
    }
    if(get_alloc_bit(next_block) == 0) {
        coalesce_block(block, next_block);
    }

    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    return NULL;
}
