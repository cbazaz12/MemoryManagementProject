// List all group member's name: Christian Bazaz, Devin Mullen

#include "my_vm.h"
#include <math.h>
#include <pthread.h>

struct tlb tlb_store;

//static variables
static unsigned long physical_memory_size;
static void* physical_memory;
static unsigned long total_pages;
static char* physical_bitmap;
static char* virtual_bitmap;
static unsigned long outer_table_bits;
static unsigned long inner_table_bits;
static unsigned long offset_bits;
static bool first_time = true;
static pde_t* outer_table;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int tlb_misses;
static unsigned int tlb_checks;

//method prototypes
static unsigned long get_top_bits(unsigned long value, int num_bits);
static unsigned long get_middle_bits(unsigned long value, int num_middle_bits, int lower_bit_pos);
static unsigned long get_lower_bits(unsigned long value, int num_bits);
static void set_map_bit(char* bitmap, int index);
static int get_map_bit(char* bitmap, int index);
static void clear_map_bit(char* bitmap, int index);
static pde_t create_new_table();
static void* new_virtual_address(unsigned long entry_num);
static void print_tlb(struct tlb *tlb);

/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //physical memory allocation
    if (MEMSIZE < MAX_MEMSIZE) {
        physical_memory_size = MEMSIZE;
    } else {
        physical_memory_size = MAX_MEMSIZE;
    }
    physical_memory = calloc(1, physical_memory_size);

    //allocate physical and virtual bitmap
    total_pages = physical_memory_size / PGSIZE;
    physical_bitmap = calloc(1, (total_pages / 8));
    virtual_bitmap = calloc(1, (total_pages / 8));

    //calculate outer and inner table bits
    offset_bits = (unsigned int) log(PGSIZE) / log(2);
    inner_table_bits = (unsigned int) log(PGSIZE / sizeof(pte_t)) / log(2);
    outer_table_bits = 32 - inner_table_bits - offset_bits;

    //place the outer table at the start of physical memory
    outer_table = (pde_t*)physical_memory;
    set_map_bit(virtual_bitmap, 0);

    //don't want to return 0x000 to user
    set_map_bit(physical_bitmap, 0);
}

int
TLB_add(void *va, void *pa)
{

	unsigned long tag = (unsigned long) va >> offset_bits;
	unsigned long row = tag % TLB_ENTRIES;

	tlb_store.entries[row].virt_addr = tag;
	tlb_store.entries[row].page_number = (unsigned long) pa;
	tlb_store.entries[row].valid = true;

    // print_tlb(&tlb_store);

	return 0;

}

pte_t *
TLB_check(void *va) {

	tlb_checks++;
	unsigned tag = (unsigned long) va >> offset_bits;
	unsigned row = tag % TLB_ENTRIES;

	if (tlb_store.entries[row].valid && tlb_store.entries[row].virt_addr == tag) {
		return (pte_t *) tlb_store.entries[row].page_number;
    }

	tlb_misses++;

    return NULL;
}

void TLB_delete(void *va) {

	unsigned long tag = (unsigned long) va >> offset_bits;
	unsigned long row = tag % TLB_ENTRIES;

	if (tlb_store.entries[row].valid && tlb_store.entries[row].virt_addr == tag) {
		tlb_store.entries[row].valid = false;
    }
}

void
print_TLB_missrate()
{
    double miss_rate = 0;	

    miss_rate = (double) tlb_misses / tlb_checks;

    printf("miss: %d total: %d\n", tlb_misses, tlb_checks);

    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}

pte_t *translate(pde_t *pgdir, void *va) {

    unsigned long page_number;

    //section off the bits
    unsigned long virtual_address = (unsigned long)va;
    unsigned long outer_index = get_top_bits(virtual_address, outer_table_bits);
    unsigned long inner_index = get_middle_bits(virtual_address, inner_table_bits, offset_bits);
    unsigned long offset = get_lower_bits(virtual_address, offset_bits);

    //check TLB
    page_number = (unsigned long) TLB_check(va);

    if (!page_number) {
        //get entry in outer table
        pde_t outer_entry = pgdir[outer_index];
        if(!outer_entry){
            return NULL;
        }
        unsigned long inner_number = get_lower_bits((unsigned long)outer_entry, outer_table_bits+inner_table_bits);

        //get entry in inner table
        pte_t* inner_table = (pte_t*) ((char*) physical_memory + inner_number * PGSIZE); 
        pte_t inner_entry = inner_table[inner_index];
        if(!inner_entry){
            return NULL;
        }
        page_number = get_lower_bits(inner_entry, outer_table_bits+inner_table_bits);
    }
    //calculate return address
    unsigned long ret_addr = (unsigned long) ((char*) physical_memory + page_number * PGSIZE);
    ret_addr += offset;
    pte_t* ret = (pte_t*)ret_addr;
    return ret;
}

