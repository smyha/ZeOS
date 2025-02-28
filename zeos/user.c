#include <libc.h>
#include <types.h>

char *buffer;

int pid;

int add(int p1,int p2){
	return p1+p2;
}

int addASM(int,int);


int write(int fd, char *buffer, int size); // SYSENTER syscall
int write_int(int fd, char *buffer, int size); // INT 0x80 syscall


int test_write() {
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


int __attribute__ ((__section__(".text.main")))
  main(void)
{
  /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
  /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  int tests_passed;

  // Run tests
  tests_passed = 0;

  write(1, "\n", 1);
  buffer = "Let's test some system calls!\n";
  
  if (test_write()) tests_passed++;

  // Convert number of tests passed to string
  itoa(tests_passed, buffer);
  
  // Print results
  write(1, "\nTests passed: ", 16);
  write(1, buffer, strlen(buffer));

  while(1) { }
}