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
#include "frsky_hack.h"
#include "modules/led.h"
#include "modules/config.h"
#include "modules/timer.h"
#include "modules/ant_switch.h"
#include "modules/cc2500.h"
#include "modules/pprzlink.h"
#include "modules/console.h"
#include "modules/counter.h"
#include "helper/frsky.h"

/* Main protocol functions */
static void protocol_frsky_hack_init(void);
static void protocol_frsky_hack_deinit(void);
static void protocol_frsky_hack_start(void);
static void protocol_frsky_hack_stop(void);
static void protocol_frsky_hack_run(void);
static void protocol_frsky_hack_state(void);
static void protocol_frsky_hack_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len);

/* Main protocol structure */
struct protocol_t protocol_frsky_hack = {
	.name = "FrSky Hack",
	.init = protocol_frsky_hack_init,
	.deinit = protocol_frsky_hack_deinit,
	.start = protocol_frsky_hack_start,
	.stop = protocol_frsky_hack_stop,
	.run = protocol_frsky_hack_run,
	.status = protocol_frsky_hack_state,
	.parse_arg = protocol_frsky_hack_parse_arg
};

/* Internal functions */
static void protocol_frsky_hack_timer(void);
static void protocol_frsky_hack_receive(uint8_t len);
static void protocol_frsky_hack_next(void);
static bool protocol_frsky_parse_data(uint8_t *packet);

/* Internal variables */
static enum frsky_hack_state_t frsky_hack_state;										/**< The status of the hack */
static enum frsky_protocol_t frsky_protocol = FRSKYX_EU;						/**< The current FrSky protocol used */
static uint8_t frsky_packet_length = FRSKY_PACKET_LENGTH_EU + 3;		/**< The FrSky receive packet length with length and status bytes */
static uint8_t frsky_hop_idx = 0;																		/**< The current hopping index */
static uint8_t frsky_chanskip = 1;																	/**< Amount of channels to skip between each receive */
static uint8_t frsky_target_id[2];																	/**< The target to hack */
static uint8_t frsky_hop_table[FRSKY_HOP_TABLE_LENGTH];							/**< The hopping table of the target */

/**
 * Configure the CC2500 chip and antenna switcher
 */
static void protocol_frsky_hack_init(void) {
	// Stop the timer
	timer1_stop();

#ifdef CC_DEV_ANT
	// Switch the antenna to the CC2500
	bool ant_state[] = CC_DEV_ANT;
	ant_switch(ant_state);
#endif

	// Configure the CC2500
	cc_reset();
	cc_strobe(CC2500_SIDLE);
	cc_set_mode(CC2500_TXRX_RX);

	// Set the callbacks
	timer1_register_callback(protocol_frsky_hack_timer);
	cc_register_recv_callback(protocol_frsky_hack_receive);
	cc_register_send_callback(NULL);

	console_print("\r\nFrSky Hack initialized 0x%02X 0x%02X", config.frsky_bind_id[0], config.frsky_bind_id[1]);
}

/**
 * Deinitialize the variables
 */
static void protocol_frsky_hack_deinit(void) {
	timer1_register_callback(NULL);
	cc_register_recv_callback(NULL);

	console_print("\r\nFrSky Hack deinitialized");
}

/**
 * Configure the CYRF and start scanning
 */
static void protocol_frsky_hack_start(void) {
	cc_strobe(CC2500_SIDLE);

	// Configure the CC2500
	frsky_set_config(frsky_protocol);
	cc_write_register(CC2500_MCSM0, 0x08);
  cc_write_register(CC2500_PKTCTRL1, CC2500_PKTCTRL1_APPEND_STATUS | CC2500_PKTCTRL1_CRC_AUTOFLUSH | CC2500_PKTCTRL1_FLAG_ADR_CHECK_01);

	// Set the correct packet length (length + 2 status bytes appended)
	if(frsky_protocol == FRSKYX_EU)
		frsky_packet_length = FRSKY_PACKET_LENGTH_EU + 3;
	else
		frsky_packet_length = FRSKY_PACKET_LENGTH + 3;

	// Set the calibration and bind ID
	cc_write_register(CC2500_FSCTRL0, config.cc_fsctrl0);
	cc_write_register(CC2500_ADDR, frsky_target_id[0]);

	// Calibrate all channels
	frsky_tune_channels(frsky_hop_table, FRSKY_HOP_TABLE_LENGTH, frsky_fscal1, &frsky_fscal2, &frsky_fscal3);
	cc_write_register(CC2500_FSCAL2, frsky_fscal2);
	cc_write_register(CC2500_FSCAL3, frsky_fscal3);

	// Go to the first channel
	frsky_hop_idx = FRSKY_HOP_TABLE_LENGTH-1;
	protocol_frsky_hack_next();

	// Start receiving
	frsky_hack_state = FRSKY_HACK_SYNC;
	cc_strobe(CC2500_SRX);
	timer1_set(FRSKY_RECV_TIME);
	console_print("\r\nFrSky Hack started...");
}

