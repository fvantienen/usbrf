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
#include "frsky_transmitter.h"
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
static void protocol_frsky_transmitter_init(void);
static void protocol_frsky_transmitter_deinit(void);
static void protocol_frsky_transmitter_start(void);
static void protocol_frsky_transmitter_stop(void);
static void protocol_frsky_transmitter_run(void);
static void protocol_frsky_transmitter_state(void);
static void protocol_frsky_transmitter_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len);

/* Main protocol structure */
struct protocol_t protocol_frsky_transmitter = {
	.name = "FrSky Transmitter",
	.init = protocol_frsky_transmitter_init,
	.deinit = protocol_frsky_transmitter_deinit,
	.start = protocol_frsky_transmitter_start,
	.stop = protocol_frsky_transmitter_stop,
	.run = protocol_frsky_transmitter_run,
	.status = protocol_frsky_transmitter_state,
	.parse_arg = protocol_frsky_transmitter_parse_arg
};

/* Internal functions */
static void protocol_frsky_transmitter_timer(void);
static void protocol_frsky_transmitter_receive(uint8_t len);
static void protocol_frsky_transmitter_send(uint8_t len);
static void protocol_frsky_transmitter_next(void);
static bool protocol_frsky_parse_telem(uint8_t *packet);
static void protocol_frsky_build_packet(void);

/* Internal variables */
static enum frsky_transmitter_state_t frsky_transmitter_state;			/**< The status of the transmitter */
static enum frsky_protocol_t frsky_protocol = FRSKYX_EU;						/**< The current FrSky protocol used */
static uint8_t frsky_packet_length = FRSKY_PACKET_LENGTH_EU;				/**< The FrSky transmit length */
static uint8_t frsky_packet[FRSKY_PACKET_LENGTH_EU+5];							/**< Transmitting FrSky packet (EU is longest) */
static uint8_t frsky_hop_idx = 0;																		/**< The current hopping index */
static uint8_t frsky_chanskip = 3;																	/**< Amount of channels to skip between each transmiter */
static uint8_t send_seq = 0x8;																			/**< The transmitter telemetry send sequencing */
static uint8_t recv_seq = 0;																				/**< The transmitter telemetry receive sequencing */
static uint8_t unk_num = 0x4;
static uint8_t rx_num = 1;
/**
 * Configure the CC2500 chip and antenna switcher
 */
static void protocol_frsky_transmitter_init(void) {
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
	cc_set_mode(CC2500_TXRX_TX);

	// Set the callbacks
	timer1_register_callback(protocol_frsky_transmitter_timer);
	cc_register_recv_callback(protocol_frsky_transmitter_receive);
	cc_register_send_callback(protocol_frsky_transmitter_send);

	console_print("\r\nFrSky Transmitter initialized 0x%02X 0x%02X", config.frsky_bind_id[0], config.frsky_bind_id[1]);
}

/**
 * Deinitialize the variables
 */
static void protocol_frsky_transmitter_deinit(void) {
	timer1_register_callback(NULL);
	cc_register_recv_callback(NULL);
	cc_register_send_callback(NULL);

	console_print("\r\nFrSky Transmitter deinitialized");
}

/**
 * Configure the CC2500 and start transmitting
 */
static void protocol_frsky_transmitter_start(void) {
	cc_strobe(CC2500_SIDLE);

	// Configure the CC2500
	frsky_set_config(frsky_protocol);
	cc_write_register(CC2500_MCSM0, 0x08);
  cc_write_register(CC2500_PKTCTRL1, CC2500_PKTCTRL1_APPEND_STATUS | CC2500_PKTCTRL1_CRC_AUTOFLUSH | CC2500_PKTCTRL1_FLAG_ADR_CHECK_01);

  // Configure the address and offset
	cc_write_register(CC2500_FSCTRL0, config.cc_fsctrl0);
	cc_write_register(CC2500_ADDR, config.frsky_bind_id[0]);

  // Calibrate all channels
	frsky_tune_channels(config.frsky_hop_table, FRSKY_HOP_TABLE_LENGTH, frsky_fscal1, &frsky_fscal2, &frsky_fscal3);

	// Set the correct packet length
	if(frsky_protocol == FRSKYX_EU)
		frsky_packet_length = FRSKY_PACKET_LENGTH_EU;
	else
		frsky_packet_length = FRSKY_PACKET_LENGTH;

	// Go to the first channel
	frsky_hop_idx = FRSKY_HOP_TABLE_LENGTH-1;
	protocol_frsky_transmitter_next();

	// Start transmitting
	frsky_transmitter_state = FRSKY_TRX_SEND;
	timer1_set(FRSKY_SEND_TIME);
	console_print("\r\nFrSky Transmitter started...");
}

