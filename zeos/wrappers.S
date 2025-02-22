#include<asm.h>

/** ENTRY(gettime) - System call wrapper for gettime
 *  int gettime() - Returns the current time in seconds
 *  Return: 
 *      int - The current time in seconds
 */
ENTRY(gettime)
    pushl %ebp            /* Save old ebp */
    movl %esp, %ebp       /* Set up new stack frame */

    /* ! Follow the ABI contract */
    /* Save the registers */
    // pushl %ebx
    pushl %ecx
    pushl %edx

    /* No arguments to pass */

    movl $10, %eax        /* Function 10: gettime */
    int $0x80
    
    /* Check if it returns an error (negative value) */
    cmpl $0, %eax        /* Compare result with 0 */
    jge gettime_end      /* If positive or zero, return as is */
 
gettime_error:
    /* Handle error case */
    negl %eax            /* Convert to positive value */
    movl %eax, errno     /* Store error code in errno */
    movl $-1, %eax       /* Return -1 to indicate error */   

gettime_end:
    /* Restore the registers */
    popl %edx
    popl %ecx
    // popl %ebx

    movl %ebp, %esp      /* Restore stack pointer */
    popl %ebp            /* Restore old ebp */
    ret                  /* Return to caller */