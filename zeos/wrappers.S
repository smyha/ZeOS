#include<asm.h>

/* errno is a position in memory */

/* ENTRY(write) - System call wrapper for write
 * int write (int fd, char * buffer, int size);
 * Parameters:
 *   - fd (8(%esp)): File descriptor
 *   - buffer (12(%esp)): Pointer to data to write
 *   - count (16(%esp)): Number of bytes to write
 * Returns:
 *   - Number of bytes written or error code
 */
ENTRY(write)
    push %ebp           /* Save old ebp */
    mov %esp, %ebp      /* Set up new stack frame */

    mov 8(%ebp), %edx   /* First param: fd -> edx */
    mov 12(%ebp), %ecx  /* Second param: buffer -> ecx */
    mov 16(%ebp), %ebx  /* Third param: count -> ebx */

    mov $4, %eax        /* System call number for write (4) */
    int $0x80           /* Generate system call interrupt */

    /* Check if it returns an error (negative value) */
    cmpl $0, %eax
    jge write_end       /* If positive or zero, return as is */

write_error:
    /* Handle error case */
    neg %eax            /* Convert to positive value */
    mov %eax, errno     /* Store error code in errno */
    mov $-1, %eax       /* Return -1 to indicate error */

write_end:
    mov %ebp, %esp      /* Restore stack pointer */
    pop %ebp            /* Restore old ebp */
    ret                 /* Return to caller */
