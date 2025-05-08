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

#define LECTURA 0
#define ESCRIPTURA 1
#define MAX_PRIORITY 100
#define MAX_STACK_SIZE 1024

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

int ret_from_fork()
{
  return 0;
}

int sys_fork(void)
{
  struct list_head *lhcurrent = NULL;
  union task_union *uchild;
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  uchild=(union task_union*)list_head_to_task_struct(lhcurrent);
  
  /* Copy the parent's task struct to child's */
  copy_data(current(), uchild, sizeof(union task_union));
  
  /* new pages dir */
  allocate_DIR((struct task_struct*)uchild);
  
  /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(&uchild->task);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      for (i=0; i<pag; i++)
      {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
      }
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freequeue);
      
      /* Return error */
      return -EAGAIN; 
    }
  }

  /* Copy parent's SYSTEM and CODE to child. */
  page_table_entry *parent_PT = get_PT(current());
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
  for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE+1; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++)  // ! +1 to not override the screen page
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+NUM_PAG_DATA, get_frame(process_PT, pag));
    copy_data((void*)(pag<<12), (void*)((pag+NUM_PAG_DATA)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+NUM_PAG_DATA);
  }
  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));

  uchild->task.PID=++global_PID;
  uchild->task.state=ST_READY;

  // Initialize thread-related fields
  uchild->task.priority=current()->priority; // Inherits the parent's priority
  uchild->task.TID = 1;
  uchild->task.thread_count = 1;
  uchild->task.main_thread = &uchild->task;
  uchild->task.thread_func = NULL;  // Main thread doesn't have a function
  uchild->task.thread_param = NULL;

  // Initialize lists
  INIT_LIST_HEAD(&(uchild->task.my_threads));
  INIT_LIST_HEAD(&(uchild->task.children));
  INIT_LIST_HEAD(&(uchild->task.sibling));

  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)current()) + (int)(uchild);

  uchild->task.register_esp=register_ebp + sizeof(DWord);

  DWord temp_ebp=*(DWord*)register_ebp;
  /* Prepare child stack for context switch */
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=(DWord)&ret_from_fork;
  uchild->task.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->task.register_esp)=temp_ebp;

  /* Set stats to 0 */
  init_stats(&(uchild->task.p_stats));

  // ! -------------------- ADDED --------------------
  INIT_LIST_HEAD(&(uchild->task.my_threads));
  INIT_LIST_HEAD(&(uchild->task.children));
  // ! ------------------------------------------------
  /* Queue child process into readyqueue */
  uchild->task.state=ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);

  // ! -------------------- ADDED --------------------
  uchild->task.pause_time = 0;  // No pause time

  // Share screen page
  int frame_screen_page = get_frame(parent_PT, (int)current()->screen_page >> 12);
  set_ss_pag(process_PT, PAG_LOG_INIT_DATA+ 2*NUM_PAG_DATA, frame_screen_page);
  uchild->task.screen_page = (void*)((PAG_LOG_INIT_DATA+2*NUM_PAG_DATA) << 12);
  
  // ! ------------------------------------------------

  return uchild->task.PID;
}

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

