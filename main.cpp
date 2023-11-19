#include <iostream>
#include "includes/memory_manager.h"
using namespace std;

/*
    In unix like OS, we have system calls to allocate/deallocate one single virtual page. 
    malloc/free are actually glibC library functions which internally makes system calls if required. 

    Two types of system calls :-
        * sbrk/brk :- not very friendly.
        * mmap_pgoff() :-
        *   Not going to use this. Instead we will use mmap/munmap which are glibC functions. 
    
    We will do system call for a virtual memory page and will further subdivide it according to the application needs. 
    Remember that mmap provided virtual page need not be contiguous unlike sbrk system call which 
    just increments the break pointer and hence is contiguous. 
    When a virtual page becomes completely free, we deallocate that page. 

    What we want to do is :-
        * We will get a virtual page from kernel using mmap system call.
        * We will use the concept of datablocks and metablocks in a virtual page.
        * Once the page is completely free, we will return it to kernel using munmap system call. 
    
    Why are we registering structures ?
        * Every structure is represent by a page family. 
        * Each page family have its own virtual data pages from which it will be assigned heap memory.
    
    
*/

struct emp_t {
    char name[32];
    uint32_t id;
};

struct node_t {
    int data;
    node_t *next;    
};

int main() {
    cout << sizeof(block_meta_data_t) << endl;
    init();
    MM_REG_STRUCT(emp_t);
    MM_REG_STRUCT(node_t);
    // print_registered_page_families();
    // vm_page_family_t *family = lookup_page_family_by_name("emp_t");
    // cout << family->struct_name << endl;
    // cout << family->struct_size << endl;
    return 0;
}