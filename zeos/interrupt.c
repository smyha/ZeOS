/*
 * interrupt.c -
 */
#include <types.h>
#include <interrupt.h>
#include <segment.h>
#include <hardware.h>
#include <io.h>

#include<stdint.h>

#include <zeos_interrupt.h>

Gate idt[IDT_ENTRIES];
Register    idtR;

char char_map[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','¡','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','ñ',
  '\0','º','\0','ç','z','x','c','v',
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
 * @brief Initializes the Interrupt Descriptor Table (IDT) for ZeOS
 * 
 * This function configures the system's IDT by:
 * 1. Setting up the IDT register (idtR) with the base address and size limit
 * 2. Calling set_handlers() to initialize default interrupt handlers
 * 3. Configuring interrupt 0x80 for system calls with system_call_handler at privilege level 3
 * 4. Setting up Model-Specific Registers (MSRs) for fast system call entry via SYSENTER:
 *    - MSR 0x174: Kernel code segment selector (__KERNEL_CS)
 *    - MSR 0x175: System call entry point address (system_call_handler)
 *    - MSR 0x176: Kernel stack pointer for SYSENTER (INITIAL_ESP)
 * 5. Loading the IDT register configuration with set_idt_reg()
 * 
 * This setup enables both traditional int 0x80 system calls and the faster SYSENTER
 * instruction for transitioning from user mode to kernel mode.
 * The stack layout and register state is preserved according to the x86 calling convention.
 * @see: entry.S for detailed stack layout documentation.
 */
void setIdt()
{
  /* Program interrups/exception service routines */
  idtR.base  = (DWord)idt;
  idtR.limit = IDT_ENTRIES * sizeof(Gate) - 1;
  
  set_handlers();

  /* ADD INITIALIZATION CODE FOR INTERRUPT VECTOR */

  // ! Configure syscall handler
  setInterruptHandler(0x80, system_call_handler, 3);
  
  // ! Write MSR registers
  writeMSR(0x174, __KERNEL_CS);         // Sets kernel code segment selector for SYSENTER
  writeMSR(0x175, INITIAL_ESP);         // Sets the kernel stack pointer for SYSENTER 
  writeMSR(0x176, (DWord)syscall_handler_sysenter);  // Sets the entry point address for FAST system calls
  // writeMSR(0x176, (DWord)system_call_handler);  
  /*@see: sched.h*/
  
  set_idt_reg(&idtR);
}