void sys_exit()
{  
  int i;

  page_table_entry *process_PT = get_PT(current());

  // Deallocate all the propietary physical pages
  for (i=0; i<NUM_PAG_DATA; i++)
  {
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
  }
  
  /* Free task_struct */
  list_add_tail(&(current()->list), &freequeue);
  
  current()->PID=-1;
  
  /* Restarts execution of the next process */
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
  if (copy_from_user(keyboard, keyboard_buffer, 128) < 0)
    return -EFAULT;

  // Clear the keyboard buffer
  for (int i = 0; i < 128; i++) {
    keyboard_buffer[i] = 0;
  }
  
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
  
  // Check if process already has a screen page,
  // if so, return it
  if (t->screen_page != (void*)-1) {
    return t->screen_page;
  }
  
  // Allocate a new physical page
  int phy_page = alloc_frame();
  if (phy_page < 0)
    return (void*)-EAGAIN;
  
  // Map the page to user space
  page_table_entry *process_PT = get_PT(t);
  set_ss_pag(process_PT, PAG_LOG_INIT_DATA + 2*NUM_PAG_DATA, phy_page);
  
  // Store the page address of the screen in task struct
  t->screen_page = (void*)((PAG_LOG_INIT_DATA + 2*NUM_PAG_DATA) << 12);
  // t->screen_page = (void*)((PAG_LOG_INIT_DATA + NUM_PAG_DATA) * PAGE_SIZE);
  
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
 */

// Thread support
int sys_clone(int what, void *(*func)(void*), void *param, int stack_size) {
  // Check if the parameters are valid
  if (what != CLONE_PROCESS && what != CLONE_THREAD) 
    return -EINVAL;

  // If it's a process, fork
  if (what == CLONE_PROCESS) {
    return sys_fork();
  }

  // If it's a thread, create a new thread
  if (what == CLONE_THREAD) {
    if (!func) 
      return -EINVAL;

    if (stack_size <= 0 || stack_size > MAX_STACK_SIZE) 
      return -EINVAL;

    // Check if the code pointer is accessible
    if (!access_ok(VERIFY_READ, func, sizeof(void (*)(void*))))
      return -EFAULT;
    if (param && !access_ok(VERIFY_READ, param, sizeof(void*)))
      return -EFAULT;
  }

  // If there are no free task structs, return an error
  if (list_empty(&freequeue)) 
    return -ENOMEM;

  // Get the first free task struct
  struct list_head *lhcurrent = list_first(&freequeue);
  list_del(lhcurrent);

  // Allocate a new physical page for the stack
  int new_pag = alloc_frame();
  if (new_pag == -1) {
    list_add_tail(lhcurrent, &freequeue);
    return -EAGAIN;
  }

  // Copy the parent's task struct to the new thread
  struct task_struct *parent = current();
  union task_union *new_thread = (union task_union*)list_head_to_task_struct(lhcurrent);
  copy_data(parent, new_thread, sizeof(union task_union));

  // Get the main thread (could be the current thread or its main thread)
  struct task_struct *main_thread = parent->main_thread;

  // Initialize the new thread
  new_thread->task.state = ST_READY;
  new_thread->task.TID = ++main_thread->thread_count;
  new_thread->task.pause_time = 0;
  new_thread->task.priority = parent->priority;  // Inherit parent's priority
  new_thread->task.main_thread = main_thread;
  new_thread->task.screen_page = parent->screen_page; // Share screen page
  new_thread->task.thread_func = func;  // Save the thread function
  new_thread->task.thread_param = param;  // Save the thread parameter
  
  // Initialize thread lists
  INIT_LIST_HEAD(&(new_thread->task.children));
  INIT_LIST_HEAD(&(new_thread->task.sibling));
  INIT_LIST_HEAD(&(new_thread->task.my_threads));

  // Add to main thread's thread list
  list_add_tail(&(new_thread->task.sibling), &(main_thread->my_threads));
  
  // Add to parent's my_threads list
  list_add_tail(&(new_thread->task.list), &(parent->my_threads));

  // Set up page directory and table
  new_thread->task.dir_pages_baseAddr = parent->dir_pages_baseAddr;
  page_table_entry *process_PT = get_PT(&new_thread->task);

  // User stack setup
  int us_st_pt_entry = PAG_LOG_INIT_DATA + NUM_PAG_DATA*2 + new_thread->task.TID;
  set_ss_pag(process_PT, us_st_pt_entry, new_pag);

  // Calculate stack top and prepare user stack
  unsigned int stack_top = (us_st_pt_entry << 12) + stack_size;
  unsigned int* user_esp = (unsigned int *)stack_top;
  
  // Prepare the user stack
  user_esp -= sizeof(Byte);
  *(user_esp) = (unsigned int)param;
  user_esp -= sizeof(Byte);
  *(user_esp) = 0;  // Return address (never used)

  // Set up hardware context
  ((unsigned long *) KERNEL_ESP(new_thread))[-0x02] = (unsigned long) user_esp;  // esp
  ((unsigned long *) KERNEL_ESP(new_thread))[-0x05] = (unsigned long) func;      // eip 

  // Set the register esp to the stack top
  new_thread->task.register_esp = (unsigned long) &(new_thread->stack[KERNEL_STACK_SIZE - 0x12]); 

  // Add to ready queue
  list_add_tail(&new_thread->task.list, &readyqueue);
  
  return new_thread->task.PID;
}

// Set priority
int sys_SetPriority(int priority) {
  if (priority < 0 || priority > MAX_PRIORITY) {
    return -EINVAL;
  }
  
  current()->priority = priority;
  return 0;
}

// Pthread exit
int sys_pthread_exit()
{
	struct task_struct *ts = current();
	union task_union *tu = (union task_union*) ts;
	
  // If it's the main thread and there are still threads available, it cannot be deleted
  if (ts->main_thread == ts && !list_empty(&ts->my_threads)) {
		return -1;
	}

  // Delete the thread from its parent's children list
	list_del(&ts->children);
	
  // Decrease the thread count
  // ts->thread_count--;

  // Free the thread's stack
  unsigned long user_esp = tu->stack[KERNEL_STACK_SIZE - 1];  // reg esp
	unsigned int user_stack_page = user_esp >> 12;
	
  page_table_entry *pt = get_PT(ts);
	free_frame(get_frame(pt, user_stack_page));	// only frees 1 page :(
	del_ss_pag(pt, user_stack_page);

	ts->PID=-1;
	ts->TID=-1;
	
  update_process_state_rr(ts, &freequeue);
	sched_next_rr();

	return 0;
}

