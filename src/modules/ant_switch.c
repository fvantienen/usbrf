/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
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
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
 
#include "ant_switch.h"
#include "board.h"

/**
 * Initialize the antenna switcher
 */
void ant_switch_init(void) {
#ifdef ANTENNA_SW1_PORT
	rcc_periph_clock_enable(ANTENNA_SW1_CLK);
	gpio_set_mode(ANTENNA_SW1_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, ANTENNA_SW1_PIN);
	gpio_clear(ANTENNA_SW1_PORT, ANTENNA_SW1_PIN);
#endif

#ifdef ANTENNA_SW2_PORT
	rcc_periph_clock_enable(ANTENNA_SW2_CLK);
	gpio_set_mode(ANTENNA_SW2_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, ANTENNA_SW2_PIN);
	gpio_clear(ANTENNA_SW2_PORT, ANTENNA_SW2_PIN);
#endif
}

/**
 * Set the antenna switching pins
 */
void ant_switch(bool *state) {
	(void) state;
#ifdef ANTENNA_SW1_PORT
	if(state[0])
		gpio_set(ANTENNA_SW1_PORT, ANTENNA_SW1_PIN);
	else
		gpio_clear(ANTENNA_SW1_PORT, ANTENNA_SW1_PIN);
#endif

#ifdef ANTENNA_SW2_PORT
	if(state[1])
		gpio_set(ANTENNA_SW2_PORT, ANTENNA_SW2_PIN);
	else
		gpio_clear(ANTENNA_SW2_PORT, ANTENNA_SW2_PIN);
#endif
}