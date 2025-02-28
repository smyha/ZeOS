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
 * For unrecognized error codes, prints "Unknown error: " followed by the errno value.
 * 
 * @note The function assumes errno contains a negative error code value
 * @note For unknown errors, uses dynamic memory to convert errno to string
 */
void perror(void){
  char *error;
  switch (errno)
  {
  case -EACCES:
    write(1, "Permission denied\n", 19);
    break;
  
  case -EFAULT:
    write(1, "Bad address\n", 1);
    break;
  
  case -EINVAL:
    write(1, "Invalid argument\n", 18);
    break;
  
  case -EBADF:
    write(1, "Bad file number\n", 17);
    break;
  
  default:
    itoa(errno, error);
    write(1, "Unknown error: ", 16);
    write(1, error, strlen(error));
    break;
  }
}

