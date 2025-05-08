#include <asm.h>

ENTRY(syscall_sysenter)
	push %ecx
	push %edx
	push $SYSENTER_RETURN
	push %ebp
	mov %esp, %ebp
	sysenter
ENTRY(SYSENTER_RETURN)
	pop %ebp
	pop %edx
	pop %edx
	pop %ecx
	ret

/* int write(int fd, char *buffer, int size) */
ENTRY(write)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $4, %eax
	movl 0x8(%ebp), %ebx;	//fd
	movl 0xC(%ebp), %ecx;	//buffer
	movl 0x10(%ebp), %edx;	//size
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -> 
	popl %ebp
	ret

/* Common code for negative return */
nok:
	neg %eax
	mov %eax, errno
	mov $-1, %eax
	popl %ebp
	ret

/* int gettime() */
ENTRY(gettime)
	pushl %ebp
	movl %esp, %ebp
	movl $10, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int getpid() */
ENTRY(getpid)
	pushl %ebp
	movl %esp, %ebp
	movl $20, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int fork() */
ENTRY(fork)
	pushl %ebp
	movl %esp, %ebp
	movl $2, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* void exit() */
ENTRY(exit)
	pushl %ebp
	movl %esp, %ebp
	movl $1, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int yield() */
ENTRY(yield)
	pushl %ebp
	movl %esp, %ebp
	movl $13, %eax
	call syscall_sysenter
	popl %ebp
	ret

/* int get_stats(int pid, struct stats *st) */
ENTRY(get_stats)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;  // Save EBX, ESI and EDI if modified
	movl $35, %eax
	movl 0x8(%ebp), %ebx;	//pid
	movl 0xC(%ebp), %ecx;	//st
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

# ------------------ MILESTONE 1 -------------------
/* int GetKeyboardState(char *keyboard) */
ENTRY(GetKeyboardState)
	push %ebp
	movl %esp, %ebp
	pushl %ebx
	movl $5,%eax
	movl 0x8(%ebp), %ebx
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

/* int pause(int miliseconds) */
ENTRY(pause)
	push %ebp
	movl %esp, %ebp
	pushl %ebx
	movl $6, %eax
	movl 0x8(%ebp), %ebx
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret	


# ------------------ MILESTONE 2 -------------------
/* void *StartScreen() */
ENTRY(StartScreen)
	pushl %ebp
	movl %esp, %ebp
	movl $7, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret

# ------------------ MILESTONE 3 -------------------
/* int clone(int what, void *(*func)(void*), void *param, int stack_size) */
ENTRY(clone)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	movl $2 ,%eax
	movl 0x8(%ebp), %ebx
	movl 0xC(%ebp), %ecx
	movl 0x10(%ebp), %edx
	movl 0x14(%ebp), %esi
	call syscall_sysenter
	popl %esi
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

/* int SetPriority(int priority) */
ENTRY(SetPriority)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl 0x8(%ebp), %ebx 
	movl $8, %eax
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

/* void pthread_exit() */
ENTRY(pthread_exit)
	pushl %ebp
	movl %esp, %ebp
	movl $9, %eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret