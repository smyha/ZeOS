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
    # Save old base pointer + Set new base pointer 
    pushl %ebp           
    movl %esp, %ebp      

    movl 8(%ebp), %ecx   # Load the MSR number from the stack into ECX 
    movl 12(%ebp), %eax  # Load the value to write from the stack into EAX (lower 32 bits) 
    xor %edx, %edx       # Force 0 value in EDX (upper 32 bits of the value) 

    wrmsr                # Write the value in EDX:EAX to the MSR specified by ECX 

    # Restore old base pointer + Return to the caller 
    movl %ebp, %esp      
    popl %ebp            
    ret                  # Return to the caller 

/* void task_switch(union task_union *new) 
    * 
    * This function performs a task switch by loading the new task's page directory
    * into the CR3 register and then performing a far jump to the new task's code.
    * 
    * Parameters:
    *   - new: A pointer to the task_union structure of the new task.
*/

ENTRY(task_switch)
    # Save old base pointer + Set new base pointer 
    pushl %ebp           
    movl %esp, %ebp      

    # Save registers that need to be preserved 
    pushl %esi
    pushl %edi
    pushl %ebx

    # Pass task_union *new as the first parameter to the switch_task function 
    # movl 8(%ebp), %edx   # This line is not needed because the parameter is already in EDX
    push 8(%ebp)
    call inner_task_switch
    
    # Skip the parameter *new 
    addl $4, %esp

    # Restore registers 	
    popl %ebx
    popl %edi
    popl %esi

    # Restore old base pointer + Return to the caller 
    # movl %ebp, %esp   # This line is not needed because the stack pointer is already correct
    popl %ebp
    ret

ENTRY(switch_stack)
    /** EBP has the address of the current system stack 
     *  where inner_task_switch begins (dynamic link)
    */

    # Store the current value of the EBP register in the PCB of the current task
    movl 4(%esp), %eax   # First parameter: old_esp
    movl %ebp, (%eax)    # Store the current EBP value the pointed direction (old_ebp) [PCB]
    # current()->kernel_esp = EBP (old_ebp)

    # Load the new stack pointer from the second parameter into the ESP register
    # This will change the stack pointer to the new task's kernel stack
    movl 8(%esp), %esp   # Second parameter: new_esp
    # esp = new_esp

    # Restore ebp of the new task and return to the caller
    popl %ebp            # Restore the EBP from the new task
    ret                  # Return to the direction stored in the new stack 
    # [inner_task_switch]