/* OpenSBI hands control to the next stage at 0x80200000: the firmware itself
 * occupies 0x80000000 upward, which is why this differs from the `-bios none`
 * arrangement where the image is the first thing in RAM. */
ENTRY(_start)
SECTIONS {
    . = 0x80200000;
    .text   : { *(.text.entry) *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; }
    . = ALIGN(16); . = . + 0x4000; __stack_top = .;
}
