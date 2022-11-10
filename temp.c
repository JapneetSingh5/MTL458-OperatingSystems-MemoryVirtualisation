#include "temp.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// byte addressable memory
unsigned char RAM[RAM_SIZE];  


// OS's memory starts at the beginning of RAM.
// Store the process related info, page tables or other data structures here.
// do not use more than (OS_MEM_SIZE: 72 MB).
unsigned char* OS_MEM = RAM;  

// memory that can be used by processes.   
// 128 MB size (RAM_SIZE - OS_MEM_SIZE)
unsigned char* PS_MEM = RAM + OS_MEM_SIZE; 


// This first frame has frame number 0 and is located at start of RAM(NOT PS_MEM).
// We also include the OS_MEM even though it is not paged. This is 
// because the RAM can only be accessed through physical RAM addresses.  
// The OS should ensure that it does not map any of the frames, that correspond
// to its memory, to any process's page. 
int NUM_FRAMES = ((RAM_SIZE) / PAGE_SIZE);

// Actual number of usable frames by the processes.
int NUM_USABLE_FRAMES = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);

// To be set in case of errors. 
int error_no; 

page_table_entry create_pte(int frame_num, int present, int flags){
    page_table_entry pte = (frame_num<<4) + (present<<3) + flags;
    return pte;
}

void os_init() {

    // TODO student 
    // initialize your data structures.
    int max_free_list = (RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE;
    for(int i=0; i<max_free_list; i++){
        RAM[i]  =  (unsigned char)0; //0 signifies not allocated yet
    }
    for(int i=0; i<MAX_PROCS; i++){
        struct PCB* temp = (struct PCB*) ( &OS_MEM[max_free_list + 4104*i]);
        temp->free = 1;
        for(int j=0; j<1024; j++){
            temp->page_table[j] = create_pte(0, 0, 0);
        }
        temp->pid = i; 
    }
}

unsigned char* allocate_segment_pages(int num_pages, int vpn_help, int flags, unsigned char* code_and_ro_data, int pcb_index_free){
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
    struct PCB* pcb_struct = (struct PCB*)(&OS_MEM[page_tables_start + 4104*pcb_index_free]);
    for(int i=0; i<num_pages; i++){
        int vpn = vpn_help + i;
        int pfn = -1;
        for(int j=0; j<page_tables_start; j++){
            if((int)RAM[j]==0){
                pfn = (NUM_FRAMES - NUM_USABLE_FRAMES) + j;
                break;
            }
        }
        RAM[(pfn-(NUM_FRAMES - NUM_USABLE_FRAMES))]=(unsigned char)1;
        pcb_struct->page_table[vpn] = create_pte(pfn, 1, flags);
        if(flags!=3){
            memcpy(OS_MEM + (pfn)*4*1024, code_and_ro_data, 4*1024);
            code_and_ro_data+=4096;
        }
    }
    return code_and_ro_data;
}


// ----------------------------------- Functions for managing memory --------------------------------- //

/**
 *  Process Virtual Memory layout: 
 *  ---------------------- (virt. memory start 0x00)
 *        code
 *  ----------------------  
 *     read only data 
 *  ----------------------
 *     read / write data
 *  ----------------------
 *        heap
 *  ----------------------
 *        stack  
 *  ----------------------  (virt. memory end 0x3fffff)
 * 
 * 
 *  code            : read + execute only
 *  ro_data         : read only
 *  rw_data         : read + write only
 *  stack           : read + write only
 *  heap            : (protection bits can be different for each heap page)
 * 
 *  assume:
 *  code_size, ro_data_size, rw_data_size, max_stack_size, are all in bytes
 *  code_size, ro_data_size, rw_data_size, max_stack_size, are all multiples of PAGE_SIZE
 *  code_size + ro_data_size + rw_data_size + max_stack_size < PS_VIRTUAL_MEM_SIZE
 *  
 * 
 *  The rest of memory will be used dynamically for the heap.
 * 
 *  This function should create a new process, 
 *  allocate code_size + ro_data_size + rw_data_size + max_stack_size amount of physical memory in PS_MEM,
 *  and create the page table for this process. Then it should copy the code and read only data from the
 *  given `unsigned char* code_and_ro_data` into processes' memory.
 *   
 *  It should return the pid of the new process.  
 *  
 */
int create_ps(int code_size, int ro_data_size, int rw_data_size,
                 int max_stack_size, unsigned char* code_and_ro_data) 
{   
    // TODO student
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);

    int code_num_pages = code_size/PAGE_SIZE;
    int ro_data_num_pages = ro_data_size/PAGE_SIZE;
    int rw_data_num_pages = rw_data_size/PAGE_SIZE;
    int max_stack_num_pages = max_stack_size/PAGE_SIZE;

    int pcb_index_free = -1;

    struct PCB* temp = (struct PCB*) ( &OS_MEM[page_tables_start]);
    for(int i=0; i<100; i++){
        int free = temp->free;
        if(free){
            pcb_index_free = i;
            break;
        }
        temp++;
    }
     
    struct PCB* pcb_struct = (struct PCB*) ( &OS_MEM[page_tables_start + 4104*pcb_index_free]);
    pcb_struct->free = 0;
    int proc_id = pcb_struct->pid;
    code_and_ro_data=allocate_segment_pages(code_num_pages, 0, 5, code_and_ro_data, pcb_index_free);
    code_and_ro_data=allocate_segment_pages(ro_data_num_pages, code_num_pages, 1, code_and_ro_data, pcb_index_free);
    allocate_segment_pages(rw_data_num_pages, ro_data_num_pages + code_num_pages, 3, code_and_ro_data, pcb_index_free);
    allocate_segment_pages(max_stack_num_pages, 1024 - max_stack_num_pages, 3, code_and_ro_data, pcb_index_free);
    return proc_id;
}


