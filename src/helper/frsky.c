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

/* The common starting register addresses */
static const uint8_t frsky_conf_addr[]= {
	CC2500_02_IOCFG0,
	CC2500_00_IOCFG2,
	CC2500_17_MCSM1,
	CC2500_18_MCSM0,
	CC2500_06_PKTLEN,
	CC2500_07_PKTCTRL1,
	CC2500_08_PKTCTRL0,
	CC2500_3E_PATABLE,
	CC2500_0B_FSCTRL1,
	CC2500_0C_FSCTRL0,
	CC2500_0D_FREQ2,	
	CC2500_0E_FREQ1,
	CC2500_0F_FREQ0,
	CC2500_10_MDMCFG4,
	CC2500_11_MDMCFG3,
	CC2500_12_MDMCFG2,
	CC2500_13_MDMCFG1,
	CC2500_14_MDMCFG0,
	CC2500_15_DEVIATN
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
	/*17_MCSM1*/   	 0x0c,
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
	/*17_MCSM1*/   	 0x0E,
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
	{ CC2500_19_FOCCFG,   0x16 },
	{ CC2500_1A_BSCFG,    0x6c },
	{ CC2500_1B_AGCCTRL2, 0x43 },
	{ CC2500_1C_AGCCTRL1, 0x40 },
	{ CC2500_1D_AGCCTRL0, 0x91 },
	{ CC2500_21_FREND1,   0x56 },
	{ CC2500_22_FREND0,   0x10 },
	{ CC2500_23_FSCAL3,   0xa9 },
	{ CC2500_24_FSCAL2,   0x0A },
	{ CC2500_25_FSCAL1,   0x00 },
	{ CC2500_26_FSCAL0,   0x11 },
	{ CC2500_29_FSTEST,   0x59 },
	{ CC2500_2C_TEST2,    0x88 },
	{ CC2500_2D_TEST1,    0x31 },
	{ CC2500_2E_TEST0,    0x0B },
	{ CC2500_03_FIFOTHR,  0x07 },
	{ CC2500_09_ADDR,     0x00 }
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
 * Tune a specific FrSky channel
 */
void frsky_tune_channel(uint8_t ch) {
	cc_strobe(CC2500_SIDLE);
	cc_write_register(CC2500_0A_CHANNR, ch);
	cc_strobe(CC2500_SCAL);

	// Wait for calibration to end
	while (cc_read_register(CC2500_35_MARCSTATE) != 0x01)
		counter_wait_poll(counter_get_ticks_of_us(100));
}