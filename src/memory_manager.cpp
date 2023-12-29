#include "../includes/memory_manager.h"

void init() {
    SYSTEM_PAGE_SIZE = getpagesize();
    if (DEBUG) cout << "PAGE SIZE: " << SYSTEM_PAGE_SIZE << endl;
}

static void* get_new_virtual_page(int units) {
    char *page = (char*)mmap(
        0,
        units * SYSTEM_PAGE_SIZE,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANON | MAP_PRIVATE,
        0, 0
    );
    if (page == MAP_FAILED) {
        printf("Error: Virtual page allocation failed\n");
        return NULL;
    }
    memset(page, 0, units * SYSTEM_PAGE_SIZE);
    current_num_of_pages++;
    return (void*) page;
}

static void return_virtual_page(void *page, int units) {
    if (munmap(page, units * SYSTEM_PAGE_SIZE)) {
        printf("Error: Could not munmap virtual page\n");
    }
}

void instantiate_new_page_family(char *struct_name, uint32_t struct_size) {
    vm_page_family_t *vm_page_family_curr = NULL;
    vm_page_for_families_t *new_vm_page_for_families = NULL;
    if (struct_size > SYSTEM_PAGE_SIZE) {
        printf("Error: struct_size(%s) > SYSTEM_PAGE_SIZE\n", struct_name);
        return;
    }
    if (!first_vm_page_for_families) {
        first_vm_page_for_families = (vm_page_for_families_t*)get_new_virtual_page(1);
        first_vm_page_for_families->next = NULL;
        strncpy(first_vm_page_for_families->vm_page_family[0].struct_name, 
            struct_name, MM_MAX_STRUCT_NAME);
        first_vm_page_for_families->vm_page_family[0].struct_size = struct_size;
        init_glthread(&first_vm_page_for_families->vm_page_family[0].free_block_priority_list_head);
        return;
    }
    uint32_t count = 0;
    ITERATE_PAGE_FAMILIES_BEGIN(first_vm_page_for_families, vm_page_family_curr) {
        if (strncmp(vm_page_family_curr->struct_name, struct_name, MM_MAX_STRUCT_NAME) != 0) {
            count++;
            continue;            
        }
        assert(0);
    } ITERATE_PAGE_FAMILIES_END(first_vm_page_for_families, vm_page_family_curr);
    if (count == MAX_FAMILIES_PER_VM_PAGE) {
        new_vm_page_for_families = (vm_page_for_families_t*) get_new_virtual_page(1);
        new_vm_page_for_families->next = first_vm_page_for_families;
        first_vm_page_for_families = new_vm_page_for_families;
        vm_page_family_curr = &first_vm_page_for_families->vm_page_family[0];
    }
    strncpy(vm_page_family_curr->struct_name, struct_name, MM_MAX_STRUCT_NAME);
    vm_page_family_curr->struct_size = struct_size;
    vm_page_family_curr->first_page = NULL;
    init_glthread(&vm_page_family_curr->free_block_priority_list_head);
}

void print_registered_page_families() {
    vm_page_for_families_t *node = first_vm_page_for_families;
    while (node != NULL) {
        vm_page_family_t *curr_family = node->vm_page_family;
        while (curr_family != NULL && curr_family->struct_size != 0) {
            cout << "#######################################" << endl;
            cout << "struct name: " << curr_family->struct_name << endl;
            cout << "struct size: " << curr_family->struct_size << endl;
            cout << "#######################################" << endl;
            curr_family++;
        }
        node = node->next;
    }
}

vm_page_family_t* lookup_page_family_by_name(char *struct_name) {
    vm_page_for_families_t *node = first_vm_page_for_families;
    while (node != NULL) {
        vm_page_family_t *curr_family = node->vm_page_family;
        while (curr_family != NULL && curr_family->struct_size != 0) {
            if (strncmp(curr_family->struct_name, struct_name, MM_MAX_STRUCT_NAME)
                == 0) {
                return curr_family;
            }
            curr_family++;
        }
        node = node->next;
    }
    return NULL;
}

static void union_free_blocks(block_meta_data_t *first, block_meta_data_t *second) {
    assert(first->is_free == true && second->is_free == true);
    first->block_size += sizeof(block_meta_data_t) + second->block_size;
    first->next = second->next;
    if (first->next) {
        first->next->prev = first;
    }
}

bool is_vm_page_empty(vm_page_t *page) {
    if (page->block_meta_data.next == NULL && 
        page->block_meta_data.prev == NULL && 
        page->block_meta_data.is_free) {
        return true;
    }
    return false;
}

static inline uint32_t max_page_allocatable_memory(int units) {
    return (uint32_t)(SYSTEM_PAGE_SIZE * units) - OFFSET_OF(vm_page_t, page_memory);
}

