/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include "sfmm.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "errno.h"
#include "remove.h"

/**
 * You should store the heads of your free lists in these variables.
 * Doing so will make it accessible via the extern statement in sfmm.h
 * which will allow you to pass the address to sf_snapshot in a different file.
 */
free_list seg_free_list[4] = {
    {NULL, LIST_1_MIN, LIST_1_MAX},
    {NULL, LIST_2_MIN, LIST_2_MAX},
    {NULL, LIST_3_MIN, LIST_3_MAX},
    {NULL, LIST_4_MIN, LIST_4_MAX}
};

int sf_errno = 0;


void *sf_malloc(size_t size) {
    //size = payload
    int i;
    int remainder = size % 16;
    int changed = 0;
    sf_free_header * ptr;// use to traverse through blocks
    if (size > 16384 || size == 0){
        sf_errno = EINVAL;
        return NULL;
    }
    if (remainder != 0){
        size += (16-(remainder));
        changed = 1;
    }
    check_free_list:
    for (i = 0; i < 4; i++){
        if ((size+16 >= seg_free_list[i].min && size+16 <= seg_free_list[i].max) || size <= seg_free_list[i].max){
            ptr = seg_free_list[i].head; //header
            //printf("ptr: %p\n", ptr);
            //printf("%X\n", seg_free_list[i].head->header);
            while (ptr != NULL){ //if there are no more blocks in the list, exit
                if ((ptr->header.block_size << 4)-16 >= size){ //find the first fit block
                    uint64_t remaining_block_size = (ptr->header.block_size << 4) - (size+16);
                    ptr->header.allocated = 1; //change allocated header bit to 1
                    if (remaining_block_size > 0 && remaining_block_size < 32){ //check for splinters
                        ptr->header.padded = 1;
                        sf_footer * footer = (sf_footer *) ((char *)ptr + (ptr->header.block_size << 4) - 8); //move pointer to end of block
                        //printf("%p\n", footer);
                        if (changed == 1) // if size was padded
                            footer->requested_size = size - (16-(remainder));
                        else
                            footer->requested_size = size;
                        footer->block_size = ptr->header.block_size;
                        //footer->block_size >>= 4; //shift to get 28 bits
                        footer->allocated = 1;
                        footer->padded = 1;
                        if (seg_free_list[i].head->next == NULL);{
                            seg_free_list[i].head = seg_free_list[i].head->next;
                            return (void *)ptr + 8;
                        }
                        seg_free_list[i].head = seg_free_list[i].head->next;
                        seg_free_list[i].head->prev = NULL;
                        return (void *)ptr + 8;
                    }
                    else{
                        ptr->header.block_size = size+16; //change block_size to the size + header and footer length
                        ptr->header.block_size >>= 4; //shift to right
                        ptr->header.unused = 0;
                        sf_footer * footer = (sf_footer *)((char *)ptr + (ptr->header.block_size << 4) - 8); //make last 8 bytes footer
                        if (changed == 1){
                            ptr->header.padded = 1;
                            footer->requested_size = size - (16-(remainder));
                            footer->padded = 1;
                        }
                        else{
                            ptr->header.padded = 0;
                            footer->padded = 0;
                            footer->requested_size = size; //change footer requested_size to bytes
                        }
                        footer->block_size = size+16; // change footer block_size to size + header and footer length
                        footer->block_size >>= 4;
                        footer->allocated = 1; //change footer allocated bit to 1
                        //set the header and footer of the remaining block
                        sf_free_header * remaining_block = (sf_free_header *) ((char *)footer + 8); //remaining block header
                        //printf("rem_block_header: %p\n", remaining_block);
                        //initialize the header components for remaining block
                        remaining_block->header.block_size = remaining_block_size;
                        remaining_block->header.block_size >>= 4;
                        remaining_block->header.allocated = 0;
                        remaining_block->header.padded = 0;
                        remaining_block->header.two_zeroes = 0;
                        remaining_block->header.unused = 0;
                        //initialize footer components for remainder of block
                        sf_footer * rem_block_footer = (sf_footer *) ((char *)remaining_block + remaining_block_size - 8);
                        rem_block_footer->requested_size = 0;
                        rem_block_footer->allocated = 0;
                        rem_block_footer->block_size = remaining_block_size;
                        rem_block_footer->block_size >>= 4;
                        rem_block_footer->padded = 0;
                        rem_block_footer->two_zeroes = 0;
                        //check if new block size is still within range
                        if ((remaining_block->header.block_size << 4) >= seg_free_list[i].min && (remaining_block->header.block_size << 4) <= seg_free_list[i].max){
                            remaining_block->prev = (sf_free_header *) NULL; //seg_free_list[i].head->prev;
                            //printf("prev: %p\n", remaining_block.prev);s
                            remaining_block->next = seg_free_list[i].head->next;
                            seg_free_list[i].head->next = (sf_free_header *) NULL;
                            seg_free_list[i].head->prev = (sf_free_header *) NULL;
                            seg_free_list[i].head = remaining_block;
                        } //else check free list for appropriate size
                        else{
                            int j;
                            for (j = 0; j < 4; j++){
                                if (remaining_block_size >= seg_free_list[j].min && remaining_block_size <= seg_free_list[j].max){
                                    if (seg_free_list[j].head == NULL){
                                        seg_free_list[j].head = remaining_block;
                                        if (seg_free_list[i].head->next == NULL){
                                            seg_free_list[i].head = NULL;
                                        }
                                        else{
                                            seg_free_list[i].head = seg_free_list[i].head->next;
                                            seg_free_list[i].head->prev = NULL;
                                        }
                                    }
                                    else{
                                        if (seg_free_list[i].head->next == NULL)
                                            seg_free_list[i].head = NULL;
                                        else{
                                            seg_free_list[i].head =seg_free_list[i].head->next;
                                            seg_free_list[i].head->prev = NULL;
                                        }
                                        remaining_block->prev = (sf_free_header *) NULL; //prev of new head is null
                                        remaining_block->next = seg_free_list[j].head; //next of new head is old head
                                        seg_free_list[j].head->prev = remaining_block;
                                        seg_free_list[j].head = remaining_block;
                                    }
                                }
                            }
                            if (remaining_block_size == 0){
                                seg_free_list[i].head->next = NULL;
                                seg_free_list[i].head->prev = NULL;
                                seg_free_list[i].head = NULL;
                            }
                        }
                    }
                    return (void *)ptr + 8; //returns starting address of payload
                }
                ptr = ptr->next;
            }
        }
    }
    //set the header of new page
    sf_free_header * list_header = (sf_free_header *) sf_sbrk();
    //printf("sbrk: %p\n", list_header);
    if (list_header == (void *) -1){ //if pages > 4, return NULL
        errno = ENOMEM;
        return NULL;
    }
    list_header->header.allocated = 0;
    list_header->header.block_size = 4096;
    list_header->header.block_size >>= 4;
    list_header->header.padded = 0;
    sf_footer * footer = (sf_footer *) ((char *)get_heap_end() - 8);
    //printf("new block footer: %p\n", footer);
    footer->requested_size = 0;
    footer->block_size = 4096;
    footer->block_size >>= 4;
    footer->allocated = 0;
    footer->padded = 0;
    //printf("%p\n", &(list_header.header));
    //printf("%p\n", header);
    //list_header->next = (sf_free_header *) NULL;
    //list_header->prev = (sf_free_header *) NULL;
     //put into last list
    if (seg_free_list[3].head == NULL){
        seg_free_list[3].head = list_header; //if head is null, set list header as new head
    }
    else{
        seg_free_list[3].head->header.block_size += list_header->header.block_size; //if not null, then coalesce
        footer->block_size = seg_free_list[3].head->header.block_size; //change footer's block size
    }
    goto check_free_list;
}