int
map_page(pde_t *pgdir, void *va, void *pa)
{

    //section off the bits
    unsigned long virtual_address = (unsigned long)va;
    pte_t physical_address = (pte_t)pa;
    unsigned long outer_index = get_top_bits(virtual_address, outer_table_bits);
    unsigned long inner_index = get_middle_bits(virtual_address, inner_table_bits, offset_bits);

    //attempt to find entry in outer table
    pde_t outer_entry = pgdir[outer_index];
    if(!outer_entry){
        outer_entry = create_new_table();
        set_map_bit(physical_bitmap, outer_entry);
        pgdir[outer_index] = outer_entry;
    }

    //find the inner table entry
    unsigned long inner_number = get_lower_bits(outer_entry, outer_table_bits + inner_table_bits);
    pte_t* inner_table = (pte_t*) ((char*) physical_memory + inner_number * PGSIZE); 
    pte_t inner_entry = inner_table[inner_index];
    pte_t page_number = (pte_t)((physical_address - (pte_t)physical_memory) / PGSIZE);
    if(inner_entry != page_number){
        inner_table[inner_index] = page_number;
    }

    //add page to tlb
    TLB_add(va, (void *) page_number);

    return 0;
}


unsigned long get_next_avail(int num_pages) {

    unsigned long ret = -1;
    int pages_found = 0;
    for(int i = 0; i < total_pages; i++){
        if(get_map_bit(virtual_bitmap, i) == 0){
            if(ret == -1){
                ret = i;
            }
            pages_found++;
            if(pages_found == num_pages){
                break;
            }
        }
        else{
            ret = -1;
            pages_found = 0;
        }
    }

    return ret;
}

//find next location available in physical memory
unsigned long get_next_physical_memory() {
    unsigned long ret = -1;
    for(int i = 0; i < total_pages; i++){
        if(get_map_bit(physical_bitmap, i) == 0){
            return (unsigned long)i;
        }
    }

    return -1;
}