/**
 * This function should deallocate all the resources for this process. 
 * 
 */
void exit_ps(int pid) 
{
    // TODO student
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
    struct PCB* pcb_struct = (struct PCB*) ( &OS_MEM[page_tables_start+ 4104*pid]);
    pcb_struct->free = 1;
    for(int i=0; i<1024; i++){
        if(is_present(pcb_struct->page_table[i])){
            int pfn = pte_to_frame_num(pcb_struct->page_table[i]);
            RAM[pfn - (NUM_FRAMES - NUM_USABLE_FRAMES)] = (unsigned char)0;
            pcb_struct->page_table[i] = create_pte(0, 0, 0);
        }
    }
}



/**
 * Create a new process that is identical to the process with given pid. 
 * 
 */
int fork_ps(int pid) {

    // TODO student:
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
    int pcb_index_free = -1;

    struct PCB* temp = (struct PCB*) ( &OS_MEM[page_tables_start]);
    for(int i=0; i<100; i++){
        int free = temp->free;
        if(free){
            pcb_index_free = i;
            break;
        }
        temp++;
    }

    struct PCB* pcb_struct1 = (struct PCB*) ( &OS_MEM[page_tables_start+ 4104*pid]);
    struct PCB* pcb_struct2 = (struct PCB*) ( &OS_MEM[page_tables_start+ 4104*pcb_index_free]);
    pcb_struct2->free = 0;
    int proc_id = pcb_struct2->pid;
    for(int i=0; i<1024; i++){
        page_table_entry pte = pcb_struct1->page_table[i];
        if(is_present(pte)){
            int vpn=-1;
            for(int j=0; j<1024; j++){
                if(is_present(pcb_struct2->page_table[j])==0){
                    vpn = j;
                    break;
                }
            }
            int pfn = -1;
            for(int j=0; j<page_tables_start; j++){
                if((int)RAM[j]==0){
                    pfn = (NUM_FRAMES - NUM_USABLE_FRAMES) + j;
                    break;
                }
            }
            RAM[(pfn-(NUM_FRAMES - NUM_USABLE_FRAMES))]=(unsigned char)1;
            unsigned int flags = (pcb_struct1->page_table[i] & 7);
            pcb_struct2->page_table[vpn] = create_pte(pfn, 1, flags);
            memcpy(OS_MEM + (pfn)*4*1024, OS_MEM + (pte_to_frame_num(pcb_struct1->page_table[i]))*4*1024, 4*1024);
        }
    }
    return proc_id;
}