void *sf_realloc(void *ptr, size_t size) {
    sf_header * header = (sf_header *) ((char *)ptr - 8);
    size_t block_size = (size_t) header->block_size << 4;
    sf_footer * footer = (void *)header + (header->block_size<<4) - 8;
    if (size == 0){
        sf_free(ptr);
        return NULL;
    }
    if (ptr == (void *) NULL || ptr < get_heap_start() || ptr > get_heap_end())
        return NULL;
    if (header->allocated == 0 || (header->allocated != footer->allocated) ||
        (header->padded == 1 && ((footer->block_size<<4) - 16) == footer->requested_size) ||
        header->padded != footer->padded || (header->padded == 0 && ((footer->block_size<<4) - 16) != footer->requested_size))
        return NULL;
    if (size > block_size){
        void * new_ptr = sf_malloc(size);
        if (new_ptr == NULL)
            return NULL;
        memcpy((void *)new_ptr-8, (void *)ptr-8, block_size);
        sf_free(ptr);
        if (size % 16 != 0)
            size = size + (16-(size % 16));
        sf_free_header * new_ptr_head = (sf_free_header *) ((char*)new_ptr - 8);
        new_ptr_head->header.block_size = (uint64_t)size+16;
        new_ptr_head->header.block_size >>= 4;
        sf_footer * new_footer = (sf_footer *) ((char *)new_ptr + (new_ptr_head->header.block_size<<4) - 16);
        if (new_footer->padded == 1)
            new_ptr_head->header.padded = 1;
        else
            new_ptr_head->header.padded = 0;
        return (void *) new_ptr;
    }
    else if (size < block_size){
        if (block_size - (size+16) < 32){
            footer->requested_size = (uint64_t) size;
            footer->padded = 1;
            header->padded = 1;
            return (void *)ptr;
        }
        else{
            int changed = 0;
            if (size % 16 != 0){
                size += (16-(size % 16));
                changed = 1;
            }
            header->block_size = size + 16;
            header->block_size >>= 4;
            //create a new footer
            sf_footer * relocated = (sf_footer *) ((char *)header+(header->block_size<<4)-8);
            relocated->block_size = size + 16;
            relocated->block_size >>= 4;
            relocated->allocated = 1;
            relocated->requested_size = size;
            if (changed == 1){
                header->padded = 1;
                relocated->padded = 1;
            }
            else{
                header->padded = 0;
                relocated->padded = 0;
            }

            footer->block_size = block_size-(size+16);
            footer->requested_size = footer->block_size - 16;
            footer->block_size >>= 4;
            footer->allocated = 1;
            //header after the relocated footer for allocated block
            sf_free_header * next_header = (sf_free_header *)((char *)relocated + 8);
            next_header->header.block_size = block_size - (size+16);
            next_header->header.block_size >>= 4;
            next_header->header.allocated = 1;
            if ((next_header->header.block_size << 4) % 16 != 0){
                next_header->header.padded = 1;
                footer->padded = 1;
            }
            else{
                next_header->header.padded = 0;
                footer->padded = 0;
            }
            next_header->header.two_zeroes = 0;
            next_header->header.unused = 0;
            //free latter block
            sf_free((void *)next_header+8);

            return (void *)header+8;
        }
    }
    else
        return ptr;
}

