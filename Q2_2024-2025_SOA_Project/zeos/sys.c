/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <p_stats.h>

#include <errno.h>

#include <types.h>

// External declaration of pthread_create from user code
extern int pthread_create(void *(*func)(void*), void *param, int stack_size);
extern void insert_ready_ordered(struct task_struct *t);

#define LECTURA 0
#define ESCRIPTURA 1
#define MAX_PRIORITY 100
#define MAX_STACK_SIZE 65536

// ! CLONE_THREAD and CLONE_PROCESS
#define CLONE_THREAD 0
#define CLONE_PROCESS 1
#define DEFAULT_STACK_SIZE 1024
#define DEFAULT_REGION PAG_LOG_INIT_DATA+NUM_PAG_DATA

void * get_ebp();

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -EBADF; 
  if (permissions!=ESCRIPTURA) return -EACCES; 
  return 0;
}

void user_to_system(void)
{
  update_stats(&(current()->p_stats.user_ticks), &(current()->p_stats.elapsed_total_ticks));
}

void system_to_user(void)
{
  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
}

int sys_ni_syscall()
{
	return -ENOSYS; 
}

int sys_getpid()
{
	return current()->PID;
}

int global_PID=1000;
int global_TID=0;   // ! Added

int ret_from_fork()
{
  return 0;
}

// ! Removing sys_fork as it's now handled by sys_clone

#define TAM_BUFFER 512

int sys_write(int fd, char *buffer, int nbytes) {
  char localbuffer [TAM_BUFFER];
  int bytes_left;
  int ret;

	if ((ret = check_fd(fd, ESCRIPTURA)))
		return ret;
	if (nbytes < 0)
		return -EINVAL;
	if (!access_ok(VERIFY_READ, buffer, nbytes))
		return -EFAULT;
	
	bytes_left = nbytes;
	while (bytes_left > TAM_BUFFER) {
		copy_from_user(buffer, localbuffer, TAM_BUFFER);
		ret = sys_write_console(localbuffer, TAM_BUFFER);
		bytes_left-=ret;
		buffer+=ret;
	}
	if (bytes_left > 0) {
		copy_from_user(buffer, localbuffer,bytes_left);
		ret = sys_write_console(localbuffer, bytes_left);
		bytes_left-=ret;
	}
	return (nbytes-bytes_left);
}

extern int zeos_ticks;

int sys_gettime()
{
  return zeos_ticks;
}

// ! Modified
// Releases all threads and all memory (data + user stacks of threads)
void sys_exit() {  
    struct task_struct *current_th = current();
    struct task_struct *master_th = current_th->master_thread;
    page_table_entry *process_PT = get_PT(current_th);

    // Free all data frames 
    for (int i = 0; i < NUM_PAG_DATA; i++) {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA + i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA + i);
    }

    // Free the semaphores
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        master_th->semaphores->sem[i].TID = -1;
        master_th->semaphores->sem[i].count = -1;
        INIT_LIST_HEAD(&master_th->semaphores->sem[i].blocked);
    }

    // If there are threads, free them
    if (!list_empty(&master_th->threads)) {
      struct list_head *lm = list_first(&master_th->threads);
      while (lm != &master_th->threads) {
        struct task_struct *ts = list_head_to_task_struct(lm);
        page_table_entry *pt = get_PT(ts);
        
        // Mark the thread as unused
        ts->PID = -1;
        ts->TID = -1;
        ts->thread_count = 0;

        // Free the thread's user stack (including the screen frame)
        if (ts->user_stack_ptr != NULL && ts->user_stack_frames > 0) {
          for (int us_frame = 0; us_frame < ts->user_stack_frames; us_frame++) {
            unsigned int user_stack_page = ((unsigned int)ts->user_stack_ptr >> 12) + us_frame;
            free_frame(get_frame(pt, user_stack_page));
            del_ss_pag(pt, user_stack_page);
          } 
        }

        // Remove the thread from the list
        list_del(&ts->list);
          
        // Add the task_struct to the free queue
        list_add_tail(&ts->list, &freequeue);

        lm = lm->next;
      }
    }

    // Free the master thread's user stack
    if (master_th->user_stack_ptr != NULL && master_th->user_stack_frames > 0) {
      page_table_entry *pt = get_PT(master_th);
      for (int us_frame = 0; us_frame < master_th->user_stack_frames; us_frame++) {
        unsigned int user_stack_page = ((unsigned int)master_th->user_stack_ptr >> 12) + us_frame;
        free_frame(get_frame(pt, user_stack_page));
        del_ss_pag(pt, user_stack_page);
      }
    }

    // Mark the master thread as unused
    master_th->PID = -1;
    master_th->TID = -1;
    master_th->thread_count = 0;

    // Add the master thread to the free queue
    list_add_tail(&master_th->list, &freequeue);

    // Schedule the next process   
    sched_next_rr();
}

