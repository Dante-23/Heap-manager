#include <iostream>
#include <iomanip>
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

void test1() {
    struct emp_t {
        char name[32];
        uint32_t id;
    };
    struct student_t {
        char name[32];
        uint32_t rollno;
        uint32_t marks_phys;
        uint32_t marks_chem;
        uint32_t marks_maths;
        student_t *next;
    };
    init();
    MM_REG_STRUCT(emp_t);
    // MM_REG_STRUCT(student_t);
    print_registered_page_families();
    // xmalloc("emp_t", 1);
    // xmalloc("emp_t", 1);
    // xmalloc("emp_t", 1);

    // xmalloc("student_t", 1);
    // xmalloc("student_t", 1);

    int n = 10000;
    for (int i = 0; i < n; i++) {
        // cout << "i: " << i << endl;
        xmalloc("emp_t", 1);
        // xmalloc("student_t", 1);
    }

    mm_print_memory_usage("emp_t");
    // mm_print_memory_usage("student_t");

    mm_print_usage_summary();
    cout << "Size of meta block: " << sizeof(block_meta_data_t) << endl;
    cout << n * (sizeof(emp_t) + sizeof(block_meta_data_t)) << endl;
}

int main() {
    test1();
    // cout << setw(10) << "Hello" << endl;
    return 0;
}