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

#ifndef HELPER_FRSKY_H_
#define HELPER_FRSKY_H_

#include <stdint.h>
#include <stdbool.h>

/* All times are in microseconds divided by 10 */
#define FRSKY_RECV_TIME			1100				/**< Time to wait for an FrSky packet */
#define FRSKY_SEND_TIME			900					/**< Time between 2 consecutive packets */ 
#define FRSKYX_USED_CHAN		47					/**< Amount of channels used by FrSkyX */

/* General defines */
#define FRSKY_MAX_CHANNEL		235					/**< The highest channel number */
#define FRSKY_BIND_CHAN			0 					/**< The channel on which the binding packet is send */
#define FRSKY_BIND_ADDR			0x03				/**< The binding address */
#define FRSKY_PACKET_LENGTH 			29		/**< Packet length for FrSky packets */
#define FRSKY_PACKET_LENGTH_EU		32		/**< Packets length for EU/LBT FrSky packets */
#define FRSKY_HOP_TABLE_PKTS			10 		/**< Amount of hopping table packets */
#define FRSKY_HOP_TABLE_LENGTH		47		/**< Amount of channels used in the hopping table */

/* The different FrSky protocols */
enum frsky_protocol_t {
	FRSKYV = 0,
	FRSKYD,
	FRSKYX,
	FRSKYX_EU
};

/* External variables */
extern uint8_t frsky_fscal1[FRSKY_HOP_TABLE_LENGTH+1];
extern uint8_t frsky_fscal2;
extern uint8_t frsky_fscal3;

/* External functions */
void frsky_set_config(enum frsky_protocol_t protocol);
void frsky_tune_channel(uint8_t ch);
void frsky_tune_channels(uint8_t *channels, uint8_t length, uint8_t *fscal1, uint8_t *fscal2, uint8_t *fscal3);
uint16_t frskyx_crc(const uint8_t *data, uint8_t length);

#endif /* HELPER_FRSKY_H_ */
