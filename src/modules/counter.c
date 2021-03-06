/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2015 Piotr Esden-Tempski <piotr@esden.net>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This module is using the systick timer to provide timing functions to the
 * system.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/systick.h>

#include "counter.h"

/* Configuration */
#define COUNTER_FREQ 25000 /* Counter interval of 0.04ms */

/* Private variables. */

/* Status of the counter module. */
struct counter_status counter_status = {
  .init = false,
  .frequency = COUNTER_FREQ,
  .fine_frequency = 0, /* We don't know that yet. */
  .ticks = 0
};


/* Module function implementations. */

/**
 * Initialize the counter module.
 * DON'T use the counter in interrupts with a lower priority!
 */
void counter_init(void)
{
  
  /* Initialize status.
   * The first few fields should be initialized globally already, but we are
   * just making sure it is done.
   */
  counter_status.init = false;
  counter_status.frequency = COUNTER_FREQ;
  counter_status.fine_frequency = rcc_ahb_frequency / 8;
  counter_status.ticks = 0;

  /* Initialize the hardware.
   * XXX: Move to the HAL.
   */
  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
  systick_set_reload((rcc_ahb_frequency / 8) / COUNTER_FREQ - 1);
  nvic_set_priority(NVIC_SYSTICK_IRQ, 0);
  systick_interrupt_enable();
  systick_counter_enable();

  /* Mark that we are done initializing. */
  counter_status.init = true;
}

/* Interrupt handlers */
void sys_tick_handler(void)
{
  counter_status.ticks++;
}

/* Implement busy sleep for STM32 */
void _usleep(uint32_t x) {
  (void)x;
  __asm("mov r1, #24;"
       "mul r0, r0, r1;"
       "b _delaycmp;"
       "_delayloop:"
       "subs r0, r0, #1;"
       "_delaycmp:;"
       "cmp r0, #0;"
       " bne _delayloop;");
}