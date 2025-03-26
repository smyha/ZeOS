/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

// Errno.h
#include "errno.h"

#define LECTURA 0
#define ESCRIPTURA 1

/* Referenced in interrupt.c */
extern int zeos_ticks;

/* Referenced in sched.c */
extern struct list_head freequeue;
extern struct list_head readyqueue;
extern struct list_head blocked;

extern struct task_struct *idle_task;

// Process ID counter
// 0 and 1 are reserved for the idle task and the first process
int PID_counter = 2;  

// Buffer System
#define BUFFER_SIZE 256
char sys_buffer[BUFFER_SIZE];

/* --------------------- AUXILIAR FUNCTIONS --------------------- */

int ret_from_fork(){
  // ? Debugging messages
  // printk("Direction of current task struct (ret_from_fork): ");
  // print_number((int)current());
  // printk("\n");
  return 0;
}

/**
 * set_ss_pag: Maps the physical page 'frame' to the logical page 'logical_page' 
 * -> child_PT[logical_page] = frame 
 * del_ss_pag: Removes mapping from logical page 'logical_page'
 * -> child_PT[logical_page] = 0
 * get_frame: Returns the physical page mapped to the logical page 'logical_page'
 * -> get_frame(child_PT, logical_page) 
 */
int init_child_address_space(struct task_struct *child_pcb)
{

  int new_frames[NUM_PAG_DATA];   /* No shared frames */
  int SHARED_SPACE, TOTAL_SPACE;

  /**
   * 4. Search frames (physical pages) in which to map 
   * logical pages for data+stack of the child process. 
   * If there are not enough free pages, an error will be returned.
   * 
   */
  for (int frame = 0; frame < NUM_PAG_DATA; frame++)
  {
    // Search for a free frame
    new_frames[frame] = alloc_frame();
    
    // Check if there's enough free frames
    if (new_frames[frame] < 0)
    {
      // Free frames of previous process that was allocated before to avoid memory leaks
      for (int frameD = 0; frameD < frame; frameD++)
      {
        free_frame(new_frames[frameD]); 
      }
      // Enqueue again the child process in the freequeue
      list_add_tail(&child_pcb->list, &freequeue);
      return -EAGAIN; /*Try Again*/
    }
  }

  /**
   * 5 .Initialize child’s address space: 
   * Modify the page table of the child process to map 
   * the logical addresses to the physical ones. 
   * This page table is accessible through the directory
   * field in the task_struct
   */
  page_table_entry *child_PT = get_PT(child_pcb);  /* User Entries */
  page_table_entry *parent_PT = get_PT(current()); /* System Entries (can be shared)*/

  /**
   * 6. Inherit user data
   * Copy the user data+stack pages from the parent process to the child process. 
   * The child’s physical pages are not accessible because 
   * they are not mapped in the parent’s page table. 
   * 
   * Therefore they must be temporarily mapped in some available space in
   * the parent so both regions (from parent and child) 
   * can be accessed simultaneously and copied.
   */
  SHARED_SPACE = NUM_PAG_KERNEL + NUM_PAG_CODE;
  TOTAL_SPACE = SHARED_SPACE + NUM_PAG_DATA;  

  // SYSTEM CODE (KERNEL)
  // Parent and child point to the same physical page in the kernel space (shared)
  for (int frame = 0; frame < NUM_PAG_KERNEL; frame++)
  {
    set_ss_pag(child_PT, frame + 0, get_frame(parent_PT, frame + 0));
    // child_PT[frame] = parent_PT[frame]
  }

  // USER CODE
  // Code pages are read-only and shared between parent and child and don't need to be copied
  // The child process will have the same code pages (physical frames) as the parent process 
  for (int frame = 0; frame < NUM_PAG_CODE; frame++)
  { 
    set_ss_pag(child_PT, PAG_LOG_INIT_CODE + frame, get_frame(parent_PT, PAG_LOG_INIT_CODE + frame));
    // child_PT[PAG_LOG_INIT_CODE + frame] = parent_PT[PAG_LOG_INIT_CODE + frame]
  }

  // DATA + STACK
  // Map logical pages of the child process to the physical pages allocated 
  for (int frame = 0; frame < NUM_PAG_DATA; frame++)
  {
    set_ss_pag(child_PT, PAG_LOG_INIT_DATA + frame, new_frames[frame]);
    // child_PT[PAG_LOG_INIT_DATA + frame] = parent_PT[PAG_LOG_INIT_DATA + frame]
  }
  
  /**
   * !IMPORTANT!
   * ! Parent process cannot access to physical pages of the child process directly
   * Grant temporary access to parent process (data+stack space of the child)
   * Parent point data+stack space (@F) of the child to make a copy.
   */
   for (int frame = 0; frame < NUM_PAG_DATA; frame++)
  {
    // Below data+stack space.
    // Map the shared space to the parent to access the child's physical frames
    set_ss_pag(parent_PT, TOTAL_SPACE + frame, new_frames[frame]); 

    /**
     * Do << 12 shift because of the page size [4KB - 12 bits] [PAGE_SIZE = 0x1000 = 4096]
     * in order to get the page address. Is the same by doing * PAGESIZE.
     */
    copy_data((void *)((PAG_LOG_INIT_DATA + frame) << 12), // Origin : Parent Data 
              (void *)((TOTAL_SPACE + frame) << 12),       // Destination : "Child" Data 
              PAGE_SIZE);                                  // Size : PAGE_SIZE (4KB)
    // parent_PT[TOTAL_SPACE + frame] = child_PT[PAG_LOG_INIT_DATA + frame]

    // Unmap the shared space to avoid the child to access the parent's data
    del_ss_pag(parent_PT, TOTAL_SPACE + frame);  
  }

  // TLB flush (invalidate TLB entries with the shared pages)
  set_cr3(get_DIR(current()));

  return 0;
}

