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

// helper functions
void initialize_free_lists() {
    for(int i=0; i<NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}
void insert_free_list(sf_block *free_list, sf_block *block) {
    block = (sf_block*)(block);
    // add to front of the list
    sf_block *sentinel = (sf_block *)((void *)free_list);
    block->body.links.next = sentinel->body.links.next;
    sentinel->body.links.next = block;
    block->body.links.prev = sentinel;
    block->body.links.next->body.links.prev = block;
}
void delete_free_list(sf_block *block) {
    block = (sf_block*)(block);
    block->body.links.prev->body.links.next = block->body.links.next; // b->prev->next = b->next
    block->body.links.next->body.links.prev = block->body.links.prev; // b->next->prev = b->prev
}
sf_block *get_next_block(sf_block *block) {
    return (sf_block *)(((void *)block)+((block->header)&BLOCK_SIZE_MASK));
}
sf_block *get_prev_block(sf_block *block) {
    return (sf_block *)(((void *)block)-((block->prev_footer)&BLOCK_SIZE_MASK));
}
long int get_block_size(sf_block *block){
    return block->header & BLOCK_SIZE_MASK;
}
long int get_alloc_bit(sf_block *block) {
    return block->header & THIS_BLOCK_ALLOCATED;
}
long int get_prev_alloc_bit(sf_block *block) { // returns non 0 if set
    return block->header & PREV_BLOCK_ALLOCATED;
}
sf_header create_header(size_t size, long int prev_alloc, long int alloc) {
    sf_header header = ((size&BLOCK_SIZE_MASK)|(prev_alloc)|(alloc));
    return header;
}
int get_free_list_index(size_t size) {
    if(size>0 && size<=M) {
        return 0;
    } else if(size>M && size<=2*M) {
        return 1;
    } else if(size>2*M && size<=3*M) {
        return 2;
    } else if(size>3*M && size<=5*M) {
        return 3;
    } else if(size>5*M && size<=8*M) {
        return 4;
    } else if(size>8*M && size<=13*M) {
        return 5;
    } else if(size>13*M && size<=21*M) {
        return 6;
    } else if(size>21*M && size<=34*M) {
        return 7;
    } else if(size>34*M) {
        return 8;
    }
    return 9;
}
sf_block *get_free_list(sf_block *block) {
    block = (sf_block*)(block);
    sf_block *epilogue = (sf_block*)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
    // if = wildernes block
    if(get_next_block(block) == epilogue) {
        return &sf_free_list_heads[NUM_FREE_LISTS-1];
    }
    long int size = get_block_size(block);
    return &sf_free_list_heads[get_free_list_index(size)];
}
sf_block *place(void *address, sf_header header) {
    // heap already exists and is initialized
    sf_block *block = (sf_block *)(((void *)address));
    int to_delete_from_free_list = get_alloc_bit(block); // get old alloc bit
    block->header = header;

    sf_block *next_block = get_next_block(block);
    sf_block *epilogue = (sf_block*)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer))); ////
    if(get_prev_alloc_bit(block) == 0) { // free block, set footer
        sf_block *prev_block = get_prev_block(block);
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
sf_block *coalesce_block(sf_block *first, sf_block *second) {
    first = (sf_block *)(first);
    second = (sf_block *)(second);
    if(get_alloc_bit(second) == 0) { // delete combining block from the free list
        delete_free_list(second);
    }
    if(get_alloc_bit(first) == 0) {
        delete_free_list(first);
    }
    // combine into one block
    place(first, create_header(get_block_size(first)+(get_block_size(second)), get_prev_alloc_bit(first), get_alloc_bit(first)));
    return first;
}
sf_block *split_block(sf_block *block, size_t size) {
    block = (sf_block*)(block);
    long int free_block_size = get_block_size(block) - size;
    if(get_alloc_bit(block) == 0) {
        delete_free_list(block); // remove from free list (reassigned later)
    }

    // split without creating a splinter (<=alignment) : 'over alloc'
    // exact size. do not need to split. use entire block
    if((free_block_size < M) || (free_block_size == 0)) {
        // alloc = 1
        block->header |= THIS_BLOCK_ALLOCATED;
    } else {
        // split and assign blocks
        // lower part = allocation request [al: 1, sz:       size, p.al: block.pal]
        block->header = create_header(size, get_prev_alloc_bit(block), THIS_BLOCK_ALLOCATED);
        // upper part = remainder [al: 0, sz:       get_block_size(block) - size, pal: 1]
        sf_block *free_block = place(get_next_block(block), create_header(free_block_size, PREV_BLOCK_ALLOCATED, 0));
        // coalesce with wilderness if it exists. and not == free block
        sf_block *epilogue = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
        if((get_prev_alloc_bit(epilogue) == 0) && (get_prev_block(epilogue) != free_block)) {
            coalesce_block(free_block, get_prev_block(epilogue));
        }
    }
    return block;
}
int is_invalid_pointer(void *pp){
    sf_block *block = (sf_block *)(((void *)pp)-(sizeof(sf_header)+sizeof(sf_footer))); // pp = payload pointer
    sf_block *prologue = (sf_block*)(sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer)))); // pp = payload pointer
    sf_block *epilogue = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer))); // pp = payload pointer
    if ((pp == NULL)
        || ((((long int)pp)%M) != 0)
        || (get_alloc_bit(block) == 0)
        || (((void *)(&(block->header))) < ((void *)get_next_block(prologue)))
        || (((void *)(&(block->header))) > ((void *)epilogue))
        || ((get_prev_alloc_bit(block) == 0) && (get_alloc_bit(get_prev_block(block)) != 0))
        ) { // invalid pointer
        // pointer == NULL
        // pointer not alligned to 64-bytes
        // allocated bit in header = 0
        // header is before the end of the prologue
        // header is after the beginning of the epilogue
        // prev_alloc = 0 && alloc(previous block)!=0
        return 1;
    }
    return 0;
}

