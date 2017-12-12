/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2015 Freek van Tienen <freek.v.tienen@gmail.com>
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
#include "dsm_hack.h"
#include "modules/config.h"
#include "modules/timer.h"
#include "modules/cyrf6936.h"
#include "modules/console.h"
#include "helper/dsm.h"

/* Main protocol structure */
struct protocol_t protocol_dsm_hack = {
	.name = "DSMH",
	.start = protocol_dsm_hack_start,
	.stop = protocol_dsm_hack_stop,
	.run = protocol_dsm_hack_run,
	.status = protocol_dsm_hack_status
};

static void protocol_dsm_hack_timer(void);
static void protocol_dsm_hack_receive(bool error);
static void protocol_dsm_hack_next(void);

static uint8_t channel = 0;
static bool dsm2 = false;
static uint8_t sop_col = 0;

/**
 * Configure the CYRF and start scanning
 */
void protocol_dsm_hack_start(void) {
	uint8_t mfg_id[6];
	console_print("\r\nDSM HACK starting...");

	// Stop the timer
	timer1_stop();

	// Configure the CYRF
	dsm_set_config();
	dsm_set_config_transfer();

	// Read the CYRF MFG and copy from the config
	cyrf_get_mfg_id(mfg_id);

	// Set the callbacks
	timer1_register_callback(protocol_dsm_hack_timer);
	cyrf_register_recv_callback(protocol_dsm_hack_receive);

	// Start receiving and timer
	dsm_set_channel(channel, dsm2, sop_col, (7 - sop_col), 0x0000);
	cyrf_start_recv();
	timer1_set(DSM_RECV_TIME*20);

	console_print("\r\nDSM HACK initialized 0x%02X 0x%02X 0x%02X 0x%02X", mfg_id[0], mfg_id[1], mfg_id[2], mfg_id[3]);
}

/**
 * Stop the timer
 */
void protocol_dsm_hack_stop(void) {
	// Stop the timer
	timer1_stop();

	// Abort the receive
	cyrf_set_mode(CYRF_MODE_SYNTH_RX, true);
	cyrf_write_register(CYRF_RX_ABORT, 0x00);
}

void protocol_dsm_hack_run(void) {

}

void protocol_dsm_hack_status(void) {
	console_print("\r\nScanning for %s at channel %d [%d]", (dsm2? "DSM2":"DSMX"), channel, sop_col);
}


static void protocol_dsm_hack_timer(void) {
	timer1_stop();

	// Abort the receive
	cyrf_set_mode(CYRF_MODE_SYNTH_RX, true);
	cyrf_write_register(CYRF_RX_ABORT, 0x00);

	// Goto the next channel
	protocol_dsm_hack_next();
	cyrf_start_recv();

	timer1_set(DSM_RECV_TIME*2);
}

static void protocol_dsm_hack_receive(bool error) {
	uint8_t packet_length, packet[16], rx_status , crc_lsb, crc_msb, i;

	// Get the receive count, rx_status and the packet
	packet_length = cyrf_read_register(CYRF_RX_COUNT);
	rx_status = cyrf_get_rx_status();
	cyrf_recv_len(packet, packet_length);

	// Get the CRC
	crc_lsb = cyrf_read_register(CYRF_RX_CRC_LSB);
	crc_msb = cyrf_read_register(CYRF_RX_CRC_MSB);

	// Abort the receive
	cyrf_set_mode(CYRF_MODE_SYNTH_RX, true);
	cyrf_write_register(CYRF_RX_ABORT, 0x00);

	console_print("\r\nScanning for %s at channel %d [%d]", (dsm2? "DSM2":"DSMX"), channel, sop_col);
	console_print("\r\nGot packet (status: %02X, error: %d) [%d]: ", rx_status, error, packet_length);
	for(i = 0; i < packet_length; i++)
		console_print("%02X", packet[i]);
	console_print(" (%02X%02X)", crc_lsb, crc_msb);

	// Start receiving
	cyrf_start_recv();
}

/**
 * Go to the next channel for scanning
 */
static void protocol_dsm_hack_next(void) {
	dsm2 = !dsm2;

	// Only change on DSM2 SOP_DATA codes
	if(!dsm2) {
		// Brute force the SOP_DATA collumns
		sop_col = (sop_col + 1) % 8;

		// Brute force channels at sop_col 0
		if(sop_col == 0)
			channel = (channel + 1) % DSM_MAX_CHANNEL;
	}

	// Configure the CYRF (crc doesn't matter)
	dsm_set_channel(channel, dsm2, sop_col, (7 - sop_col), 0x0000);
}