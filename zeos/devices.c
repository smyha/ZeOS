#include <io.h>
#include <utils.h>
#include <list.h>

// Queue for blocked processes in I/O 
struct list_head blocked;

int sys_write_console(char *buffer,int size)
{
  int i;
  
  for (i=0; i<size; i++)
    printc(buffer[i]);
    // To test printc_color uncomment the following line
    // printc_color(buffer[i], 0xF);
  
  return size;
}