void *sf_malloc(size_t size) {
    if(size == 0) {
        return NULL;
    } // else size !=0
    // initialize heap
    if(sf_mem_start()==sf_mem_end()) { // initial call, if heap is empty
        // initialize free lists: list is empty
        initialize_free_lists();

        // extend heap
        sf_block *ptr = sf_mem_grow();
        if(ptr == NULL) { // no more space to allocate
            sf_errno = ENOMEM;
            return NULL;
        }
        // create prologue [al: 1, sz:       64, pal: 1]
        sf_block *prologue = (sf_block *)(sf_mem_start()+(M-(sizeof(sf_header)+sizeof(sf_footer))));
        prologue->header = create_header(M, PREV_BLOCK_ALLOCATED, THIS_BLOCK_ALLOCATED);
        // no prev_footer

        // wilderness block 112 [al: 0, sz:       3968, pal: 1]
        sf_block *wilderness = (sf_block *)((void *)(prologue)+M);
        wilderness->header = create_header(PAGE_SZ-((M-(sizeof(sf_header)+sizeof(sf_footer)))+M)-(2*sizeof(sf_header)), PREV_BLOCK_ALLOCATED, 0);
        wilderness->prev_footer = prologue->header; // prologue footer = same as header
        // insert into free list
        insert_free_list(&sf_free_list_heads[NUM_FREE_LISTS-1] ,wilderness);
        // footer set in epilogue

        // create epilogue. (only need header, prev_footer) [al: 1, sz:       0, pal: 0]
        sf_block *epilogue = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
        epilogue->header = create_header(0, 0, THIS_BLOCK_ALLOCATED);
        epilogue->prev_footer = wilderness->header;
        // end, has no next block
    }
    // determine size of block to be allocated
    int block_size = size + sizeof(sf_header); // add header size +8
    int remainder = block_size%M;
    if(remainder != 0){
        remainder = M-(remainder); // add padding = multiple of 64 alignment
    }
    block_size += remainder;

    // block placement
    // free list searched starting from the list for the determined size class
    sf_block *free_list = &sf_free_list_heads[get_free_list_index(size)];
    while(free_list != &sf_free_list_heads[NUM_FREE_LISTS-1]) {
        sf_block *found_block = ((sf_block *)(free_list))->body.links.next; // head = sentinel
        // first-fit policy : get first block in the list
        while(found_block != free_list) { // search link list
            if(get_block_size(found_block) >= block_size) { // block found
                // success return pointer to of request to payload/valid regin of memory
                return split_block(found_block, block_size)->body.payload;
            }
        }
        //if no block found, continue to next size class
        free_list += 1; // move to the next free list
    }
    // block not found - use wilderness block (if it exists)
    // exists when epilogue prev_alloc = 0
    sf_block *epilogue = (sf_block*)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
    // if = wildernes block
    if(get_prev_alloc_bit(epilogue) == 0) { // wilderness exists
        sf_block *wilderness = get_prev_block(epilogue);
        if(get_block_size(wilderness) >= block_size) { // wilderness has enough space
            return split_block(wilderness, block_size)->body.payload;
        }
    }

    // wilderness block does not exist || wilderness block is not big enough > create more space in memory
    // old epilogue = header block
    sf_block *new_block = ((sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer))));
    // coalesce with previous wilderness block - if it exist
    if(get_prev_alloc_bit(new_block) == 0) {
        new_block = get_prev_block(new_block); // wilderness block
        // do not coalesce with prologue/header
        // update footer in epilogue (no need to set pal, same)
    }
    // block size not found. block_size > wilderness
    int total_size = block_size - get_block_size(new_block);
    int pages = total_size/PAGE_SZ; // how many more pages of memory needed
    if(total_size % PAGE_SZ) { // need to alloc another page
        pages += 1;
    }
    for(int i=0; i<pages; i++) { // call multiple times if size needed is multiple pages
        sf_block *ptr = sf_mem_grow(); // extend heap
        if(ptr == NULL) { // no more space to allocate
            sf_errno = ENOMEM;
            return NULL;
        }
        // coalesce
        new_block->header = create_header(get_block_size(new_block)+PAGE_SZ, get_prev_alloc_bit(new_block), get_alloc_bit(new_block));

        // create new epilogue
        sf_block *new_epilogue = (sf_block *)(sf_mem_end()-(sizeof(sf_header)+sizeof(sf_footer)));
        new_epilogue->header = create_header(0, 0, THIS_BLOCK_ALLOCATED);
        new_epilogue->prev_footer = new_block->header;
        // end, has no next block
    }
    // insert new wilderness block to the beginning of the last free list
    return split_block(new_block, block_size)->body.payload; // return pointer to allocated block
}