// dynamic heap allocation
//
// Allocate num_pages amount of pages for process pid, starting at vmem_addr.
// Assume vmem_addr points to a page boundary.  
// Assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
//
//
// Use flags to set the protection bits of the pages.
// Ex: flags = O_READ | O_WRITE => page should be read & writeable.
//
// If any of the pages was already allocated then kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
void allocate_pages(int pid, int vmem_addr, int num_pages, int flags) 
{
    // TODO student
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
    struct PCB* pcb_struct = (struct PCB*) ( &OS_MEM[page_tables_start+ 4104*pid]);
    for(int i = (vmem_addr)/(PAGE_SIZE); i < (vmem_addr)/(PAGE_SIZE) + num_pages; i++){
        if(is_present(pcb_struct->page_table[i])==1 || pcb_struct->free==1){
            error_no = ERR_SEG_FAULT;
            exit_ps(pid);
            return;
        }
        int pfn = -1;
        for(int j=0; j<page_tables_start; j++){
            if((int)RAM[j]==0){
                pfn = (NUM_FRAMES - NUM_USABLE_FRAMES) + j;
                break;
            }
        }
        pcb_struct->page_table[i] = create_pte(pfn, 1, flags);
        RAM[pfn - (NUM_FRAMES - NUM_USABLE_FRAMES)] = (unsigned char)1;
    }
}



// dynamic heap deallocation
//
// Deallocate num_pages amount of pages for process pid, starting at vmem_addr.
// Assume vmem_addr points to a page boundary
// Assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE

// If any of the pages was not already allocated then kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
void deallocate_pages(int pid, int vmem_addr, int num_pages) 
{
    // TODO student
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
    struct PCB* pcb_struct = (struct PCB*) ( &OS_MEM[page_tables_start+ 4104*pid]);
    for(int i = (vmem_addr)/(PAGE_SIZE); i < (vmem_addr)/(PAGE_SIZE) + num_pages; i++){
        if(is_present(pcb_struct->page_table[i])==1 || pcb_struct->free==1){
            error_no = ERR_SEG_FAULT;
            exit_ps(pid);
            return;
        }
        int pfn = pte_to_frame_num(pcb_struct->page_table[i]);
        pcb_struct->page_table[i] = create_pte(0, 0, 0);
        RAM[pfn - (NUM_FRAMES - NUM_USABLE_FRAMES)] = (unsigned char)0;
    }
}

// Read the byte at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
unsigned char read_mem(int pid, int vmem_addr) 
{
    // TODO: student
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
    struct PCB* pcb_struct = (struct PCB*) ( &OS_MEM[page_tables_start+ 4104*pid]);
    if(pcb_struct->free){
        error_no = ERR_SEG_FAULT;
    }
    int vpn = (int)(vmem_addr/PAGE_SIZE); 
    int offset = (vmem_addr % PAGE_SIZE);
    if(is_present(pcb_struct->page_table[vpn])==0 || is_readable(pcb_struct->page_table[vpn])==0){
        error_no = ERR_SEG_FAULT;
        exit_ps(pid);
        return -1;
    }
    else{
        int pfn = pte_to_frame_num(pcb_struct->page_table[vpn]);
        unsigned char value = RAM[pfn*4*1024 + offset];
        return value;
    }
}

// Write the given `byte` at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
void write_mem(int pid, int vmem_addr, unsigned char byte) 
{
    // TODO: student
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
    struct PCB* pcb_struct = (struct PCB*) ( &OS_MEM[page_tables_start+ 4104*pid]);
    int vpn = (int)(vmem_addr / PAGE_SIZE);
    int offset = (vmem_addr % PAGE_SIZE);
    if(is_present(pcb_struct->page_table[vpn])==0 || is_writeable(pcb_struct->page_table[vpn])==0){
        error_no = ERR_SEG_FAULT;
        exit_ps(pid);
    }
    else{
        int pfn = pte_to_frame_num(pcb_struct->page_table[vpn]);
        RAM[pfn*4*1024 + offset] = byte;
    }
}





// ---------------------- Helper functions for Page table entries ------------------ // 

// return the frame number from the pte
int pte_to_frame_num(page_table_entry pte) 
{
    // TODO: student
    unsigned int mask = 65535<<4;
    unsigned int pfn = (pte & mask)>>4;
    return pfn;
}


// return 1 if read bit is set in the pte
// 0 otherwise
int is_readable(page_table_entry pte) {
    // TODO: student
    unsigned int mask = O_READ;
    unsigned int read = (pte & mask);
    if(read == 1) return 1;
    return 0;
}

// return 1 if write bit is set in the pte
// 0 otherwise
int is_writeable(page_table_entry pte) {
    // TODO: student
    unsigned int mask = O_WRITE;
    unsigned int write = (pte & mask)>>1;
    if(write == 1) return 1;
    return 0;
}