/* --------------------- SYSTEM CALLS --------------------- */

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

/**
 * @brief Returns the Process ID (PID) of the current process
 *
 * This function is a system call implementation that retrieves the
 * Process ID of the currently executing process.
 *
 * @return The PID of the current process
 */
int sys_getpid()
{
	return current()->PID;
}

int sys_fork()
{
  int PID=-1;

  // creates the child process
  // 1. If there is no space for a new process, an error will be returned.
  if (list_empty(&freequeue)) 
    return -ENOMEM;  // Theres no free PCB task_struct

  struct list_head *first_free = list_first(&freequeue);
  list_del(first_free);

  // Get the task_struct pointer of the process
  struct task_struct *child_pcb = list_head_to_task_struct(first_free);
  union task_union *child_union = (union task_union*)child_pcb;

  /** @see utils.c
   * void copy_data(void *start, void *dest, int size)
   */

  // 2. Copy the parent's task_union to the child's task_union
  copy_data(current(), child_union, PAGE_SIZE);
  // PAGE_SIZE = KERNEL_STACK_SIZE * sizeof(unsigned long) 

  /**
   * 3. Get a new page directory to store the child’s address space 
   * and initialize the dir_pages_baseAddr field using the allocate_DIR routine.
   */
  allocate_DIR(child_pcb);

  // Initialize the child's address space
  int res = init_child_address_space(child_pcb);
  if (res < 0)
    return res;

  /**
   * 7. Assign the PID to the child process.
   * The PID must be different from its position in the task_array table.
   */
  PID = child_pcb->PID = PID_counter;
  child_pcb->parent = current();

  PID_counter++;
  
  // 8. Initialize the fields of the task_struct that are not common to the child
  child_pcb->quantum = DEFAULT_QUANTUM;
  child_pcb->state = ST_READY;  
  child_pcb->pending_unblocks = 0;

  INIT_LIST_HEAD(&child_pcb->kids); // Initialize the children list of the child
  list_add_tail(&child_pcb->list, &current()->kids);  // Add the child to the parent's children list

  /**
   * 9. Prepare the child stack so that task_switch will return 0 in the child process
   * Higher memory addresses
   * +--------------------+
   * | task_union (k_esp) |
   * +--------------------+
   * |        ...         | 
   * +--------------------+ 
   * |    fake EBP (0)    |
   * +--------------------+ <- stack[KERNEL_STACK_SIZE - 19] 
   * | @ret ret_from_fork |
   * +--------------------+ <- stack[KERNEL_STACK_SIZE - 18]
   * |@ret sycall_handler |
   * +--------------------+ <- stack[KERNEL_STACK_SIZE - 17]
   * |  CTX SW - 11 regs  |
   * +--------------------+ <- stack[KERNEL_STACK_SIZE - 6]
   * |  CTX HW - 5 regs   | 
   * +--------------------+ <- stack[KERNEL_STACK_SIZE - 1]
   * Lower memory addresses
   */
  child_union->stack[KERNEL_STACK_SIZE - 19] = 0; // ebp
  child_union->stack[KERNEL_STACK_SIZE - 18] = (unsigned long) ret_from_fork; // Return address
  
  // esp points to fake ebp (it will pop and on top of the stack will be the ret from fork)
  child_union->task.kernel_esp = (unsigned long) &(child_union->stack[KERNEL_STACK_SIZE - 19]);

  // 10. Enqueue the child process in the readyqueue to be executed 
  list_add_tail(&child_pcb->list, &readyqueue);

  return PID;
}

