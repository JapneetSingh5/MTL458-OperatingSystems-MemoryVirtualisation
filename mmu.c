#include "mmu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define FREE_LIST_SIZE = 32*1024
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


unsigned int page_number_extractor = 1023<<22;
unsigned int page_frame_extractor = 65535<<6;
unsigned int readable_extractor = O_READ;
unsigned int writable_extractor = O_WRITE;
unsigned int executable_extractor = O_EX;
unsigned int present_extractor = 1<<3;

// storing the free list as a boolean array the size of total page_frames possible i.e.
// 128*1024*1024/4*1024 = 32*1024 Bytes Needed = 32KB
// 0 means that the frame has not been allocated yet, 1 means frame allocated 
// to read from unsigned char array, read as (int)RAM[i]
int start_index_free_list = 0;
int end_index_free_list = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE) - 1; //end_index has been filled,loop till <= end_index
// 4100 bytes per PCB struct, 100 processes can exist simultaneously
// 4100 * 100 bytes < 1024 * 500 bytes < 500KB total used up
int start_index_page_tables = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);
int end_index_page_tables = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE) + 4108*100 - 1;

// storing all the page tables as an array of PCB structs
// each PCB struct has size

page_table_entry build_pte(int page_num, int frame_num, int present, int flags){
    if(page_num>1023 || page_num<0){
        printf("Error : page number out of range \n");
    }
    // if(frame_num!=0 && (frame_num>51199 || frame_num<18432)){
    //     printf("Error : page frame number out of range \n");
    // }
    if(present!=0 && present!=1){
        printf("Error : present bit can only be 0 or 1 \n");
    }
    if(flags>7 || flags<0){
        printf("Error : flags can range from 0 to 7 \n");
    }
    page_table_entry res = (page_num<<22) + (frame_num<<6) + (present<<3) + flags;
    return res;
}

void os_init() {
    // TODO student 
    // initialize your data structures.

    // first 32*1024 bytes are for the binary free list we created i.e. 32KB is  for the binary free list
    // intitalise them all to 0 since nothing has been allocated yet
    for(int i=start_index_free_list; i<=end_index_free_list; i++){
        RAM[i]  =  (unsigned char)0;
    }
    for(int i=0; i<100; i++){
        struct PCB* temp = (struct PCB*) ( &OS_MEM[start_index_page_tables + 4108*i]);
        temp->is_free = 1;
        temp->pid = i; 
        temp->page_table_count = 0;
        for(int i=0; i<1024; i++){
            // printf("Setting value as %d\n", build_pte(0, 0, 0, 0));
            temp->page_table[i] = build_pte(0, 0, 0, 0);
            // printf("Set value is %d\n", temp->page_table[i]);
        }
        // if(i==32){
        //     temp->is_free = 1;
        // }
    }
}

int get_free_page(page_table_entry page_table[1024]){
    for(int i=0; i<1024; i++){
        if(is_present(page_table[i])==0){
            return i;
        }
    }
    return -1;
} 

int get_free_page_frame_index(){
    for(int i=start_index_free_list; i<=end_index_free_list; i++){
        if((int)RAM[18432 + i*4]==0){
            printf("%d is free\n", 18432 + i);
            return 18432 + i;
        }else{
            printf("%d is used\n", 18432 + i);
        }
    }
    return -1;
} 

