#pragma once
#ifndef __MEMORY_MANAGER__
#define __MEMORY_MANAGER__ 1

#define DEBUG 1

#if DEBUG == 1
    #endif
    #include <iostream>
    using namespace std;

#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include "glthread.h"
#include "styles.h"
#include <iomanip>

static size_t SYSTEM_PAGE_SIZE = 0;
static size_t current_num_of_pages = 0;

#define MM_MAX_STRUCT_NAME 32

#define OFFSET_OF(struct_name, field_name) \
    ((size_t)&(((struct_name*)0)->field_name))

struct vm_page_t;

struct vm_page_family_t {
    char struct_name[MM_MAX_STRUCT_NAME];
    uint32_t struct_size;
    vm_page_t *first_page;
    glthread_t free_block_priority_list_head;
};

struct vm_page_for_families_t {
    vm_page_for_families_t *next;
    vm_page_family_t vm_page_family[0];
};

struct block_meta_data_t {
    bool is_free;
    uint32_t block_size;
    uint32_t offset;
    glthread_t priority_thread_glue;
    block_meta_data_t *prev;
    block_meta_data_t *next;
};

GLTHREAD_TO_STRUCT(glthread_to_block_meta_data, block_meta_data_t, priority_thread_glue
                    , glthread_ptr);

struct vm_page_t {
    vm_page_t *next;
    vm_page_t *prev;
    vm_page_family_t *pg_family;
    block_meta_data_t block_meta_data;
    char page_memory[0];
};

static vm_page_for_families_t *first_vm_page_for_families = NULL;

#define MAX_FAMILIES_PER_VM_PAGE \
    (SYSTEM_PAGE_SIZE - sizeof(vm_page_for_families_t*)) / sizeof(vm_page_family_t)

#define ITERATE_PAGE_FAMILIES_BEGIN(ptr, curr) \
{ \
    uint32_t count = 0; \
    for (curr = (vm_page_family_t*)&ptr->vm_page_family[0]; \
        curr->struct_size && count < MAX_FAMILIES_PER_VM_PAGE; \
        curr++, count++) {

#define ITERATE_PAGE_FAMILIES_END(ptr, curr) }}

#define MM_GET_PAGE_FROM_META_BLOCK(block_meta_data_ptr) \
    ((void *)((char *)block_meta_data_ptr - block_meta_data_ptr->offset))

#define NEXT_META_BLOCK(block_meta_data_ptr)  \
    block_meta_data_ptr->next;

#define NEXT_META_BLOCK_BY_SIZE(block_meta_data_ptr) \
    (block_meta_data_t *)((char *)(block_meta_data_ptr + 1) \
        + block_meta_data_ptr->block_size)

#define PREV_META_BLOCK(block_meta_data_ptr)    \
    (block_meta_data_ptr->prev_block)

#define MM_BIND_BLOCKS_FOR_ALLOCATION(allocated_meta_block, free_meta_block) \
    free_meta_block->prev = allocated_meta_block; \
    free_meta_block->next = allocated_meta_block->next; \
    allocated_meta_block->next = free_meta_block; \
    if (free_meta_block->next) { \
        free_meta_block->next->prev = free_meta_block; \
    }

#define MARK_VM_PAGE_EMPTY(vm_page_t_ptr) \
    vm_page_t_ptr->block_meta_data.next = NULL; \
    vm_page_t_ptr->block_meta_data.prev = NULL; \
    vm_page_t_ptr->block_meta_data.is_free = true; \
    vm_page_t_ptr->block_meta_data.block_size = SYSTEM_PAGE_SIZE - sizeof(block_meta_data_t)

#define ITERATE_VM_PAGE_ALL_BLOCKS_BEGIN(vm_page_ptr, curr)    \
{\
    curr = &vm_page_ptr->block_meta_data;\
    block_meta_data_t *next = NULL;\
    for( ; curr; curr = next){\
        next = NEXT_META_BLOCK(curr);

#define ITERATE_VM_PAGE_ALL_BLOCKS_END(vm_page_ptr, curr)   \
    }}

#define NEXT_META_BLOCK_BY_SIZE(block_meta_data_ptr)    \
    (block_meta_data_t *)((char *)(block_meta_data_ptr + 1) \
        + block_meta_data_ptr->block_size)

void init();
void instantiate_new_page_family(char *struct_name, uint32_t struct_size);
void print_registered_page_families();
vm_page_family_t* lookup_page_family_by_name(char *struct_name);
bool is_vm_page_empty(vm_page_t *page);
vm_page_t* allocate_vm_page(vm_page_family_t *vm_page_family);
void vm_page_delete_and_free(vm_page_t *vm_page);
void* xmalloc(char *struct_name, int units);
void mm_print_memory_usage(char *struct_name);
void mm_print_usage_summary();

#define MM_REG_STRUCT(struct_name) \ 
    (instantiate_new_page_family(#struct_name, sizeof(struct_name)))

#endif