void *n_malloc(unsigned int num_bytes) {

    pthread_mutex_lock(&mem_lock);

    //initialize physical memory on first call
    if(first_time){
        set_physical_mem();
        first_time = false;
    }

    //calculate number of pages needed
    int num_pages = (num_bytes + PGSIZE - 1) / PGSIZE;

    //find next available entry and make new virtual address
    pthread_mutex_lock(&mutex);
    unsigned long virtual_entry = get_next_avail(num_pages);
    if(virtual_entry == -1){
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    num_pages--;
    void* ret = new_virtual_address(virtual_entry);
    set_map_bit(virtual_bitmap, virtual_entry);

    //map virtual address to physical address
    unsigned long physical_entry = get_next_physical_memory();
    set_map_bit(physical_bitmap, physical_entry);
    void* physical_address = physical_memory + physical_entry*PGSIZE;
    map_page(outer_table, ret, physical_address);

    //repeat the above process for remaining pages
    for(int i = num_pages; i > 0; i--){

        //find next virtual address
        virtual_entry++;
        void* new_virt = new_virtual_address(virtual_entry);
        set_map_bit(virtual_bitmap, virtual_entry);

        //map new virtual address to physical address
        unsigned long physical_entry = get_next_physical_memory();
        set_map_bit(physical_bitmap, physical_entry);
        void* physical_address = physical_memory + physical_entry*PGSIZE;
        map_page(outer_table, new_virt, physical_address);
    }
    pthread_mutex_unlock(&mutex);
    pthread_mutex_unlock(&mem_lock);

    return ret;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void n_free(void *va, int size) {
    
    pthread_mutex_lock(&mem_lock);
    //if malloc hasn't been called yet, return
    if(first_time){
        return;
    }

    //calculate number of pages to free
    int num_pages = (size + PGSIZE - 1) / PGSIZE;

    //find index in virtual bitmap
    unsigned long virtual_address = (unsigned long)va;
    int virtual_index = get_top_bits(virtual_address, outer_table_bits) << inner_table_bits;
    virtual_index = virtual_index + get_middle_bits(virtual_address, inner_table_bits, offset_bits);

    //check if all pages are valid
    pthread_mutex_lock(&mutex);
    for(int i = 0; i < num_pages; i++){
        if(get_map_bit(virtual_bitmap, virtual_index + i) == 0){
            pthread_mutex_unlock(&mutex);
            return;
        }
    }
    pthread_mutex_unlock(&mutex);

    //free all pages
    for(int i = 0; i < num_pages; i++){
        //find index in physical bitmap
        unsigned long physical_address = (unsigned long)translate(outer_table, (void*)virtual_address);
        int physical_index = (physical_address - (unsigned long)physical_memory) / PGSIZE;

        //clear bits in bitmaps
        pthread_mutex_lock(&mutex);
        clear_map_bit(physical_bitmap, physical_index);
        clear_map_bit(virtual_bitmap, virtual_index);
        pthread_mutex_unlock(&mutex);

        TLB_delete((void *) virtual_address);

        virtual_index++;
        virtual_address += PGSIZE;
    }
    pthread_mutex_unlock(&mem_lock);
}

int put_data(void *va, void *val, int size) {

    //cast void pointers to char pointers for arithmetic
    char* virtual_address = (char*)va;
    char* value = (char*)val;
    
    //change each bit in the virtual address to each bit in value
    for(int i = 0; i < size; i++){
        char* physical_address = (char*)translate(outer_table, virtual_address);
        if(physical_address == NULL){
            return -1;
        }
        *physical_address = *value;
        virtual_address++;
        value++;
    }

    return 0;
}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_data(void *va, void *val, int size) {

    //cast void pointers to char pointers for arithmetic
    char* virtual_address = (char*)va;
    char* value = (char*)val;
    
    //change each bit in the virtual address to each bit in value
    for(int i = 0; i < size; i++){
        char* physical_address = (char*)translate(outer_table, virtual_address);
        if(physical_address == NULL){
            return;
        }
        *value = *physical_address;
        virtual_address++;
        value++;
    }
}




void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_data( (void *)address_a, &a, sizeof(int));
                get_data( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n", 
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_data((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

static unsigned long get_top_bits(unsigned long value, int num_bits){
    return value >> (32 - num_bits);
}
static unsigned long get_middle_bits(unsigned long value, int num_middle_bits, int lower_bit_pos){
    return (value >> lower_bit_pos) & ((1UL << num_middle_bits) -1);
}
static unsigned long get_lower_bits(unsigned long value, int num_bits){
    return (1UL << num_bits) - 1 & value;
}

static void set_map_bit(char* bitmap, int index){
    bitmap[index /8] |= (1U << (index % 8));
}
static int get_map_bit(char* bitmap, int index){
    return (bitmap[index /8] & (1U << (index % 8))) != 0;
}
static void clear_map_bit(char* bitmap, int index){
    bitmap[index /8] &= ~(1U << (index % 8));
}

static pde_t create_new_table(){
    for(pde_t i = 0; i < total_pages; i++){
        if(!get_map_bit(physical_bitmap, i)){
            set_map_bit(physical_bitmap, i);
            return i;
        }
    }
    pde_t ret = -1;
    return ret;
}

static void* new_virtual_address(unsigned long entry_num){
    unsigned long outer_entries = 1 << outer_table_bits;
    unsigned long inner_entries = 1 << inner_table_bits;
    unsigned long ret = (entry_num / outer_entries) << inner_table_bits;
    ret |= entry_num % inner_entries;
    ret = ret << offset_bits;
    return (void*)ret;
}


static void print_tlb(struct tlb *tlb) {
    printf("TLB Contents:\n");
    printf("%-10s %-20s %-20s %-10s\n", "Index", "Virt Addr", "Page Number", "Valid");
    printf("-------------------------------------------------------------\n");

    for (int i = 0; i < TLB_ENTRIES; i++) {
        struct tlb_entry *entry = &tlb->entries[i];

        printf("%-10d 0x%016lx 0x%016lx %-10s\n",
               i,
               entry->virt_addr,
               entry->page_number,
               entry->valid ? "true" : "false");
    }
}