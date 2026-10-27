// the module header every NSO starts with: word 1 = offset of MOD0, MOD0 = offsets of DYNAMIC, .bss, .eh_frame_hdr
    .section .text.mod0, "ax"
    .global __module_start
__module_start:
    b 1f
    .word __mod0 - __module_start
__mod0:
    .ascii "MOD0"
    .word _DYNAMIC - __mod0
    .word __bss_start__ - __mod0
    .word __bss_end__ - __mod0
    .word __eh_frame_hdr_start - __mod0
    .word __eh_frame_hdr_end - __mod0
    .word 0
1:  ret
