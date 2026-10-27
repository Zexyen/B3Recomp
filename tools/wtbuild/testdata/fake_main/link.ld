/* the NSO shape: text at 0 (MOD0 first, then .plt), rodata with the dynamic symbols and exception tables, data */
SECTIONS
{
    . = 0;
    .text : { KEEP(*(.text.mod0)) *(.text .text.*) }
    .plt : { *(.plt) }
    . = ALIGN(0x1000);
    .rodata : { *(.rodata .rodata.*) }
    .hash : { *(.hash) }
    .dynsym : { *(.dynsym) }
    .dynstr : { *(.dynstr) }
    .rela.dyn : { *(.rela.dyn) }
    .rela.plt : { *(.rela.plt) }
    .eh_frame_hdr : { HIDDEN(__eh_frame_hdr_start = .); *(.eh_frame_hdr) HIDDEN(__eh_frame_hdr_end = .); }
    .eh_frame : { KEEP(*(.eh_frame)) }
    .gcc_except_table : { *(.gcc_except_table .gcc_except_table.*) }
    . = ALIGN(0x1000);
    .data : { *(.data .data.*) *(.data.rel.ro .data.rel.ro.*) }
    .init_array : { KEEP(*(.init_array .init_array.*)) }
    .dynamic : { *(.dynamic) }
    .got : { *(.got) }
    .got.plt : { *(.got.plt) }
    HIDDEN(__bss_start__ = .);
    .bss : { *(.bss .bss.*) }
    HIDDEN(__bss_end__ = .);
    /DISCARD/ : { *(.comment) *(.note*) *(.gnu.hash) }
}
