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

// Buffer System
#define BUFFER_SIZE 256
char sys_buffer[BUFFER_SIZE];

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
void sys_gettime(){
  return zeos_ticks;
}
