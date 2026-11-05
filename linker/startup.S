#include <config.h>

.syntax unified
#if defined(CUBESAT_STM32N6)
.arch armv8.1-m.main
#elif defined(CUBESAT_STM32H7)
.cpu cortex-m7
#endif
.thumb

.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
    cpsid i
    movs r0, #0
#if defined(CUBESAT_STM32N6)
    msr msplim, r0
#endif
    msr control, r0
    isb
    ldr r0, =_estack
    msr msp, r0
#if defined(CUBESAT_STM32N6)
    ldr r0, =_sstack
    msr msplim, r0
#endif
    movs r0, #0
    msr basepri, r0
    msr faultmask, r0
    ldr r0, =vector_table
    ldr r1, =0xE000ED08
    str r0, [r1]
    dsb
    isb
    bl SystemInit
    b runtime_start
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
.global Default_Handler
.type Default_Handler, %function
.thumb_func
Default_Handler:
    b .
.size Default_Handler, .-Default_Handler

.macro core_vector handler
    .word \handler
    .weak \handler
    .thumb_set \handler, Default_Handler
.endm

.section .isr_vector,"a",%progbits
.balign 1024
.global vector_table
.type vector_table, %object
vector_table:
    .word _estack
    .word Reset_Handler
    core_vector NMI_Handler
    core_vector HardFault_Handler
    core_vector MemManage_Handler
    core_vector BusFault_Handler
    core_vector UsageFault_Handler
#if defined(CUBESAT_STM32N6)
    core_vector SecureFault_Handler
    .word 0, 0, 0
#elif defined(CUBESAT_STM32H7)
    .word 0, 0, 0, 0
#endif
    core_vector SVC_Handler
    core_vector DebugMon_Handler
    .word 0
    core_vector PendSV_Handler
    core_vector SysTick_Handler
#if defined(CUBESAT_STM32N6)
    .rept 195
#elif defined(CUBESAT_STM32H7)
    .rept 150
#endif
    .word Default_Handler
    .endr
.size vector_table, .-vector_table

.section .note.GNU-stack,"",%progbits