vm_page_t* allocate_vm_page(vm_page_family_t *vm_page_family) {
    vm_page_t *new_page = (vm_page_t*) get_new_virtual_page(1);
    MARK_VM_PAGE_EMPTY(new_page);
    new_page->block_meta_data.block_size = max_page_allocatable_memory(1);
    new_page->block_meta_data.offset = OFFSET_OF(vm_page_t, block_meta_data);
    init_glthread(&new_page->block_meta_data.priority_thread_glue);
    new_page->next = new_page->prev = NULL;
    new_page->pg_family = vm_page_family;
    if (!vm_page_family->first_page) {
        vm_page_family->first_page = new_page;
        return new_page;
    }
    new_page->next = vm_page_family->first_page;
    vm_page_family->first_page->prev = new_page;
    vm_page_family->first_page = new_page;
    return new_page;
}

void vm_page_delete_and_free(vm_page_t *vm_page) {
    vm_page_family_t *vm_page_family = vm_page->pg_family;
    if (vm_page_family->first_page == vm_page) {
        vm_page_family->first_page = vm_page->next;
        if (vm_page->next) vm_page->next->prev = NULL;
        vm_page->next = vm_page->prev = NULL;
        return_virtual_page((void*)vm_page, 1);
        return;
    }
    if (vm_page->next) vm_page->next->prev = vm_page->prev;
    vm_page->prev->next = vm_page->next;
    return_virtual_page((void*)vm_page, 1);
}

static int free_blocks_comparison_function(void *_block_meta_data1,
                                        void *_block_meta_data2) {
    block_meta_data_t *block_meta_data1 = (block_meta_data_t*) _block_meta_data1;
    block_meta_data_t *block_meta_data2 = (block_meta_data_t*) _block_meta_data2;
    if (block_meta_data1->block_size > block_meta_data2->block_size) 
        return -1;
    else if(block_meta_data1->block_size < block_meta_data2->block_size) 
        return 1;
    else return 0;
}

static void add_free_block_meta_data_to_free_block_list(vm_page_family_t *vm_page_family, 
                        block_meta_data_t *free_block) {
    assert(free_block->is_free == true);
    glthread_priority_insert(&vm_page_family->free_block_priority_list_head, 
                            &free_block->priority_thread_glue,
                            free_blocks_comparison_function,
                            OFFSET_OF(block_meta_data_t, priority_thread_glue));
}

static inline block_meta_data_t* get_biggest_free_block_page_family(vm_page_family_t *vm_page_family) {
    glthread_t *biggest_free_block_glue = vm_page_family->free_block_priority_list_head.right;
    if (biggest_free_block_glue) return glthread_to_block_meta_data(biggest_free_block_glue);
    return NULL;
}

static bool split_free_data_block_for_allocation(vm_page_family_t *pg_family, 
                block_meta_data_t *block_meta_data, uint32_t size) {
    assert(block_meta_data->is_free == true);
    block_meta_data_t *next_block_meta_data = NULL;
    if (block_meta_data->block_size < size) {
        return false;
    }
    uint32_t remaining_size = block_meta_data->block_size - size;
    block_meta_data->is_free = false;
    block_meta_data->block_size = size;
    remove_glthread(&block_meta_data->priority_thread_glue);

    // Case 1 :- No split. size == available block size. 
    if (!remaining_size) return true;
    /* Case 2 :- Partial split :- soft internal fragmentation. 
    *  Available block is split. Free memory after allocation can
    *  hold another meta block. 
    */
    else if (sizeof(block_meta_data_t) < remaining_size && 
        remaining_size < sizeof(block_meta_data_t) + pg_family->struct_size) {
        next_block_meta_data = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
        next_block_meta_data->is_free = true;
        next_block_meta_data->block_size = remaining_size - sizeof(block_meta_data_t);
        next_block_meta_data->offset = block_meta_data->offset + 
            sizeof(block_meta_data_t) + block_meta_data->block_size;
        init_glthread(&next_block_meta_data->priority_thread_glue);
        add_free_block_meta_data_to_free_block_list(pg_family, next_block_meta_data);
        MM_BIND_BLOCKS_FOR_ALLOCATION(block_meta_data, next_block_meta_data);
    }
    /* Case 2 :-
    *   Partial split :- hard internal fragmentation. 
    *   Remaining size cannot even hold a meta block. 
    */
    else if (remaining_size < sizeof(block_meta_data_t)) {

    }
    /* Case 3 :- 
    *  Full split :- new meta block is created. 
    */
    else {
        next_block_meta_data = NEXT_META_BLOCK_BY_SIZE(block_meta_data);
        next_block_meta_data->is_free = true;
        next_block_meta_data->block_size = remaining_size - sizeof(block_meta_data_t);
        next_block_meta_data->offset = block_meta_data->offset + 
            sizeof(block_meta_data_t) + block_meta_data->block_size;
        init_glthread(&next_block_meta_data->priority_thread_glue);
        add_free_block_meta_data_to_free_block_list(pg_family, next_block_meta_data);
        MM_BIND_BLOCKS_FOR_ALLOCATION(block_meta_data, next_block_meta_data);
    }
    return true;
}

