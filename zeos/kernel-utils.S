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
    * 
    * Stack layout (before calling inner_task_switch):
    * Higher memory addresses
    * +--------------------+
    * | *new parameter     | <-- 8(%ebp)  (original parameter)
    * +--------------------+
    * | Return address     | <-- 4(%ebp)
    * +--------------------+
    * | Old %ebp value     | <-- 0(%ebp)  (%ebp points here)
    * +--------------------+
    * | Saved %esi         |
    * +--------------------+
    * | Saved %edi         |
    * +--------------------+
    * | Saved %ebx         |
    * +--------------------+
    * | *new parameter     | <-- %esp  (parameter for inner_task_switch)
    * +--------------------+
    * | Return address     | <- Next instr: addl $4, %esp
    * +--------------------+
    * Lower memory addresses
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
    push 8(%ebp)
    call inner_task_switch  # void inner_task_switch(union task_union *new)
    
    # Skip the parameter *new 
    addl $4, %esp               # Or popl %eax

    # Restore registers 	
    popl %ebx
    popl %edi
    popl %esi

    # Restore old base pointer + Return to the caller 
    # Following line not needed because now execution continues on the other task
    # movl %ebp, %esp   
    popl %ebp
    ret


/* void switch_stack(unsigned long *old_esp, unsigned long new_esp)
 *
 * This function performs the actual context switch 
 * by changing the stack pointers between tasks.
 * It saves the current execution context and loads the new task's context.
 *
 * Parameters:
 *   - old_esp: Pointer to where the current task's ESP should be saved
 *   - new_esp: The ESP value of the new task to be loaded
 *
 * Stack manipulation:
 *   - Before: Current task's stack is active
 *   - After:  New task's stack is active with its own EBP restored
 *
 * Stack layout (before calling switch_stack):
 * Higher memory addresses
 * +--------------------+
 * | new_esp parameter   | <-- 8(%esp)  (original parameter)
 * +--------------------+
 * | old_esp parameter   | <-- 4(%esp)  (original parameter)
 * +--------------------+
 * | Return address      | <-- (%esp)
 * +--------------------+
 * Lower memory addresses
 *
 * This function is called by inner_task_switch() to finalize the task switch operation.
 * When it returns, execution continues in the context of the new task.
 */
ENTRY(switch_stack)
    /** EBP has the address of the current system stack 
     *  where inner_task_switch begins (dynamic link)
     *  (task_switch -> inner_task_switch -> switch_stack -> inner_task_switch)
     *  ESP has the address of the current system stack
    */

    # Store the current value of the EBP register in the PCB of the current task
    movl 4(%esp), %eax   # First parameter: old_esp
    movl %ebp, (%eax)    # Store the current EBP value the pointed direction (old_ebp) [PCB]
    # current()->kernel_esp = EBP (old_ebp)

    # Load the new stack pointer from the second parameter into the ESP register
    # This will change the stack pointer to the new task's kernel stack
    movl 8(%esp), %esp   # Second parameter: new_esp
    # esp = new_esp

    /* From here on, the stack pointer is pointing to the new task's kernel stack 
    * The EBP register is still pointing to the old task's stack so we need to restore it
    * Stack layout at this point (new process):
    * Higher memory addresses
    * +--------------------+
    * | t_u: kernel_esp    | ---
    * +--------------------+   |    
    * |        ...         |   |
    * +--------------------+   |
    * |        ebp         | <-- %esp
    * +--------------------+
    * | @ret (task_switch) |
    * +--------------------+
    * |        new*        |
    * +--------------------+
               ...
    * Lower memory addresses
    */

    # Restore ebp of the new task and return to the caller
    popl %ebp            # Restore the EBP from the new task
    ret                  # Return to the direction stored in the new stack 
    # [inner_task_switch]