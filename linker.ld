ENTRY(_start)

SECTIONS
{
    . = 1M;
    _kernel_start = .;

    .text :
    {
        *(.multiboot)
        *(.text*)
    }

    .rodata :
    {
        *(.rodata*)
    }

    .data :
    {
        *(.data*)
    }

    .bss :
    {
        *(COMMON)
        *(.bss*)
    }

    . = ALIGN(4096);
    _kernel_end = .;
}
