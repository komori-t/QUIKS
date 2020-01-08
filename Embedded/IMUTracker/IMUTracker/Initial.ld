/*
 * GENERATED FILE - DO NOT EDIT
 * Copyright (c) 2008 - 2013 Code Red Technologies Ltd, 
 * Copyright 2015, 2018 NXP
 * (c) NXP Semiconductors 2013-2019
 * Generated linker script file for LPC802
 * Created from linkscript.ldt by FMCreateLinkLibraries
 * Using Freemarker v2.3.23
 * MCUXpresso IDE v10.3.0 [Build 2200] [2018-12-03] on 2019/01/26 23:13:33
 */

MEMORY
{
  /* Define each memory region */
  Bootloader (rx) : ORIGIN = 0x0, LENGTH = 0x380
  MFlash16 (rx)   : ORIGIN = 0x380, LENGTH = 0x3C00
  RamLoc2 (rwx)   : ORIGIN = 0x10000000, LENGTH = 0x800 /* 2K bytes (alias RAM) */
}

/* Define a symbol for the top of each memory region */
__base_MFlash16 = 0x0  ; /* MFlash16 */
__base_Flash = 0x0 ; /* Flash */
__top_MFlash16 = 0x0 + 0x3f80 ; /* 16256 bytes */
__top_Flash = 0x0 + 0x3f80 ; /* 16256 bytes */
__base_RamLoc2 = 0x10000000  ; /* RamLoc2 */
__base_RAM = 0x10000000 ; /* RAM */
__top_RamLoc2 = 0x10000000 + 0x800 ; /* 2K bytes */
__top_RAM = 0x10000000 + 0x800 ; /* 2K bytes */

ENTRY(bootloader_reset_handler)

SECTIONS
{
    /* Bootloader text section */
    .text_bootloader : ALIGN(4)
    {
        KEEP(*(.bootloader_vector*))
        KEEP(*(.bootloader1*))
        /* Code Read Protection data */
        . = 0x000002FC ;
        PROVIDE(__CRP_WORD_START__ = .) ;
        KEEP(*(.crp))
        PROVIDE(__CRP_WORD_END__ = .) ;
        ASSERT(!(__CRP_WORD_START__ == __CRP_WORD_END__), "Linker CRP Enabled, but no CRP_WORD provided within application");
        /* End of Code Read Protection */
        KEEP(*(.bootloader2*))
    } > Bootloader

    /* MAIN TEXT SECTION */
    .text ORIGIN(MFlash16) : ALIGN(4)
    {
        ASSERT(ABSOLUTE(.) == ORIGIN(MFlash16), "text does not start correctly");
        KEEP(*(.vectmark))
        /* Global Section Table */
        . = ALIGN(4) ;
        __section_table_start = .;
        __data_section_table = .;
        ASSERT(ABSOLUTE(__data_section_table) == ORIGIN(MFlash16) + 0x4, "User ISR vector marker is not at top of user flash");
        LONG(LOADADDR(.data));
        LONG(    ADDR(.data));
        LONG(  SIZEOF(.data));
        __data_section_table_end = .;
        __bss_section_table = .;
        LONG(    ADDR(.bss));
        LONG(  SIZEOF(.bss));
        __bss_section_table_end = .;
        __section_table_end = . ;
        /* End of Global Section Table */

        *(.after_vectors*)
    } > MFlash16

    .text : ALIGN(4)
    {
       *(.text*)
       *(.rodata .rodata.* .constdata .constdata.*)
       . = ALIGN(4);
    } > MFlash16
    /*
     * for exception handling/unwind - some Newlib functions (in common
     * with C++ and STDC++) use this. 
     */
    .ARM.extab : ALIGN(4) 
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > MFlash16

    __exidx_start = .;

    .ARM.exidx : ALIGN(4)
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > MFlash16
    __exidx_end = .;
 
    _etext = .;

    /* MAIN DATA SECTION */
    .uninit_RESERVED : ALIGN(4)
    {
        KEEP(*(.bss.$RESERVED*))
        . = ALIGN(4) ;
        _end_uninit_RESERVED = .;
    } > RamLoc2

    /* Main DATA section (RamLoc2) */
    .data : ALIGN(4)
    {
       FILL(0xff)
       _data = . ;
       *(vtable)
       *(.ramfunc*)
       *(.data*)
       . = ALIGN(4) ;
       _edata = . ;
    } > RamLoc2 AT>MFlash16

    /* MAIN BSS SECTION */
    .bss : ALIGN(4)
    {
        _bss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4) ;
        _ebss = .;
        PROVIDE(end = .);
    } > RamLoc2

    /* DEFAULT NOINIT SECTION */
    .noinit (NOLOAD): ALIGN(4)
    {
        _noinit = .;
        *(.noinit*) 
         . = ALIGN(4) ;
        _end_noinit = .;
    } > RamLoc2
    PROVIDE(_pvHeapStart = DEFINED(__user_heap_base) ? __user_heap_base : .);
    PROVIDE(_vStackTop = DEFINED(__user_stack_top) ? __user_stack_top : __top_RamLoc2 - 0);

    /* ## Create checksum value (used in startup) ## */
    PROVIDE(__valid_user_code_checksum = 0 - 
                                         (_vStackTop 
                                         + (ResetISR + 1) 
                                         + (( DEFINED(NMI_Handler) ? NMI_Handler : M0_NMI_Handler ) + 1) 
                                         + (( DEFINED(HardFault_Handler) ? HardFault_Handler : M0_HardFault_Handler ) + 1) 
                                         )
           );
    PROVIDE(bootloader_checksum = 0 - (_vStackTop
                                    + (bootloader_reset_handler + 1)
                                    + (bootloader_reboot + 1)
                                    + (bootloader_reboot + 1)));

    /* Provide basic symbols giving location and size of main text
     * block, including initial values of RW data sections. Note that
     * these will need extending to give a complete picture with
     * complex images (e.g multiple Flash banks).
     */
    _image_start = LOADADDR(.text);
    _image_end = LOADADDR(.data) + SIZEOF(.data);
    _image_size = _image_end - _image_start;
}
