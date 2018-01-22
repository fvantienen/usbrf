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

#ifndef HELPER_DSM_H_
#define HELPER_DSM_H_

#include <stdint.h>
#include <stdbool.h>

/* All times are in microseconds divided by 10 */
#define DSM_BIND_RECV_TIME			1000		/**< Time before timeout when receiving bind packets */
#define DSM_SYNC_RECV_TIME			2000		/**< Time before timeout when trying to sync */
#define DSM_RECV_TIME_A			    1950		/**< Time before timeout when trying to receive channel A */
#define DSM_RECV_TIME_A_SHORT		850			/**< Time before timeout when trying to receive channel A */
#define DSM_RECV_TIME_B					550			/**< Time before timeout when trying to receive between B */

#define DSM_BIND_SEND_TIME			1000		/**< Time between sending bind packets */
#define DSM_SEND_TIME				    2200		/**< Time between sending both Channel A and Channel B */
#define DSM_SEND_TIME_SHORT			1100		/**< Time between sending both Channel A and Channel B */
#define DSM_CHA_CHB_SEND_TIME		400			/**< Time between Channel A and Channel B send */
#define DSM_CHB_CHA_SEND_TIME		700			/**< Time between Channel B and Channel A send */

/* The maximum channekl number for DSM2 and DSMX */
#define DSM_MAX_CHANNEL				  0x4F		/**< Maximum channel number used for DSM2 and DSMX */
#define DSM_MAX_USED_CHANNELS		23			/**< Maximum amunt of used channels when using DSMX */
#define DSM_BIND_PACKETS			  300			/**< The amount of bind packets to send */

/* The different kind of protocol definitions DSM2 and DSMX with 1 and 2 packets of data */
enum dsm_protocol {
	DSM_DSM2_1			= 0x01,		/**< The original DSM2 protocol with 1 packet of data */
	DSM_DSM2_2			= 0x02,		/**< The original DSM2 protocol with 2 packets of data */
	DSM_DSMX_1			= 0xA2,		/**< The original DSMX protocol with 1 packet of data */
	DSM_DSMX_2			= 0xB2,		/**< The original DSMX protocol with 2 packets of data */
};
#define IS_DSM2(x)			(x == DSM_DSM2_1 || x == DSM_DSM2_2 || usbrf_config.dsm_force_dsm2)
#define IS_DSMX(x)			(!IS_DSM2(x))

#define CHECK_MFG_ID(protocol, packet, id) ((IS_DSM2(protocol) && packet[0] == (~id[2]&0xFF) && packet[1] == (~id[3]&0xFF)) || \
		(IS_DSMX(protocol) && packet[0] == id[2] && packet[1] == id[3]))

/* The different kind of resolutions the commands can be */
enum dsm_resolution {
	DSM_10_BIT_RESOLUTION			= 0x00,		/**< It has a 10 bit resolution */
	DSM_11_BIT_RESOLUTION			= 0x01,		/**< It has a 11 bit resolution */
};

/* External variables used in DSM2 and DSMX */
extern const uint8_t pn_codes[5][9][8];			/**< The pn_codes for the DSM2/DSMX protocol */
extern const uint8_t pn_bind[];					/**< The pn_code used during binding */

/* External functions */
void dsm_set_config(void);
void dsm_set_config_bind(void);
void dsm_set_config_transfer(void);
void dsm_generate_channels_dsmx(uint8_t mfg_id[], uint8_t *channels);
void dsm_set_chan(uint8_t channel, uint8_t pn_row, uint8_t sop_col, uint8_t data_col, uint16_t crc_seed);
void dsm_set_channel(uint8_t channel, bool is_dsm2, uint8_t sop_col, uint8_t data_col, uint16_t crc_seed);
void dsm_radio_to_channels(uint8_t* data, uint8_t nb_channels, bool is_11bit, int16_t* channels);

#endif /* HELPER_DSM_H_ */
