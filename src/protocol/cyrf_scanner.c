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
#include "cyrf_scanner.h"
#include "modules/led.h"
#include "modules/config.h"
#include "modules/timer.h"
#include "modules/ant_switch.h"
#include "modules/cyrf6936.h"
#include "modules/pprzlink.h"
#include "modules/console.h"
#include "helper/dsm.h"

/* Main protocol functions */
static void protocol_cyrf_scanner_init(void);
static void protocol_cyrf_scanner_deinit(void);
static void protocol_cyrf_scanner_start(void);
static void protocol_cyrf_scanner_stop(void);
static void protocol_cyrf_scanner_run(void);
static void protocol_cyrf_scanner_status(void);
static void protocol_cyrf_scanner_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len);

/* Main protocol structure */
struct protocol_t protocol_cyrf_scanner = {
	.name = "CYRF6936 Scanner",
	.init = protocol_cyrf_scanner_init,
	.deinit = protocol_cyrf_scanner_deinit,
	.start = protocol_cyrf_scanner_start,
	.stop = protocol_cyrf_scanner_stop,
	.run = protocol_cyrf_scanner_run,
	.status = protocol_cyrf_scanner_status,
	.parse_arg = protocol_cyrf_scanner_parse_arg
};

/* Internal functions */
static void protocol_cyrf_scanner_timer(void);
static void protocol_cyrf_scanner_receive(bool error);
static void protocol_cyrf_scanner_next(void);

/* Internal variables */
static uint8_t *cyrf_scan_args = NULL;			//*< Channel, pn_row<<4 | pn_col
static uint16_t cyrf_scan_args_len = 0;
static uint16_t cyrf_scan_idx = 0;

/**
 * Configure the CYRF chip and antenna switcher
 */
static void protocol_cyrf_scanner_init(void) {
	uint8_t mfg_id[6];
	// Stop the timer
	timer1_stop();

#ifdef CYRF_DEV_ANT
	// Switch the antenna to the CYRF
	bool ant_state[] = CYRF_DEV_ANT;
#ifdef CLOSEBY_SCAN
	ant_state[0] = !ant_state[0];
	ant_state[1] = !ant_state[1];
#endif
	ant_switch(ant_state);
#endif

	// Configure the CYRF
	dsm_set_config();
	dsm_set_config_transfer();

	// Read the CYRF MFG and copy from the config
	cyrf_get_mfg_id(mfg_id);

	// Configure to accept only non empty CRC16
	cyrf_set_rx_override(CYRF_DIS_CRC0);

	// Set the callbacks
	timer1_register_callback(protocol_cyrf_scanner_timer);
	cyrf_register_recv_callback(protocol_cyrf_scanner_receive);
	cyrf_register_send_callback(NULL);

	console_print("\r\nCYRF Scanner initialized 0x%02X 0x%02X 0x%02X 0x%02X", mfg_id[0], mfg_id[1], mfg_id[2], mfg_id[3]);
}

/**
 * Deinitialize the variables
 */
static void protocol_cyrf_scanner_deinit(void) {
	timer1_register_callback(NULL);
	cyrf_register_recv_callback(NULL);

	if(cyrf_scan_args != NULL) {
		free(cyrf_scan_args);
		cyrf_scan_args = NULL;
		cyrf_scan_args_len = 0;
	}
	console_print("\r\nCYRF Scanner deinitialized");
}

/**
 * Configure the CYRF and start scanning
 */
static void protocol_cyrf_scanner_start(void) {
	// Start receiving and timer
	cyrf_scan_idx = 0;
	uint8_t channel = cyrf_scan_args[cyrf_scan_idx*2];
	uint8_t row_col = cyrf_scan_args[cyrf_scan_idx*2+1];
	uint8_t sop_col = row_col & 0xF;
	dsm_set_chan(channel, row_col >> 4, sop_col, (7 - sop_col), 0x0000);
	cyrf_start_recv();

	// Short or long time based on DSM2/DSMX channel
	if(channel%5 == row_col >> 4)
		timer1_set(DSM_RECV_TIME_A*1.5); // DSM2
	else
		timer1_set(DSM_RECV_TIME_A_SHORT*DSM_MAX_USED_CHANNELS); //DSMX
	console_print("\r\nCYRF Scanner started...");
}

/**
 * Stop the timer
 */
static void protocol_cyrf_scanner_stop(void) {
	// Stop the timer
	timer1_stop();

	// Abort the receive
	cyrf_set_mode(CYRF_MODE_SYNTH_RX, true);
	cyrf_write_register(CYRF_RX_ABORT, 0x00);
	console_print("\r\nCYRF Scanner stopped...");
}

