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
#include "frsky_receiver.h"
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
static void protocol_frsky_receiver_init(void);
static void protocol_frsky_receiver_deinit(void);
static void protocol_frsky_receiver_start(void);
static void protocol_frsky_receiver_stop(void);
static void protocol_frsky_receiver_run(void);
static void protocol_frsky_receiver_state(void);
static void protocol_frsky_receiver_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len);

/* Main protocol structure */
struct protocol_t protocol_frsky_receiver = {
	.name = "FrSky Receiver",
	.init = protocol_frsky_receiver_init,
	.deinit = protocol_frsky_receiver_deinit,
	.start = protocol_frsky_receiver_start,
	.stop = protocol_frsky_receiver_stop,
	.run = protocol_frsky_receiver_run,
	.status = protocol_frsky_receiver_state,
	.parse_arg = protocol_frsky_receiver_parse_arg
};

/* Internal functions */
static void protocol_frsky_receiver_timer(void);
static void protocol_frsky_receiver_receive(uint8_t len);
static void protocol_frsky_receiver_next(void);
static bool protocol_frsky_parse_bind(uint8_t *packet);
static bool protocol_frsky_parse_data(uint8_t *packet);
static void protocol_frsky_start_sync(void);
static void protocol_frsky_start_bind(void);

/* Internal variables */
static enum frsky_receiver_state_t frsky_receiver_state;						/**< The status of the receiver */
static enum frsky_protocol_t frsky_protocol = FRSKYX_EU;						/**< The current FrSky protocol used */
static uint8_t frsky_packet_length = FRSKY_PACKET_LENGTH_EU + 3;		/**< The FrSky receive packet length with length and status bytes */
static int8_t frsky_tune = -127;																		/**< The current tuning of the CC2500 during the tuning phase */
static int8_t frsky_tune_min = 127;																	/**< Minimum tuning value on which a binding packet is successfully received */ 
static int8_t frsky_tune_max = -127;																/**< Maximum tuning value on which a binding packet is successfully received */ 
static uint16_t frsky_bind_table = 0;																/**< The FrSky received bind table indexes divided by 5 as bit */
static uint8_t frsky_hop_idx = 0;																		/**< The current hopping index */
static uint8_t frsky_chanskip = 1;																	/**< Amount of channels to skip between each receive */

/**
 * Configure the CC2500 chip and antenna switcher
 */
static void protocol_frsky_receiver_init(void) {
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
	frsky_set_config(frsky_protocol);
	cc_write_register(CC2500_MCSM0, 0x18);
  cc_write_register(CC2500_PKTCTRL1, CC2500_PKTCTRL1_APPEND_STATUS | CC2500_PKTCTRL1_CRC_AUTOFLUSH | CC2500_PKTCTRL1_FLAG_ADR_CHECK_01);

  // Tune the binding channel
  frsky_tune_channel(FRSKY_BIND_CHAN);
  frsky_fscal1[FRSKY_HOP_TABLE_LENGTH] = cc_read_register(CC2500_FSCAL1);
  frsky_fscal2 = cc_read_register(CC2500_FSCAL2);
	frsky_fscal3 = cc_read_register(CC2500_FSCAL3);

	// Set the callbacks
	timer1_register_callback(protocol_frsky_receiver_timer);
	cc_register_recv_callback(protocol_frsky_receiver_receive);
	cc_register_send_callback(NULL);

	console_print("\r\nFrSky Receiver initialized 0x%02X 0x%02X", config.frsky_bind_id[0], config.frsky_bind_id[1]);
}

/**
 * Deinitialize the variables
 */
static void protocol_frsky_receiver_deinit(void) {
	timer1_register_callback(NULL);
	cc_register_recv_callback(NULL);

	console_print("\r\nFrSky Receiver deinitialized");
}

/**
 * Configure the CYRF and start scanning
 */
