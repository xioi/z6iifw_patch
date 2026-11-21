ENTRY(extension_boot)
SECTIONS {
    . = 0x125147b8;
    .extension : { *(.text .text.*) *(.rodata .rodata.*) . = ALIGN(4); }
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
}