/**
 * Stop the timer
 */
static void protocol_frsky_transmitter_stop(void) {
	// Stop the timer and put the CC2500 to idle
	cc_strobe(CC2500_SIDLE);
	timer1_stop();
	console_print("\r\nFrSky Transmitter stopped...");
}

/**
 * In main loop running function
 */
static void protocol_frsky_transmitter_run(void) {

}

/**
 * Print the status of the scanner
 */
static void protocol_frsky_transmitter_state(void) {
	
}

/**
 * Parse arguments given to the scanner
 */
static void protocol_frsky_transmitter_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len) {
	if(type == PROTOCOL_START) {
		if(offset != 0 || len != 1 || tot_len != 1)
			return;

		frsky_protocol = arg[0];
	}
}

	static uint32_t ticks = 0;
	static uint32_t old_ticks = 0;

static void protocol_frsky_transmitter_timer(void) {
	ticks = counter_status.ticks;
	switch(frsky_transmitter_state) {
		case FRSKY_TRX_SEND:
			timer1_set(FRSKY_SEND_TIME);
			cc_set_mode(CC2500_TXRX_TX+10);
			protocol_frsky_transmitter_next();
			cc_set_power(7);
			cc_strobe(CC2500_SFRX);

			protocol_frsky_build_packet();
			cc_strobe(CC2500_SIDLE);
			cc_write_data(frsky_packet, frsky_packet[0]+1);	
			console_print("\r\nS %d", ticks-old_ticks);
			old_ticks = ticks;
			break;
	}
	
}

static void protocol_frsky_transmitter_receive(uint8_t len) {
	static uint8_t packet_len = 0;

	/* Check if we receieved a packet length */
	if(packet_len == 0) {
		ticks = counter_status.ticks;
		cc_read_data(&packet_len, 1);
		len--;
	}

	/* Check if we received a full packet */
	if(len < packet_len+2)
		return;

	/* Read the packet */
	uint8_t data[packet_len + 3 + 2];
	data[0] = packet_len;
	cc_read_data(&data[1], packet_len+2);
	packet_len = 0;

	/* Parse the packet */
	if(protocol_frsky_parse_telem(data)) {
		console_print("\r\nT %d %d", ticks-old_ticks, timer1_get_time());
		old_ticks = ticks;
	}
	cc_strobe(CC2500_SIDLE);
	cc_strobe(CC2500_SFRX);
	cc_strobe(CC2500_SRX);
}

static void protocol_frsky_transmitter_send(uint8_t len __attribute__((unused))) {
	ticks = counter_status.ticks;
	cc_set_mode(CC2500_TXRX_RX);
	cc_strobe(CC2500_SIDLE);
	cc_strobe(CC2500_SRX);
	console_print("\r\nD %d", ticks-old_ticks);
	old_ticks = ticks;
}

/**
 * Go to the next channel for receiver
 */
static void protocol_frsky_transmitter_next(void) {
	frsky_hop_idx = (frsky_hop_idx + frsky_chanskip) % FRSKY_HOP_TABLE_LENGTH;
	cc_strobe(CC2500_SIDLE);

	cc_write_register(CC2500_FSCAL1, frsky_fscal1[frsky_hop_idx]);
	cc_write_register(CC2500_FSCAL2, frsky_fscal2);
	cc_write_register(CC2500_FSCAL3, frsky_fscal3);
	cc_write_register(CC2500_CHANNR, config.frsky_hop_table[frsky_hop_idx]);
}

/**
 * Parse a telemetry packet from FrSky
 */
