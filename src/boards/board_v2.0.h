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

#ifndef BOARD_V2_0_H_
#define BOARD_V2_0_H_

#define BOARD_ID 2

/* Define the LEDS (optional) */
#define LED_INV						1 							/**< Inverse the led states */
#define LED_BIND					NO_LED 					/**< Define the Binding led number */
#define LED_RX						1								/**< define the Receive led number */
#define LED_TX						NO_LED					/**< define the Transmit led number */

#define USE_LED_1					1								/**< If the board has the frist led */
#define LED_1_GPIO_PORT				GPIOA				/**< The 1 led GPIO port */
#define LED_1_GPIO_PIN				GPIO1				/**< The 1 led GPIO pin */
#define LED_1_GPIO_CLK				RCC_GPIOA		/**< The 1 led GPIO clock */

/* Define the BIND button (optional) */
//#define USE_BTN_BIND				1								/**< If the board has a bind button */
//#define BTN_BIND_GPIO_PORT			GPIOA				/**< The Bind button GPIO port */
//#define BTN_BIND_GPIO_PIN			GPIO0					/**< The Bind button GPIO pin */
//#define BTN_BIND_GPIO_CLK			RCC_GPIOA			/**< The Bind button GPIO clock */
//#define BTN_BIND_EXTI				EXTI0						/**< The Bind button EXTI for the interrupt */
//#define BTN_BIND_ISR				exti0_isr				/**< The Bind button ISR function for the interrupt */
//#define BTN_BIND_NVIC				NVIC_EXTI0_IRQ	/**< The Bind button NVIC for the interrupt */

/* Define the SPI busses */
#define __SPI(i, j) i ## _ ## j
#define _SPI(i, j)  __SPI(i, j)

#define _SPI2_BUS						SPI2 							/**< The SPI bus */
#define _SPI2_CLK						RCC_SPI2					/**< The SPI clock */
#define _SPI2_SCK_PORT			GPIOB							/**< The SPI SCK port */
#define _SPI2_SCK_PIN				GPIO13						/**< The SPI SCK pin */
#define _SPI2_MISO_PORT			GPIOB							/**< The SPI MISO port */
#define _SPI2_MISO_PIN			GPIO14						/**< The SPI MISO pin */
#define _SPI2_MOSI_PORT			GPIOB							/**< The SPI MOSI port */
#define _SPI2_MOSI_PIN			GPIO15						/**< The SPI MOSI pin */

/* Define the Antenna Switcher */
#define ANTENNA_SW1_PORT 		GPIOB							/**< The first antenna switcher port */
#define ANTENNA_SW1_PIN 		GPIO4							/**< The first antenna switcher pin */
#define ANTENNA_SW1_CLK 		RCC_GPIOB					/**< The first antenna switcher clock */
#define ANTENNA_SW2_PORT 		GPIOB							/**< The second antenna switcher port */
#define ANTENNA_SW2_PIN 		GPIO5							/**< The second antenna switcher pin */
#define ANTENNA_SW2_CLK 		RCC_GPIOB					/**< The second antenna switcher clock */

/* Define the CYRF6936 chip */
#define CYRF_DEV_SPI					_SPI2							/**< The SPI connection number */
#define CYRF_DEV_ANT					{true, true}			/**< The antenna switcher state */
#define CYRF_DEV_SS_PORT			GPIOB							/**< The SPI SS port */
#define CYRF_DEV_SS_PIN				GPIO12						/**< The SPI SS pin */
#define CYRF_DEV_RST_PORT			GPIOB							/**< The RST GPIO port*/
#define CYRF_DEV_RST_PIN			GPIO8							/**< The RST GPIO pin */
#define CYRF_DEV_RST_CLK			RCC_GPIOB					/**< The RST GPIO clock */

/* Define the CC2500 chip */
#define CC_DEV_SPI					_SPI2							/**< The SPI connection number */
#define CC_DEV_ANT					{false, true}			/**< The antenna switcher state */
#define CC_DEV_SS_PORT			GPIOB							/**< The SPI SS port */
#define CC_DEV_SS_PIN				GPIO6							/**< The SPI SS pin */

/* Define the DSM timer */
#define TIMER1					  TIM2							/**< The DSM timer */
#define TIMER1_NVIC				NVIC_TIM2_IRQ			/**< The DSM timer NVIC */
#define TIMER1_IRQ				tim2_isr					/**< The DSM timer function interrupt */


#endif /* BOARD_V2_0_H_ */
