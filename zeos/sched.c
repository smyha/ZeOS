/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

union task_union task[NR_TASKS]
  __attribute__((__section__(".data.task")));

// #if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
// #endif

extern struct list_head blocked;

/* ----------------------------------------------------------------------- */

// Queue variables for the scheduler
struct list_head freequeue;
struct list_head readyqueue;

/* Global variable to access easily to the idle task */
struct task_struct *idle_task;
struct task_struct *task1;

/* ----------------------------------------------------------------------- */



/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	/** Enable interrupts to allow the timer to interrupt 
	 *  the CPU and perform the context switch 
	*/
	__asm__ __volatile__("sti": : :"memory");	

	while(1)
	{
	;
	}
}


/**
 * @brief Initializes the idle process (task 0 - kernel process)
 *
 * This function initializes the idle process, which is a special kernel process
 * that executes when no other processes are ready to run. The idle process uses
 * only the pages containing kernel code and data, which are already initialized
 * in the kernel's page table during the general memory initialization.
 * 
 * Has no hardware context or software context. Need only save the information 
 * task_switch will need to launch one process and make the change. 
 * This information is ebp (any value, 0) and return address (cpu_idle).
 *
 * The initialization process includes:
 * 1. Obtaining a free task structure from the freequeue
 * 2. Assigning PID 0 to the idle task
 * 3. Initializing the task's page directory
 * 4. Setting up the execution context by preparing the stack to simulate 
 *    returning from an interrupt
 * 5. Setting the kernel stack pointer to the appropriate location
 * 6. Storing a reference to the idle task in a global variable
 *
 * The idle task is configured to execute the cpu_idle function when scheduled.
 *
 * @see allocate_DIR
 * @see cpu_idle
 * @see init_mm in mm.c: Allocate all the he physical pages
 * required for the kernel pages (both code and data pages) and initializes all the page tables in the
 * system with the translation for these kernel pages, which are common for all the processes.
 * @see init_frames in mm.c: Initializes the ByteMap of free physical pages. The kernel pages are marked as used
 * @see init_table_pages in mm.c: Initializes the page tables for the kernel pages
 */
void init_idle (void)
{
	struct list_head *first_free = list_first(&freequeue);	// Get the first free task
	list_del(first_free);									// Remove it from the freequeue

	// From the position of the list get the task_struct pointer of the process
	struct task_struct *idle_pcb = list_head_to_task_struct(first_free);
	union task_union *idle_union = (union task_union*)idle_pcb;	

	// 1. Assign PID 0 to the idle task
	idle_pcb->PID = 0;

	// Initalize variables
	idle_pcb->quantum = DEFAULT_QUANTUM;
	idle_pcb->pending_unblocks = 0;
	idle_pcb->state = ST_READY;	// ! Mark as ready
	INIT_LIST_HEAD(&idle_pcb->kids);

	// 2. Initalize the task's page directory
	// Init dir_pages_baseAddr with a new base address of the 
	// page directory of the idle task, to save his address space 
	allocate_DIR(idle_pcb);
	
	// EXECUTION CONTEXT

	// 3. Prepare the context to execute the idle task
	// Prepare stack to simulate a context switch returning from an interrupt
	idle_union->stack[KERNEL_STACK_SIZE-1] = (DWord)&cpu_idle; // Return address
	idle_union->stack[KERNEL_STACK_SIZE-2] = 0;				   // Register EBP (undoing the dynamic link)

	// 4. Save the pointer into stack where was saved initial EBP
	idle_union->task.kernel_esp = (DWord)&(idle_union->stack[KERNEL_STACK_SIZE-2]); 
	// kernel_esp -> EBP (content: top of the stack)

	// 5. Initalize the global variable idle_task
	idle_task = idle_pcb;
}

