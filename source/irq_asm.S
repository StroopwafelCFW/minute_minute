/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "latte.h"
//#include "irq.h"

.globl v_irq
.globl irq_setup_stack
.globl irq_kill
.globl irq_restore
.extern __irqstack_addr
.extern irq_handler
.extern irq_deferred_run

@ Frame of the interrupted context, saved on the System stack while
@ deferred work runs (see irq_defer_start)
#define FRAME_SP    (13*4)
#define FRAME_LR    (14*4)
#define FRAME_PC    (15*4)
#define FRAME_CPSR  (16*4)
#define FRAME_SIZE  (18*4)

irq_setup_stack:
    @ Switch to IRQ mode
    mrs r0, cpsr
    msr cpsr_c, #0xd2

    @ Setup interrupt stack
    ldr sp, =__irqstack_addr

    @ Restore mode
    msr cpsr_c, r0
    bx  lr

v_irq:
    @ r4 is only pushed to keep sp 8-byte aligned (AAPCS) for the C call
    push    {r0-r4, r9, r12, lr}

    mrs     r0, spsr
    blx     irq_handler

    @ Non-zero: run deferred work before resuming the interrupted code
    cmp     r0, #0
    bne     irq_defer_start

    pop     {r0-r4, r9, r12, lr}
    subs    pc, lr, #4

@ Save the interrupted (System mode) context on its own stack, run
@ irq_deferred_run in System mode with IRQs enabled, then restore the
@ context and resume as if returning from the original IRQ.
@ r4-r8, r10, r11 still hold the interrupted values, as irq_handler
@ preserved them. The rest is on the IRQ stack.
irq_defer_start:
    ldr     r2, [sp, #28]
    sub     r2, r2, #4              @ interrupted pc
    mrs     r3, spsr                @ interrupted cpsr
    mov     r0, sp
    @ Free the IRQ stack now, its contents stay valid while IRQs are off
    add     sp, sp, #32

    @ Switch to System mode, IRQs/FIQs still off
    msr     cpsr_c, #0xdf
    mov     r1, sp
    bic     sp, sp, #7
    sub     sp, sp, #FRAME_SIZE

    str     r1, [sp, #FRAME_SP]
    str     lr, [sp, #FRAME_LR]
    str     r2, [sp, #FRAME_PC]
    str     r3, [sp, #FRAME_CPSR]
    add     r1, sp, #(4*4)
    stmia   r1, {r4-r11}
    ldr     r1, [r0, #20]           @ r9
    str     r1, [sp, #(9*4)]
    ldr     r1, [r0, #24]           @ r12
    str     r1, [sp, #(12*4)]
    ldmia   r0, {r0-r3}
    stmia   sp, {r0-r3}

    @ Back to the interrupted mode and mask bits (IRQs enabled)
    ldr     r0, [sp, #FRAME_CPSR]
    bic     r0, r0, #0x20           @ never set T through msr
    msr     cpsr_c, r0

    bl      irq_deferred_run

    @ IRQs off, then restore through IRQ mode as System mode has no SPSR
    msr     cpsr_c, #0xdf
    mov     r0, sp
    msr     cpsr_c, #0xd2
    ldr     r1, [r0, #FRAME_CPSR]
    msr     spsr_cxsf, r1
    ldr     lr, [r0, #FRAME_PC]
    add     r1, r0, #FRAME_SP
    ldmia   r1, {sp, lr}^           @ System mode sp/lr, pops the frame
    nop                             @ no banked register access right after ldm ^
    ldmia   r0, {r0-r12}
    movs    pc, lr

irq_kill:
    mrs     r1, cpsr
    and     r0, r1, #(0x80|0x40)
    orr     r1, r1, #(0x80|0x40)
    msr     cpsr_c, r1
    bx      lr

irq_restore:
    mrs     r1, cpsr
    bic     r1, r1, #(0x80|0x40)
    orr     r1, r1, r0
    msr     cpsr_c, r1
    bx      lr