void sf_free(void *pp) {
    sf_block *block = (sf_block *)(((void *)pp)-(sizeof(sf_header)+sizeof(sf_footer))); // pp = payload pointer
    // verify pointer
    if(is_invalid_pointer(pp)) { // invalid pointer
        abort();
    }
    // valid pointer
    // set alloc = 0
    // set footer
    // add to free list
    // set pal
    place(block, create_header(get_block_size(block), get_prev_alloc_bit(block), 0));

    // coalesce block with adjacent free blocks and re-insert into appropriate free list
    if(get_prev_alloc_bit(block) == 0) {
        sf_block *prev_block = get_prev_block(block);
        block = coalesce_block(prev_block, block);
    }
    sf_block *next_block = get_next_block(block);
    if(get_alloc_bit(next_block) == 0) {
        coalesce_block(block, next_block);
    }
    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    // invalid pointer
    if(is_invalid_pointer(pp)) {
        sf_errno = EINVAL;
        return NULL;
    }
    // valid pointer
    sf_block *block = (sf_block *)(((void *)pp)-(sizeof(sf_header)+sizeof(sf_footer))); // pp = payload pointer
    if(rsize == 0) {
        sf_free(block); // free block
        return NULL;
    }
    int block_size = rsize + sizeof(sf_header); // add header size +8
    // if re-size = size, no need to reallocate
    if(get_block_size(block) == block_size) {
        return pp;
    }
    // larger size
    if(get_block_size(block) < block_size) {
        // call sf_malloc to obtain a larger block
        void *reallocated_pointer = sf_malloc(rsize);
        // no more memory
        if(reallocated_pointer == NULL) { // sf_errno = ENOMEM, set in malloc
            return NULL;
        }
        // call memcpy to copy the data
        memcpy(reallocated_pointer, pp, rsize);
        // call free on the original block
        sf_free(pp); // coalescing done in free
        // return block from sf_malloc
        return reallocated_pointer;
    }
    // smaller size
    if(get_block_size(block) > block_size) {
        // determine size of block to be allocated
        int remainder = block_size%M;
        if(remainder != 0){
            remainder = M-(remainder); // add padding = multiple of 64 alignment
        }
        block_size += remainder;
        return split_block(block, block_size)->body.payload;
    }
    return NULL;
}

void *sf_memalign(size_t size, size_t align) {
    if(size == 0) { // if size = 0, return NULL without setting sf_errno
        return NULL;
    }
    // requested alignment is at least the minimum block size
    // requested alignment is a power of two
    if((align < M) || ((align & (align-1)) != 0)) {
        sf_errno = EINVAL;
        return NULL;
    }
    // obtain memory with requestded alignment, allocate larged block than requested
    // allocated block with size = requested size + alignment size + minimum block size + header size
    void *block_payload = sf_malloc(size + align + M + sizeof(sf_header));
    // if allocation is not successful, return NULL and set sf_serrno to ENOMEM
    if(block_payload == NULL) {
        // sf_errno = EINVAL; // set in malloc
        return NULL;
    }

    sf_block *block = ((void *)block_payload) - (sizeof(sf_header) + sizeof(sf_footer));
    // determine size of block to be allocated
    int block_size = size + sizeof(sf_header); // add header size +8
    int remainder = block_size%M;
    if(remainder != 0){
        remainder = M-(remainder); // add padding = multiple of 64 alignment
    }
    block_size += remainder;

    int move_up = 0;
    int alignment_remainder = ((long int)block_payload)%align;
    if(alignment_remainder != 0){
        alignment_remainder = align-(alignment_remainder);
    }
    move_up += alignment_remainder;
    // normal payload address satisfies the requested alignment
    if(move_up == 0) { // if (((long int)block_payload)%align == 0) = aligned
        // do nothing
    }
    // larger address within the block that satisfies the requested alignment
    // has sufficient space after to hold requested size payload
    // initial portion of the block can be split off (without splitner = at least minimum size) and freed
    if (move_up >= M) {
        int split_size = get_block_size(block) - move_up;

        // initial portion
        sf_block *initial_portion = ((sf_block *)block);
        initial_portion->header = create_header(move_up, get_prev_alloc_bit(block), get_alloc_bit(block));

        block = ((void *)block) + move_up;
        place(block, create_header(split_size, 0, THIS_BLOCK_ALLOCATED));

        sf_free(initial_portion->body.payload);
    }
    // allocation is successful
    // return pointer to valid region of memory of requested size and with requested alignment ->payload
    // if block is still too large for requested size, split and free unused reaminder
    return split_block(block, block_size)->body.payload;
}
