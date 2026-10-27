/* Freestanding setjmp.h for the code compiled on the player's PC (README.md): glibc's x86-64 jmp_buf (200 bytes; the
 * prebuilt sdklift_rt.c sees the same SdkJmpRec through glibc's own header) and setjmp. */
#ifndef WT_FREESTANDING_SETJMP_H
#define WT_FREESTANDING_SETJMP_H
typedef long int __jmp_buf[8];
typedef struct { unsigned long int __val[(1024 / (8 * sizeof (unsigned long int)))]; } __sigset_t;
struct __jmp_buf_tag {
    __jmp_buf __jmpbuf;
    int __mask_was_saved;
    __sigset_t __saved_mask;
};
typedef struct __jmp_buf_tag jmp_buf[1];
extern int setjmp (jmp_buf __env) __attribute__ ((__nothrow__));
#endif
