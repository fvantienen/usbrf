/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2018 Freek van Tienen <freek.v.tienen@gmail.com>
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cc_scanner.h"
#include "modules/led.h"
#include "modules/config.h"
#include "modules/timer.h"
#include "modules/counter.h"
#include "modules/ant_switch.h"
#include "modules/cc2500.h"
#include "modules/pprzlink.h"
#include "modules/console.h"
#include "helper/frsky.h"

/* Main protocol functions */
static void protocol_cc_scanner_init(void);
static void protocol_cc_scanner_deinit(void);
static void protocol_cc_scanner_start(void);
static void protocol_cc_scanner_stop(void);
static void protocol_cc_scanner_run(void);
static void protocol_cc_scanner_status(void);
static void protocol_cc_scanner_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len);

/* Main protocol structure */
struct protocol_t protocol_cc_scanner = {
	.name = "CC2500 Scanner",
	.init = protocol_cc_scanner_init,
	.deinit = protocol_cc_scanner_deinit,
	.start = protocol_cc_scanner_start,
	.stop = protocol_cc_scanner_stop,
	.run = protocol_cc_scanner_run,
	.status = protocol_cc_scanner_status,
	.parse_arg = protocol_cc_scanner_parse_arg
};

/* Internal functions */
static void protocol_cc_scanner_timer(void);
static void protocol_cc_scanner_receive(uint8_t len);
static void protocol_cc_scanner_next(void);

/* Internal variables */
static enum frsky_protocol_t frsky_protocol = FRSKYX_EU;						/**< The current FrSky protocol used */
static uint8_t *cc_scan_args = NULL;																/**< CHAN, FSCTRL0 */
static uint16_t cc_scan_args_len = 0;
static uint16_t cc_scan_idx = 0;
static uint8_t last_chan_num = 0;

/**
 * Configure the CC2500 chip and antenna switcher
 */
static void protocol_cc_scanner_init(void) {
	uint8_t mfg_id[2];
	// Stop the timer
	timer1_stop();

#ifdef CC_DEV_ANT
	// Switch the antenna to the CC2500
	bool ant_state[] = CC_DEV_ANT;
#ifdef CLOSEBY_SCAN
	ant_state[0] = !ant_state[0];
	ant_state[1] = !ant_state[1];
#endif
	ant_switch(ant_state);
#endif

	// Read the CC2500 MFG and copy from the config
	cc_strobe(CC2500_SIDLE);
	cc_get_mfg_id(mfg_id);
	cc_set_mode(CC2500_TXRX_RX);
	last_chan_num = cc_read_register(CC2500_CHANNR);

	// Set the callbacks
	timer1_register_callback(protocol_cc_scanner_timer);
	cc_register_recv_callback(protocol_cc_scanner_receive);
	cc_register_send_callback(NULL);

	console_print("\r\nCC Scanner initialized 0x%02X 0x%02X", mfg_id[0], mfg_id[1]);
}

/**
 * Deinitialize the variables
 */
static void protocol_cc_scanner_deinit(void) {
	timer1_register_callback(NULL);
	cc_register_recv_callback(NULL);

	if(cc_scan_args != NULL) {
		free(cc_scan_args);
		cc_scan_args = NULL;
		cc_scan_args_len = 0;
	}
	console_print("\r\nCC Scanner deinitialized");
}

/**
 * Configure the CC2500 and start scanning
 */
static void protocol_cc_scanner_start(void) {
	cc_scan_idx = 0;
	
	cc_strobe(CC2500_SIDLE);
	frsky_set_config(frsky_protocol);
	cc_write_register(CC2500_MCSM0, 0x18); // Enable auto tuning
	cc_write_register(CC2500_PKTCTRL1, CC2500_PKTCTRL1_APPEND_STATUS | CC2500_PKTCTRL1_CRC_AUTOFLUSH); // Disable address checks
	cc_write_register(CC2500_CHANNR, cc_scan_args[0]);
	cc_write_register(CC2500_FSCTRL0, config.cc_fsctrl0 + cc_scan_args[1]);
	cc_strobe(CC2500_SFRX);
	cc_strobe(CC2500_SRX);
	timer1_set(FRSKY_SEND_TIME * (FRSKYX_USED_CHAN+1));

	console_print("\r\nCC Scanner started %d...", frsky_protocol);
}