static block_meta_data_t* allocate_free_data_block(vm_page_family_t *pg_family, 
            uint32_t size) {
    bool status = false;
    vm_page_t *vm_page = NULL;
    block_meta_data_t *block_meta_data = NULL;
    block_meta_data_t *biggest_block_meta_data = get_biggest_free_block_page_family(pg_family);
    if (biggest_block_meta_data == NULL) {
        if (DEBUG) cout << "biggest_block_meta_data is NULL" << endl;
    }
    if (!biggest_block_meta_data || biggest_block_meta_data->block_size < size) {
        vm_page = allocate_vm_page(pg_family);
        status = split_free_data_block_for_allocation(pg_family, &vm_page->block_meta_data, size);
        if (status) return &vm_page->block_meta_data;
        if (DEBUG) cout << "Returning NULL" << endl;
        return NULL;
    }
    if (biggest_block_meta_data) {
        status = split_free_data_block_for_allocation(pg_family, biggest_block_meta_data, size);
    }
    if (status) return biggest_block_meta_data;
    return NULL;
}

void* xmalloc(char *struct_name, int units) {
    vm_page_family_t *pg_family = lookup_page_family_by_name(struct_name);
    if (pg_family == NULL) {
        cout << "Error: structure " << struct_name << " is not registered" << endl;
        return NULL;
    }
    if (units * pg_family->struct_size > max_page_allocatable_memory(1)) {
        cout << "Requested memory greater than page size" << endl;
        return NULL;
    }
    block_meta_data_t *free_block_meta_data = NULL;
    free_block_meta_data = allocate_free_data_block(pg_family, units * pg_family->struct_size);
    if (free_block_meta_data) {
        memset((char*)(free_block_meta_data + 1), 0, free_block_meta_data->block_size);
        return (void*) (free_block_meta_data + 1);
    }
    cout << "Some error occurred" << endl;
    return NULL;
}

static void print_block_meta_data(block_meta_data_t *node, uint32_t block_num) {
    char *allocation_message = "";
    if (node->is_free) allocation_message = "F R E E D";
    else allocation_message = "Allocated";
    printf("0x%x Block %d   %s block_size = %d   offset = %d prev = 0x%x next = 0x%x\n",
    node, block_num, allocation_message, node->block_size, node->offset, node->prev, node->next);
}

void mm_print_memory_usage(char *struct_name) {
    printf("#########################################################\n");
    printf("Page size: %d bytes\n", SYSTEM_PAGE_SIZE);
    printf("Printing memory usage of: %s\n", struct_name);
    vm_page_family_t *pg_family = lookup_page_family_by_name(struct_name);
    vm_page_t *page = pg_family->first_page;
    uint32_t total_pages = 0;
    while (page) {
        block_meta_data_t *block_meta_data = &page->block_meta_data;
        uint32_t block_num = 0;
        while(block_meta_data) {
            print_block_meta_data(block_meta_data, block_num);
            block_meta_data = block_meta_data->next;
            block_num++;
        }
        page = page->next;
        total_pages++;
    }
    printf("Number of pages in use: %d\n", total_pages);
    printf("#########################################################\n");
}

void mm_print_family_summary(vm_page_family_t *pg_family) {
    if (pg_family == NULL) return;
    vm_page_t *page = pg_family->first_page;
    uint32_t free_blocks = 0, allocated_blocks = 0, total_pages = 0;
    while(page) {
        block_meta_data_t *block_meta_data = &page->block_meta_data;
        while(block_meta_data) {
            if (block_meta_data->is_free) free_blocks++;
            else allocated_blocks++;
            block_meta_data = block_meta_data->next;
        }
        total_pages++;
        page = page->next;
    }
    cout << "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
    cout << "Page family       : " << pg_family->struct_name << endl;
    cout << "Total blocks      : " << allocated_blocks + free_blocks << endl;
    cout << "Allocated blocks  : " << allocated_blocks << endl;
    cout << "Free blocks       : " << free_blocks << endl;
    cout << "Usage (in bytes)  : " << (allocated_blocks) * pg_family->struct_size + (allocated_blocks) * sizeof(block_meta_data_t) << endl;
    cout << "Total pages used  : " << total_pages << endl;
    cout << "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
}

void mm_print_usage_summary() {
    vm_page_for_families_t *curr = first_vm_page_for_families;
    uint32_t families_in_one_page = (SYSTEM_PAGE_SIZE - sizeof(void*)) / sizeof(vm_page_family_t);
    while(curr != NULL) {
        bool done = false;
        for (int i = 0; i < families_in_one_page; i++) {
            vm_page_family_t *pg_family = &curr->vm_page_family[i];
            if (!pg_family->struct_size) {
                done = true;
                break;
            }
            mm_print_family_summary(pg_family);
        }
        if (done) break;
        curr = curr->next;
    }
}