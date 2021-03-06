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

#include "frsky.h"

#include "modules/cc2500.h"
#include "modules/counter.h"

/* External variables which are used in multiple protocols */
uint8_t frsky_fscal1[FRSKY_HOP_TABLE_LENGTH+1];							/**< The FSCAL1 values for each of the channels in the hopping table + 1 for the binding channel */
uint8_t frsky_fscal2 = 0;																		/**< The calibration value for FSCAL2 */
uint8_t frsky_fscal3 = 0;																		/**< The calibration value for FSCAL3 */

/* The common starting register addresses */
static const uint8_t frsky_conf_addr[]= {
	CC2500_IOCFG0,
	CC2500_IOCFG2,
	CC2500_MCSM1,
	CC2500_MCSM0,
	CC2500_PKTLEN,
	CC2500_PKTCTRL1,
	CC2500_PKTCTRL0,
	CC2500_PATABLE,
	CC2500_FSCTRL1,
	CC2500_FSCTRL0,
	CC2500_FREQ2,	
	CC2500_FREQ1,
	CC2500_FREQ0,
	CC2500_MDMCFG4,
	CC2500_MDMCFG3,
	CC2500_MDMCFG2,
	CC2500_MDMCFG1,
	CC2500_MDMCFG0,
	CC2500_DEVIATN
};

static const uint8_t frskyv_conf[]= {
	/*02_IOCFG0*/  	 0x06,
	/*00_IOCFG2*/  	 0x06,
	/*17_MCSM1*/   	 0x0c,
	/*18_MCSM0*/   	 0x18,
	/*06_PKTLEN*/  	 0xff,
	/*07_PKTCTRL1*/	 0x04,
	/*08_PKTCTRL0*/	 0x05,
	/*3E_PATABLE*/ 	 0xfe,
	/*0B_FSCTRL1*/ 	 0x08,
	/*0C_FSCTRL0*/ 	 0x00,
	/*0D_FREQ2*/   	 0x5c,
	/*0E_FREQ1*/   	 0x58,
	/*0F_FREQ0*/   	 0x9d,
	/*10_MDMCFG4*/ 	 0xAA,
	/*11_MDMCFG3*/ 	 0x10,
	/*12_MDMCFG2*/ 	 0x93,
	/*13_MDMCFG1*/ 	 0x23,
	/*14_MDMCFG0*/ 	 0x7a,
	/*15_DEVIATN*/ 	 0x41
};

static const uint8_t frskyd_conf[]= {
	/*02_IOCFG0*/  	 0x06,
	/*00_IOCFG2*/  	 0x06,
	/*17_MCSM1*/   	 0x0c,
	/*18_MCSM0*/   	 0x18,
	/*06_PKTLEN*/  	 0x19,
	/*07_PKTCTRL1*/	 0x04,
	/*08_PKTCTRL0*/	 0x05,
	/*3E_PATABLE*/ 	 0xff,
	/*0B_FSCTRL1*/ 	 0x08,
	/*0C_FSCTRL0*/ 	 0x00,
	/*0D_FREQ2*/   	 0x5c,
	/*0E_FREQ1*/   	 0x76,
	/*0F_FREQ0*/   	 0x27,
	/*10_MDMCFG4*/ 	 0xAA,
	/*11_MDMCFG3*/ 	 0x39,
	/*12_MDMCFG2*/ 	 0x11,
	/*13_MDMCFG1*/ 	 0x23,
	/*14_MDMCFG0*/ 	 0x7a,
	/*15_DEVIATN*/ 	 0x42
};

static const uint8_t frskyx_conf[]= {
	/*02_IOCFG0*/  	 0x06,
	/*00_IOCFG2*/  	 0x06,
	/*17_MCSM1*/   	 0x0F,
	/*18_MCSM0*/   	 0x18,
	/*06_PKTLEN*/  	 0x1E,
	/*07_PKTCTRL1*/	 0x04,
	/*08_PKTCTRL0*/	 0x01,
	/*3E_PATABLE*/ 	 0xff,
	/*0B_FSCTRL1*/ 	 0x0A,
	/*0C_FSCTRL0*/ 	 0x00,
	/*0D_FREQ2*/   	 0x5c,
	/*0E_FREQ1*/   	 0x76,
	/*0F_FREQ0*/   	 0x27,
	/*10_MDMCFG4*/ 	 0x7B,
	/*11_MDMCFG3*/ 	 0x61,
	/*12_MDMCFG2*/ 	 0x13,
	/*13_MDMCFG1*/ 	 0x23,
	/*14_MDMCFG0*/ 	 0x7a,
	/*15_DEVIATN*/ 	 0x51
};