static void protocol_frsky_receiver_start(void) {
	cc_strobe(CC2500_SIDLE);

	// Set the correct packet length (length + 2 status bytes appended)
	if(frsky_protocol == FRSKYX_EU)
		frsky_packet_length = FRSKY_PACKET_LENGTH_EU + 3;
	else
		frsky_packet_length = FRSKY_PACKET_LENGTH + 3;

	// Check whether or not we are already bound to a transmitter and tuned
	if(config.cc_tuned && config.frsky_bound) {
		protocol_frsky_start_sync();
		console_print("\r\nSync mode");
	}
	// We are not bound but only tuned
	else if(config.cc_tuned) {
		frsky_bind_table = 0;
		protocol_frsky_start_bind();
		console_print("\r\nBind mode");
	}
	// We first need to tune
	else {
		frsky_bind_table = 0;
		frsky_tune = -127;
		frsky_tune_min = 127;
		frsky_tune_max = -127;

		cc_write_register(CC2500_FSCTRL0, frsky_tune);
		cc_write_register(CC2500_ADDR, FRSKY_BIND_ADDR);
		cc_write_register(CC2500_CHANNR, FRSKY_BIND_CHAN);
		cc_write_register(CC2500_FSCAL1, frsky_fscal1[FRSKY_HOP_TABLE_LENGTH]);

		frsky_receiver_state = FRSKY_RECV_TUNE;
		cc_strobe(CC2500_SRX);
		timer1_set(FRSKY_RECV_TIME);
		console_print("\r\nTune mode");
	}
	
	console_print("\r\nFrSky Receiver started...");
}

/**
 * Stop the timer
 */
static void protocol_frsky_receiver_stop(void) {
	// Stop the timer and put the CC2500 to idle
	cc_strobe(CC2500_SIDLE);
	timer1_stop();
	console_print("\r\nFrSky receiver stopped...");
}

/**
 * In main loop running function
 */
static void protocol_frsky_receiver_run(void) {

}

/**
 * Print the status of the scanner
 */
static void protocol_frsky_receiver_state(void) {
	
}

/**
 * Parse arguments given to the scanner
 */
static void protocol_frsky_receiver_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len) {
	if(type == PROTOCOL_START) {
		if(offset != 0 || len != 1 || tot_len != 1)
			return;

		frsky_protocol = arg[0];
	}
}


static void protocol_frsky_receiver_timer(void) {
	switch(frsky_receiver_state) {
		/* Tuning the crystal cours */
		case FRSKY_RECV_TUNE:
			// Check if the course tuning is ended
			if(frsky_tune > 127-10) {
				// Check if the course tuning valid
				if (frsky_tune_min > frsky_tune_max) {
					frsky_tune = -127;
				}
				else {
					frsky_tune = frsky_tune_min-8;
					frsky_receiver_state = FRSKY_RECV_FINETUNE;
				}
			}
			else
				frsky_tune += 9;
			
			cc_strobe(CC2500_SIDLE);
			cc_write_register(CC2500_FSCTRL0, frsky_tune);
			cc_strobe(CC2500_SFRX);
			cc_strobe(CC2500_SRX);
			timer1_set(FRSKY_RECV_TIME);
			console_print("B");
			break;

		/* Fine tuning the crystal */
		case FRSKY_RECV_FINETUNE:
			// Check if the fine tuning is ended
			if(frsky_tune == frsky_tune_max+8) {
				frsky_tune = (frsky_tune_max+frsky_tune_min)/2;
				config.cc_fsctrl0 = frsky_tune;
				config.cc_tuned = true;
				console_print("\r\nD%d", frsky_tune);

				// If we received the full hopping table go to Sync
				if(frsky_bind_table == ((1 << FRSKY_HOP_TABLE_PKTS) - 1)) {
					config.frsky_bound = true;
					protocol_frsky_start_sync();
					break;
				}
				else {
					protocol_frsky_start_bind();
					break;
				}
	
				frsky_receiver_state = FRSKY_RECV_BIND;
			}
			
			// We are still fine tuning
			frsky_tune += 1;
			cc_strobe(CC2500_SIDLE);
			cc_write_register(CC2500_FSCTRL0, frsky_tune);
			cc_strobe(CC2500_SFRX);
			cc_strobe(CC2500_SRX);
			timer1_set(FRSKY_RECV_TIME);
			break;

		/* Receiving binding packets */
		case FRSKY_RECV_BIND:
			cc_strobe(CC2500_SIDLE);
			cc_strobe(CC2500_SFRX);
			cc_strobe(CC2500_SRX);
			timer1_set(FRSKY_RECV_TIME);
			break;

		/* Trying to synchronize with the transmitter */
		case FRSKY_RECV_SYNC:
			protocol_frsky_receiver_next();
			cc_strobe(CC2500_SFRX);
			cc_strobe(CC2500_SRX);
			timer1_set(FRSKY_RECV_TIME);
			break;

		/* We missed a packet during receiving */
		case FRSKY_RECV_RECV:
			console_print("M");
			protocol_frsky_receiver_next();
			cc_strobe(CC2500_SFRX);
			cc_strobe(CC2500_SRX);
			timer1_set(FRSKY_RECV_TIME);
			break;
	}
	
}

