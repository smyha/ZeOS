/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>

#include <zeos_interrupt.h>

typedef unsigned int Hexa;

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
 * Page Fault Exception
 * 
 * Handles page fault exceptions in the system. When a page fault occurs,
 * it prints the address (EIP) where the fault happened in hexadecimal format
 * and halts the system.
 *
 * The routine performs the following:
 * - Receives the error code and EIP (instruction pointer) where fault occurred
 * - Prints a diagnostic message with the EIP in hexadecimal format
 * - Halts the system in an infinite loop
 * 
 * Parameters:
 * @param error - Error code provided by CPU (unused)
 * @param EIP   - Program counter value when the page fault occurred (only for routine)
 *
 * @note After printing the fault address, the system enters an infinite loop.
 *       This is a fatal error handler - execution does not continue.
 */
void _page_fault_handler(void);                                           //HANDLER
void _page_fault_routine(unsigned long error, unsigned long EIP){         //ROUTINE
  printk("\n");
  printk("Procces generates a PAGE FAULT exception at EIP: 0x");
  
  // Convert EIP (unsigned int) to HEX
  char hex_digits[] = "0123456789ABCDEF";
  for (int i = 7; i >= 0; i--)                  /*8 HEX digits = 32 bits*/
    printc(hex_digits[(EIP >> (4 * i)) & 0xF]); /*Each digit is 4 bits*/
  
  printk("\n");
  printk("Halting the system...\n");
  while(1);     // Infinite loop
} 
/**
 * @brief Handler for system calls in the ZeOS operating system
 * 
 * This function handles system calls by:
 * 1. Saving the current execution context using SAVE_ALL macro
 * 2. Validating the system call number in EAX (must be between 0 and MAX_SYSCALL)
 * 3. Calling the appropriate system call handler from sys_call_table
 * 4. Returning ENOSYS error if system call number is invalid
 * 5. Restoring the execution context using RESTORE_ALL macro
 *
 * The stack layout and register state is preserved according to the x86 calling convention.
 * See entry.S for detailed stack layout documentation.
 * 
 * @note: (Deprecated) This function is replaced by syscall_handler_sysenter which uses the SYSENTER instruction.
 */
void system_call_handler(void);

/**
 * @brief Assembly "wrapper" for SYSENTER system call handler
 * 
 * This function serves as the entry point for system calls made using the SYSENTER instruction.
 * It's declared in C but implemented in assembly (entry.S). The actual implementation:
 * 
 * 1. Saves the user context including:
 *    - User stack address
 *    - CPU flags
 *    - User return address
 *    - All general purpose registers
 * 
 * 2. Validates the system call number in EAX:
 *    - Must be non-negative
 *    - Must not exceed MAX_SYSCALL
 * 
 * 3. Executes the appropriate system call from sys_call_table
 * 
 * 4. Restores the user context and returns via SYSEXIT
 * 
 * @note This function is called automatically by the CPU when a user process
 *       executes the SYSENTER instruction. It should not be called directly.
 * 
 * @return Returns to user space using SYSEXIT instruction
 *         System call result is placed in EAX
 *         Returns -ENOSYS if system call number is invalid
 */
void syscall_handler_sysenter(void);

/**
 * @brief Writes a value to a Model Specific Register (MSR)
 * 
 * Model Specific Registers are special registers used for controlling and
 * monitoring processor-specific features. This function provides an interface
 * to write 64-bit values to these registers.
 * 
 * @param msr The MSR number/identifier to write to
 * @param value The 64-bit value to write to the specified MSR
 * 
 * @note This function is implemented in assembly (func_MSR.S) and uses
 *       the WRMSR instruction which requires kernel privileges
 * @see func_MSR.S
 */
void writeMSR(unsigned long msr, unsigned long value);

/**
 * @brief Clock interrupt handling system
 * 
 * Contains two main components:
 * - Handler function that manages clock interrupt events
 * - Clock routine that updates and displays the system time
 * 
 * The system uses these functions to maintain and show the current time,
 * with the clock advancing at a rate faster than real time for demonstration purposes.
 * The display is managed through zeos_show_clock() function calls.
 */
void clock_handler(void);                     // HANDLER
void clock_routine() {                        // ROUTINE
  // NOTE: Showed time goes faster than the real one
  ++zeos_ticks;
  zeos_show_clock();
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

  setInterruptHandler(32, clock_handler, 0);    /* Clock */
  setInterruptHandler(33, keyboard_handler, 0); /* Keyboard */
setInterruptHandler(14, _page_fault_handler, 0); /* Page Fault, PL0 */
  setInterruptHandler(0x80, system_call_handler, 3); /* System calls */

  // ! Write MSR registers
  writeMSR(0x174, __KERNEL_CS);         // Sets kernel code segment selector for SYSENTER
  writeMSR(0x175, INITIAL_ESP);         // Sets the kernel stack pointer for SYSENTER 
  writeMSR(0x176, (DWord)syscall_handler_sysenter);  // Sets the entry point address for FAST system calls
  // writeMSR(0x176, (DWord)system_call_handler);  
  /*@see: sched.h*/
  
  set_idt_reg(&idtR);
}