static const uint8_t frskyx_eu_conf[]= {
	/*02_IOCFG0*/  	 0x06,
	/*00_IOCFG2*/  	 0x06,
	/*17_MCSM1*/   	 0x0F,
	/*18_MCSM0*/   	 0x18,
	/*06_PKTLEN*/  	 0x23,
	/*07_PKTCTRL1*/	 0x04,
	/*08_PKTCTRL0*/	 0x01,
	/*3E_PATABLE*/ 	 0xff,
	/*0B_FSCTRL1*/ 	 0x08,
	/*0C_FSCTRL0*/ 	 0x00,
	/*0D_FREQ2*/   	 0x5c,
	/*0E_FREQ1*/   	 0x80,
	/*0F_FREQ0*/   	 0x00,
	/*10_MDMCFG4*/ 	 0x7B,
	/*11_MDMCFG3*/ 	 0xF8,
	/*12_MDMCFG2*/ 	 0x03,
	/*13_MDMCFG1*/ 	 0x23,
	/*14_MDMCFG0*/ 	 0x7a,
	/*15_DEVIATN*/ 	 0x53
};

static const uint8_t frsky_conf_common[][2]= {
	{ CC2500_FOCCFG,   0x16 },
	{ CC2500_BSCFG,    0x6c },
	{ CC2500_AGCCTRL2, 0x43 },
	{ CC2500_AGCCTRL1, 0x40 },
	{ CC2500_AGCCTRL0, 0x91 },
	{ CC2500_FREND1,   0x56 },
	{ CC2500_FREND0,   0x10 },
	{ CC2500_FSCAL3,   0xa9 },
	{ CC2500_FSCAL2,   0x0A },
	{ CC2500_FSCAL1,   0x00 },
	{ CC2500_FSCAL0,   0x11 },
	{ CC2500_FSTEST,   0x59 },
	{ CC2500_TEST2,    0x88 },
	{ CC2500_TEST1,    0x31 },
	{ CC2500_TEST0,    0x0B },
	{ CC2500_FIFOTHR,  0x07 },
	{ CC2500_ADDR,     0x00 }
};

/* CRC table for FrSkyX */
static const uint16_t frskyx_crc_table[] = {
  0x0000,	0x1189,	0x2312,	0x329b,	0x4624,	0x57ad,	0x6536,	0x74bf,
  0x8c48,	0x9dc1,	0xaf5a,	0xbed3,	0xca6c,	0xdbe5,	0xe97e,	0xf8f7,
  0x1081,	0x0108,	0x3393,	0x221a,	0x56a5,	0x472c,	0x75b7,	0x643e,
  0x9cc9,	0x8d40,	0xbfdb,	0xae52,	0xdaed,	0xcb64,	0xf9ff,	0xe876,
  0x2102,	0x308b,	0x0210,	0x1399,	0x6726,	0x76af,	0x4434,	0x55bd,
  0xad4a,	0xbcc3,	0x8e58,	0x9fd1,	0xeb6e,	0xfae7,	0xc87c,	0xd9f5,
  0x3183,	0x200a,	0x1291,	0x0318,	0x77a7,	0x662e,	0x54b5,	0x453c,
  0xbdcb,	0xac42,	0x9ed9,	0x8f50,	0xfbef,	0xea66,	0xd8fd,	0xc974,
  0x4204,	0x538d,	0x6116,	0x709f,	0x0420,	0x15a9,	0x2732,	0x36bb,
  0xce4c,	0xdfc5,	0xed5e,	0xfcd7,	0x8868,	0x99e1,	0xab7a,	0xbaf3,
  0x5285,	0x430c,	0x7197,	0x601e,	0x14a1,	0x0528,	0x37b3,	0x263a,
  0xdecd,	0xcf44,	0xfddf,	0xec56,	0x98e9,	0x8960,	0xbbfb,	0xaa72,
  0x6306,	0x728f,	0x4014,	0x519d,	0x2522,	0x34ab,	0x0630,	0x17b9,
  0xef4e,	0xfec7,	0xcc5c,	0xddd5,	0xa96a,	0xb8e3,	0x8a78,	0x9bf1,
  0x7387,	0x620e,	0x5095,	0x411c,	0x35a3,	0x242a,	0x16b1,	0x0738,
  0xffcf,	0xee46,	0xdcdd,	0xcd54,	0xb9eb,	0xa862,	0x9af9,	0x8b70,
  0x8408,	0x9581,	0xa71a,	0xb693,	0xc22c,	0xd3a5,	0xe13e,	0xf0b7,
  0x0840,	0x19c9,	0x2b52,	0x3adb,	0x4e64,	0x5fed,	0x6d76,	0x7cff,
  0x9489,	0x8500,	0xb79b,	0xa612,	0xd2ad,	0xc324,	0xf1bf,	0xe036,
  0x18c1,	0x0948,	0x3bd3,	0x2a5a,	0x5ee5,	0x4f6c,	0x7df7,	0x6c7e,
  0xa50a,	0xb483,	0x8618,	0x9791,	0xe32e,	0xf2a7,	0xc03c,	0xd1b5,
  0x2942,	0x38cb,	0x0a50,	0x1bd9,	0x6f66,	0x7eef,	0x4c74,	0x5dfd,
  0xb58b,	0xa402,	0x9699,	0x8710,	0xf3af,	0xe226,	0xd0bd,	0xc134,
  0x39c3,	0x284a,	0x1ad1,	0x0b58,	0x7fe7,	0x6e6e,	0x5cf5,	0x4d7c,
  0xc60c,	0xd785,	0xe51e,	0xf497,	0x8028,	0x91a1,	0xa33a,	0xb2b3,
  0x4a44,	0x5bcd,	0x6956,	0x78df,	0x0c60,	0x1de9,	0x2f72,	0x3efb,
  0xd68d,	0xc704,	0xf59f,	0xe416,	0x90a9,	0x8120,	0xb3bb,	0xa232,
  0x5ac5,	0x4b4c,	0x79d7,	0x685e,	0x1ce1,	0x0d68,	0x3ff3,	0x2e7a,
  0xe70e,	0xf687,	0xc41c,	0xd595,	0xa12a,	0xb0a3,	0x8238,	0x93b1,
  0x6b46,	0x7acf,	0x4854,	0x59dd,	0x2d62,	0x3ceb,	0x0e70,	0x1ff9,
  0xf78f,	0xe606,	0xd49d,	0xc514,	0xb1ab,	0xa022,	0x92b9,	0x8330,
  0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78
};

