#include <libc.h>
#include <types.h>

char *buffer;

int pid;

int add(int p1,int p2){
	return p1+p2;
}

int addASM(int,int);

int gettime(); // SYSENTER
int gettime_int(); // INT 0x80

int test_gettime(){
  // Print a newline
  write(1, "\n", 1);

  int time1, time2;
  
  // Test 1: Check if gettime returns valid values
  time1 = gettime();
  
  // Print time 1
  write(1, "Time: ", 7);
  itoa(time1, buffer);
  write(1, buffer, strlen(buffer));
  write(1, "\n", 1);

  if (time1 < 0) 
    return -1;
  
  // Test 2: Check if time increases
  int i;
  for (i = 0; i < 1e9; i++); // Small delay
  time2 = gettime();

  // Print time 2
  write(1, "Time: ", 7);
  itoa(time2, buffer);
  write(1, buffer, strlen(buffer));

  if (time2 <= time1) 
    return -1;

  // Test 3: Check consecutive calls
  time1 = gettime();
  time2 = gettime();
  if (time2 < time1) 
    return -1;

  return 1;
}

int write(int fd, char *buffer, int size); // SYSENTER syscall
int write_int(int fd, char *buffer, int size); // INT 0x80 syscall

int test_write() {
  // Print a newline
  write(1, "\n", 1);

  // Strings to test
  char *test_buf, *picture;
  
  test_buf = "Test string\n";
  
  // Test writing to stdout
  if (write(1, test_buf, 13) != 13) perror();

  // Test writing a large picture (SMYHA nickname) buffer (485 bytes + \ns)
  picture = "                              __                   \n\
                             /\\ \\                  \n\
  ____    ___ ___     __  __ \\ \\ \\___       __     \n\
 /',__\\ /' __` __`\\  /\\ \\/\\ \\ \\ \\  _ `\\   /'__`\\   \n\
/\\__, `\\/\\ \\/\\ \\/\\ \\ \\ \\ \\_\\ \\ \\ \\ \\ \\ \\ /\\ \\L\\.\\_\n\
\\/\\____/\\ \\_\\ \\_\\ \\_\\ \\/`____ \\ \\ \\_\\ \\_\\\\ \\__/.\\_\\\n\
 \\/___/  \\/_/\\/_/\\/_/  `/___/> \\ \\/_/\\/_/ \\/__/\\/_/\n\
                          /\\___/                      \n\
                          \\/__/                       ";

  if (write(1, picture, strlen(picture)) != strlen(picture)) perror();
  
  // Test invalid fd
  if (write(-1, test_buf, 13) != -1) perror();
  
  // Test null buffer
  if (write(1, NULL, 5) != -1) perror();
  
  // Test zero length
  if (write(1, test_buf, 0) != 0) perror();
  
  return 1;
}

int getpid(); // SYSENTER
int getpid_int(); // INT 0x80

int test_getpid() {
  // Print a newline
  write(1, "\n", 1);

  // Test getpid
  buffer = "Testing getpid: PID = ";
  pid = getpid();
  
  // Print PID (should be 1)
  write(1, buffer, strlen(buffer));
  itoa(pid, buffer);
  write(1, buffer, strlen(buffer));
  write(1,  "\n", 1);

  return 1;
}

int fork(); // SYSENTER
int fork_int(); // INT 0x80

void block(); // SYSENTER
void block_int(); // INT 0x80

int unblock(int pid); // SYSENTER
int unblock_int(int pid); // INT 0x80

int test_fork_and_blocks() {
  // Print a newline
  write(1, "\n", 1);

  int pid_test = fork();
  if (pid_test == 0) 
  {
    int pid_test2 = fork();
    if (pid_test2 > 0)
    {
      //PROCESS 2 -> unblock test
      buffer = "\nIm the PROCESS: ";  // PROCESS 1
      if (write(1,buffer,17) == -1) {perror(); exit();}
      itoa(getpid(),buffer);
      if (write(1,buffer,2) ==-1) {perror(); exit();}

      while(1)
      {
        if (unblock(pid_test2) == -1) {perror(); exit();}
      }
    }
    else if (pid_test2 == 0)
    {
      //PROCESS pid_test2 -> block test
      buffer = "\nIm the PROCESS: ";    // PROCESS 2
      if (write(1,buffer,17) == -1) {perror(); exit();}
      itoa(getpid(),buffer);
      if (write(1,buffer,2) == -1) {perror(); exit();}
      buffer = " im going to block!\n";
      if (write(1,buffer,21) == -1) {perror(); exit();}
      block();
      buffer = "Unlocked! \n";
      if (write(1,buffer,12) == -1) {perror(); exit();}
      
      //EXIT -> should not write 
      exit(); 
      buffer = "\nI should not be here\n";  
      if (write(1,buffer,21) == -1) {perror(); exit();}
    }
    else perror();
  }
  else if (pid_test > 0)
  {
    //PROCESS 1 -> Task1
    buffer = "\nIm the PROCESS: ";
    if (write(1,buffer,17) == -1) {perror(); exit();}
    itoa(getpid(),buffer);
    if (write(1,buffer,2) == -1) {perror(); exit();}
    write(1, "\n", 1);
  }
  else {perror(); exit();}

  return 1;
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
  /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
  /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  int tests_passed; 

  // Run tests
  tests_passed = 0;

  write(1, "\n", 1);

  if (write(1, "AAA\n", -1)) {
	// write(1, "error\n", 6);
  	perror();
	  tests_passed++;
  }

  buffer = "Let's test some system calls!\n";
  
  if (test_write()) tests_passed++;
  if (test_gettime()) tests_passed++;
  if (test_getpid()) tests_passed++;
  if (test_fork_and_blocks()) tests_passed++;
  
  // Convert number of tests passed to string
  itoa(tests_passed, buffer);
  
  // Print results
  write(1, "Tests passed: ", 15);
  write(1, buffer, strlen(buffer));
  write(1, "\n", 1);

  while(1) { }
}
