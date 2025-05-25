/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <types.h>
#include <hardware.h>
#include <segment.h>
#include <sched.h>
#include <mm.h>
#include <io.h>
#include <utils.h>
#include <p_stats.h>

/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

extern struct list_head blocked;

// Free task structs
struct list_head freequeue;
// Ready queue
struct list_head readyqueue;

void init_stats(struct stats *s)
{
	s->user_ticks = 0;
	s->system_ticks = 0;
	s->blocked_ticks = 0;
	s->ready_ticks = 0;
	s->elapsed_total_ticks = get_ticks();
	s->total_trans = 0;
	s->remaining_ticks = get_ticks();
}

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
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

int remaining_quantum=0;

int get_quantum(struct task_struct *t)
{
  return t->total_quantum;
}

void set_quantum(struct task_struct *t, int new_quantum)
{
  t->total_quantum=new_quantum;
}

struct task_struct *idle_task=NULL;

/**
 * @brief Inserts a task into the ready queue based on priority
 * 
 * This function implements a priority-based insertion into the ready queue.
 * Tasks with higher priority are placed at the beginning of the queue.
 * The function ensures:
 * 1. Thread priority inheritance from master thread
 * 2. Proper state transitions
 * 3. Accurate statistics tracking
 * 
 * @param t Pointer to the task structure to be inserted
 */
void insert_ready_ordered(struct task_struct *t) {
    struct list_head *pos;
    struct task_struct *current_pos;
    
    // Validate task pointer
    if (!t) 
      return;
    
    // Ensure thread priority matches master thread
    if (t->master_thread != t) {
        t->priority = t->master_thread->priority;
    }
    
    // Fast path: empty queue
    if (list_empty(&readyqueue)) {
        list_add_tail(&t->list, &readyqueue);
        t->state = ST_READY;
        update_stats(&t->p_stats.system_ticks, &t->p_stats.elapsed_total_ticks);
        return;
    }
    
    // Find insertion point based on priority
    list_for_each(pos, &readyqueue) {
        current_pos = list_head_to_task_struct(pos);
        
        // Insert before first task with lower priority
        if (t->priority > current_pos->priority) {
            list_add(&t->list, pos);
            t->state = ST_READY;
            update_stats(&t->p_stats.system_ticks, &t->p_stats.elapsed_total_ticks);
            
            // ! If the inserted task has higher priority than current, force reschedule
            if (t->priority > current()->priority) {
                force_task_switch();
            }
            return;
        }
    }
    
    // If we get here, add at the end (lowest priority)
    list_add_tail(&t->list, &readyqueue);
    t->state = ST_READY;
    update_stats(&t->p_stats.system_ticks, &t->p_stats.elapsed_total_ticks);
}

void update_sched_data_rr(void)
{
  remaining_quantum--;
}

int needs_sched_rr(void)
{
  // Check if current quantum is over
  if (remaining_quantum==0) {
    if (!list_empty(&readyqueue)) return 1;
    remaining_quantum=get_quantum(current());
    return 0;
  }

  // Check if there's a higher priority thread in ready queue
  if (!list_empty(&readyqueue)) {
    struct task_struct *next_task = list_head_to_task_struct(list_first(&readyqueue));
    if (next_task->priority > current()->priority) {
      return 1;
    }
  }

  return 0;
}
void update_process_state_rr(struct task_struct *t, struct list_head *dst_queue)
{
  // Remove from current queue if not running
  if (t->state != ST_RUN) {
    list_del(&t->list);
  }
  
  if (dst_queue != NULL) {
    if (dst_queue == &readyqueue) {
      // Insert into ready queue based on priority
      insert_ready_ordered(t);
    }
    else {
      // Blocked queue
      list_add_tail(&t->list, dst_queue);
      t->state = ST_BLOCKED;
    }
  }
  else {
    // Running state
    t->state = ST_RUN;
  }
}

void sched_next_rr(void)
{
  struct list_head *e;
  struct task_struct *t;

  if (!list_empty(&readyqueue)) {
    // Get the highest priority task (first in queue)
    e = list_first(&readyqueue);
    list_del(e);
    t = list_head_to_task_struct(e);
  }
  else {
    t = idle_task;
  }

  // Update current task stats before switching
  update_stats(&current()->p_stats.system_ticks, &current()->p_stats.elapsed_total_ticks);

  // Set new task as running
  t->state = ST_RUN;
  remaining_quantum = get_quantum(t);

  // Update new task stats
  update_stats(&t->p_stats.ready_ticks, &t->p_stats.elapsed_total_ticks);
  t->p_stats.total_trans++;

  task_switch((union task_union*)t);
}