int get_free_pcb_index(){
    struct PCB* iter = (struct PCB*) ( &OS_MEM[start_index_page_tables]);
    for(int i=0; i<100; i++){
        int pid = iter->pid;
        int ptc = iter->page_table_count;
        int is_free = iter->is_free;
        // printf("%d, %d, %d, %d \n", i, is_free , pid, ptc);
        if(is_free){
            return i;
        }
        iter++;
    }
    return -1;
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
    int no_pages_code = code_size/PAGE_SIZE;
    int no_pages_ro_data = ro_data_size/PAGE_SIZE;
    int no_pages_rw_data = rw_data_size/PAGE_SIZE;
    int no_pages_stack = max_stack_size/PAGE_SIZE;
    int pcb_index_to_allocate = get_free_pcb_index();
    if(pcb_index_to_allocate==-1){
        printf("Error : no free space \n");
    }
    struct PCB* curr = (struct PCB*) ( &OS_MEM[start_index_page_tables+ 4108*pcb_index_to_allocate]);
    curr->is_free = 0;
    int process_id_allocated = curr->pid;
    for(int i=0; i<no_pages_code; i++){
        int page_to_allocate = get_free_page(curr->page_table);
        if(page_to_allocate==-1){
            printf("Error : no page available to allocate in  virt mem");
        }
        int page_frame_to_allocate = get_free_page_frame_index();
        printf("free page frame is %d\n", page_frame_to_allocate);
        RAM[18432+ (page_frame_to_allocate-18432)*4]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+1]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+2]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+3]=(unsigned char)1;
        printf("Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, 5));
        curr->page_table[page_to_allocate] = build_pte(page_to_allocate, page_frame_to_allocate, 1, 5);
        printf("Set value is %d\n", curr->page_table[page_to_allocate]);
        memcpy(OS_MEM + page_frame_to_allocate, code_and_ro_data + 4*i, 4);
        curr->page_table_count++;
    }
    for(int i=0; i<no_pages_ro_data; i++){
        int page_to_allocate = get_free_page(curr->page_table);
        if(page_to_allocate==-1){
            printf("Error : no page available to allocate in  virt mem");
        }
        int page_frame_to_allocate = get_free_page_frame_index();
        printf("free page frame is %d\n", page_frame_to_allocate);
        RAM[18432+ (page_frame_to_allocate-18432)*4]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+1]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+2]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+3]=(unsigned char)1;
        printf("Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, 1));
        curr->page_table[page_to_allocate]=build_pte(page_to_allocate, page_frame_to_allocate, 1, 1);
        printf("Set value is %d\n", curr->page_table[page_to_allocate]);
        memcpy(OS_MEM + page_frame_to_allocate, code_and_ro_data + 4*no_pages_code + i*4, 4);
        curr->page_table_count++;
    }
    for(int i=0; i<no_pages_rw_data; i++){
        int page_to_allocate = get_free_page(curr->page_table);
        if(page_to_allocate==-1){
            printf("Error : no page available to allocate in  virt mem");
        }
        int page_frame_to_allocate = get_free_page_frame_index();
        printf("free page frame is %d\n", page_frame_to_allocate);
        RAM[18432+ (page_frame_to_allocate-18432)*4]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+1]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+2]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+3]=(unsigned char)1;
        printf("Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, 3));
        curr->page_table[page_to_allocate]=build_pte(page_to_allocate, page_frame_to_allocate, 1, 3);
        printf("Set value is %d\n", curr->page_table[page_to_allocate]);
        curr->page_table_count++;
    }
    for(int i=0; i<no_pages_stack; i++){
        int page_to_allocate = get_free_page(curr->page_table);
        if(page_to_allocate==-1){
            printf("Error : no page available to allocate in  virt mem");
        }
        int page_frame_to_allocate = get_free_page_frame_index();
        printf("free page frame is %d\n", page_frame_to_allocate);
        RAM[18432+ (page_frame_to_allocate-18432)*4]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+1]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+2]=(unsigned char)1;
        RAM[18432+ (page_frame_to_allocate-18432)*4+3]=(unsigned char)1;
        printf("Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, 3));
        curr->page_table[page_to_allocate]=build_pte(page_to_allocate, page_frame_to_allocate, 1, 3);
        printf("Set value is %d\n", curr->page_table[page_to_allocate]);
        curr->page_table_count++;

    }
    return process_id_allocated;
}

/**
 * This function should deallocate all the resources for this process. 
 * 
 */
void exit_ps(int pid) 
{
   // TODO student
}



/**
 * Create a new process that is identical to the process with given pid. 
 * 
 */
int fork_ps(int pid) {

    // TODO student:
    return 0;
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
}

// Read the byte at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
unsigned char read_mem(int pid, int vmem_addr) 
{
    // TODO: student
    return 0;
}

// Write the given `byte` at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
void write_mem(int pid, int vmem_addr, unsigned char byte) 
{
    // TODO: student
}