/**
 * @brief Terminates the current process and performs cleanup operations.
 * 
 * It performs the following operations:
 * 1. Verifies that the process is not the init process (PID 1), which cannot exit
 * 2. Handles orphaned child processes by reassigning them to the idle task
 * 3. Frees physical memory frames allocated for the process's data and stack
 * 4. Returns the process control block (PCB) to the free queue for reuse
 * 5. Schedules a new process to run using the round-robin scheduler
 * 
 * The kernel pages and code pages (which may be shared) are preserved during cleanup.
 * This function does not return to the caller as it triggers a context switch.
 * 
 * @note In ZeOS, unlike standard Unix, exit() does not return any value.
 */
void sys_exit()
{
  struct task_struct *current_pcb = (struct task_struct *)current();
  page_table_entry *current_PT = get_PT(current_pcb);
  
  // Get the parent process
  struct list_head *current_kids = list_first(&current_pcb->kids);

  // If the process is the task 1, it cannot exit
  if (current_pcb->PID == 1)
  {
    printk("\nThe task 1 cannot exit\n");
    return;
  }
  
  // ? Debugging messages
  // printk("\nPID of the process that is going to exit: ");
  // print_number(current_pcb->PID);
  // printk("\n");

  // 1. Free data structures & resources of current process (PCB, frames, etc.)
  // If the process has children, they will be adopted by the idle task
  if (current_kids != NULL)
  {
    list_for_each(current_kids, &current_pcb->kids)
    { 
      // Adopt the child processes of the current process by the idle task 
      list_add_tail(current_kids, &idle_task->kids);  
      
      
      // Get the task_struct pointer of the child process
      struct task_struct *child_pcb = list_head_to_task_struct(current_kids); 
      child_pcb->parent = idle_task;  // Adopted by the idle task
      list_del(current_kids); // Remove from the parent's children list
    }
  }

  // ! Free the physical frames of the data+stack pages of the current process
  // ! Keep the kernel pages and the code pages (shared)
  for (int i = 0; i < NUM_PAG_DATA; i++)
  {
    int frame = get_frame(current_PT, PAG_LOG_INIT_DATA + i);
    free_frame(frame);
    del_ss_pag(current_PT, PAG_LOG_INIT_DATA + i);
  }

  // current_pcb->PID = -1;  // ! Mark as unused
  list_add_tail(&current_pcb->list, &freequeue); // Add the process to the freequeue

  /**
   * 2. Use the scheduler interface to select a new process 
   * to be executed and make a context switch.
   */
  sched_next_rr();
}

/**
 * @brief Blocks the current process unless there are pending unblocks
 *
 * This function attempts to block the current process. If there are no pending unblocks,
 * it changes the process state to ST_BLOCKED, adds it to the blocked queue, and triggers
 * a context switch by calling schedule(). If there are pending unblocks, it decrements
 * the pending_unblocks counter instead of blocking the process.
 *
 */
void sys_block(void)
{
  struct task_struct *current_pcb;
  int pending_unblocks;
  
  current_pcb = current();
  pending_unblocks = current_pcb->pending_unblocks;

  // If there are no pending unblocks, block the process
  if (pending_unblocks == 0)
  {
    current_pcb->state = ST_BLOCKED;  // Change the state to blocked
    list_add_tail(&current_pcb->list, &blocked); // Add to the blocked queue
    schedule(); // Select a new process to run
    // sched_next_rr(); // Select a new process to run
  }
  else
  {
    current_pcb->pending_unblocks -= 1; // Decrement pending unblocks
    // ? Debugging messages
    // printk("Pending unblocks: ");
    // print_number(current_pcb->pending_unblocks);
    // printk("\n");
  }
}

