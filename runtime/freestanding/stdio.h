/* Freestanding stdio.h for the code compiled on the player's PC (README.md): FILE for the prototypes in hostrt.h and
 * sdklift.h (only pointers to it are passed), and printf. */
#ifndef WT_FREESTANDING_STDIO_H
#define WT_FREESTANDING_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct _IO_FILE FILE;
extern int printf (const char *__restrict __format, ...);
#endif