/**
 * @brief Initializes the first user process (task1) in the ZeOS operating system
 *
 * This function prepares the first user process by:
 * 1. Obtaining an available PCB (Process Control Block) from the freequeue
 * 2. Configuring its basic process attributes including PID assignment
 * 3. Setting up memory management structures (page directory and user pages)
 * 4. Updating processor control structures for task switching
 * 
 * Detailed steps:
 * - Retrieves the first free PCB from the freequeue and removes it from the list
 * - Converts the list head pointer to a task_struct pointer for convenient access
 * - Assigns PID 1 to this process, marking it as the first user process
 * - Allocates and initializes a page directory for this process via allocate_DIR()
 * - Configures user-space memory pages through set_user_pages()
 * - Updates TSS.ESP0 to point to this process's kernel stack, enabling proper kernel/user transitions
 * - Updates MSR register 0x175 for the sysenter instruction to support fast system calls
 * - Sets this process's page directory as the current directory by updating CR3 register
 * - Stores a global reference to this task in the task1 variable for future access
 *
 * This function is critical for system initialization as it bridges from kernel-only
 * operation to the execution of the first user process.
 * 
 * @see allocate_DIR
 * @see set_user_pages in mm.c: Configures the user address space for a process by mapping physical pages
 */
void init_task1(void)
{
	struct list_head *first_free = list_first(&freequeue);	// Get the first free task
	list_del(first_free);									// Remove it from the freequeue	

	// From the position of the list get the task_struct pointer of the process
	struct task_struct *task1_pcb = list_head_to_task_struct(first_free);
	union task_union *task1_union = (union task_union*)task1_pcb;

	// 1. Assign PID 1 to the task 1
	task1_pcb->PID = 1;

	// Initalize variables
	task1_pcb->quantum = DEFAULT_QUANTUM;
	task1_pcb->pending_unblocks = 0;
	task1_pcb->state = ST_RUN;	// ! Mark as running 
	INIT_LIST_HEAD(&task1_pcb->kids);

	// 2. Initalize the task's page directory
	allocate_DIR(task1_pcb);

	// ! NOTE: Kernel region alredy configured and is the same for all processes

	/**
	 * 3. Confifure the direction space for the task 1 (code + data) 
	 * map physical pages containing user address space (code + data) 
	 * and add translation of these pages to the page table (logical-physical) 
	*/ 
	set_user_pages(task1_pcb);

	// 4. Update TSS.ESP0 to point to system stack of task 1
	tss.esp0 = KERNEL_ESP(task1_union);

	// If using sysenter, update MSR esp0 with the new task's kernel stack pointer
	// SYSENTER_ESP_MSR register 0x175: System call entry point address (system_call_handler)  
	writeMSR(0x175, (unsigned long)tss.esp0);		

	// 5. Set his page directory as the current page directory
	set_cr3(task1_union->task.dir_pages_baseAddr);
	// set_cr3(task1_pcb->dir_pages_baseAddr);
	// set_cr3(get_DIR(task1_union));

	// 6. Store a global reference to this task in the task1 variable
	task1 = task1_pcb;
}


void init_sched()
{
	/**
	 * 1. Initialize the freequeue and the readyqueue 
	 *    as empty lists. 
	 **/
  	INIT_LIST_HEAD(&freequeue);
  	INIT_LIST_HEAD(&readyqueue);
	// INIT_LIST_HEAD(&blocked);

	/* 2. Add all the tasks to the freequeue */
	for (int i = 0; i < NR_TASKS; i++)
	{
		// task[i].task.PID = -1;	// Mark as no used with -1
		list_add(&(task[i].task.list), &freequeue);
	}
}


void inner_task_switch(union task_union *new)
{	
	// 1. Update the TSS to point to the new task's kernel stack
	tss.esp0 = KERNEL_ESP(new);

	// 2. Change the user address space by updating the current page directory 
	// Write cr3 register with the new task's page directory
	set_cr3(new->task.dir_pages_baseAddr); // TLB flush -> new address space
	// set_cr3(get_DIR(&new->task)); 

	// 3. Update the MSR register 0x175 for sysenter
	writeMSR(0x175, (unsigned long)tss.esp0);

	// 4. Switch current task stack to the new task stack values
	switch_stack(&(current()->kernel_esp), new->task.kernel_esp);
}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