/**
 * @brief Unblocks a process identified by its PID
 *
 * This function searches for a process with the specified PID in the blocked queue.
 * If found and the process is in ST_BLOCKED state, it changes its state to ST_READY,
 * removes it from the blocked queue, and adds it to the ready queue.
 * If the process is found but not in blocked state, it increments the current process's
 * pending_unblocks counter.
 *
 * @param PID The process ID of the process to unblock
 * @return 0 on success (process found), -1 if process not found
 */
int sys_unblock(int PID)
{
  struct task_struct *unblocked_pcb;
  struct list_head *unblocked_list;

  // Check the blocked child processes of the current process
  list_for_each(unblocked_list, &current()->kids)
  {
    // Get the task_struct pointer of the process
    unblocked_pcb = list_head_to_task_struct(unblocked_list);
    if (unblocked_pcb->PID == PID)
    {
      // If the process is blocked, unblock it
      if (unblocked_pcb->state == ST_BLOCKED)
      {
        unblocked_pcb->state = ST_READY;  // Change the state to ready
        // unblocked_pcb->pending_unblocks = 0; // Reset pending unblocks
        list_del(unblocked_list); // Remove from the blocked queue
        list_add_tail(unblocked_list, &readyqueue); // Add to the ready queue
        return 0;
      }
      // If the process is not blocked, return an error
      else 
      {
        current()->pending_unblocks += 1; 
        return 0; // Process not found
      }
    }
  }
  return -ECHILD; // Child not found
}

/**
 * @brief Writes data from a user buffer to a file descriptor (console).
 * 
 * This system call writes up to 'size' bytes from the user buffer to the specified
 * file descriptor. The implementation handles large writes by breaking them into
 * smaller chunks of BUFFER_SIZE bytes.
 *
 * @param fd File descriptor where data will be written (currently only supports console)
 * @param buffer Pointer to the user space buffer containing data to be written
 * @param size Number of bytes to write from the buffer
 *
 * @return On success, returns the number of bytes written (may be less than size).
 *         On error, returns a negative error code:
 *         - -EFAULT: Buffer points to invalid address or is NULL
 *         - -EINVAL: Size parameter is negative
 *
 * @note The function implements buffered writing using a kernel buffer of size BUFFER_SIZE
 * @note If the write is partial (less bytes written than requested), it returns early
 * @note This implementation only supports writing to the console device
 */
int sys_write(int fd, char *buffer, int size)
{
  int bytes_written, remaining_bytes, offset;
  int error_fd;

  bytes_written = 0;

  // 1. Check Permissions and file descriptor
  error_fd = check_fd(fd, ESCRIPTURA);
  if (error_fd) 
    return error_fd; 
    
  // 2.Check buffer (ony verify that is not NULL)
  if (buffer == NULL) 
    return -EFAULT; /*NULL pointer*/
 
  // 3. Check buffer access 
  if(!access_ok(VERIFY_READ, buffer, size)) {
    return -EFAULT;
  }

  // 4. Check size
  if (size < 0) 
    return -EINVAL; /*Invalid argument*/

  // 5. Write to the device
  remaining_bytes = size;
  offset = 0;

  while (remaining_bytes > BUFFER_SIZE)
  {
    copy_from_user(buffer + offset, sys_buffer, BUFFER_SIZE);
  
    bytes_written += sys_write_console(sys_buffer, BUFFER_SIZE);

    remaining_bytes -= BUFFER_SIZE;
    offset += BUFFER_SIZE;
  }

  // Handle remaining bytes
  if (remaining_bytes > 0)
  {
    copy_from_user(buffer + offset, sys_buffer, remaining_bytes);
    bytes_written += sys_write_console(sys_buffer, remaining_bytes);
  }

  return bytes_written;
}

/**
 * @brief Returns the number of system ticks since system start
 * 
 * This system call retrieves the current value of the system timer counter
 * (zeos_ticks) which tracks the number of timer interrupts since boot.
 * Each tick represents one timer interrupt interval.
 *
 * @return Number of system ticks elapsed since system initialization
 */
int sys_gettime(){
  return zeos_ticks;
}