static bool protocol_frsky_parse_telem(uint8_t *packet) {
	// Validate the packet length (without length and status bytes)
	if(packet[0] != FRSKY_TELEM_LENGTH)
		return false;

	// Validate the CRC of the CC2500
	if(!(packet[FRSKY_TELEM_LENGTH+2] & 0x80))
		return false;

	// Validate the transmitter id
	if(packet[1] != config.frsky_bind_id[0] || packet[2] != config.frsky_bind_id[1])
		return false;

	// Validate the inner CRC for FrSkyX
	if(frsky_protocol == FRSKYX_EU || frsky_protocol == FRSKYX) {
		uint16_t calc_crc = frskyx_crc(&packet[3], FRSKY_TELEM_LENGTH-4);
		uint16_t packet_crc = (packet[FRSKY_TELEM_LENGTH-1] << 8) | packet[FRSKY_TELEM_LENGTH];
		if(calc_crc != packet_crc)
			return false;
	}

	// Send the data to the PC
	packet[FRSKY_TELEM_LENGTH+3] = config.frsky_hop_table[frsky_hop_idx];
	packet[FRSKY_TELEM_LENGTH+4] = 0;
	uint8_t chip_id = 1;
	pprz_msg_send_RECV_DATA(&pprzlink.tp.trans_tx, &pprzlink.dev, 1, &chip_id, FRSKY_TELEM_LENGTH+5, packet);

	// Update the telemetry sequence based on the received data
	if((packet[5] & 0xF) == 0x8 || (packet[5] >> 4) == 0x8) {
		recv_seq = 0x8;
		send_seq = 0x0;
	} else {
		recv_seq = ((packet[5] & 0x03) + 1) % 4;
		send_seq = (((packet[5] >> 4) & 0x3) + 3) % 4;
	}

	return true;
}

/**
 * Build the transmitting packet
 */
static void protocol_frsky_build_packet(void) {
//[32, 143, 125, 4, 102, 1, 1, 0, 0, 0, 1, 192, 255, 3, 192, 0, 12, 192, 0, 12, 192, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 241, 149, 11, 140, 28, 0]
	static bool send_part2 = false;

	frsky_packet[0] = frsky_packet_length;
	frsky_packet[1] = config.frsky_bind_id[0];
	frsky_packet[2] = config.frsky_bind_id[1];
	frsky_packet[3] = unk_num;
	frsky_packet[4] = (frsky_chanskip << 6) | frsky_hop_idx;
	frsky_packet[5] = frsky_chanskip >> 2;
	frsky_packet[6] = rx_num; // RX number
	frsky_packet[7] = 0; // No failsafe values
	frsky_packet[8] = 0;

	for(uint8_t i = 0; i < 12; i += 3) {
		uint16_t value0 = send_part2? 3900 : 1500;
		uint16_t value1 = send_part2? 3900 : 1500;
		frsky_packet[i + 9]  = value0;
		frsky_packet[i + 10] = ((value0 >> 8) & 0xF) | (value1 << 4);
		frsky_packet[i + 11]  = (value1 >> 4);
	}

	// Update the sequence and transmit
	if(send_seq != 0x8)
		send_seq = (send_seq + 1) & 0x03;
	frsky_packet[21] = (recv_seq << 4) | send_seq; // Sequence

	for(uint8_t i = 22; i < frsky_packet_length-1; i++)
		frsky_packet[i] = 0;

	uint16_t crc = frskyx_crc(&frsky_packet[3], frsky_packet_length-4);
	frsky_packet[frsky_packet_length-1] = crc >> 8;
	frsky_packet[frsky_packet_length] = crc;

	// DEBUG
	frsky_packet[frsky_packet_length+1] = 0;
	frsky_packet[frsky_packet_length+2] = 0x80;
	frsky_packet[frsky_packet_length+3] = config.frsky_hop_table[frsky_hop_idx];
	frsky_packet[frsky_packet_length+4] = 0;
	//send_part2 = !send_part2;
	//uint8_t chip_id = 1;
	//pprz_msg_send_RECV_DATA(&pprzlink.tp.trans_tx, &pprzlink.dev, 1, &chip_id, frsky_packet_length+2, frsky_packet);
}