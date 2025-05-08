/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>

#include <sched.h>

#include <zeos_interrupt.h>

Gate idt[IDT_ENTRIES];
Register    idtR;

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

int zeos_ticks = 0;

// Keyboard buffer for system calls
char keyboard_buffer[128];  // Vector of the possible keys -> 1 if pressed 0 if not.

extern struct list_head blocked;
extern struct list_head readyqueue;
/**
 * @brief Updates the remaining pause time of blocked tasks.
 *
 * Iterates through the blocked list, decrementing the pause_time of each task.
 * If a task's pause_time reaches 0, it is moved to the ready queue.
 */
void update_blocked_time() {
	struct list_head *pos, *tmp;
	
  // Iterate through the blocked list
  list_for_each_safe(pos, tmp, &blocked) {
		struct task_struct *t = list_entry(pos, struct task_struct, list);
		t->pause_time--;  // Decrement the pause time
		
    // If the pause time is 0, move the task to the ready queue
    if (t->pause_time <= 0) {
			update_process_state_rr(t, &readyqueue);
		}
	}
}

void dumpScreen() {
  Word* screen = (Word *)0xb8000;
  Word* content = (Word*)(current()->screen_page);  
  Word color = WHITE; 

  // Copy the screen content to the video memory
  for (int i = 0; i < 80*25; ++i) {
    screen[i] = content[i];
  }
}

/**
 * @brief Clock interrupt handler.
 *
 * This routine is called upon a clock interrupt. It updates the ZeOS clock,
 * increments the global tick counter, and triggers the scheduler.
 */
void clock_routine()
{
  zeos_show_clock();
  zeos_ticks++;
  
  // Update blocked processes
  update_blocked_time();
  
  // Update screen if process has a screen page
  struct task_struct *t = current();

  // Check if the process has a screen page and is running
  if (t->PID != -1 && t->screen_page != (void*)-1) 
    dumpScreen();
  
  // Schedule the next process
  schedule();
}

/**
 * @brief Keyboard interrupt handler.
 *
 * This routine is called upon a keyboard interrupt. It reads the scancode from
 * port 0x60 and, if the key is pressed (not released), updates the keyboard buffer
 * and prints the corresponding character to the screen.
 */
void keyboard_routine()
{
  unsigned char c = inb(0x60);
  
  // Check if the key is pressed (not released)
  if (!(c&0x80)) {
    keyboard_buffer[c] = 1;
    // printc_xy(0, 0, char_map[c&0x7f]);
  } 
  else if ((c&0x80)) {
    keyboard_buffer[c&0x7f] = 0;
    // printc_xy(0, 0, char_map[c&0x7f]);
  }
}

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

void clock_handler();
void keyboard_handler();
void system_call_handler();

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void setSysenter()
{
  setMSR(0x174, 0, __KERNEL_CS);
  setMSR(0x175, 0, INITIAL_ESP);
  setMSR(0x176, 0, (unsigned long)system_call_handler);
}

void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */
  setInterruptHandler(32, clock_handler, 0);
  setInterruptHandler(33, keyboard_handler, 0);

  setSysenter();

  set_idt_reg(&idtR);
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

   
  // printk(" with error code: 0x");
  // printc(hex_digits[error & 0xF]);
  
  printk("\n");
  printk("Halting the system...\n");
  while(1);     // Infinite loop
} 



