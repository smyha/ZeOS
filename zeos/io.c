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
 
 // Print at System Level
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
 
 
 /**
  * @brief Prints a character at position (x, y) on the screen
  * /param mx x position
  * /param my y position
  * /param c character to print
  */
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
 