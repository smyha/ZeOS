#include<asm.h>
# ECX and EBX which will be modified by sysexit 
#define SAVE_REG \
    pushl %ebx;\
    pushl %ecx;\
    pushl %edx;

#define RESTORE_REG \
    popl %edx;\
    popl %ecx;\
    popl %ebx;

# ------------------------- FAST SYSTEM CALL WRAPPERS ------------------------- 

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