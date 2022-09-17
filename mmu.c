#include "mmu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define MB (1024 * 1024)

#define KB (1024)

// just a random array to be passed to ps_create
unsigned char code_ro_data[10 * MB];


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


int pte_to_frame_num(page_table_entry pte);
int get_flags(page_table_entry pte);
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
        if((int)RAM[i]==0){
            // printf("%d is free\n", 18432 + i);
            return 18432 + i;
        }else{
            // printf("%d is used\n", 18432 + i);
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
    // DONE student
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
        // printf("free page frame is %d\n", page_frame_to_allocate);
        RAM[(page_frame_to_allocate-18432)]=(unsigned char)1;
        // printf("Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, 5));
        curr->page_table[page_to_allocate] = build_pte(page_to_allocate, page_frame_to_allocate, 1, 5);
        // printf("Set value is %d\n", curr->page_table[page_to_allocate]);
        // RAM[(page_frame_to_allocate)*4*1024] to RAM[((page_frame_to_allocate)+1)*4*1024 - 1] to be filled now
        memcpy(OS_MEM + (page_frame_to_allocate)*4*1024, code_and_ro_data, 4*1024);
        code_and_ro_data+=4096;
        curr->page_table_count++;
    }
    for(int i=0; i<no_pages_ro_data; i++){
        int page_to_allocate = get_free_page(curr->page_table);
        if(page_to_allocate==-1){
            printf("Error : no page available to allocate in  virt mem");
        }
        int page_frame_to_allocate = get_free_page_frame_index();
        // printf("free page frame is %d\n", page_frame_to_allocate);
        RAM[(page_frame_to_allocate-18432)]=(unsigned char)1;
        // printf("Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, 1));
        curr->page_table[page_to_allocate]=build_pte(page_to_allocate, page_frame_to_allocate, 1, 1);
        // printf("Set value is %d\n", curr->page_table[page_to_allocate]);
        // RAM[(page_frame_to_allocate)*4*1024] to RAM[((page_frame_to_allocate)+1)*4*1024 - 1] to be filled now
        memcpy(OS_MEM + (page_frame_to_allocate)*4*1024, code_and_ro_data, 4*1024);
        code_and_ro_data+=4096;
        curr->page_table_count++;
    }
    for(int i=0; i<no_pages_rw_data; i++){
        int page_to_allocate = get_free_page(curr->page_table);
        if(page_to_allocate==-1){
            printf("Error : no page available to allocate in  virt mem");
        }
        int page_frame_to_allocate = get_free_page_frame_index();
        // printf("free page frame is %d\n", page_frame_to_allocate);
        RAM[(page_frame_to_allocate-18432)]=(unsigned char)1;
        // printf("Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, 3));
        curr->page_table[page_to_allocate]=build_pte(page_to_allocate, page_frame_to_allocate, 1, 3);
        // printf("Set value is %d\n", curr->page_table[page_to_allocate]);
        curr->page_table_count++;
    }
    for(int i=0; i<no_pages_stack; i++){
        int page_to_allocate = get_free_page(curr->page_table);
        if(page_to_allocate==-1){
            printf("Error : no page available to allocate in  virt mem");
        }
        int page_frame_to_allocate = get_free_page_frame_index();
        // printf("free page frame is %d\n", page_frame_to_allocate);
        RAM[(page_frame_to_allocate-18432)]=(unsigned char)1;
        // printf("Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, 3));
        curr->page_table[page_to_allocate]=build_pte(page_to_allocate, page_frame_to_allocate, 1, 3);
        // printf("Set value is %d\n", curr->page_table[page_to_allocate]);
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
   // DONE student
   struct PCB* curr = (struct PCB*) ( &OS_MEM[start_index_page_tables+ 4108*pid]);
   curr->is_free = 1;
    for(int i=0; i<1024; i++){
        // printf("Setting value as %d\n", build_pte(0, 0, 0, 0));
        // for(int i=start_index_free_list; i<=end_index_free_list; i++){
        // RAM[i - curr-]  =  (unsigned char)0;
        // }
        if(is_present(curr->page_table[i])){
            int frame_number_to_drop = pte_to_frame_num(curr->page_table[i]);
            RAM[frame_number_to_drop - 18432] = (unsigned char)0;
            curr->page_table[i] = build_pte(0, 0, 0, 0);
        }
        // printf("Set value is %d\n", temp->page_table[i]);
    }
   curr->page_table_count = 0;
}



/**
 * Create a new process that is identical to the process with given pid. 
 * 
 */
