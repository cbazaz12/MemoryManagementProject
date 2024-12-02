# MemoryManagementProject
Simulation of User-Level Memory Management on Linux Systems.

Run the files in the benchmark folder to test the library.

1. set_physical_mem():
 This function allocates “physical memory” and both virtual and physical memory using
 calloc. Next we calculate the number of bits used for outer/inner page and offset by using the
 formula provided in the writeup. Lastly we store a pointer to address 0 for our page directory.
 2. TLB_add():
 This function removes offset bits from the provided virtual address then uses modulus to
 index to the correct row in the TLB. Then we simply store the translation inside the TLB and
 validate the entry.
 3. TLB_check():
 This function navigates to the correct row in the TLB as in _add, then checks to see if the
 entry matches the provided virtual address and is valid. Each check and miss increments a
 respective counter.
 4. TLB_delete():
 This is just a helper function to set a TLB entry to invalid in the case that we are freeing
 pages.
 5. print_TLB_missrate():
 Simply divide misses/checks and print the result.
 6. translate():
 First we store each section of bits in the virtual address in local variables for
 convenience using bit manipulation helper functions from project 1. Next we check the TLB for
 the translation. If the translation is not in the TLB we use the outermost bits of the virtual
 address to index into the outer page table. Next we extract the physical page number of the
 inner table from the outer table entry. Then we can get the physical page number that
 corresponds to our virtual address from the lower bits of the inner page table entry and finally
 return its address with the proper offset.
7. map_page():
 This is basically the same as translate but instead we are just storing a new entry in the
 outer and inner page tables. At the end of this function we also add the page to the TLB.
 8. get_next_avail()/get_next_physical_memory():
 These functions simply loop through the virtual bitmap and return the index of a
 contiguous group of free pages equal to the required number.
 9. n_malloc():
 This function uses a mutex lock to support multithreading. First we check if we have
 initialized physical memory and do so if not. Then we calculate the number of pages we need
 and use the get_next_avail function to find a contiguous group of open pages in the virtual
 bitmap. Next we use the new_virtual_adderss function to generate the return address, and map
 the page to a free physical page. This process is repeated in a loop for all of the pages
 requested.
 10. n_free():
 This function uses a mutex lock to support multithreading. First we make sure that we
 aren’t freeing without malloc having been used. Then we calculate the number of pages to free.
 Next we check the validity of each page to be freed in the virtual bitmap. Then we use a loop to
 free each page, clearing its index in the bitmaps and invalidating its tlb entry.
 11. put_data()/get_data():
 These functions loop through each bit of the requested size and either store or record
 value at the proper physical memory address which we compute using the translate function.
 12. create_new_table():
 Helper function to create a new page table entry and return its index.
 13. new_virtual_address():
 Helper function to create a virtual address based on a virtual bitmap index.
