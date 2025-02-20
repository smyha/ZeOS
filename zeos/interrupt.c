/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>

#include <zeos_interrupt.h>

/**
 * @brief IDT table
 *  - IDT_ENTRIES: Number of entries in the IDT table
 * - Gate: Structure that represents an entry in the IDT table
 * 
 * The IDT table is an array of Gate structures that represent the different
 * interrupt/exception vectors. Each entry in the table contains the following
 * fields:
 * - lowOffset: Lower 16 bits of the handler function
 * - segmentSelector: Segment selector
 * - flags: Flags that define the type of interrupt/exception
 * - highOffset: Higher 16 bits of the handler function
 * 
 *  [0 - 31] ->  Exceptions
 *  [32 - 47] -> Hardware interrupts
 *  [48 - 255] -> System calls
 */
Gate idt[IDT_ENTRIES];
Register    idtR;

/* #ticks since the system started */
unsigned int zeos_ticks = 0;

char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','�','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','�',
  '\0','�','\0','�','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

void setInterruptHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE INTERRUPTION GATE FLAGS:                          R1: pg. 5-11  */
  /* ***************************                                         */
  /* flags = x xx 0x110 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

void setTrapHandler(int vector, void (*handler)(), int maxAccessibleFromPL)
{
  /***********************************************************************/
  /* THE TRAP GATE FLAGS:                                  R1: pg. 5-11  */
  /* ********************                                                */
  /* flags = x xx 0x111 000 ?????                                        */
  /*         |  |  |                                                     */
  /*         |  |   \ D = Size of gate: 1 = 32 bits; 0 = 16 bits         */
  /*         |   \ DPL = Num. higher PL from which it is accessible      */
  /*          \ P = Segment Present bit                                  */
  /***********************************************************************/
  Word flags = (Word)(maxAccessibleFromPL << 13);

  //flags |= 0x8F00;    /* P = 1, D = 1, Type = 1111 (Trap Gate) */
  /* Changed to 0x8e00 to convert it to an 'interrupt gate' and so
     the system calls will be thread-safe. */
  flags |= 0x8E00;    /* P = 1, D = 1, Type = 1110 (Interrupt Gate) */

  idt[vector].lowOffset       = lowWord((DWord)handler);
  idt[vector].segmentSelector = __KERNEL_CS;
  idt[vector].flags           = flags;
  idt[vector].highOffset      = highWord((DWord)handler);
}

/**
 * @brief Handles keyboard interrupts and processes keyboard input
 * 
 * This routine reads a byte from keyboard port 0x60 which contains the scancode
 * of the pressed/released key. 
 * 
 * NOTE: The scancode is an 8-bit value that contains the key code and the key state.
 * 
 * The scancode consists of:
 * - Bit 7: Key state (0 = key press/make, 1 = key release/break)
 * - Bits 0-6: Scan code value
 * 
 * The routine processes the scancode by:
 * 1. Reading the port value
 * 2. Extracting the scan code (bits 0-6)
 * 3. Extracting the key state (bit 7)
 * 4. On key press (scan_code = 0):
 *    - Prints the corresponding character from char_map if exists
 *    - Prints 'C' if no mapping exists for that key code
 * 
 * The character is printed at position (0,0) on screen.
 * 
 * @see char_map
 * @see entry.S
 * @see io.c
 * @see hardware.c enable_int(void)
 */
void keyboard_handler(void);                  // HANDLER
void keyboard_routine() {                     // ROUTINE           
  // Byte scancode = inb(0x60);
  unsigned char port_value;
  int pressed, scan_code;

  port_value = inb(0x60);
  // printk("User pressed: ");

  pressed = port_value >> 7;      /* Bit 7 : Key State */     
  scan_code = port_value & 0x7F;  /* 0x7F = 0111 1111  */
  
  // Check if it's a make (0) or break (1) code
  if (!pressed) { // key press

    // If the key is not mapped, print 'C'
    if (char_map[scan_code] == '\0')
       printc_xy(0, 0, 'C');
    else 
      printc_xy(0, 0, char_map[scan_code]);
  }
}

/**
 * @brief Initializes the IDT table
 * 
 * This routine initializes the IDT table by setting the base and limit of the IDT
 * register and setting the handlers for the different interrupt vectors.
 * 
 * The routine sets the base of the IDT register to the address of the idt table and
 * the limit to the size of the table.
 * 
 * The routine then sets the handlers for the different interrupt vectors by calling
 * the setInterruptHandler() routine.
 * 
 * @see setInterruptHandler
 * @see idt
 * @see idtR
 * @see hardware.c enable_int(void)
 */
void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /**
   * setInterruptHandler(int position, void (*handler)(), int privLevel)
   * position: position in the IDT table
   * handler: pointer to the function that will handle the interrupt
   * privLevel: maximum privilege level that can access the interrupt
  */

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */

  setInterruptHandler(33, keyboard_handler, 0); /* Keyboard */

  set_idt_reg(&idtR);
}

