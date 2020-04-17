/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (ARM startup code).
 *
 *  CT800/NGPlay is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  CT800/NGPlay is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CT800/NGPlay. If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*the boot file. performs the following actions:
- declares the stack
- puts the (minimalist) interrupt vector table to the ROM
- checks the RAM
- copies the .data section to RAM
- initialises BSS and stack to zero
*/

#include <stdint.h>
#include <stddef.h>
#include "ctdefs.h"
#include "arm_driver.h"

/*some function prototypes*/
static void Boot_Reset_Handler(void);
static void Boot_Default_Handler(void);
static void Boot_Reset_Handler(void);
extern void Hw_Fault_Exception_Handler(void);
extern void Hw_SysTick_Handler(void);
extern int  main(int argc, char **argv);

/*---------- external variables ----------*/
/*-- READ-ONLY  --*/
/*from the .ld linker script*/
/*the .data section*/
extern uint32_t _start_of_initdata; /*in the ROM*/
extern uint32_t _start_of_data;     /*to the RAM*/
extern uint32_t _end_of_data;
/*bss section in SRAM is initialised during the RAM test*/
/*ccm RAM / SRAM*/
extern uint32_t _start_of_ram;
extern uint32_t _start_of_ccm;
extern uint32_t _end_of_ram;
extern uint32_t _end_of_ccm;

/*-------- global exported stack ---**********-------*/
#define STACK_SIZE       0x00002100UL
/*in uint32_t, so multiply by four for the bytes: 33 kb*/
__attribute__ ((used,section(".ct_stack"))) uint32_t ct_stack[STACK_SIZE];


/*-------- global exported interrupt table ----------*/
/*minimalist interrupt table - only stack pointer, reset,
hardware faults and the systick are used*/
__attribute__ ((used,section(".isr_vector"))) void (* const cm4_vectors[])(void) =
{
    /* exceptions*/
    (void *)&ct_stack[STACK_SIZE],  /*Initial stack pointer*/
    Boot_Reset_Handler,             /*Reset*/
    Hw_Fault_Exception_Handler,     /*NMI*/
    Hw_Fault_Exception_Handler,     /*HardFault*/
    Hw_Fault_Exception_Handler,     /*MemManage*/
    Hw_Fault_Exception_Handler,     /*BusFault*/
    Hw_Fault_Exception_Handler,     /*UsageFault*/
    0UL,                            /*Reserved*/
    0UL,
    0UL,
    0UL,
    Boot_Default_Handler,           /*SVCall*/
    Boot_Default_Handler,           /*Debug Monitor*/
    0UL,                            /*Reserved*/
    Boot_Default_Handler,           /*PendSV*/
    Hw_SysTick_Handler,             /*SysTick*/

    /*interrupts - none used.
      82 entries as per the STM32F405xx reference manual.*/

    /*0-9*/
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,

    /*10-19*/
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,

    /*20-29*/
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,

    /*30-39*/
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,

    /*40-49*/
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,

    /*50-59*/
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,

    /*60-69*/
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,

    /*70-79*/
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,
    Boot_Default_Handler,

    /*80-81*/
    Boot_Default_Handler,
    Boot_Default_Handler
};

/*endless loop - the watchdog will bite anyway.*/
static void __attribute__((used)) Boot_Default_Handler(void)
{
    for (;;) ;
}

