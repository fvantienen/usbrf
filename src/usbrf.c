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

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/desig.h>
#define SW_VERSION 1000

/* Load the modules */
#include "modules/config.h"
#include "modules/led.h"
#include "modules/spi.h"
#include "modules/button.h"
#include "modules/timer.h"
#include "modules/cdcacm.h"
#include "modules/counter.h"
#include "modules/ant_switch.h"
#include "modules/cyrf6936.h"
#include "modules/cc2500.h"
#include "modules/console.h"
#include "modules/pprzlink.h"
#include "modules/protocol.h"

static void msg_req_info_cb(uint8_t *data);

int main(void) {
	// Setup the clock
#ifndef BOARD_V2_0
	rcc_clock_setup_in_hse_12mhz_out_72mhz();
#else
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
#endif

	// Initialize the modules
	config_init();
	led_init();
	timer_init();
	cdcacm_init();
	button_init();
	counter_init();
	ant_switch_init();
	spi_init();
	cyrf_init();
	cc_init();
	console_init();
	pprzlink_init();
	protocol_init();

	// Bind INFO callback
	pprzlink_register_cb(PPRZ_MSG_ID_REQ_INFO, msg_req_info_cb);

	/* The main loop */
	uint32_t start_ticks = 0;
	while (1) {
		if(start_ticks + 10 <= counter_get_ticks()) {
			start_ticks = counter_get_ticks();
			cyrf_run();
			cc_run();
			protocol_run();
		}
		else if(start_ticks + 8 >= counter_get_ticks()) {
			cdcacm_run();
			pprzlink_run();
			console_run();
		}
	}

	return 0;
}

/**
 * Receive information request
 */
static void msg_req_info_cb(uint8_t *data) {
	uint32_t hw_id[3];
	uint32_t board = BOARD_ID;
	uint32_t sw_version = SW_VERSION;
	desig_get_unique_id(hw_id);
	console_print("\r\nPPRZLINK connected (version: %d)", DL_REQ_INFO_version(data));

	// Send the information back
	pprz_msg_send_INFO(&pprzlink.tp.trans_tx, &pprzlink.dev, 1, &board, &sw_version, 3, hw_id);
}