// return 1 if executable bit is set in the pte
// 0 otherwise
int is_executable(page_table_entry pte) {
    // TODO: student
    unsigned int mask = O_EX;
    unsigned int exec = (pte & mask)>>2;
    if(exec == 1) return 1;
    return 0;
}


// return 1 if present bit is set in the pte
// 0 otherwise
int is_present(page_table_entry pte) {
    // TODO: student
    unsigned int mask = 1<<3;
    unsigned int present = (pte & mask)>>3;
    if(present == 1) return 1;
    return 0;
}

// -------------------  functions to print the state  --------------------------------------------- //

void print_page_table(int pid) 
{
    int page_tables_start = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
    struct PCB* pcb_struct = (struct PCB*) ( &OS_MEM[page_tables_start + 4104*pid]);
    page_table_entry* page_table_start = pcb_struct->page_table; // TODO student: start of page table of process pid
    int num_page_table_entries = 1024;           // TODO student: num of page table entries

    // Do not change anything below
    puts("------ Printing page table-------");
    for (int i = 0; i < num_page_table_entries; i++) 
    {
        page_table_entry pte = page_table_start[i];
        printf("Page num: %d, frame num: %d, R:%d, W:%d, X:%d, P%d\n", 
                i, 
                pte_to_frame_num(pte),
                is_readable(pte),
                is_writeable(pte),
                is_executable(pte),
                is_present(pte)
                );
    }

}

#include <assert.h>

#define MB (1024 * 1024)

#define KB (1024)

// just a random array to be passed to ps_create
unsigned char code_ro_data[10 * MB];


int main() {

	os_init();
    
	code_ro_data[10 * PAGE_SIZE] = 'c';   // write 'c' at first byte in ro_mem
	code_ro_data[10 * PAGE_SIZE + 1] = 'd'; // write 'd' at second byte in ro_mem

	int p1 = create_ps(10 * PAGE_SIZE, 1 * PAGE_SIZE, 2 * PAGE_SIZE, 1 * MB, code_ro_data);

	error_no = -1; // no error


    // print_page_table(p1);
	unsigned char c = read_mem(p1, 10 * PAGE_SIZE);
    printf("unsigned char: %c\n", c);
	assert(c == 'c');

	unsigned char d = read_mem(p1, 10 * PAGE_SIZE + 1);
	assert(d == 'd');

	assert(error_no == -1); // no error


	write_mem(p1, 10 * PAGE_SIZE, 'd');   // write at ro_data

	assert(error_no == ERR_SEG_FAULT);  


	int p2 = create_ps(1 * MB, 0, 0, 1 * MB, code_ro_data);	// no ro_data, no rw_data

	error_no = -1; // no error

    // print_page_table(p2);
	int HEAP_BEGIN = 1 * MB;  // beginning of heap

	// allocate 250 pages
	allocate_pages(p2, HEAP_BEGIN, 250, O_READ | O_WRITE);

	write_mem(p2, HEAP_BEGIN + 1, 'c');

	write_mem(p2, HEAP_BEGIN + 2, 'd');

	assert(read_mem(p2, HEAP_BEGIN + 1) == 'c');

	assert(read_mem(p2, HEAP_BEGIN + 2) == 'd');

	deallocate_pages(p2, HEAP_BEGIN, 10);

	// print_page_table(p2); // output should atleast indicate correct protection bits for the vmem of p2.

	write_mem(p2, HEAP_BEGIN + 1, 'd'); // we deallocated first 10 pages after heap_begin

	assert(error_no == ERR_SEG_FAULT);


	int ps_pids[100];

	// requesting 2 MB memory for 64 processes, should fill the complete 128 MB without complaining.   
	// for (int i = 0; i < 64; i++) {
    // 	ps_pids[i] = create_ps(1 * MB, 0, 0, 1 * MB, code_ro_data);
    // 	print_page_table(ps_pids[i]);	// should print non overlapping mappings.  
	// }


	exit_ps(ps_pids[0]);
    

	ps_pids[0] = create_ps(1 * MB, 0, 0, 500 * KB, code_ro_data);

	// print_page_table(ps_pids[0]);   

	// allocate 500 KB more
	allocate_pages(ps_pids[0], 1 * MB, 125, O_READ | O_READ | O_EX);

	// for (int i = 0; i < 64; i++) {
    // 	print_page_table(ps_pids[i]);	// should print non overlapping mappings.  
	// }
}