void schedule()
{
  update_sched_data_rr();
  if (needs_sched_rr())
  {
    update_process_state_rr(current(), &readyqueue);
    sched_next_rr();
  }
}

void init_idle (void)
{
  struct list_head *l = list_first(&freequeue);
  list_del(l);
  struct task_struct *c = list_head_to_task_struct(l);
  union task_union *uc = (union task_union*)c;

  c->PID=0;

  c->total_quantum=DEFAULT_QUANTUM;

  init_stats(&c->p_stats);

  c->screen_page = (void*)-1; // No screen page
  c->priority = DEFAULT_PRIORITY;
  c->TID = 1;
  c->master_thread = c;
  
  INIT_LIST_HEAD(&(c->threads));
  INIT_LIST_HEAD(&(c->threads_list));

  allocate_DIR(c);

  uc->stack[KERNEL_STACK_SIZE-1]=(unsigned long)&cpu_idle; /* Return address */
  uc->stack[KERNEL_STACK_SIZE-2]=0; /* register ebp */

  c->register_esp=(int)&(uc->stack[KERNEL_STACK_SIZE-2]); /* top of the stack */

  idle_task=c;
}

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void init_task1(void)
{
  struct list_head *l = list_first(&freequeue);
  list_del(l);
  struct task_struct *c = list_head_to_task_struct(l);
  union task_union *uc = (union task_union*)c;

  c->PID=1;

  c->total_quantum=DEFAULT_QUANTUM;

  c->state=ST_RUN;

  c->pause_time = 0; // No pause time
  c->screen_page = (void*)-1; // No screen page
  c->priority = DEFAULT_PRIORITY;
  c->TID = 1;
  c->master_thread = c;
  c->next_sem_id = 0;
  c->user_stack_ptr = NULL;
  c->thread_count = 1;

  INIT_LIST_HEAD(&(c->threads));
  INIT_LIST_HEAD(&(c->threads_list));

  remaining_quantum=c->total_quantum;

  init_stats(&c->p_stats);

  allocate_DIR(c);

  set_user_pages(c);

  tss.esp0=(DWord)&(uc->stack[KERNEL_STACK_SIZE]);
  setMSR(0x175, 0, (unsigned long)&(uc->stack[KERNEL_STACK_SIZE]));

  set_cr3(c->dir_pages_baseAddr);

  // ! Initialize the semaphore array
  c->semaphores = &(semaphores[0]); // First semaphore in the array
  c->semaphores->owner = c->TID;
  
  // ! Initialize the semaphore structure
  init_sem_array();

}

void init_sem_array() {
  for (int i = 0; i < NR_TASKS; i++) {
    semaphores[i].owner = -1;
    for (int j = 0; j < MAX_SEMAPHORES; j++) {
      semaphores[i].sem[j].count = -1;
      semaphores[i].sem[j].TID = -1;
      INIT_LIST_HEAD(&(semaphores[i].sem[j].blocked));
    }
  }
}

void init_freequeue()
{
  int i;

  INIT_LIST_HEAD(&freequeue);

  /* Insert all task structs in the freequeue */
  for (i=0; i<NR_TASKS; i++)
  {
    task[i].task.PID=-1;
    list_add_tail(&(task[i].task.list), &freequeue);
  }
}

extern char keyboard_buffer[128];
void init_sched()
{
  init_freequeue(); 
  INIT_LIST_HEAD(&readyqueue);
  INIT_LIST_HEAD(&blocked);

  // ! Initialize the keyboard buffer
  for (int i = 0; i < 128; i++) 
    keyboard_buffer[i] = 0;

  // ! Initialize the semaphore array
  init_sem_array();
}

struct task_struct* current()
{
  int ret_value;
  
  return (struct task_struct*)( ((unsigned int)&ret_value) & 0xfffff000);
}

struct task_struct* list_head_to_task_struct(struct list_head *l)
{
  return (struct task_struct*)((int)l&0xfffff000);
}

/* Do the magic of a task switch */
void inner_task_switch(union task_union *new)
{
  page_table_entry *new_DIR = get_DIR(&new->task);

  /* Update TSS and MSR to make it point to the new stack */
  tss.esp0=(int)&(new->stack[KERNEL_STACK_SIZE]);
  setMSR(0x175, 0, (unsigned long)&(new->stack[KERNEL_STACK_SIZE]));

  /* TLB flush. New address space */
  set_cr3(new_DIR);

  switch_stack(&current()->register_esp, new->task.register_esp);
}


/* Force a task switch assuming that the scheduler does not work with priorities */
void force_task_switch()
{
  update_process_state_rr(current(), &readyqueue);

  sched_next_rr();
}