/* System call to force a task switch */
int sys_yield()
{
  force_task_switch();
  return 0;
}

extern int remaining_quantum;

int sys_get_stats(int pid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT; 
  
  if (pid<0) return -EINVAL;
  for (i=0; i<NR_TASKS; i++)
  {
    if (task[i].task.PID==pid)
    {
      task[i].task.p_stats.remaining_ticks=remaining_quantum;
      copy_to_user(&(task[i].task.p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -ESRCH; /*ESRCH */
}

// ------------------ MILESTONE 1 -------------------

extern char keyboard_buffer[128];
// Get keyboard state
int sys_GetKeyboardState(char *keyboard) {
  // Check buffer size
  if (!access_ok(VERIFY_WRITE, keyboard, 128))  
    return -EFAULT;
  
  // Copy the keyboard buffer to user space
  if (copy_to_user(keyboard_buffer, keyboard, 128) < 0)
    return -EFAULT;
  
  return 0;
}

extern struct list_head blocked;
// Pause
int sys_pause (int miliseconds) {
  struct task_struct *t = current();
  
  // Check if the time is valid
  if (miliseconds < 0) return -EINVAL;

  // ! 18 ticks = 1 ms
  t->pause_time = miliseconds * 0.018;  
  update_process_state_rr(t, &blocked); // Block the process
  sched_next_rr();

  return 0;
}

// ------------------ MILESTONE 2 -------------------

// Screen support 
void *sys_StartScreen() {
  struct task_struct *t = current();
  
  // Check if process already has a screen page
  if (t->screen_page != (void*)-1) {
    return t->screen_page;
  }
  
  // Allocate a new physical page
  int phy_page = alloc_frame();
  if (phy_page < 0)
    return (void*)-EAGAIN;
  
  // Map the page to user space
  page_table_entry *process_PT = get_PT(t);
  
  // Map the screen page at a fixed logical address
  // This ensures all processes access the screen at the same address
  set_ss_pag(process_PT, DEFAULT_REGION, phy_page);
  
  // Store the page address of the screen in task struct
  t->screen_page = (void*)((DEFAULT_REGION) << 12);
  
  // Return the page address
  return t->screen_page;
}

// ------------------ MILESTONE 3 -------------------

/**
 * User Stack (in user space):
 * +------------------+
 * |     stack_size   | <- stack_top (initial position)
 * +------------------+
 * |        ...       |
 * +------------------+
 * |        0         | <- user_esp points here (return address, never used)
 * +------------------+
 * |      param       | <- param value
 * +------------------+
 * |        ...       |
 * +------------------+
 * |    stack_base    |
 * +------------------+
 * 
 * 
 * Kernel Stack (in kernel space):
 * +------------------+
 * |    KERNEL_ESP    | <- Base of kernel stack
 * +------------------+
 * |        ...       |
 * +------------------+
 * |      func        | <- eip (at KERNEL_ESP - 0x05)
 * +------------------+
 * |        ...       |
 * +------------------+
 * |     user_esp     | <- esp (at KERNEL_ESP - 0x02)
 * +------------------+
 * |        ...       |
 * +------------------+
 * | register_esp     | <- Points to KERNEL_STACK_SIZE - 0x12
 * +------------------+
 * 
 * For the calculation of the frame pointer (register_ebp) of the new thread:
 * 
 * register_ebp = (register_ebp - (int)current()) + (int)(uchild);
 * 
 * This is equivalent to:
 * OFFSET = EBP_PARENT - PARENT_TASK_STRUCT
 * NEW_EBP = OFFSET + NEW_TASK_STRUCT
 * 
 * - EBP is used to access to local variables and parameters of the current thread.
 * - The current thread is the thread that is calling the clone system call.
 */

// ! Helper functions for sys_clone

/**
 * @brief Search for contiguous free space in the page table
 * 
 * This function searches for a contiguous block of free pages in the page table,
 * taking into account the existing thread stacks to avoid overlaps.
 * 
 * Memory Layout Considered:
 * +------------------------+
 * | Thread 1 Stack         |
 * +------------------------+
 * | Thread 2 Stack         |
 * +------------------------+
 * | Master Thread Stack    |
 * +------------------------+
 * 
 * The function ensures that:
 * 1. The requested number of pages is available
 * 2. The pages are contiguous
 * 3. The pages don't overlap with existing thread stacks
 * 4. The pages are aligned to page boundaries
 * 
 * @param PT The page table to search in
 * @param start_page The starting page number to begin search
 * @param pages_needed The number of contiguous pages needed
 * @param master_th The master thread of the process
 * @return The first page number of the free space, or -1 if not found
 */
int search_free_frame(page_table_entry *PT, int start_page, int pages_needed, struct task_struct *master_th) {
  // Check if the number of pages is valid
  if (pages_needed <= 0) return -1;

  // Get the current task and its master thread
  struct list_head   *threads_list = &master_th->threads_list;

  // Search for contiguous free space
  for (int i = start_page; i <= TOTAL_PAGES; ++i) {
    // Check if the first page is free
    if (PT[i].entry == 0) {
      int new_start = i;
      int new_end = i + pages_needed;
      int conflict = 0;

      // Check for conflicts with other threads' stacks
      struct list_head *pos;
      list_for_each(pos, threads_list) {
        struct task_struct *thr = list_head_to_task_struct(pos);
        
        // Calculate thread's stack boundaries
        int thr_start = ((unsigned int)thr->user_stack_ptr) >> 12;
        int thr_end = thr_start + thr->user_stack_frames;

        // Check for overlap
        if (!(new_end <= thr_start || new_start >= thr_end)) {
          conflict = 1;
          i = thr_end - 1;  // Skip to after this thread's stack
          break;
        }
      }

      // Check for conflict with master thread's stack
      if (!conflict && master_th->user_stack_ptr != NULL) {
        int master_start = ((unsigned int)master_th->user_stack_ptr) >> 12;
        int master_end = master_start + master_th->user_stack_frames;
      
        // Check if the new stack overlaps with the master's stack
        if (!(new_end <= master_start || new_start >= master_end)) {
          conflict = 1;
          i = master_end - 1;  // Skip to after master's stack
        }
      }

      // If no conflicts found, return the start page
      if (!conflict) {
        return i;
      }
    }
  }

  // If no free space is found, return -1
  return -1;
}

/**
 * @brief Initialize common task fields for both threads and processes
 * @param task The task to initialize
 * @param parent The parent task
 */
static void init_common_task_fields(struct task_struct *task, struct task_struct *parent) {
  task->state = ST_READY;
  task->priority = parent->priority;
  task->pause_time = 0;
  
  // Initialize thread lists
  INIT_LIST_HEAD(&(task->threads_list));
  INIT_LIST_HEAD(&(task->threads));
  
  // Initialize stats
  init_stats(&(task->p_stats));
}

/**
 * @brief Handle memory allocation errors in clone
 * @param process_PT The process page table
 * @param start_page The start page to deallocate
 * @param num_pages Number of pages to deallocate
 * @param task_struct The task struct to return to free queue
 * @return Error code
 */
static int handle_memory_error(page_table_entry *process_PT, int start_page, int num_pages, 
                               struct list_head *task_struct) {
  // Deallocate allocated pages
  for (int i = 0; i < num_pages; i++) {
    free_frame(get_frame(process_PT, start_page + i));
    del_ss_pag(process_PT, start_page + i);
  }
  
  // Return task struct to free queue
  list_add_tail(task_struct, &freequeue);
  return -EAGAIN;
}

/**
 * @brief Setup screen page for new task
 * @param child_task The child task
 * @param parent_task The parent task
 * @param process_PT The process page table
 * @param parent_PT The parent page table
 */
static void setup_screen_page(struct task_struct *child_task, struct task_struct *parent_task, 
                              page_table_entry *process_PT, page_table_entry *parent_PT) {
  if (parent_task->screen_page != (void*)-1) {
    unsigned int frame_screen_page = get_frame(parent_PT, (unsigned)parent_task->screen_page >> 12);
    unsigned int screen_log_page = (unsigned)parent_task->screen_page >> 12;
    set_ss_pag(process_PT, screen_log_page, frame_screen_page);
    child_task->screen_page = (void*)(screen_log_page << 12);
  } else {
    child_task->screen_page = (void*)-1;
  }
}

/**
 * @brief Creates a new thread or process using clone
 * @param what CLONE_THREAD or CLONE_PROCESS
 * @param func Function to execute in the new thread
 * @param param Parameter for the thread function
 * @param stack_size Size of the user stack
 * @return TID of the new thread or PID of the new process
 */
int sys_clone(int what, void *(*func)(void*), void *param, int stack_size) {
  // Check if the parameters are valid
  if (what != CLONE_PROCESS && what != CLONE_THREAD) 
    return -EINVAL;

  // If it's a thread, check function pointer and stack size
  if (what == CLONE_THREAD) {
    if (!func) 
      return -EINVAL;
    if (!access_ok(VERIFY_READ, func, sizeof(void (*)(void*))))
      return -EFAULT;
    if (param && !access_ok(VERIFY_READ, param, sizeof(void*)))
      return -EFAULT;
    if (stack_size <= 0 || stack_size > MAX_STACK_SIZE) 
      return -EINVAL;
  }

  // If there are no free task structs, return an error
  if (list_empty(&freequeue)) 
    return -ENOMEM;

  // Get the first free task struct
  struct list_head *lhcurrent = list_first(&freequeue);
  list_del(lhcurrent);

  // Copy the parent's task struct to the new thread/process
  struct task_struct *current_thread = current();
  union task_union *uchild = (union task_union*)list_head_to_task_struct(lhcurrent);
  copy_data(current_thread, uchild, sizeof(union task_union));

  // Get the main thread (could be the current thread or its main thread)
  struct task_struct *master_thread = current_thread->master_thread;
  page_table_entry *process_PT = get_PT(&uchild->task);
  page_table_entry *parent_PT = get_PT(current());

  // ! Thread creation
  if (what == CLONE_THREAD) {
    // Calculate number of pages needed for the stack
    int pages_needed = (stack_size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Find a free region of consecutive logical pages for the user stack
    int stack_start = search_free_frame(process_PT, DEFAULT_REGION+1, pages_needed, master_thread);
    if (stack_start == -1) {
      list_add_tail(lhcurrent, &freequeue);
      return -ENOMEM;
    }

    // Allocate and map pages for the user stack
    for (int i = 0; i < pages_needed; i++) {
      int user_stack_page = alloc_frame();
      if (user_stack_page == -1) {
        return handle_memory_error(process_PT, stack_start, i, lhcurrent);
      }
      set_ss_pag(process_PT, stack_start + i, user_stack_page);
    }

    // Increment the thread count on the master thread
    master_thread->thread_count++;
    
    // Initialize thread-specific fields
    uchild->task.TID = master_thread->thread_count;
    uchild->task.master_thread = master_thread;
    uchild->task.next_sem_id = master_thread->next_sem_id;

    // Share screen page with parent
    setup_screen_page(&uchild->task, master_thread, process_PT, parent_PT);

    // Set up user stack for thread
    int* user_stack_esp = (int*)(stack_start << 12);
    user_stack_esp[stack_size - 1] = (int)param;      // Parameter
    user_stack_esp[stack_size - 2] = (int)func;       // Function
    user_stack_esp[stack_size - 3] = 0;               // Return address (never used)

    // Set up kernel stack [HARDWARE CONTEXT]
    uchild->stack[KERNEL_STACK_SIZE - 2] = (unsigned int)user_stack_esp;  // esp (user stack pointer)
    uchild->stack[KERNEL_STACK_SIZE - 5] = (unsigned int)func;            // eip (entry point)
 
    // Set up frame pointer [SOFTWARE CONTEXT]
    uchild->task.register_esp = (unsigned int) &(uchild->stack[KERNEL_STACK_SIZE - 18]);  // ebp

    // Store user stack pointer and frames
    uchild->task.user_stack_ptr = user_stack_esp;
    uchild->task.user_stack_frames = pages_needed;

    // Add to thread list
    list_add_tail(&(uchild->task.threads_list), &(master_thread->threads));
  } else {  
    // ! Process creation (like old sys_fork)
    // Allocate and map pages for DATA
    int new_ph_pag, pag, i;
    for (pag=0; pag<NUM_PAG_DATA; pag++) {
      new_ph_pag = alloc_frame();
      if (new_ph_pag != -1) { /* One page allocated */
        set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
      } else {/* No more free pages left. Deallocate everything */
        return handle_memory_error(process_PT, PAG_LOG_INIT_DATA, pag, lhcurrent);
      }
    }

    // Find a free region for temporary mapping
    int temp_frame_start = search_free_frame(process_PT, DEFAULT_REGION+1, NUM_PAG_DATA, master_thread);
    if (temp_frame_start == -1) {
      return -EFAULT; 
    }

    // Copy parent's SYSTEM and CODE to child
    for (pag=0; pag<NUM_PAG_KERNEL; pag++) {
      set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
    }
    for (pag=0; pag<NUM_PAG_CODE; pag++) {
      set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
    }

    /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
    for (pag = PAG_LOG_INIT_DATA; pag < NUM_PAG_DATA; pag++) {
      // Get the scratch page that will be used to map the parent's page
      int scratch_page = temp_frame_start + pag;

      // Map the parent's page to the scratch page
      set_ss_pag(parent_PT, scratch_page, get_frame(process_PT, pag));

      // Copy the data from the parent's page to the scratch page
      copy_data((void*)(pag<<12),           // Source: Parent's page
                (void*)(scratch_page<<12),  // Destination: Scratch page
                PAGE_SIZE);
      
      // Remove the parent's page from the scratch page
      del_ss_pag(parent_PT, scratch_page);
    }

    /* Deny access to the child's memory space */
    set_cr3(get_DIR(current()));

    // Copy user stack to child if it exists
    if (current_thread->TID != 1 && current_thread->user_stack_ptr != NULL) {
      int stack_start = search_free_frame(process_PT, DEFAULT_REGION+1, current_thread->user_stack_frames, &uchild->task);
      if (stack_start == -1) {
        return handle_memory_error(process_PT, PAG_LOG_INIT_DATA, NUM_PAG_DATA, lhcurrent);
      }

      for (i = 0; i < current_thread->user_stack_frames; i++) {
        new_ph_pag = alloc_frame();
        if (new_ph_pag == -1) {
          // Deallocate stack pages
          handle_memory_error(process_PT, stack_start, i, NULL);
          // Deallocate data pages
          return handle_memory_error(process_PT, PAG_LOG_INIT_DATA, NUM_PAG_DATA, lhcurrent);
        }
        set_ss_pag(process_PT, stack_start + i, new_ph_pag);
        set_ss_pag(parent_PT, temp_frame_start + i, new_ph_pag);
        copy_data((void*)(unsigned)(current_thread->user_stack_ptr) + (i << 12),
                  (void*)((stack_start + i) << 12),
                  PAGE_SIZE);
        del_ss_pag(parent_PT, temp_frame_start + i);
      }

      uchild->task.user_stack_ptr = (int*)(stack_start << 12);
      uchild->task.user_stack_frames = current_thread->user_stack_frames;

      // Set up kernel stack for process
      unsigned int parent_esp = ((unsigned long *)KERNEL_ESP((union task_union*)current_thread))[-0x02];
      unsigned int offset = parent_esp - (unsigned int)(current_thread->user_stack_ptr);
      unsigned int new_esp = ((unsigned int)(uchild->task.user_stack_ptr)) + offset;
      ((unsigned long *)KERNEL_ESP(uchild))[-0x02] = new_esp;
    } else {
      uchild->task.user_stack_ptr = NULL;
      uchild->task.user_stack_frames = 0;
    }

    // Set up frame pointer and return address for the child process
    int register_ebp = (int)get_ebp();
    register_ebp = (register_ebp - (int)current()) + (int)(uchild);
    uchild->task.register_esp = register_ebp + sizeof(DWord);
    DWord temp_ebp = *(DWord*)register_ebp;
    uchild->task.register_esp -= sizeof(DWord);
    *(DWord*)(uchild->task.register_esp) = (DWord)&ret_from_fork;
    uchild->task.register_esp -= sizeof(DWord);
    *(DWord*)(uchild->task.register_esp) = temp_ebp;

    // Initialize process-specific fields
    uchild->task.PID = ++global_PID;
    uchild->task.TID = 1;
    uchild->task.thread_count = 1;
    uchild->task.master_thread = &uchild->task;
    uchild->task.next_sem_id = 0;

    // Share screen page with parent
    setup_screen_page(&uchild->task, current_thread, process_PT, parent_PT);

    // Get a free semaphore from the semaphore array
    for (int i = 0; i < NR_TASKS; i++) {
      // If the semaphore is free, set the owner and next_sem_id
      if (semaphores[i].owner == -1) {
        // Set the owner and next_sem_id
        semaphores[i].owner = uchild->task.PID;
        uchild->task.next_sem_id = i;
        // Set the semaphore array
        uchild->task.semaphores = &semaphores[i];
        break;
      }
    }
  }

  // Initialize common task fields
  init_common_task_fields(&uchild->task, current_thread);

  // Add to ready queue with priority 
  // If the inserted task has higher priority than current, force reschedule
  insert_ready_ordered(&uchild->task);

  // Return the TID of the new thread or PID of the new process
  return (what == CLONE_THREAD) ? uchild->task.TID : uchild->task.PID;
}

// Set priority
int sys_SetPriority(int priority) {
  // Check if the priority is valid
  if (priority < 0 || priority > MAX_PRIORITY) {
    return -EINVAL;
  }

  // Set the priority
  current()->priority = priority;
  return 0;
}

// Pthread exit
int sys_pthread_exit() {
  struct task_struct *current_thread = current();
  struct task_struct *master_thread = current_thread->master_thread;

  // If it's the last thread, terminate the entire process
  if (master_thread->thread_count == 1) {
      sys_exit();
      return 0; // ! This should never happen
  }

  page_table_entry *pt = get_PT(current_thread);

  // Free the thread's stack
  if (current_thread->user_stack_ptr != NULL && current_thread->user_stack_frames > 0) {
      for (int us_frame = 0; us_frame < current_thread->user_stack_frames; us_frame++) {
          unsigned int user_stack_page = ((unsigned int)current_thread->user_stack_ptr >> 12) + us_frame;
          free_frame(get_frame(pt, user_stack_page));
          del_ss_pag(pt, user_stack_page);  // Remove the stack page from the page table
      }
  }

  // Mark the thread as unused
  current_thread->PID = -1;
  current_thread->TID = -1;

  // If it's the master thread, we need to choose a new master
  if (current_thread == master_thread) {
      struct list_head *lm = list_first(&master_thread->threads);
      int found = 0;

      // Search for the first non-blocked thread to be the new master
      while (!found && lm != &master_thread->threads) {
          struct task_struct *ts = list_head_to_task_struct(lm);

          // If the thread is not blocked, it can be the new master
          if (ts->state != ST_BLOCKED) {
              found = 1;
          } else {
            // Move to the next thread
            lm = lm->next;
          }
      }
      // Get the new master
      struct task_struct *new_master = list_head_to_task_struct(lm);

      // Update the thread count
      new_master->thread_count = master_thread->thread_count - 1;

      // Copy the semaphores to the new master
      for (int i = 0; i < MAX_SEMAPHORES; i++) {
          new_master->semaphores->sem[i].count = master_thread->semaphores->sem[i].count;
          new_master->semaphores->sem[i].TID = master_thread->semaphores->sem[i].TID;
          INIT_LIST_HEAD(&new_master->semaphores->sem[i].blocked);

          // Move the blocked threads to the new master
          lm = list_first(&master_thread->semaphores->sem[i].blocked);
          while (lm != &master_thread->semaphores->sem[i].blocked) {
              list_add_tail(lm, &new_master->semaphores->sem[i].blocked);
              lm = lm->next;
          }
      }

      // Update the thread list
      list_del(&new_master->threads_list);
      lm = list_first(&master_thread->threads);
      new_master->master_thread = new_master;

      // Update the master of all threads
      while (lm != &master_thread->threads) {
          // Add the thread to the new master's thread list
          list_add_tail(lm, &new_master->threads);

          // Update the master of the thread
          struct task_struct *ts = list_head_to_task_struct(lm);
          ts->master_thread = new_master;

          // Move to the next thread
          lm = lm->next;
      }
  }
  // Add the task_struct to the free queue
  list_add_tail(&current_thread->list, &freequeue);

  // Schedule the next thread
  sched_next_rr();

  return 0;
}

// ------------------ MILESTONE 4 -------------------

struct sem_array semaphores[NR_TASKS];
 
int sys_sem_init(int value) {
  struct task_struct *master = current()->master_thread;
  
  // Check if we've reached the maximum number of semaphores for this process
  if (master->next_sem_id >= MAX_SEMAPHORES) 
    return -ENOMEM; // No more semaphores available
  
  // Initialize the semaphore at the next available position for this process
  master->semaphores->sem[master->next_sem_id].count = value;
  master->semaphores->sem[master->next_sem_id].TID = current()->TID;
  master->semaphores->owner = current()->TID;
  INIT_LIST_HEAD(&master->semaphores->sem[master->next_sem_id].blocked);
  
  // Return the current semaphore ID and increment the counter for this process
  return master->next_sem_id++;
}

// Semaphore wait
int sys_sem_wait(int sem_id) {
  struct task_struct *master = current()->master_thread;
  
  // A process can only have MAX_SEMAPHORES semaphores
  // Check if the semaphore id is valid 
  if (sem_id < 0 || sem_id > MAX_SEMAPHORES || master->semaphores->sem[sem_id].TID == -1)
    return -EINVAL;

  // Decrease the semaphore count
  master->semaphores->sem[sem_id].count -= 1;  

  // Check if the semaphore is already locked
  if (master->semaphores->sem[sem_id].count < 0) {
    // Block the thread
    current()->state = ST_BLOCKED;
    list_add_tail(&(current()->list), &(master->semaphores->sem[sem_id].blocked));

    // Schedule the next thread
    sched_next_rr();
  }

  return 0;
}

// Semaphore post
int sys_sem_post(int sem_id) {
  struct task_struct *master = current()->master_thread;  

  // A process can only have MAX_SEMAPHORES semaphores
  // Check if the semaphore id is valid and if the semaphore is initialized
  if (sem_id < 0 || sem_id > MAX_SEMAPHORES || master->semaphores->sem[sem_id].TID == -1)
    return -EINVAL;

  // Increment the semaphore count
  master->semaphores->sem[sem_id].count += 1;

  // If the semaphore is already locked, wake up a thread
  if (master->semaphores->sem[sem_id].count >= 0) {
    struct list_head *l = NULL;

    // Check if the blocked list is empty
    if (list_empty(&(master->semaphores->sem[sem_id].blocked)))
      return -EAGAIN;

    // Get the first blocked thread and remove it from the blocked list
    l = list_first(&(master->semaphores->sem[sem_id].blocked));
    list_del(l);

    // Add the thread to the ready queue
    struct task_struct *tu = (struct task_struct*)list_head_to_task_struct(l);  // Unlocked thread
    tu->state = ST_READY;
    list_add_tail(&tu->list, &readyqueue);

    // update_process_state_rr(tu, &readyqueue);
  }

  return 0;
}

// Semaphore destroy
int sys_sem_destroy(int sem_id) {
  struct task_struct *master = current()->master_thread;

  // A process can only have MAX_SEMAPHORES semaphores
  // Check if the semaphore id is valid
  if (sem_id < 0 || sem_id >= master->next_sem_id)
    return -EAGAIN; 

  // A process can only destroy its own semaphores
  // Check if the semaphore is not the current process's semaphore
  if (master->semaphores->sem[sem_id].TID != current()->TID)
    return -EAGAIN;

  // Free the semaphore
  master->semaphores->sem[sem_id].TID = -1;

  // Wake up and notify all the threads blocked on the semaphore
  if (!list_empty(&(master->semaphores->sem[sem_id].blocked))) {
    struct list_head *pos, *tmp;
    list_for_each_safe(pos, tmp, &(master->semaphores->sem[sem_id].blocked)) {
      // Get the first blocked thread and remove it from the blocked list
      struct task_struct *tu = list_head_to_task_struct(pos);
      list_del(pos);

      // Add the thread to the ready queue
      tu->state = ST_READY;
      list_add_tail(&tu->list, &readyqueue);
      // update_process_state_rr(tu, &readyqueue);
    }
  }
  // Free the semaphore (mark it as unused)
  master->semaphores->sem[sem_id].TID = -1;    
  master->semaphores->sem[sem_id].count = -1;
  master->next_sem_id--;

  return 0;
}