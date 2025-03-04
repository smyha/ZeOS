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

  /**************/
  /** Colors  ***/
  /**************/

  #define BLACK     0x0
  #define BLUE      0x1
  #define GREEN     0x2
  #define CYAN      0x3
  #define RED       0x4
  #define MAGENTA   0x5
  #define BROWN     0x6
  #define LGRAY     0x7
  #define DGRAY     0x8
  #define LBLUE     0x9
  #define LGREEN    0xA
  #define LCYAN     0xB
  #define LRED      0xC
  #define LMAGENTA  0xD
  #define YELLOW    0xE
  #define WHITE     0xF
 
 Byte x, y=19;
 
 /* Read a byte from 'port' */
 Byte inb (unsigned short port)
 {
   Byte v;
 
   __asm__ __volatile__ ("inb %w1,%0":"=a" (v):"Nd" (port));
   return v;
 }

 /** 
  * Internal implementation function to print characters
  * It is not visible from outside the file (static)
 */
static void _printc_impl(char c, Byte color) {
  __asm__ __volatile__ ( "movb %0, %%al; outb $0xe9" ::"a"(c)); 
  
  if (c=='\n') {
      x = 0;
      y++;
      if (y >= NUM_ROWS) {
          scroll_screen();
          y = NUM_ROWS-1;
      }
  }
  else {
      Word ch = (Word)(c & 0x00FF) | (color << 8);
      Word *screen = (Word *)0xb8000;
      screen[(y * NUM_COLUMNS + x)] = ch;
      
      if (++x >= NUM_COLUMNS) {
          x = 0;
          y++;
          if (y >= NUM_ROWS) {
              scroll_screen();
              y = NUM_ROWS-1;
          }
      }
  }
}
 
/* Scroll screen one line up */
void scroll_screen() {
  Word *screen = (Word *)0xb8000;
  
  // Copy each line to previous one
  for(int i = 0; i < NUM_ROWS-1; i++) {
      for(int j = 0; j < NUM_COLUMNS; j++) {
          screen[i * NUM_COLUMNS + j] = screen[(i+1) * NUM_COLUMNS + j];
      }
  }
  
  // Clear last line
  for(int j = 0; j < NUM_COLUMNS; j++) {
      screen[(NUM_ROWS-1) * NUM_COLUMNS + j] = (Word)(' ' | 0x0200);
  }
}

/* Print with color at System Level */
void printc_color(char c, Byte color) {
  _printc_impl(c, color);
}

/* Standard print function with default color */
void printc(char c) {
  _printc_impl(c, GREEN); 
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
 