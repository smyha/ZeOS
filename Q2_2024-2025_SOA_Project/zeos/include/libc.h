/*
 * libc.h - macros per fer els traps amb diferents arguments
 *          definició de les crides a sistema
 */
 
#ifndef __LIBC_H__
#define __LIBC_H__

#include <stats.h>

extern int errno;

int write(int fd, char *buffer, int size);

void itoa(int a, char *b);

int strlen(char *a);

void perror();

int getpid();

int fork();

void exit();

int yield();

int get_stats(int pid, struct stats *st);

// ------------------ MILESTONE 1 -------------------
int GetKeyboardState(char* keyboard);

int pause(int miliseconds);

// ------------------ MILESTONE 2 -------------------

void* StartScreen();

// ------------------ MILESTONE 3 -------------------

int clone(int what, void *(*func)(void*), void *param, int stack_size);

int SetPriority(int priority);

int pthread_exit();

// ------------------ MILESTONE 4 -------------------



#endif  /* __LIBC_H__ */