int fork_ps(int pid) {
    int pcb_index_to_allocate = get_free_pcb_index();
    struct PCB* to_cpy = (struct PCB*) ( &OS_MEM[start_index_page_tables+ 4108*pid]);
    if(pcb_index_to_allocate==-1){
        printf("Error : no free space \n");
    }
    struct PCB* curr = (struct PCB*) ( &OS_MEM[start_index_page_tables+ 4108*pcb_index_to_allocate]);
    curr->is_free = 0;
    int process_id_allocated = curr->pid;
    for(int i=0; i<1024; i++){
        page_table_entry pte = to_cpy->page_table[i];
        if(is_present(pte)){
            int page_to_allocate = get_free_page(curr->page_table);
            if(page_to_allocate==-1){
                printf("Error : no page available to allocate in  virt mem");
            }
            int page_frame_to_allocate = get_free_page_frame_index();
            // printf("free page frame is %d\n", page_frame_to_allocate);
            RAM[(page_frame_to_allocate-18432)]=(unsigned char)1;
            // printf("FORK CASE : Setting value as %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, get_flags(curr->page_table[page_to_allocate])));
            curr->page_table[page_to_allocate] = build_pte(page_to_allocate, page_frame_to_allocate, 1, get_flags(to_cpy->page_table[i]));
            // printf("Set value is %d\n", build_pte(page_to_allocate, page_frame_to_allocate, 1, get_flags(to_cpy->page_table[i])));
            // RAM[(page_frame_to_allocate)*4*1024] to RAM[((page_frame_to_allocate)+1)*4*1024 - 1] to be filled now
            memcpy(OS_MEM + (page_frame_to_allocate)*4*1024, OS_MEM + (pte_to_frame_num(to_cpy->page_table[i]))*4*1024, 4*1024);
            curr->page_table_count++;
        }
    }
    // DONE student:
    return process_id_allocated;
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
// If any of the pages was already allocated then kill the process, deallocate all its resources(exit_ps) 
// and set error_no to ERR_SEG_FAULT.
void allocate_pages(int pid, int vmem_addr, int num_pages, int flags) 
{
   // TODO student
    struct PCB* curr = (struct PCB*) ( &OS_MEM[start_index_page_tables + 4108*pid]);
    for(int i = (vmem_addr)/(PAGE_SIZE)-1; i < (vmem_addr)/(PAGE_SIZE)- 1 +num_pages; i++){
        if(is_present(curr->page_table[i])==1){
            error_no = ERR_SEG_FAULT;
            exit_ps(pid);
            return;
        }else{
            //TODO complete allocation with page no, frame no
            curr->page_table[i] = build_pte(0, 0, 1, flags);
        }
    }
    curr->page_table_count+=num_pages;
}



// dynamic heap deallocation
//
// Deallocate num_pages amount of pages for process pid, starting at vmem_addr.
// Assume vmem_addr points to a page boundary
// Assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE

// If any of the pages was not already allocated then kill the process, deallocate all its resources(exit_ps) 
// and set error_no to ERR_SEG_FAULT.
void deallocate_pages(int pid, int vmem_addr, int num_pages) 
{
   // TODO student
    struct PCB* curr = (struct PCB*) ( &OS_MEM[start_index_page_tables + 4108*pid]);
    for(int i = (vmem_addr)/(PAGE_SIZE)-1; i < (vmem_addr)/(PAGE_SIZE)- 1 +num_pages; i++){
        if(is_present(curr->page_table[i])==0){
            error_no = ERR_SEG_FAULT;
            exit_ps(pid);
            return;
        }else{
            int frame_number_to_drop = pte_to_frame_num(curr->page_table[i]);
            RAM[frame_number_to_drop - 18432] = (unsigned char)0;
            curr->page_table[i] = build_pte(0, 0, 0, 0);
        }
    }
    curr->page_table_count-=num_pages;
}

// Read the byte at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(exit_ps) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
unsigned char read_mem(int pid, int vmem_addr) 
{
    // TODO: student
    struct PCB* curr = (struct PCB*) ( &OS_MEM[start_index_page_tables + 4108*pid]);
    int page_number = vmem_addr%PAGE_SIZE == 0 ? (int)(vmem_addr/PAGE_SIZE): (int)(vmem_addr/PAGE_SIZE);
    printf("%d \n", page_number);
    int byte_offset = (vmem_addr%PAGE_SIZE);
    printf("%d\n", byte_offset);
    if(is_present(curr->page_table[page_number])==0 || is_readable(curr->page_table[page_number])==0){
        error_no = ERR_SEG_FAULT;
        exit_ps(pid);
        printf("Error\n");
        return -1;
    }else{
        int frame_number = pte_to_frame_num(curr->page_table[page_number]);
        printf("%d\n", frame_number);
        unsigned char res = (unsigned char) RAM[frame_number*4*1024 + byte_offset];
        printf("%c \n", res);
        return res;
    }
}

// Write the given `byte` at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(exit_ps) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
void write_mem(int pid, int vmem_addr, unsigned char byte) 
{
    // TODO: student
    struct PCB* curr = (struct PCB*) ( &OS_MEM[start_index_page_tables + 4108*pid]);
    int page_number = (int)(vmem_addr/PAGE_SIZE);
    int byte_offset = (vmem_addr % PAGE_SIZE);
    if(is_present(curr->page_table[page_number])==0 || is_writeable(curr->page_table[page_number])==0){
        error_no = ERR_SEG_FAULT;
        exit_ps(pid);
    }else{
        int frame_number = pte_to_frame_num(curr->page_table[page_number]);
        RAM[frame_number*4*1096 + byte_offset] = byte;
    }
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

int get_flags(page_table_entry pte){
    unsigned int res = (pte & 7);
    // printf("Res is %u\n",res);
    return res;
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

// int main(){
//     int page_num = 234;
//     int frame_num = 11223;
//     int flags = 6;
//     int read = 0;
//     int write = 1;
//     int exec = 1;
//     int present = 0;
//     page_table_entry pte = build_pte(page_num, frame_num, present, 6);
//     printf("%u should be %d\n",is_readable(pte), read);
//     printf("%u should be %d\n",is_executable(pte), exec);
//     printf("%u should be %d\n",is_writeable(pte), write);
//     printf("%u should be %d\n",is_present(pte), present);
//     printf("%u should be %d\n",pte_to_page_num(pte), page_num);
//     printf("%u should be %d\n",pte_to_frame_num(pte), frame_num);
//     printf("%d",NUM_USABLE_FRAMES);
//     os_init();
//     // for(int i=start_index_free_list; i<=end_index_free_list; i++){
//     //     printf("%d\n",(int)RAM[i]);
//     // }
//     struct PCB x;
//     // printf("%lu",sizeof(char));
//     printf("%lu\n",sizeof(x));
//     printf("%d, %d\n",start_index_page_tables,end_index_page_tables);
//     // struct PCB* sap = (struct PCB*) ( &OS_MEM[start_index_page_tables]);
//     // int pid = sap->pid;
//     // int ptc = sap->page_table_count;
//     // printf("%d, %d \n", pid, ptc);
//     // for(int i=0; i<99; i++){
//     //     sap++;
//     //     pid = sap->pid;
//     //     ptc = sap->page_table_count;
//     //     printf("%d, %d, %d \n", i+2, pid, ptc);
//     // }
//     unsigned char* p = (unsigned char*) "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";
//     int pid = create_ps(4*1024, 4*1024, 12*1024, 24*1024, p);
//     printf("pid: %d \n", pid);

//     // printf("%d\n", get_free_pcb_index());
//     // printf("%d", NUM_FRAMES);
//     printf("%s \n", OS_MEM + 18432*4*1024); 
//     int fork_pid = fork_ps(pid);
//     print_page_table(pid);
//     exit_ps(pid);
//     print_page_table(fork_pid); 
//     printf("%s \n", OS_MEM + 18443*4*1024);   
//     int pid2 = create_ps(4*1024, 4*1024, 12*1024, 24*1024, p);
//     print_page_table(pid2); 
// }





int main() {

	os_init();
    
	code_ro_data[10 * PAGE_SIZE] = 'c';   // write 'c' at first byte in ro_mem
	code_ro_data[10 * PAGE_SIZE + 1] = 'd'; // write 'd' at second byte in ro_mem

	int p1 = create_ps(10 * PAGE_SIZE, 1 * PAGE_SIZE, 2 * PAGE_SIZE, 1 * MB, code_ro_data);

	error_no = -1; // no error


    print_page_table(p1);
	unsigned char c = read_mem(p1, 10 * PAGE_SIZE);
    printf("%c \n", RAM[18442*4*1024]);
	assert(c == 'c');

	unsigned char d = read_mem(p1, 10 * PAGE_SIZE + 1);
	assert(d == 'd');

	assert(error_no == -1); // no error


	write_mem(1, 10 * PAGE_SIZE, 'd');   // write at ro_data

	assert(error_no == ERR_SEG_FAULT);  


	int p2 = create_ps(1 * MB, 0, 0, 1 * MB, code_ro_data);	// no ro_data, no rw_data

	error_no = -1; // no error


	int HEAP_BEGIN = 1 * MB;  // beginning of heap

	// allocate 250 pages
	allocate_pages(p2, HEAP_BEGIN, 250, O_READ | O_WRITE);

    print_page_table(p2);

	write_mem(p2, HEAP_BEGIN + 1, 'c');

	write_mem(p2, HEAP_BEGIN + 2, 'd');

	assert(read_mem(p2, HEAP_BEGIN + 1) == 'c');

	assert(read_mem(p2, HEAP_BEGIN + 2) == 'd');

	deallocate_pages(p2, HEAP_BEGIN, 10);

	print_page_table(p2); // output should atleast indicate correct protection bits for the vmem of p2.

	write_mem(p2, HEAP_BEGIN + 1, 'd'); // we deallocated first 10 pages after heap_begin

	assert(error_no == ERR_SEG_FAULT);


	int ps_pids[100];

	// requesting 2 MB memory for 64 processes, should fill the complete 128 MB without complaining.   
	for (int i = 0; i < 64; i++) {
    	ps_pids[i] = create_ps(1 * MB, 0, 0, 1 * MB, code_ro_data);
    	print_page_table(ps_pids[i]);	// should print non overlapping mappings.  
	}


	exit_ps(ps_pids[0]);
    

	ps_pids[0] = create_ps(1 * MB, 0, 0, 500 * KB, code_ro_data);

	print_page_table(ps_pids[0]);   

	// allocate 500 KB more
	allocate_pages(ps_pids[0], 1 * MB, 125, O_READ | O_READ | O_EX);

	for (int i = 0; i < 64; i++) {
    	print_page_table(ps_pids[i]);	// should print non overlapping mappings.  
	}
}