// ---------------------- Helper functions for Page table entries ------------------ // 

// return the page number from the pte
int pte_to_page_num(page_table_entry pte) 
{
    // DONE: student
    // printf("Getting page from page table entry %u \n", pte);
    unsigned int res = (pte & page_number_extractor)>>22;
    // printf("Res is %u\n",res);
    return res;
}

// return the frame number from the pte
int pte_to_frame_num(page_table_entry pte) 
{
    // DONE: student
    // printf("Getting frame_num from page table entry %u \n", pte);
    unsigned int res = (pte & page_frame_extractor)>>6;
    return res;
}


// return 1 if read bit is set in the pte
// 0 otherwise
int is_readable(page_table_entry pte) {
    // DONE: student
    // printf("Getting is_readable bit from page table entry %u \n", pte);
    unsigned int res = pte & readable_extractor;
    // printf("Res is %u\n",res);
    return res==1;
}

// return 1 if write bit is set in the pte
// 0 otherwise
int is_writeable(page_table_entry pte) {
    // DONE: student
    // printf("Getting is_writable bit from page table entry %u \n", pte);
    unsigned int res = (pte & writable_extractor)>>1;
    // printf("Res is %u\n",res);
    return res==1;
}

// return 1 if executable bit is set in the pte
// 0 otherwise
int is_executable(page_table_entry pte) {
    // DONE: student
    // printf("Getting is_executable bit from page table entry %u \n", pte);
    unsigned int res = (pte & executable_extractor)>>2;
    // printf("Res is %u\n",res);
    return res==1;
}

// return 1 if present bit is set in the pte
// 0 otherwise
int is_present(page_table_entry pte) {
    // DONE: student
    // printf("Getting present bit from page table entry %u \n", pte);
    unsigned int res = (pte & present_extractor)>>3;
    // printf("Res is %u\n",res);
    return res==1;
}


// -------------------  functions to print the state  --------------------------------------------- //

void print_page_table(int pid) 
{
    struct PCB* temp = (struct PCB*) ( &OS_MEM[start_index_page_tables + 4108*pid]);
    page_table_entry* page_table_start = temp->page_table; // TODO student: start of page table of process pid
    int num_page_table_entries = temp->page_table_count;           // TODO student: num of page table entries
    printf("No of page table entries %d \n", num_page_table_entries);
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

int main(){
    int page_num = 234;
    int frame_num = 11223;
    int flags = 6;
    int read = 0;
    int write = 1;
    int exec = 1;
    int present = 0;
    page_table_entry pte = build_pte(page_num, frame_num, present, 6);
    printf("%u should be %d\n",is_readable(pte), read);
    printf("%u should be %d\n",is_executable(pte), exec);
    printf("%u should be %d\n",is_writeable(pte), write);
    printf("%u should be %d\n",is_present(pte), present);
    printf("%u should be %d\n",pte_to_page_num(pte), page_num);
    printf("%u should be %d\n",pte_to_frame_num(pte), frame_num);
    printf("%d",NUM_USABLE_FRAMES);
    os_init();
    // for(int i=start_index_free_list; i<=end_index_free_list; i++){
    //     printf("%d\n",(int)RAM[i]);
    // }
    struct PCB x;
    // printf("%lu",sizeof(char));
    printf("%lu\n",sizeof(x));
    printf("%d, %d\n",start_index_page_tables,end_index_page_tables);
    // struct PCB* sap = (struct PCB*) ( &OS_MEM[start_index_page_tables]);
    // int pid = sap->pid;
    // int ptc = sap->page_table_count;
    // printf("%d, %d \n", pid, ptc);
    // for(int i=0; i<99; i++){
    //     sap++;
    //     pid = sap->pid;
    //     ptc = sap->page_table_count;
    //     printf("%d, %d, %d \n", i+2, pid, ptc);
    // }
    unsigned char* p = (unsigned char*) "This is the source string";
    int pid = create_ps(8*1024, 8*1024, 12*1024, 24*1024, p);
    printf("pid: %d \n", pid);
    print_page_table(pid);
    // printf("%d\n", get_free_pcb_index());
    // printf("%d", NUM_FRAMES);
}




