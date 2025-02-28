#include <asm.h>

/*
 * writeMSR - Write to a Model Specific Register
 *
 * This function writes a value to a Model Specific Register (MSR).
 * MSRs are special registers used for controlling and monitoring
 * processor-specific features.
 *
 * Parameters:
 *   - msr_number: The MSR number to write to (first parameter on the stack).
 *   - value: The value to write to the MSR (second parameter on the stack).
 */
ENTRY(writeMSR)
    /* Save old base pointer + Set new base pointer */
    pushl %ebp           
    movl %esp, %ebp      

    movl 8(%ebp), %ecx   /* Load the MSR number from the stack into ECX */
    movl 12(%ebp), %eax  /* Load the value to write from the stack into EAX (lower 32 bits) */
    xor %edx, %edx       /* Force 0 value in EDX (upper 32 bits of the value) */

    wrmsr                /* Write the value in EDX:EAX to the MSR specified by ECX */

    /* Restore old base pointer + Return to the caller */
    movl %ebp, %esp      
    popl %ebp            
    ret                  /* Return to the caller */