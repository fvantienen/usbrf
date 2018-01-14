/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2013 Freek van Tienen <freek.v.tienen@gmail.com>
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

#ifndef MODULES_LED_H_
#define MODULES_LED_H_

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

// Include the board specifications for the leds
#include "../board.h"

/* Define the empty LED */
#define NO_LED 0
#define LED_0_GPIO_PORT				0
#define LED_0_GPIO_PIN				0

/* Control the leds from the board */
#define _(i)  i
#define LED_GPIO_PORT(i)	_(LED_ ## i ## _GPIO_PORT)
#define LED_GPIO_PIN(i)		_(LED_ ## i ## _GPIO_PIN)
#define LED_GPIO_CLK(i)		_(LED_ ## i ## _GPIO_CLK)

#ifdef LED_INV
#define LED_ON(i)		if(i > 0) gpio_set(LED_GPIO_PORT(i), LED_GPIO_PIN(i))
#define LED_OFF(i)	if(i > 0) gpio_clear(LED_GPIO_PORT(i), LED_GPIO_PIN(i))
#else
#define LED_ON(i)		if(i > 0) gpio_clear(LED_GPIO_PORT(i), LED_GPIO_PIN(i))
#define LED_OFF(i)	if(i > 0) gpio_set(LED_GPIO_PORT(i), LED_GPIO_PIN(i))
#endif
#define LED_TOGGLE(i)	if(i > 0) gpio_toggle(LED_GPIO_PORT(i), LED_GPIO_PIN(i))

#define LED_INIT(i) {                         \
	rcc_periph_clock_enable(LED_GPIO_CLK(i));		\
	gpio_set_mode(LED_GPIO_PORT(i),             \
				  GPIO_MODE_OUTPUT_50_MHZ,          	\
				  GPIO_CNF_OUTPUT_PUSHPULL,         	\
				  LED_GPIO_PIN(i));                 	\
	LED_OFF(i);																	\
}


/* External functions for the leds */
void led_init(void);

#endif /* MODULES_LED_H_ */
