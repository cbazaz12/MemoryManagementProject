// List all group member's name: Christian Bazaz, Devin Mullen

#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024-1

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_ENTRIES 512

//Structure for each tlb row
struct tlb_entry {
    unsigned long virt_addr;    
    unsigned long page_number; 
    bool valid;              
};

//Structure to represents TLB
struct tlb {
    struct tlb_entry entries[TLB_ENTRIES]; 
};

// Setup functions
void set_physical_mem();

// TLB Functions
int TLB_add(void *va, void *pa);
pte_t *TLB_check(void *va);
void print_TLB_missrate();

// Page Table Functions
pte_t* translate(pde_t *pgdir, void *va);
int map_page(pde_t *pgdir, void *va, void* pa);

// Allocation functions
void *n_malloc(unsigned int num_bytes);
void n_free(void *va, int size);

// Data operations
int put_data(void *va, void *val, int size);
void get_data(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);

#endif