/**
 * In main loop running function
 */
static void protocol_cyrf_scanner_run(void) {

}

/**
 * Print the status of the scanner
 */
static void protocol_cyrf_scanner_status(void) {
	uint8_t channel = cyrf_scan_args[cyrf_scan_idx*2];
	uint8_t row_col = cyrf_scan_args[cyrf_scan_idx*2+1];
	console_print("\r\n\tScanning at index %d at channel %d [%d, %d]", cyrf_scan_idx, channel, row_col >> 4, row_col & 0xF);
}

/**
 * Parse arguments given to the scanner
 */
static void protocol_cyrf_scanner_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len) {
	// Only parse arguments when starting
	if(type != PROTOCOL_START)
		return;

	// Allocate the arguments if needed
	if(cyrf_scan_args == NULL) {
		cyrf_scan_args = malloc(tot_len);
		cyrf_scan_args_len = tot_len;
	}

	// Reallocate if need
	if(cyrf_scan_args_len < tot_len) 
		cyrf_scan_args = realloc(cyrf_scan_args, tot_len);

	// Save the arguments
	cyrf_scan_args_len = tot_len;
	memcpy(cyrf_scan_args + offset, arg, len);
}


static void protocol_cyrf_scanner_timer(void) {
	// Abort the receive
	cyrf_abort_recv();

	// Goto the next channel
	protocol_cyrf_scanner_next();
	cyrf_start_recv();

	uint8_t channel = cyrf_scan_args[cyrf_scan_idx*2];
	uint8_t row_col = cyrf_scan_args[cyrf_scan_idx*2+1];
	//console_print("\r\nScanning at index %d at channel %d [%d, %d]", cyrf_scan_idx, channel, row_col >> 4, row_col & 0xF);

	if(channel%5 == row_col >> 4)
		timer1_set(DSM_RECV_TIME_A*1.5); // DSM2
	else
		timer1_set(DSM_RECV_TIME_A_SHORT*DSM_MAX_USED_CHANNELS); //DSMX
}

static void protocol_cyrf_scanner_receive(bool error) {
	uint8_t packet_length, packet[21], rx_status, i, rx_err;

	// Get the receive count, rx_status and the packet
	packet_length = cyrf_read_register(CYRF_RX_COUNT);
	rx_status = cyrf_get_rx_status();
	rx_err = rx_status & (CYRF_RX_ACK|CYRF_PKT_ERR|CYRF_EOP_ERR);
	cyrf_recv_len(&packet[1], packet_length);

	// Since we are only waiting for packets for DSM length, ignore the rest also invalid
	if(packet_length == 16 && (!error || !rx_err)) {
		// Get the CRC
		packet[17] = cyrf_read_register(CYRF_RX_CRC_LSB);
		packet[18] = cyrf_read_register(CYRF_RX_CRC_MSB);

		// Append the channel and length data
		packet[0] = packet_length;
		packet[19] = cyrf_scan_args[cyrf_scan_idx*2];
		packet[20] = cyrf_scan_args[cyrf_scan_idx*2+1];

		// Abort the receive
		cyrf_set_mode(CYRF_MODE_SYNTH_RX, true);
		cyrf_write_register(CYRF_RX_ABORT, 0x00);

		console_print("\r\nScanning at index %d", cyrf_scan_idx);
		console_print("\r\nGot packet (status: %02X, error: %d) [%d]: ", rx_status, error, packet_length);
		for(i = 0; i < packet_length+2; i++)
			console_print("%02X", packet[i]);;

		uint8_t chip_id = 0;
		pprz_msg_send_RECV_DATA(&pprzlink.tp.trans_tx, &pprzlink.dev, 1, &chip_id, packet_length+5, packet);

		LED_TOGGLE(LED_RX);
	}

	// Start receiving
	cyrf_start_recv();
}

/**
 * Go to the next channel for scanning
 */
static void protocol_cyrf_scanner_next(void) {
	cyrf_scan_idx = (cyrf_scan_idx+1) % (cyrf_scan_args_len/2);
	uint8_t channel = cyrf_scan_args[cyrf_scan_idx*2];
	uint8_t row_col = cyrf_scan_args[cyrf_scan_idx*2+1];
	uint8_t sop_col = row_col & 0xF;
	dsm_set_chan(channel, row_col >> 4, sop_col, (7 - sop_col), 0x0000);
}