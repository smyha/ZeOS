/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#define LECTURA 0
#define ESCRIPTURA 1

/* Referenced in interrupt.c */
extern int zeos_ticks;

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

int sys_getpid()
{
	return current()->PID;
}

int sys_fork()
{
  int PID=-1;

  // creates the child process
  
  return PID;
}

void sys_exit()
{  
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
void sys_gettime(){
  return zeos_ticks;
}