static void protocol_frsky_receiver_receive(uint8_t len) {
	/* Check if we received a full packet */
	if(len < frsky_packet_length)
		return;

	uint8_t data[frsky_packet_length + 2];		// Added 2 bytes for PC transmission of channel and fsctrl0
	cc_read_data(data, frsky_packet_length);

	switch(frsky_receiver_state) {
		/* Tuning the crystal based on binding packets */
		case FRSKY_RECV_TUNE:
		case FRSKY_RECV_FINETUNE:
			console_print("A");
			// Check if the binding packet is valid
			if(protocol_frsky_parse_bind(data)) {
				if(frsky_tune < frsky_tune_min)
				frsky_tune_min = frsky_tune;
				if(frsky_tune > frsky_tune_max)
					frsky_tune_max = frsky_tune;

				console_print("\r\nR%d %d %d", frsky_tune, data[0], len);
				LED_TOGGLE(LED_RX);
			}

			cc_strobe(CC2500_SRX);
			break;

		/* Receiving bind packets */
		case FRSKY_RECV_BIND:
			// Check if the binding packet is valid and parse it
			if(protocol_frsky_parse_bind(data)) {
				LED_TOGGLE(LED_RX);

				// If we received the full hopping table go to Sync
				if(frsky_bind_table == ((1 << FRSKY_HOP_TABLE_PKTS) - 1)) {
					config.frsky_bound = true;
					protocol_frsky_start_sync();
					break;
				}
			}

			cc_strobe(CC2500_SRX);
			break;

		/* When receiving a data packet */
		case FRSKY_RECV_SYNC:
		case FRSKY_RECV_RECV:
			// Check if the data packet is valid
			if(protocol_frsky_parse_data(data)) {
				LED_TOGGLE(LED_RX);

				frsky_receiver_state = FRSKY_RECV_RECV;
				protocol_frsky_receiver_next();
				cc_strobe(CC2500_SFRX);
				timer1_set(FRSKY_RECV_TIME);
			} else
				console_print("E");
			cc_strobe(CC2500_SRX);
			break;

	}
}

/**
 * Go to the next channel for receiver
 */
static void protocol_frsky_receiver_next(void) {
	frsky_hop_idx = (frsky_hop_idx + frsky_chanskip) % FRSKY_HOP_TABLE_LENGTH;
	cc_strobe(CC2500_SIDLE);

	cc_write_register(CC2500_CHANNR, config.frsky_hop_table[frsky_hop_idx]);
	cc_write_register(CC2500_FSCAL1, frsky_fscal1[frsky_hop_idx]);
}

/**
 * Parse the incomming bind packet
 * @param[in] *packet The bytes of the received binding packet
 * @return Whether the packet was a valid binding packet
 */
