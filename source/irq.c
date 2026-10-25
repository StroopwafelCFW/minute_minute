/*
 *  minute - a port of the "mini" IOS replacement for the Wii U.
 *
 *  Copyright (C) 2016          SALT
 *  Copyright (C) 2016          Daz Jones <daz@dazzozo.com>
 *
 *  Copyright (C) 2008, 2009    Hector Martin "marcan" <marcan@marcansoft.com>
 *  Copyright (C) 2008, 2009    Sven Peter <svenpeter@gmail.com>
 *  Copyright (C) 2009          Andre Heider "dhewg" <dhewg@wiibrew.org>
 *
 *  This code is licensed to you under the terms of the GNU GPL, version 2;
 *  see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#include "irq.h"
#include "latte.h"
#include "gfx.h"
#include "utils.h"
#include "crypto.h"
#include "nand.h"
#include "sdcard.h"
#include "mlc.h"
#include "serial.h"

static u32 _alarm_frequency = 0;

static irq_work_fn _work_fn[32];
static irq_work_blocked_fn _work_blocked[32];
static volatile u32 _work_pending = 0;
static volatile int _work_running = 0;

void irq_setup_stack(void);
void irq_deferred_run(void);

void irq_initialize(void)
{
    irq_setup_stack();
    write32(LT_INTMR_AHBALL_ARM, 0);
    write32(LT_INTSR_AHBALL_ARM, 0xffffffff);
    write32(LT_INTMR_AHBLT_ARM, 0);
    write32(LT_INTSR_AHBLT_ARM, 0xffffffff);
    irq_restore(CPSR_FIQDIS);

    //???
    write32(LT_INTMR_AHBALL_ARM2X, 0);
    write32(LT_INTMR_AHBLT_ARM2X, 0);
    write32(LT_ERROR_MASK, 0);

    write32(LT_ALARM, 0);
}

void irq_shutdown(void)
{
    write32(LT_INTMR_AHBALL_ARM, 0);
    write32(LT_INTSR_AHBALL_ARM, 0xffffffff);
    write32(LT_INTMR_AHBLT_ARM, 0);
    write32(LT_INTSR_AHBLT_ARM, 0xffffffff);
    irq_kill();
}

void irq_defer_register(u32 work, irq_work_fn fn, irq_work_blocked_fn blocked)
{
    _work_fn[work] = fn;
    _work_blocked[work] = blocked;
}

void irq_defer(u32 work)
{
    u32 cookie = irq_kill();
    _work_pending |= 1 << work;
    irq_restore(cookie);
}

void irq_defer_cancel(u32 work)
{
    u32 cookie = irq_kill();
    _work_pending &= ~(1 << work);
    irq_restore(cookie);
}

// pending work that may run now, call with IRQs disabled
static u32 irq_deferred_runnable(void)
{
    u32 runnable = 0;
    for (u32 pending = _work_pending; pending; pending &= pending - 1) {
        int i = __builtin_ctz(pending);
        if (!_work_blocked[i] || !_work_blocked[i]())
            runnable |= 1 << i;
    }
    return runnable;
}

// Called from v_irq in System mode with IRQs enabled, or from irq_deferred_poll
void irq_deferred_run(void)
{
    _work_running = 1;
    for (;;) {
        u32 cookie = irq_kill();
        u32 runnable = irq_deferred_runnable();
        if (!runnable) {
            _work_running = 0;
            irq_restore(cookie);
            return;
        }
        int i = __builtin_ctz(runnable);
        _work_pending &= ~(1 << i);
        irq_restore(cookie);

        _work_fn[i]();
    }
}

// Run work that was blocked when its IRQ fired. Call when the blocking
// condition ends.
void irq_deferred_poll(void)
{
    u32 cpsr = get_cpsr();
    // only from plain System mode code that isn't in an IRQ-off section
    if ((cpsr & 0x1f) != 0x1f || (cpsr & CPSR_IRQDIS))
        return;
    if (_work_running || !_work_pending)
        return;
    irq_deferred_run();
}

// Returns non-zero to make v_irq run irq_deferred_run before resuming
int irq_handler(u32 spsr)
{
#ifdef MINUTE_BOOT1
    // somehow needed on WUP-50 when loaded from UDPIH
    udelay(10);
#endif
    u32 all_enabled = read32(LT_INTMR_AHBALL_ARM);
    u32 all_flags = read32(LT_INTSR_AHBALL_ARM);
    u32 all_mask = all_enabled & all_flags;

    u32 lt_enabled = read32(LT_INTMR_AHBLT_ARM);
    u32 lt_flags = read32(LT_INTSR_AHBLT_ARM);
    u32 lt_mask = lt_enabled & lt_flags;

    /*printf("In IRQ handler: ALL (0x%08x 0x%08x 0x%08x) LT (0x%08x 0x%08x 0x%08x)\n",
            all_enabled, all_flags, all_mask, lt_enabled, lt_flags, lt_mask);*/

    if(all_mask & IRQF_TIMER) {
        if (_alarm_frequency)
            write32(LT_ALARM, read32(LT_TIMER) + _alarm_frequency);

        write32(LT_INTSR_AHBALL_ARM, IRQF_TIMER);
    }

    if(all_mask & IRQF_NAND) {
//      printf("IRQ: NAND\n");
        write32(NAND_CTRL, 0x7fffffff); // shut it up
        write32(LT_INTSR_AHBALL_ARM, IRQF_NAND);
        nand_irq();
    }
    /*
    if(all_mask & IRQF_GPIO1B) {
//      printf("IRQ: GPIO1B\n");
        write32(HW_GPIO1BINTFLAG, 0xFFFFFF); // shut it up
        write32(LT_INTSR_AHBALL_ARM, IRQF_GPIO1B);
    }
    if(all_mask & IRQF_GPIO1) {
//      printf("IRQ: GPIO1\n");
        write32(HW_GPIO1INTFLAG, 0xFFFFFF); // shut it up
        write32(LT_INTSR_AHBALL_ARM, IRQF_GPIO1);
    }
    if(all_mask & IRQF_RESET) {
//      printf("IRQ: RESET\n");
        write32(LT_INTSR_AHBALL_ARM, IRQF_RESET);
    }*/
    if(all_mask & IRQF_SHA1) {
        printf("IRQ: SHA1\n");
        write32(LT_INTSR_AHBALL_ARM, IRQF_SHA1);
    }
    if(all_mask & IRQF_AES) {
//      printf("IRQ: AES\n");
        write32(LT_INTSR_AHBALL_ARM, IRQF_AES);
    }
    if(all_mask & IRQF_SD0) {
//      printf("IRQ: SD0\n");
        sdcard_irq();
        write32(LT_INTSR_AHBALL_ARM, IRQF_SD0);
    }

    // Technically it shouldn't even be here...
    if(lt_mask & IRQLF_IOP2X) {
        write32(LT_INTSR_AHBLT_ARM, IRQLF_IOP2X);
    }

    all_mask &= ~IRQF_ALL;
    if(all_mask) {
        printf("IRQ: ALL unknown 0x%08x\n", all_mask);
        write32(LT_INTSR_AHBALL_ARM, all_mask);
    }
    lt_mask &= ~IRQLF_ALL;
    if(lt_mask) {
        printf("IRQ: LT unknown 0x%08x\n", lt_mask);
        write32(LT_INTSR_AHBLT_ARM, lt_mask);
    }

    // Only redirect plain System mode code, never an exception handler
    if (!_work_pending || _work_running || (spsr & 0x1f) != 0x1f)
        return 0;
    return irq_deferred_runnable() != 0;
}

void irq_enable(u32 irq)
{
    set32(LT_INTMR_AHBALL_ARM, 1<<irq);
}

void irql_enable(u32 irq)
{
    set32(LT_INTMR_AHBLT_ARM, 1<<irq);
}

void irq_disable(u32 irq)
{
    clear32(LT_INTMR_AHBALL_ARM, 1<<irq);
}

void irql_disable(u32 irq)
{
    clear32(LT_INTMR_AHBLT_ARM, 1<<irq);
}

void irq_set_alarm(u32 ms, u8 enable)
{
    _alarm_frequency = IRQ_ALARM_MS2REG(ms);

    if (enable)
        write32(LT_ALARM, read32(LT_TIMER) + _alarm_frequency);
}

void irq_wait(void)
{
    u32 data = 0;
    __asm__ volatile ( "mcr p15, 0, %0, c7, c0, 4" : : "r" (data) );
}
