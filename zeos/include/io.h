/*
 * io.h - Definició de l'entrada/sortida per pantalla en mode sistema
 */

#ifndef __IO_H__
#define __IO_H__

#include <types.h>

/** Screen functions **/
/**********************/

Byte inb (unsigned short port);
void printc(char c);
void printc_xy(Byte x, Byte y, char c);
void printk(char *string);


/** ADDITIONAL FUNCTIONS **/
/**********************/
// static void _printc_impl(char c, Byte color);
void scroll_screen();
void printc_color(char c, Byte color);
void print_number(int num);

#endif  /* __IO_H__ */