/** 
 * Set the initialization config
 * @param[in] protocol The protocol to initialize
 */
void frsky_set_config(enum frsky_protocol_t protocol) {
	uint16_t size = sizeof(frsky_conf_addr)/sizeof(frsky_conf_addr[0]);
	const uint8_t *frsky_conf = frskyx_conf;

	switch(protocol) {
		case FRSKYV:
			frsky_conf = frskyv_conf;
			break;
		case FRSKYD:
			frsky_conf = frskyd_conf;
			break;
		case FRSKYX:
			frsky_conf = frskyx_conf;
			break;
		case FRSKYX_EU:
			frsky_conf = frskyx_eu_conf;
			break;
	}

	for(uint8_t i = 0; i < size; i++) {
		cc_write_register(frsky_conf_addr[i], frsky_conf[i]);
	}

	size = sizeof(frsky_conf_common)/sizeof(frsky_conf_common[0]);
	for(uint8_t i = 0; i < size; i++) {
		cc_write_register(frsky_conf_common[i][0], frsky_conf_common[i][1]);
	}
}

/**
 * Tune a specific channel for the CC2500 and wait for completion
 * @param[in] ch The channel to tune
 */
void frsky_tune_channel(uint8_t ch) {
	cc_strobe(CC2500_SIDLE);
	cc_write_register(CC2500_CHANNR, ch);
	cc_strobe(CC2500_SCAL);

	// Wait for calibration to end (busy sleep, because can be called in interrupt)
	while (cc_read_register(CC2500_MARCSTATE) != 0x01)
		usleep(100);
}

/**
 * Tune multiple channels and fetch the output
 * @param[in] *channels An array of channels that needs to be tuned
 * @param[in] length The amount of channels in the channels array
 * @param[out] *fscal1 An output array of the same size as the channels with the FSCAL1 values per channel
 * @param[out] *fscal2 The FSCAL2 value (only a single value, not an array)
 * @param[out] *fscal3 The FSCAL2 value (only a single value, not an array)
 */
void frsky_tune_channels(uint8_t *channels, uint8_t length, uint8_t *fscal1, uint8_t *fscal2, uint8_t *fscal3) {
	for(uint8_t i = 0; i < length; i++) {
		frsky_tune_channel(channels[i]);

		fscal1[i] = cc_read_register(CC2500_FSCAL1);
	}

	// FSCAL2 and FSCAL3 only need to be read out once
	*fscal2 = cc_read_register(CC2500_FSCAL2);
	*fscal3 = cc_read_register(CC2500_FSCAL3);

	cc_strobe(CC2500_SIDLE);
}

/**
 * Calculate the CRC for FrSkyX packets
 * @param[in] data The data over which the crc needs to be calculated
 * @param[in] length The length of the input data
 * @return The calculate crc-16
 */
uint16_t frskyx_crc(const uint8_t *data, uint8_t length) {
	uint16_t crc = 0;
  for(uint8_t i = 0; i < length; i++)
    crc = (crc<<8) ^ frskyx_crc_table[((uint8_t)(crc>>8) ^ data[i]) & 0xFF];
  return crc;
}