/**
 * Stop the timer
 */
static void protocol_frsky_hack_stop(void) {
	// Stop the timer and put the CC2500 to idle
	cc_strobe(CC2500_SIDLE);
	timer1_stop();
	console_print("\r\nFrSky Hack stopped...");
}

/**
 * In main loop running function
 */
static void protocol_frsky_hack_run(void) {

}

/**
 * Print the status of the scanner
 */
static void protocol_frsky_hack_state(void) {
	
}

/**
 * Parse arguments given to the scanner
 */
static void protocol_frsky_hack_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len) {
	if(type == PROTOCOL_START) {
		if(offset != 0 || len != FRSKY_HOP_TABLE_LENGTH+3 || tot_len != FRSKY_HOP_TABLE_LENGTH+3)
			return;

		frsky_protocol = arg[0];
		memcpy(frsky_target_id, &arg[1], 2);
		memcpy(frsky_hop_table, &arg[3], FRSKY_HOP_TABLE_LENGTH);
	}
}


static void protocol_frsky_hack_timer(void) {
	switch(frsky_hack_state) {

		/* Trying to synchronize with the transmitter */
		case FRSKY_HACK_SYNC:
			protocol_frsky_hack_next();
			cc_strobe(CC2500_SFRX);
			cc_strobe(CC2500_SRX);
			timer1_set(FRSKY_RECV_TIME);
			break;

		/* We missed a packet during receiving */
		case FRSKY_HACK_RECV:
			protocol_frsky_hack_next();
			cc_strobe(CC2500_SFRX);
			cc_strobe(CC2500_SRX);
			timer1_set(FRSKY_RECV_TIME);
			break;
	}
	
}

static void protocol_frsky_hack_receive(uint8_t len) {
	/* Check if we received a full packet */
	if(len < frsky_packet_length)
		return;

	uint8_t data[frsky_packet_length + 2];		// Added 2 bytes for PC transmission of channel and fsctrl0
	cc_read_data(data, frsky_packet_length);

	switch(frsky_hack_state) {
		/* When receiving a data packet */
		case FRSKY_HACK_SYNC:
		case FRSKY_HACK_RECV:
			// Check if the data packet is valid
			if(protocol_frsky_parse_data(data)) {
				LED_TOGGLE(LED_RX);

				frsky_hack_state = FRSKY_HACK_RECV;
				protocol_frsky_hack_next();
				cc_strobe(CC2500_SFRX);
				timer1_set(FRSKY_RECV_TIME);
			}
			cc_strobe(CC2500_SRX);
			break;

	}
}

/**
 * Go to the next channel for receiver
 */
static void protocol_frsky_hack_next(void) {
	frsky_hop_idx = (frsky_hop_idx + frsky_chanskip) % FRSKY_HOP_TABLE_LENGTH;
	cc_strobe(CC2500_SIDLE);

	cc_write_register(CC2500_CHANNR, frsky_hop_table[frsky_hop_idx]);
	cc_write_register(CC2500_FSCAL1, frsky_fscal1[frsky_hop_idx]);
}

/**
 * Parse the incomming data packet
 * @param[in] *packet The bytes of the received data packet
 * @return Whether the packet was a valid data packet
 */
static bool protocol_frsky_parse_data(uint8_t *packet) {
	// Validate the packet length (without length and status bytes)
	if(packet[0] != frsky_packet_length-3)
		return false;

	// Validate the CRC of the CC2500
	if(!(packet[frsky_packet_length-1] & 0x80))
		return false;

	// Validate the transmitter id
	if(packet[1] != frsky_target_id[0] || packet[2] != frsky_target_id[1])
		return false;

	// Validate the inner CRC for FrSkyX
	if(frsky_protocol == FRSKYX_EU || frsky_protocol == FRSKYX) {
		uint16_t calc_crc = frskyx_crc(&packet[3], frsky_packet_length-7);
		uint16_t packet_crc = (packet[frsky_packet_length-4] << 8) | packet[frsky_packet_length-3];
		if(calc_crc != packet_crc)
			return false;
	}

	// Send the data to the PC
	packet[frsky_packet_length+0] = frsky_hop_table[frsky_hop_idx];
	packet[frsky_packet_length+1] = config.cc_fsctrl0;
	uint8_t chip_id = 1;
	pprz_msg_send_RECV_DATA(&pprzlink.tp.trans_tx, &pprzlink.dev, 1, &chip_id, frsky_packet_length+2, packet);

	// Update the channel skip and channel index based on received values
	frsky_chanskip = (packet[4] >> 6) | (packet[5] << 2);
	frsky_hop_idx = packet[4] & 0x3F;

	return true;
}