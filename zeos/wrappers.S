#include<asm.h>

# ECX and EBX which will be modified by sysexit 
#define SAVE_REG \
    pushl %ecx;\
    pushl %edx;

#define RESTORE_REG \
    popl %edx;\
    popl %ecx;\

# ------------------------- FAST SYSTEM CALL WRAPPERS ------------------------- 

/* ENTRY(write) - FAST System call wrapper for write
 * int write (int fd, char * buffer, int size);
 * Parameters:
 *   - fd (8(%esp)): File descriptor
 *   - buffer (12(%esp)): Pointer to data to write
 *   - count (16(%esp)): Number of bytes to write
 * Returns:
 *   - Number of bytes written or error code
*/
ENTRY(write)
    pushl %ebp            # Save old ebp 
    movl %esp, %ebp       # Set up new stack frame 

    movl $4, %eax        # System call number for write (4) 
    
    # ! Follow the System V ABI contract 
    # Save registers that will be used here
    pushl %ebx 

    movl 8(%ebp), %edx   # First param: fd -> edx 
    movl 12(%ebp), %ecx  # Second param: buffer -> ecx 
    movl 16(%ebp), %ebx  # Third param: size -> ebx 


    SAVE_REG        

    # Store return addres write_return 
    pushl $write_return

    # Create fake dynamic link 
    pushl %ebp
    movl %esp, %ebp

    SYSENTER            # Generate FAST system call interrupt 

write_return:
    # Clean up the stack 
    popl %ebp
    addl $4, %esp

    # Restore saved registers 
    RESTORE_REG
    popl %ebx

    # Check if it returns an error (negative value) 
    cmpl $0, %eax        # Compare result with 0 
    jge write_end        # If positive or zero, return as is 

write_error:
    # Handle error case 
    negl %eax            # Convert to positive value 
    movl %eax, errno     # Store error code in errno 
    movl $-1, %eax       # Return -1 to indicate error 

    addl $8, %esp        # Put @ret at the top of the stack 

write_end:
    movl %ebp, %esp      # Restore stack pointer 
    popl %ebp            # Restore old ebp 
    ret                  # Return to caller 

/** ENTRY(gettime) - System call wrapper for gettime
 *  int gettime() - Returns the current time in seconds
 *  Return: 
 *      int - The current time in seconds
 */
ENTRY(gettime)
    pushl %ebp            # Save old ebp 
    movl %esp, %ebp       # Set up new stack frame 

    movl $10, %eax        # System call number for gettime (10)
    
    # ! Follow the System V ABI contract 
    # Save registers that will be used here 
    SAVE_REG        

    # No arguments to pass

    # Store return addres gettime_return 
    pushl $gettime_return

    # Create fake dynamic link 
    pushl %ebp
    movl %esp, %ebp

    SYSENTER            # Generate FAST system call interrupt 

gettime_return:
    # Clean up the stack 
    popl %ebp
    addl $4, %esp

    # Restore saved registers 
    RESTORE_REG

    # Check if it returns an error (negative value) 
    cmpl $0, %eax        # Compare result with 0 
    jge gettime_end        # If positive or zero, return as is 

gettime_error:
    # Handle error case 
    negl %eax            # Convert to positive value 
    movl %eax, errno     # Store error code in errno 
    movl $-1, %eax       # Return -1 to indicate error 

    addl $8, %esp        # Put @ret at the top of the stack 

gettime_end:
    movl %ebp, %esp      # Restore stack pointer 
    popl %ebp            # Restore old ebp 
    ret                  # Return to caller 


# ------------------------- SYSTEM CALL WRAPPERS ------------------------- 

ENTRY(write_int)
    push %ebp            # Save old ebp 
    mov %esp, %ebp       # Set up new stack frame 

    # ! Follow the ABI contract 
    # Save registers that will be used here 
    SAVE_REG   

    movl 8(%ebp), %edx   # First param: fd -> edx 
    movl 12(%ebp), %ecx  # Second param: buffer -> ecx 
    movl 16(%ebp), %ebx  # Third param: size -> ebx 

    movl $4, %eax        # System call number for write (4)
    int $0x80            # Generate system call interrupt 

    # Check if it returns an error (negative value) 
    cmpl $0, %eax        # Compare result with 0 
    jge writeINT_end        # If positive or zero, return as is 

writeINT_error:
    # Handle error case 
    negl %eax            # Convert to positive value 
    movl %eax, errno     # Store error code in errno 
    movl $-1, %eax       # Return -1 to indicate error 

writeINT_end:
    # Restore saved registers 
    RESTORE_REG

    movl %ebp, %esp      # Restore stack pointer 
    popl %ebp            # Restore old ebp 
    ret                  # Return to caller 

ENTRY(gettime_int)
    pushl %ebp            # Save old ebp 
    movl %esp, %ebp       # Set up new stack frame 

    # ! Follow the ABI contract 
    # Save the registers 
    SAVE_REG

    # No arguments to pass 

    movl $10, %eax        # System call number for gettime (10) 
    int $0x80
    
    # Check if it returns an error (negative value) 
    cmpl $0, %eax        # Compare result with 0 
    jge gettimeINT_end   # If positive or zero, return as is 
 
gettimeINT_error:
    # Handle error case 
    negl %eax            # Convert to positive value 
    movl %eax, errno     # Store error code in errno 
    movl $-1, %eax       # Return -1 to indicate error    

gettimeINT_end:
    # Restore the registers 
    RESTORE_REG

    movl %ebp, %esp      # Restore stack pointer 
    popl %ebp            # Restore old ebp 
    ret                  # Return to caller 
