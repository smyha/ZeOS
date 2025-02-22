/*
 * entry.S - Entry point to system mode from user mode
 */

#include <asm.h>
#include <segment.h>

// Errno.h
#include "errno.h"

/**************************************************/
/**** Save & Restore ******************************/
/**                                              **/
/** When we change to privilege level 0 (kernel) **/
/** (through an interrupt, a system call, an     **/
/** exception ...) we must save the state of the **/
/** currently running task (save).               **/
/**                                              **/
/** Stack layout in 'systemCall':                **/
/**                                              **/
/**   0(%esp) - %edx    \                        **/
/**   4(%esp) - %ecx     |                       **/
/**   8(%esp) - %ebx     |                       **/
/**   C(%esp) - %esi     | Register saved        **/
/**  10(%esp) - %edi     |  by 'save'            **/
/**  14(%esp) - %ebp     |                       **/
/**  18(%esp) - %eax     |                       **/
/**  1C(%esp) - %ds      |                       **/
/**  20(%esp) - %es      |                       **/
/**  24(%esp) - %fs      |                       **/
/**  28(%esp) - %gs     /                        **/
/**  2C(%esp) - %eip    \                        **/
/**  30(%esp) - %cs      |                       **/
/**  34(%esp) - %eflags  |  Return context saved **/
/**  38(%esp) - %oldesp  |   by the processor.   **/
/**  3C(%esp) - %oldss  /                        **/
/**                                              **/
/**************************************************/

#define SAVE_ALL \
      pushl %gs; \
      pushl %fs; \
      pushl %es; \
      pushl %ds; \
      pushl %eax; \
      pushl %ebp; \
      pushl %edi; \
      pushl %esi; \
      pushl %ebx; \
      pushl %ecx; \
      pushl %edx; \
      movl $__KERNEL_DS, %edx;    \
      movl %edx, %ds;           \
      movl %edx, %es

#define RESTORE_ALL \ 
      popl %edx; \
      popl %ecx; \
      popl %ebx; \
      popl %esi; \
      popl %edi; \
      popl %ebp; \
      popl %eax; \
      popl %ds; \
      popl %es; \
      popl %fs; \
      popl %gs;


// EOI - END OF INTERRUPTION
#define EOI \
      movb $0x20, %al ; \
      outb %al, $0x20 ;

/**
 * System Call Handler Documentation
 * ---------------------------------
 * This is the main entry point for system calls in the kernel. When a user program
 * executes an 'int 0x80' instruction (from a sys wrapper), control transfers here.
 *
 * Flow:
 * 1. SAVE_ALL macro:
 *    - Saves all general purpose registers
 *    - Saves segment registers (gs, fs, es, ds)
 *    - Sets up kernel data segment
 *
 * 2. Validation:
 *    - Checks if system call number in EAX is valid:
 *      * Must be non-negative
 *      * Must be less than or equal to MAX_SYSCALL
 *
 * 3. System Call Execution:
 *    - If valid, calls corresponding handler from sys_call_table using EAX as index
 *    - If invalid, sets error code ENOSYS
 *
 * 4. Return:
 *    - Stores return value back in user's EAX via stack
 *    - RESTORE_ALL macro restores all saved registers
 *    - iret returns to user mode
 *
 * Register State on Entry:
 * - EAX: System call number (e.g. 4 for write)
 * - Other registers: Can contain system call parameters
 *   ! For write syscall:
 *   - EDX: file descriptor
 *   - ECX: buffer pointer
 *   - EBX: size
 **/
ENTRY(system_call_handler)
      SAVE_ALL // Save the current context
      cmpl $0, %EAX // Is syscall number negative?
      jl err // If it is, jump to return an error
      cmpl $MAX_SYSCALL, %EAX // Is syscall greater than MAX_SYSCALL (4)?
      jg err // If it is, jump to return an error
      call *sys_call_table(, %EAX, 0x04) // Call the corresponding service routine
      jmp fin // Finish
err:
      movl $-ENOSYS, %EAX // Move to EAX the ENOSYS error
fin:
      movl %EAX, 0x18(%esp) // Change the EAX value in the stack
      RESTORE_ALL // Restore the context
      iret

// CLOCK HANDLER
ENTRY(clock_handler)
      SAVE_ALL
      EOI                     // !
      call clock_routine
      RESTORE_ALL
      iret

// KEYBOARD HANDLER
ENTRY(keyboard_handler) 
      SAVE_ALL
      EOI                     // !
      call keyboard_routine
      RESTORE_ALL
      iret