static bool protocol_frsky_parse_bind(uint8_t *packet) {
	// Validate the packet length (without length and status bytes)
	if(packet[0] != frsky_packet_length-3)
		return false;

	// Validate the CRC of the CC2500
	if(!(packet[frsky_packet_length-1] & 0x80))
		return false;

	// Validate if the type is correct
	if(packet[1] != 0x03 || packet[2] != 0x01)
		return false;

	// Validate the inner CRC for FrSkyX
	if(frsky_protocol == FRSKYX_EU || frsky_protocol == FRSKYX) {
		uint16_t calc_crc = frskyx_crc(&packet[3], frsky_packet_length-7);
		uint16_t packet_crc = (packet[frsky_packet_length-4] << 8) | packet[frsky_packet_length-3];
		if(calc_crc != packet_crc)
			return false;
	}

	// Check if the hopping table index is valid
	uint8_t idx = packet[5];
	if(idx > (5 * FRSKY_HOP_TABLE_PKTS - 5))
		return false;

	// Parse the packet ID
	config.frsky_bind_id[0] = packet[3];
	config.frsky_bind_id[1] = packet[4];

	// Parse the hopping table
	frsky_bind_table |= (1 << (idx/5));
	config.frsky_hop_table[idx + 0] = packet[6];
	config.frsky_hop_table[idx + 1] = packet[7];
	config.frsky_hop_table[idx + 2] = packet[8];
	config.frsky_hop_table[idx + 3] = packet[9];
	config.frsky_hop_table[idx + 4] = packet[10];

	// TODO: parse RX number (packet[12])

	return true;
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
	if(packet[1] != config.frsky_bind_id[0] || packet[2] != config.frsky_bind_id[1])
		return false;

	// Validate the inner CRC for FrSkyX
	if(frsky_protocol == FRSKYX_EU || frsky_protocol == FRSKYX) {
		uint16_t calc_crc = frskyx_crc(&packet[3], frsky_packet_length-7);
		uint16_t packet_crc = (packet[frsky_packet_length-4] << 8) | packet[frsky_packet_length-3];
		if(calc_crc != packet_crc)
			return false;
	}

	// Send the data to the PC
	packet[frsky_packet_length+0] = config.frsky_hop_table[frsky_hop_idx];
	packet[frsky_packet_length+1] = config.cc_fsctrl0;
	uint8_t chip_id = 1;
	pprz_msg_send_RECV_DATA(&pprzlink.tp.trans_tx, &pprzlink.dev, 1, &chip_id, frsky_packet_length+2, packet);

	// Update the channel skip and channel index based on received values
	frsky_chanskip = (packet[4] >> 6) | (packet[5] << 2);
	frsky_hop_idx = packet[4] & 0x3F;

	return true;
}

/**
 * Start the synchronisation with the transmitter
 */
static void protocol_frsky_start_sync(void) {
	// Set the calibration and bind ID
	cc_write_register(CC2500_FSCTRL0, config.cc_fsctrl0);
	cc_write_register(CC2500_ADDR, config.frsky_bind_id[0]);

	// Calibrate all channels
	frsky_tune_channels(config.frsky_hop_table, FRSKY_HOP_TABLE_LENGTH, frsky_fscal1, &frsky_fscal2, &frsky_fscal3);
	cc_write_register(CC2500_FSCAL2, frsky_fscal2);
	cc_write_register(CC2500_FSCAL3, frsky_fscal3);

	// Go to the first channel
	frsky_hop_idx = FRSKY_HOP_TABLE_LENGTH-1;
	protocol_frsky_receiver_next();

	// Start receiving
	cc_strobe(CC2500_SRX);
	frsky_receiver_state = FRSKY_RECV_SYNC;
	timer1_set(FRSKY_RECV_TIME);
}

/**
 * Start receiving binding packets from the transmitter
 */
static void protocol_frsky_start_bind(void) {
	cc_write_register(CC2500_FSCTRL0, config.cc_fsctrl0);
	cc_write_register(CC2500_ADDR, FRSKY_BIND_ADDR);
	cc_write_register(CC2500_CHANNR, FRSKY_BIND_CHAN);
	cc_write_register(CC2500_FSCAL1, frsky_fscal1[FRSKY_HOP_TABLE_LENGTH]);

	cc_strobe(CC2500_SRX);
	frsky_receiver_state = FRSKY_RECV_BIND;
	timer1_set(FRSKY_RECV_TIME);
}