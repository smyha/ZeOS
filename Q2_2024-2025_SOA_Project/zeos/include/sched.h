/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>
#include <p_stats.h>

#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024

#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

#define DEFAULT_QUANTUM 10
#define DEFAULT_PRIORITY 20
#define DEFAULT_STACK_SIZE 1024

#define MAX_SEMAPHORES 20


// ! ----------------- SEMAPHORES -----------------

/**
 * @brief Semaphore structure for thread synchronization
 * 
 * This structure implements a counting semaphore that can be used for
 * thread synchronization and mutual exclusion. It supports:
 * - Multiple threads waiting on the same semaphore
 * - Counting semaphore functionality
 * - Thread ownership tracking
 */
struct sem_t {
    int count;                  /* Current semaphore value */
    int TID;                    /* Thread ID of the owner */
    struct list_head blocked;   /* Queue of blocked threads */
    // int sem_id;              /* Semaphore ID */ 
};

/**
 * @brief Semaphore array structure
 * 
 * This structure holds an array of semaphores and their owner information
 */
struct sem_array {
    int owner;                              /* Owner thread ID */
    struct sem_t sem[MAX_SEMAPHORES]; /* Array of semaphores */
};

extern struct sem_array semaphores[NR_TASKS];

// ! ----------------- TASK STRUCT -----------------

struct task_struct {
  int PID;			/* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr;  /* Page directory for this task */
  struct list_head list;	/* Task struct enqueuing */
  int register_esp;		/* position in the stack */
  enum state_t state;		/* State of the process */
  int total_quantum;		/* Total quantum of the process */
  struct stats p_stats;		/* Process stats */
  
  /*  ---------------- THREAD SUPPORT ---------------- */  
  void *screen_page;     /* Screen page for video output */
  int pause_time;        /* Time to pause in milliseconds */
  int priority;          /* Priority of the process/thread */
  
  int TID;              /* Thread ID */
  int thread_count;      /* Number of threads in the proces */

  struct task_struct *master_thread; /* Pointer to the main thread (master) of the process */
  struct list_head threads;          /* List of threads created by this process */
  struct list_head threads_list;     /* Link to parent thread */

  /* ---------------- MEMORY MANAGEMENT ---------------- */
  int *user_stack_ptr; /* Pointer to the user stack */
  int user_stack_frames; /* Number of pages allocated for user stack */

  /* ---------------- SYNCHRONIZATION ---------------- */
  struct sem_array *semaphores; /* Pointer to semaphore array */
  int next_sem_id; /* Counter for semaphore ids */  
};

// ! ----------------- TASK UNION -----------------

union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE]; /* system stack, for process */
};

extern union task_union protected_tasks[NR_TASKS+2];
extern union task_union *task; /* Vector of tasks */
extern struct task_struct *idle_task;


int get_quantum(struct task_struct *t);
void set_quantum(struct task_struct *t, int new_quantum);

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

extern struct list_head freequeue;
extern struct list_head readyqueue;

// ! ----------------- INITIALIZATION -----------------

/* Initialize the data of the initial process */
void init_task1(void);

/* Initialize the idle process */
void init_idle(void);

/* Initialize the scheduler */
void init_sched(void);

/* Initialize the semaphore array */
void init_sem_array(void);

// ! ----------------- SCHEDULING -----------------

void schedule(void);

struct task_struct * current();

void task_switch(union task_union*t);
void switch_stack(int * save_sp, int new_sp);

void sched_next_rr(void);

void force_task_switch(void);

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */

// ! Insertion into ready queue
void insert_ready_ordered(struct task_struct *t);
  
// ! Round Robin scheduling
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

void init_stats(struct stats *s);

#endif  /* __SCHED_H__ */
