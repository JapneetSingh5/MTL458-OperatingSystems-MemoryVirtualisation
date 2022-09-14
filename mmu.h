#define RAM_SIZE 200 * 1024 * 1024 // 200 MB

#define OS_MEM_SIZE 72 * 1024 * 1024 // 72 MB

#define PAGE_SIZE 4 * 1024 // 4 KB


// interpret each page table entry as unsigned int
// 32 bits should be enough to store all the info
typedef unsigned int page_table_entry;
#define PAGE_TABLE_ENTRY_SIZE sizeof(page_table_entry); 


#define PS_VIRTUAL_MEM_SIZE 4 * 1024 * 1024  // Each process has 4 MB of virtual memory

#define MAX_PROCS 100  // Assume that the maximum number of processes that can exist at a time is 100
                       // Total processes created may be more than 100(as some of them will exit).
                       

// Block for storing information of each process
struct PCB {
    int pid;
    page_table_entry* page_table;
    // TODO student: can add more fields
};


// Protections associated with each page
enum PAGE_PROTECTIONS {
    O_READ  = 1,    // read allowed
    O_WRITE = 2,    // write allowed
    O_EX    = 4     // execute allowed
};


enum ERROR {
    ERR_SEG_FAULT
};




// See mmu.c file for description of functions

int create_ps(int code_size, int ro_data_size, int rw_data_size,
                 int max_stack_size, unsigned char* code_and_ro_data);

void exit_ps(int pid);

int fork_ps(int pid);

void allocate_pages(int pid, int vmem_addr, int num_pages, int flags);

void deallocate_pages(int pid, int vmem_addr);

unsigned char read_mem(int pid, int vmem_addr);

void write_mem(int pid, int vmem_addr, unsigned char byte);



int page_num_to_frame_num(page_table_entry pte);

int is_readable(page_table_entry pte);

int is_writeable(page_table_entry pte);

int is_executable(page_table_entry pte);

int is_present(page_table_entry pte);


void print_page_table(int pid);