#include <asm.h>            /* Incluye el archivo de cabecera asm.h */

ENTRY(addASM)               
    push %ebp               /* Guarda el valor del base pointer actual (Dirección Retorno) en la pila */
    mov %esp,%ebp           /* Crea un nuevo frame pointer copiando esp a ebp */
    mov 0x8(%ebp),%eax      /* Mueve el primer parámetro a eax (offset 8 desde ebp) */
    add 0xc(%ebp), %eax     /* Suma el segundo parámetro (offset 12) al valor en eax */
    mov %ebp,%esp           /* Restaura el stack pointer */
    pop %ebp                /* Restaura el valor original del base pointer */
    ret                     /* Retorna de la función */