void sf_free(void *ptr) {
    int i;
    sf_footer * var_footer;
    sf_footer * next_footer;
    sf_header * header;
    sf_header * next_header;
    sf_free_header * list_head;
    header = (sf_header *) ((char *)ptr - 8);
    var_footer = (sf_footer *) ((char *)header + (header->block_size  << 4) - 8);
    if (ptr == (void *) NULL || ptr < get_heap_start() || ptr > get_heap_end())
        abort();
    if (header->allocated == 0 || (header->allocated != var_footer->allocated) ||
        (header->padded == 1 && ((var_footer->block_size<<4) - 16) == var_footer->requested_size) ||
        header->padded != var_footer->padded || (header->padded == 0 && ((var_footer->block_size<<4) - 16) != var_footer->requested_size))
        abort();
    header->allocated = 0;
    header->padded = 0;
    var_footer->allocated = 0;
    var_footer->padded = 0;
    if ((void *) var_footer == get_heap_end()){
        next_header = (sf_header *) NULL;
        next_footer = (sf_footer *) NULL;
    }
    else{
        next_header = (sf_header *) ((char *)var_footer + 8);
        next_footer = (sf_footer *) ((char *)next_header + (next_header->block_size << 4)-8);
    }
    if (next_footer == (sf_footer *) NULL){
        for (i = 0; i < 4; i++){
            if ((header->block_size << 4) >= seg_free_list[i].min && (header->block_size << 4) <= seg_free_list[i].max){
                /*
                list_head->next = seg_free_list[i].head;
                seg_free_list[i].head->prev = list_head;
                seg_free_list[i].head = list_head;
                */
                list_head = (sf_free_header *) ((char *) ptr - 8);
                if (seg_free_list[i].head == NULL)
                    seg_free_list[i].head = list_head;
                else{
                    list_head->next = seg_free_list[i].head;
                    seg_free_list[i].head->prev = list_head;
                    seg_free_list[i].head = list_head;
                }
            }
        }
    }
    else{
        if (next_header->allocated == 1){
            for (i = 0; i < 4; i++){
                if ((header->block_size << 4) >= seg_free_list[i].min && (header->block_size << 4) <= seg_free_list[i].max){
                    list_head = (sf_free_header *) ((char *) ptr - 8);
                    if (seg_free_list[i].head == NULL){
                        seg_free_list[i].head = list_head;
                        seg_free_list[i].head->next =  NULL;
                        seg_free_list[i].head->prev = NULL;
                    }
                    else{
                        list_head->next = seg_free_list[i].head;
                        seg_free_list[i].head->prev = list_head;
                        seg_free_list[i].head = list_head;
                    }
                }
            }
        }
        else{
            //coalescing
            list_head = (sf_free_header *)((char *)ptr - 8);
            list_head->next = (sf_free_header *) NULL;
            list_head->header.block_size += next_footer->block_size;
            sf_free_header * middle_header = (sf_free_header *)next_header;
            remove_from_list(middle_header);
            next_footer->block_size = list_head->header.block_size;
            for (i = 0; i < 4; i++){
                if ((list_head->header.block_size << 4) >= seg_free_list[i].min && (list_head->header.block_size<< 4) <= seg_free_list[i].max){
                    if (seg_free_list[i].head == NULL)
                        seg_free_list[i].head = list_head;
                    else{
                        list_head->next = seg_free_list[i].head;
                        seg_free_list[i].head->prev = list_head;
                        seg_free_list[i].head = list_head;
                    }
                }
            }
        }
    }

    return;
}

void remove_from_list(sf_free_header * header){
    int i;
    for (i = 0; i < 4; i++){
        if ( (header->header.block_size << 4) >= seg_free_list[i].min && (header->header.block_size << 4) <= seg_free_list[i].max){
            sf_free_header * ptr = seg_free_list[i].head;
            while(ptr != NULL){
                if (ptr == header){
                    if (ptr->prev == NULL)
                        seg_free_list[i].head = ptr->next;
                    else{
                        ptr->prev->next = ptr->next;
                        ptr->next->prev = ptr->prev;
                    }
                    ptr->next = (sf_free_header *) NULL;
                    ptr->prev = (sf_free_header *) NULL;
                    break;
                }
                ptr = ptr->next;
            }
        }
    }
}


