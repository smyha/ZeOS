/*
 * libc.c 
 */

#include <libc.h>

#include <types.h>

// Errno.h
#include "errno.h"

int errno;

void itoa(int a, char *b)
{
  int i, i1;
  char c;
  
  if (a==0) { b[0]='0'; b[1]=0; return ;}
  
  i=0;
  while (a>0)
  {
    b[i]=(a%10)+'0';
    a=a/10;
    i++;
  }
  
  for (i1=0; i1<i/2; i1++)
  {
    c=b[i1];
    b[i1]=b[i-i1-1];
    b[i-i1-1]=c;
  }
  b[i]=0;
}

int strlen(char *a)
{
  int i;
  
  i=0;
  
  while (a[i]!=0) i++;
  
  return i;
}


/**
 * @brief Prints the error message corresponding to the current errno value
 * 
 * Prints a human-readable error message to standard output (file descriptor 1)
 * based on the current value of the global errno variable. Handles specific
 * error codes with predefined messages:
 * - EACCES: Permission denied
 * - EFAULT: Bad address  
 * - EINVAL: Invalid argument
 * - EBADF: Bad file number
 * - ENOSYS: Function (Syscall) not implemented
 * For unrecognized error codes, prints "Unknown error: " followed by the errno value.
 * 
 * @note The function assumes errno contains a negative error code value
 * @note For unknown errors, uses dynamic memory to convert errno to string
 */
void perror(void){
  char error[3];
  
  switch (errno)
  {
    // EACCES -> Permission denied [13]
    case EACCES:  
      write(1, "Permission denied\n", 18);
      break;
    // EFAULT -> Bad address [14]
    case EFAULT:
      write(1, "Bad address\n", 12);
      break;
    // EINVAL -> Invalid argument [22]
    case EINVAL:
      write(1, "Invalid argument\n", 17);
      break;
    // EBADF -> Bad file number [9]
    case EBADF:
      write(1, "Bad file number\n", 16);
      break;
    // ENOSYS -> Function not implemented [38]
    case ENOSYS:
      write(1, "Function (Syscall) not implemented\n", 25);
      break;
    
    default:
      itoa(errno, error);
      write(1, "Unknown error: ", 15);
      write(1, error, strlen(error));
      break;
  }
}

