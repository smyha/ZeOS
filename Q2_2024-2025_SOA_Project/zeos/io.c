/*
 * io.c - 
 */

#include <io.h>

#include <types.h>

/**************/
/** Screen  ***/
/**************/

#define NUM_COLUMNS 80
#define NUM_ROWS    25

Byte x, y=19;

/* Read a byte from 'port' */
Byte inb (unsigned short port)
{
  Byte v;

  __asm__ __volatile__ ("inb %w1,%0":"=a" (v):"Nd" (port));
  return v;
}

void printc(char c)
{
     __asm__ __volatile__ ( "movb %0, %%al; outb $0xe9" ::"a"(c)); /* Magic BOCHS debug: writes 'c' to port 0xe9 */
  if (c=='\n')
  {
    x = 0;
    y=(y+1)%NUM_ROWS;
  }
  else
  {
    Word ch = (Word) (c & 0x00FF) | 0x0200;
	Word *screen = (Word *)0xb8000;
	screen[(y * NUM_COLUMNS + x)] = ch;
    if (++x >= NUM_COLUMNS)
    {
      x = 0;
      y=(y+1)%NUM_ROWS;
    }
  }
}

void printc_xy(Byte mx, Byte my, char c)
{
  Byte cx, cy;
  cx=x;
  cy=y;
  x=mx;
  y=my;
  printc(c);
  x=cx;
  y=cy;
}

void printk(char *string)
{
  int i;
  for (i = 0; string[i]; i++)
    printc(string[i]);
}

// Auxiliary functions
void print_string_xy(int x, int y, const char *s) {
  int i = 0;
  while (s[i] != '\0') {
    printc_xy(x + i, y, s[i]);
    i++;
  }
}

void print_number(int num) {
    char buffer[12];  // Enough for INT_MIN
    int i = 0;
    int is_negative = 0;
    
    // Handle negative numbers
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    // Handle zero
    if (num == 0) {
        printc('0');
        return;
    }
    
    // Convert number to string in reverse
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num = num / 10;
    }
    
    // Add negative sign if needed
    if (is_negative) {
        buffer[i++] = '-';
    }
    
    // Print in reverse order
    while (i > 0) {
        printc(buffer[--i]);
    }
}

// Function to print a number to the screen
void print_number_to_screen(int num, unsigned short *screen, int pos) {
    char buffer[12];
    int i = 0;
    int is_negative = 0;
    
    // Handle negative numbers
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    // Handle zero
    if (num == 0) {
        screen[pos] = 0x0F00 | '0';
        return;
    }
    
    // Convert number to string in reverse
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num = num / 10;
    }
    
    // Add negative sign if needed
    if (is_negative) {
        buffer[i++] = '-';
    }
    
    // Write to screen in reverse order
    while (i > 0) {
        screen[pos + (i-1)] = 0x0F00 | buffer[--i];
    }
}