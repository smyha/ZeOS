#include <asm.h>

#define CLONE_THREAD 0
#define CLONE_PROCESS 1
#define DEFAULT_STACK_SIZE 1024

#define SYS_GETKEYBOARDSTATE 5
#define SYS_PAUSE 6

#define SYS_START_SCREEN 7

#define SYS_CLONE 2
#define SYS_SET_PRIORITY 8
#define SYS_PTHREAD_EXIT 9

#define SYS_SEM_INIT 23
#define SYS_SEM_WAIT 24
#define SYS_SEM_POST 25
#define SYS_SEM_DESTROY 26

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

/*****************************************************/
/* fork and pthread_create are wrappers around clone */
/*        check on user.c for more details           */
/*****************************************************/

/* int fork() */
ENTRY(fork)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl $SYS_CLONE,%eax
	movl $CLONE_PROCESS,%ebx
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret

# /* int pthread_create(void *(*func)(void*), void *param, int stack_size) */
ENTRY(pthread_create)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	movl $SYS_CLONE,%eax
	movl $CLONE_THREAD,%ebx
	movl 0x8(%ebp), %ecx		// func
	movl 0xC(%ebp), %edx		// param
	movl 0x10(%ebp), %esi		// stack_size
	call syscall_sysenter
	popl %esi
	popl %ebx
	test %eax, %eax
	js nok
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
# ------------------ KEYBOARD ------------------

/* int GetKeyboardState(char *keyboard) */
ENTRY(GetKeyboardState)
	push %ebp
	movl %esp, %ebp
	pushl %ebx
	movl $SYS_GETKEYBOARDSTATE,%eax
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
	movl $SYS_PAUSE,%eax
	movl 0x8(%ebp), %ebx
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok
	popl %ebp
	ret	


# ------------------ MILESTONE 2 -------------------
# ------------------ SCREEN ------------------

/* void *StartScreen() */
ENTRY(StartScreen)
	pushl %ebp
	movl %esp, %ebp
	movl $SYS_START_SCREEN,%eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret

# ------------------ MILESTONE 3 -------------------
# ------------------ THREADS ------------------

/* int clone(int what, void *(*func)(void*), void *param, int stack_size) */

/* int SetPriority(int priority) */
ENTRY(SetPriority)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	movl 0x8(%ebp), %ebx 
	movl $SYS_SET_PRIORITY,%eax
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
	movl $SYS_PTHREAD_EXIT,%eax
	call syscall_sysenter
	test %eax, %eax
	js nok
	popl %ebp
	ret

# ------------------ MILESTONE 4 -------------------
# ------------------ SEMAPHORES ------------------

/* int sem_init(int value) */
ENTRY(sem_init)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;
	movl $SYS_SEM_INIT,%eax
	movl 0x8(%ebp), %ebx;
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* int sem_wait(int sem_id) */
ENTRY(sem_wait)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;
	movl $SYS_SEM_WAIT,%eax
	movl 0x8(%ebp), %ebx;
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* int sem_post(int sem_id) */
ENTRY(sem_post)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;
	movl $SYS_SEM_POST,%eax
	movl 0x8(%ebp), %ebx;
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret

/* int sem_destroy(int sem_id) */	
ENTRY(sem_destroy)
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx;
	movl $SYS_SEM_DESTROY,%eax
	movl 0x8(%ebp), %ebx;
	call syscall_sysenter
	popl %ebx
	test %eax, %eax
	js nok	// if (eax < 0) -->
	popl %ebp
	ret