/**
 * Stop the timer
 */
static void protocol_cc_scanner_stop(void) {
	// Stop the timer
	cc_strobe(CC2500_SIDLE);
	timer1_stop();
	console_print("\r\nCC Scanner stopped...");
}

/**
 * In main loop running function
 */
static void protocol_cc_scanner_run(void) {

}

/**
 * Print the status of the scanner
 */
static void protocol_cc_scanner_status(void) {
	console_print("\r\n\tScanning at index %d at channel %d [%d]", cc_scan_idx, cc_scan_args[cc_scan_idx*2], cc_scan_args[cc_scan_idx*2+1]);
}

/**
 * Parse arguments given to the scanner
 */
static void protocol_cc_scanner_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len) {
	// Only parse arguments when starting
	if(type != PROTOCOL_START)
		return;

	// Allocate the arguments if needed
	if(cc_scan_args == NULL) {
		cc_scan_args = malloc(tot_len-1);
		cc_scan_args_len = tot_len-1;
	}

	// Reallocate if need
	if(cc_scan_args_len < tot_len-1) 
		cc_scan_args = realloc(cc_scan_args, tot_len-1);

	// Save the protocol type
	uint8_t arg_offset = 0;
	if(offset == 0) {
		frsky_protocol = arg[0];
		arg_offset = 1;
	} else {
		offset--;
	}

	// Save the arguments scanning list
	cc_scan_args_len = tot_len-1;
	memcpy(cc_scan_args + offset, arg + arg_offset, len - arg_offset);
}


static void protocol_cc_scanner_timer(void) {
	protocol_cc_scanner_next();
	timer1_set(FRSKY_SEND_TIME * (FRSKYX_USED_CHAN+1));
}

static void protocol_cc_scanner_receive(uint8_t len) {
	static uint8_t packet_len = 0;
	//static uint32_t ticks = 0;

	/* Check if we receieved a packet length */
	if(packet_len == 0) {
		//ticks = counter_status.ticks;
		cc_read_data(&packet_len, 1);
		len--;
	}

	/* Check if we received a full packet */
	if(len < packet_len+2)
		return;

	uint8_t packet[packet_len+5];
	packet[0] = packet_len;
	cc_read_data(&packet[1], packet_len+2);

	packet[packet_len+3] = cc_scan_args[cc_scan_idx*2];
	packet[packet_len+4] = cc_scan_args[cc_scan_idx*2 + 1];

	uint8_t chip_id = 1;
	pprz_msg_send_RECV_DATA(&pprzlink.tp.trans_tx, &pprzlink.dev, 1, &chip_id, packet_len+5, packet);

	packet_len = 0;
	LED_TOGGLE(LED_RX);
	cc_strobe(CC2500_SIDLE);
	cc_strobe(CC2500_SFRX);
	cc_strobe(CC2500_SRX);
}

/**
 * Go to the next channel for scanning
 */
static void protocol_cc_scanner_next(void) {
	cc_scan_idx = (cc_scan_idx+1) % (cc_scan_args_len/2);
	cc_strobe(CC2500_SIDLE);
	cc_write_register(CC2500_CHANNR, cc_scan_args[cc_scan_idx*2]);
	cc_write_register(CC2500_FSCTRL0, config.cc_fsctrl0 + cc_scan_args[cc_scan_idx*2 + 1]);
	cc_strobe(CC2500_SFRX);
	cc_strobe(CC2500_SRX);
}