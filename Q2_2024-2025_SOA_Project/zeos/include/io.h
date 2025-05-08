/*
 * io.h - Definició de l'entrada/sortida per pantalla en mode sistema
 */

#ifndef __IO_H__
#define __IO_H__

#include <types.h>

// Screen dimensions
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25

// Screen colors
#define BLACK 0x0
#define WHITE 0xF

/** Screen functions **/
/**********************/

Byte inb (unsigned short port);
void printc(char c);
void printc_xy(Byte x, Byte y, char c);
void printk(char *string);

// Auxiliary functions
void print_number(int num);
void print_number_to_screen(int num, unsigned short *screen, int pos);

#endif  /* __IO_H__ */
