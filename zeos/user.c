#include <libc.h>

char *buff;

int pid;

int add(int p1,int p2){
	return p1+p2;
}

int addASM(int,int);

int gettime(); // SYSENTER
int gettime_int(); // INT 0x80

int test_gettime(){
  int time1, time2;
  
  // Test 1: Check if gettime returns valid values
  time1 = gettime();
  if (time1 < 0) 
    return -1;
  

  // Test 2: Check if time increases
  int i;
  for (i = 0; i < 1000000; i++); // Small delay
  time2 = gettime();
  if (time2 <= time1) 
    return -1;

  // Test 3: Check consecutive calls
  time1 = gettime();
  time2 = gettime();
  if (time2 < time1) 
    return -1;

  return 1;
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */


    while(1) { }
}