void __attribute__((used)) Boot_Reset_Handler(void)
{
    /*relocate interrupt table*/
    SCB_VTOR = FLASH_BASE_ADDR;

    SCB_CCR = SCB_CCR_STKALIGN | SCB_CCR_DIV0_TRP;

    /*with the RAM test, the system frequency must already be set to maximum
      because that is more demanding for the RAM. Besides, the test will take
      much less time.

      this C function is called before even the RAM is properly initialised,
      but the whole chain does not use any globals, and the stack pointer has
      been set up via the linker file in the first four bytes of the resulting
      hex file.

      store the result of the sys speed setting - 0 means OK, but since the
      stack RAM is initialised to 0, too, this could mean that in case of a
      programming mistake, the system would misread the init-0 as OK-0.
      ct_stack[1] is just a dummy here because the system core clock gets set
      up in hardware_arm.c / Hw_System_Setup() anyway.*/
    *ct_stack = Drv_Set_Sys_Speed_Type(SYS_SPEED_168MHZ, ct_stack + 1) + 0x00000021UL;

    /*Test the RAM and store the error counter in *CCM_Start .
      The whole RAM area is first filled with TestInt, but not always with the same
      word. Between each write, the TestInt is rotated by 19 bits. This gives
      a periodicity of 19*32, and this doesn't fit in even binary boundaries.
      With always writing the same value, it cannot be detected if there is a
      problem with the address lines - i.e. you write data, read them back,
      but they have not been stored in the correct address. This will get
      caught with this algorithm.
      Afterwards, the whole thing is repeated with a binary inverted TestInt
      so that all bits have been tested with both 1 and 0.

      Besides, in the second read loop, the RAM is initialised to 0 to spare
      a separate zero init loop.

      The start/end addresses come from the linker script.*/

    __asm volatile ("\n"
    ".thumb_func\n"
    "RAM_OK         = 0x00000042UL\n"
    "TestInt        = 0x8F0E32A6UL\n"

    /*store the system speed result (XTAL error?) in R6.*/
    "LDR      R0, =ct_stack\n"
    "LDR      R6, [R0]\n"

    "LDR      R0, =_start_of_ram\n"
    "LDR      R1, =_end_of_ram\n"
    "LDR      R2, =TestInt\n"
    "LDR      R4, =RAM_OK\n"
    "MOV      R5, #0\n"

    "RAM_Fill_Loop_1:\n"
    "STR      R2, [R0], #4\n"
    "ROR      R2, R2, #19\n"
    "CMP      R0, R1\n"
    "BLO      RAM_Fill_Loop_1\n"

    "LDR      R0, =_start_of_ram\n"
    "LDR      R1, =_end_of_ram\n"
    "LDR      R2, =TestInt\n"

    "RAM_Read_Loop_1:\n"
    "LDR      R3, [R0]\n"
    "LDR      R3, [R0], #4\n"
    "CMP      R2, R3\n"
    "IT       NE\n"
    "ADDNE    R4, #1\n"
    "ROR      R2, #19\n"
    "CMP      R0, R1\n"
    "BLO      RAM_Read_Loop_1\n"

    "LDR      R0, =_start_of_ram\n"
    "LDR      R1, =_end_of_ram\n"
    "LDR      R2, =TestInt\n"
    "MVN      R2, R2\n"

    "RAM_Fill_Loop_2:\n"
    "STR      R2, [R0], #4\n"
    "ROR      R2, #19\n"
    "CMP      R0, R1\n"
    "BLO      RAM_Fill_Loop_2\n"

    "LDR      R0, =_start_of_ram\n"
    "LDR      R1, =_end_of_ram\n"
    "LDR      R2, =TestInt\n"
    "MVN      R2, R2\n"

    "RAM_Read_Loop_2:\n"
    "LDR      R3, [R0]\n"
    "LDR      R3, [R0]\n"
    /*the following instruction does the zero initialisation in SRAM.*/
    "STR      R5, [R0], #4\n"
    "CMP      R2, R3\n"
    "IT       NE\n"
    "ADDNE    R4, #1\n"
    "ROR      R2, #19\n"
    "CMP      R0, R1\n"
    "BLO      RAM_Read_Loop_2\n"

    "LDR      R0, =_start_of_ccm\n"
    "LDR      R1, =_end_of_ccm\n"
    "LDR      R2, =TestInt\n"

    "CCM_Fill_Loop_1:\n"
    "STR      R2, [R0], #4\n"
    "ROR      R2, #19\n"
    "CMP      R0, R1\n"
    "BLO      CCM_Fill_Loop_1\n"

    "LDR      R0, =_start_of_ccm\n"
    "LDR      R1, =_end_of_ccm\n"
    "LDR      R2, =TestInt\n"

    "CCM_Read_Loop_1:\n"
    "LDR      R3, [R0]\n"
    "LDR      R3, [R0], #4\n"
    "CMP      R2, R3\n"
    "IT       NE\n"
    "ADDNE    R4, #1\n"
    "ROR      R2, #19\n"
    "CMP      R0, R1\n"
    "BLO      CCM_Read_Loop_1\n"

    "LDR      R0, =_start_of_ccm\n"
    "LDR      R1, =_end_of_ccm\n"
    "LDR      R2, =TestInt\n"
    "MVN      R2, R2\n"

    "CCM_Fill_Loop_2:\n"
    "STR      R2, [R0], #4\n"
    "ROR      R2, #19\n"
    "CMP      R0, R1\n"
    "BLO      CCM_Fill_Loop_2\n"

    "LDR      R0, =_start_of_ccm\n"
    "LDR      R1, =_end_of_ccm\n"
    "LDR      R2, =TestInt\n"
    "MVN      R2, R2\n"

    "CCM_Read_Loop_2:\n"
    "LDR      R3, [R0]\n"
    "LDR      R3, [R0]\n"
    /*the following instruction does the zero initialisation in CCM.*/
    "STR      R5, [R0], #4\n"
    "CMP      R2, R3\n"
    "IT       NE\n"
    "ADDNE    R4, #1\n"
    "ROR      R2, #19\n"
    "CMP      R0, R1\n"
    "BLO      CCM_Read_Loop_2\n"

    /*store the RAM test result at the low end of the stack.*/
    "LDR      R0, =ct_stack\n"
    "STR      R4, [R0], #4\n"
    /*store the XTAL bootup test which is still in R6. Ends up at ct_stack[1].*/
    "STR      R6, [R0]\n"

    /*init the non-zero-initialised data. only for the SRAM since the
      CCM only gets zero initialised variables.*/
    "LDR      R0, =_start_of_initdata\n"
    "LDR      R1, =_start_of_data\n"
    "LDR      R2, =_end_of_data\n"
    "RAM_Init_Loop:\n"
    "LDR      R3, [R0], #4\n"
    "STR      R3, [R1], #4\n"
    "CMP      R1, R2\n"
    "BLO      RAM_Init_Loop\n"

    "\n" : : : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "cc", "memory");

    /*and here we go for the application.*/
    (void) main(0, NULL);

    /*main() should never return, and even if it did, the watchdog would bite
      in this endless loop.*/
    for (;;) ;
}
