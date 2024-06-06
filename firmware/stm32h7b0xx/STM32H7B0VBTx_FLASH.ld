/*
******************************************************************************
**

**  File        : LinkerScript.ld
**
**  Author		: STM32CubeMX
**
**  Abstract    : Linker script for STM32H7B0VBTx series
**                128Kbytes FLASH and 1216Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed “as is,” without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** <h2><center>&copy; COPYRIGHT(c) 2019 STMicroelectronics</center></h2>
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**   1. Redistributions of source code must retain the above copyright notice,
**      this list of conditions and the following disclaimer.
**   2. Redistributions in binary form must reproduce the above copyright notice,
**      this list of conditions and the following disclaimer in the documentation
**      and/or other materials provided with the distribution.
**   3. Neither the name of STMicroelectronics nor the names of its contributors
**      may be used to endorse or promote products derived from this software
**      without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
*****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);    /* end of RAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x2000;  /* required amount of heap  */
_Min_Stack_Size = 0x4000; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
ITCMRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
AXISRAM (xrw) : ORIGIN = 0x24000000, LENGTH = 1024K
AHBSRAM (xrw) : ORIGIN = 0x30000000, LENGTH = 128K
SRDSRAM (xrw) : ORIGIN = 0x38000000, LENGTH = 32K
BKPRAM  (xrw) : ORIGIN = 0x38800000, LENGTH = 4K
/* Use first 96k of flash for the firmware */
FLASH (xr)    : ORIGIN = 0x08000000, LENGTH = 128K - 32K
}

/* Define output sections */
SECTIONS
{
  /* Used by the startup to load vector into ITCMRAM */
  _sivect = LOADADDR(.isr_vector);

  /* The startup code goes first into FLASH and ITCMRAM */
  .isr_vector :
  {
    . = ALIGN(4);
    _svect = .;          /* define a global symbol at vector start */
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
    _evect = .;         /* define a global symbols at end of vector */
  } >ITCMRAM AT> FLASH

  /* The program code or data that should only go into FLASH */
  .flash :
  {
    . = ALIGN(4);
    *(.flash)
    *(.flash*)
    *(.text.Reset_Handler)
    *(.text.Default_Handler)
    *(.text.SystemInit)
    . = ALIGN(4);
  } >FLASH

  /* Used by the startup to load code into ITCMRAM */
  _sitext = LOADADDR(.text);

  /* The program code and other data goes into FLASH and ITCMRAM */
  .text :
  {
    . = ALIGN(4);
    _stext = .;        /* define a global symbol at code start */
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >ITCMRAM AT> FLASH

  /* Uninitialized data section, not initialized to zero */
  /* goes first into DTCMRAM */
  .uninit (NOLOAD) :
  {
    . = ALIGN(4);
    *(SORT(.uninit.*))  /* .uninit.* sections */
    *(.uninit*)         /* .uninit* sections */
    . = ALIGN(4);
  } >DTCMRAM

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.rodata);

  /* Constant data goes into FLASH and DTCMRAM */
  .rodata :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >DTCMRAM AT> FLASH

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >DTCMRAM AT> FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >DTCMRAM AT> FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >DTCMRAM AT> FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >DTCMRAM AT> FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >DTCMRAM AT> FLASH

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >DTCMRAM AT> FLASH


  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >DTCMRAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >DTCMRAM

  /* SRAM1 section
  * Used as an uninitialized data section
  */
  .sram1 (NOLOAD) :
  {
    . = ALIGN(4);
    *(SORT(.sram1.*))
    *(.sram1*)

    . = ALIGN(4);
  } >AXISRAM

  /* SRAM2 section
  * Used as an uninitialized data section
  */
  .sram2 (NOLOAD) :
  {
    . = ALIGN(4);
    *(SORT(.sram2.*))
    *(.sram2*)

    . = ALIGN(4);
  } >AHBSRAM

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
