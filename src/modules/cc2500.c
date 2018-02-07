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

#include <unistd.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/gpio.h>

#include "cc2500.h"
#include "modules/counter.h"
#include "modules/config.h"
#include "modules/spi.h"

/* The CC2500 receive and send callbacks */
cc_on_event _cc_recv_callback = NULL;
cc_on_event _cc_send_callback = NULL;

/* The pin for selecting the device */
#define CC_CS_HI() gpio_set(CC_DEV_SS_PORT, CC_DEV_SS_PIN)
#define CC_CS_LO() gpio_clear(CC_DEV_SS_PORT, CC_DEV_SS_PIN)

/* Internal functions and settings */
static void cc_process(void);
static uint8_t cc_tx_bytes = 0;

/**
 * Initialize the CC2500
 */
void cc_init(void) {
	DEBUG(cc, "Initializing");

	/* Initialize the GPIO */
	gpio_set_mode(CC_DEV_SS_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, CC_DEV_SS_PIN);

	/* Also a software reset */
	cc_reset();
	DEBUG(cc, "Initializing done");
}

/**
 * Poll the status registers to check if packet is end/received
 */
void cc_run(void) {
	static uint32_t start_ticks = 0;
	if(start_ticks + counter_get_ticks_of_us(200) <= counter_get_ticks()) {
		start_ticks = counter_get_ticks();
		cc_process();
	}
}

/**
 * Process the CC2500 requests
 * We assume variable packet length mode and appended status, which means 3 extra bytes
 */
static void cc_process(void) {
	cc_handle_overflows();

	/* Handle RX callback */
	uint8_t len = cc_read_register(CC2500_RXBYTES) & 0x7F;
	if(len && len != 1) {
		uint8_t len2 = cc_read_register(CC2500_RXBYTES) & 0x7F;
		if(len == len2 && _cc_recv_callback != NULL) {
			_cc_recv_callback(len);
		}
	}

	/* Handle TX callback */
	if(cc_tx_bytes > 0) {
		uint8_t txlen = cc_read_register(CC2500_TXBYTES) & 0x7F;
		if(txlen == 0) {
			txlen = cc_read_register(CC2500_TXBYTES) & 0x7F;
			if(txlen == 0) {
				cc_tx_bytes = 0;

				if(_cc_send_callback != NULL)
					_cc_send_callback(0);
			}
		}
	}
}

/**
 * Register the receive callback
 * @param[in] callback The callback when it receives an interrupt for receive
 */
void cc_register_recv_callback(cc_on_event callback) {
	_cc_recv_callback = callback;
}

/**
 * Register the send callback
 * @param[in] callback The callback when it receives an interrupt for send
 */
void cc_register_send_callback(cc_on_event callback) {
	_cc_send_callback = callback;
}

/**
 * Write a byte to the register
 * @param[in] address The one byte address number of the register
 * @param[in] data The one byte data that needs to be written to the address
 */
void cc_write_register(const uint8_t address, const uint8_t data) {
	cm_disable_interrupts();
	CC_CS_LO();
	spi_xfer(CC_DEV_SPI, address);
	spi_xfer(CC_DEV_SPI, data);
	CC_CS_HI();
	cm_enable_interrupts();
}

/**
 * Write a block to the register
 * @param[in] address The one byte address number of the register
 * @param[in] data The data that needs to be written to the address
 * @param[in] length The length in bytes of the data that needs to be written
 */
void cc_write_block(const uint8_t address, const uint8_t data[], const int length) {
	int i;
	cm_disable_interrupts();
	CC_CS_LO();
	spi_xfer(CC_DEV_SPI, CC2500_WRITE_BURST | address);

	for (i = 0; i < length; i++)
		spi_xfer(CC_DEV_SPI, data[i]);

	CC_CS_HI();
	cm_enable_interrupts();
}

/**
 * Read a byte from the register
 * @param[in] The one byte address of the register
 * @return The one byte data of the register
 */
uint8_t cc_read_register(const uint8_t address) {
	uint8_t data;
	cm_disable_interrupts();
	CC_CS_LO();
	spi_xfer(CC_DEV_SPI, CC2500_READ_SINGLE | address);
	data = spi_xfer(CC_DEV_SPI, 0);
	CC_CS_HI();
	cm_enable_interrupts();
	return data;
}

/**
 * Read a block from the register
 * @param[in] address The one byte address of the register
 * @param[out] data The data that was received from the register
 * @param[in] length The length in bytes what needs to be read
 */
void cc_read_block(const uint8_t address, uint8_t data[], const int length) {
	int i;
	cm_disable_interrupts();
	CC_CS_LO();
	spi_xfer(CC_DEV_SPI, CC2500_READ_BURST | address);

	for (i = 0; i < length; i++)
		data[i] = spi_xfer(CC_DEV_SPI, 0);

	CC_CS_HI();
	cm_enable_interrupts();
}

/**
 * Reset the CC2500 chip
 * @return Wheter it was reset succesfull
 */
bool cc_reset(void) {
	cc_strobe(CC2500_SRES);
	counter_wait_poll(counter_get_ticks_of_ms(1));
	cc_set_mode(CC2500_TXRX_OFF);

	return cc_read_register(CC2500_FREQ1) == 0xC4;
}

/**
 * Read the MFG id from the register
 * @param[out] mfg_id The MFG id from the device
 */
void cc_get_mfg_id(uint8_t *mfg_id) {
	mfg_id[0] = cc_read_register(CC2500_PARTNUM);
	mfg_id[1] = cc_read_register(CC2500_VERSION);
	DEBUG(cc, "READ MFG_ID: 0x%02X%02X", mfg_id[0], mfg_id[1]);
}

/**
 * Strobe a command
 * @param[in] cmd The command to execute
 */
void cc_strobe(uint8_t cmd) {
	cm_disable_interrupts();
	CC_CS_LO();
  spi_xfer(CC_DEV_SPI, cmd);
	CC_CS_HI();
	cm_enable_interrupts();
}

/**
 * Write the packet data to the chip
 * @param[in] *packet The packet data to send
 * @param[in] length The packet length
 */
void cc_write_data(uint8_t *packet, uint8_t length) {
	cc_strobe(CC2500_SFTX);
	cc_write_block(CC2500_TXFIFO, packet, length);
	cc_tx_bytes = length;
	cc_strobe(CC2500_STX);
}

/**
 * Read the packet data from the chip
 * @param[out] *packet The read packet bytes
 * @param[in] *length The length to read
 */
void cc_read_data(uint8_t *packet, uint8_t length) {
	cc_read_block(CC2500_RXFIFO, packet, length);
}

/**
 * Set the transmission mode of the chip
 * @param[in] mode The mode to put the chip in (OFF, RX, TX)
 */
void cc_set_mode(enum cc2500_mode_t mode) {
	if(mode == CC2500_TXRX_TX) {
		cc_write_register(CC2500_IOCFG2, 0x2F);
		cc_write_register(CC2500_IOCFG0, 0x2F | 0x40);
	} else if (mode == CC2500_TXRX_RX) {
		cc_write_register(CC2500_IOCFG0, 0x2F);
		cc_write_register(CC2500_IOCFG2, 0x2F | 0x40);
	} else {
		cc_write_register(CC2500_IOCFG0, 0x2F);
		cc_write_register(CC2500_IOCFG2, 0x2F);
	}
}

/**
 * Set the output power of the chip
 * @param[in] power The ouput power to set
 */
void cc_set_power(uint8_t power) {
	const unsigned char patable[8]=
	{
		0x60, // -30dbm
		0xA0, // -25dbm
		0x46, // -20dbm
		0x57, // -15dbm
		0x97, // -10dbm
		0xE7, //  -5dbm
		0xFE, //   0dbm
		0xFF  // 1.5dbm
	};
	if (power > 7)
		power = 7;

	cc_write_register(CC2500_PATABLE, patable[power]);
}

/**
 * Handle overflows in the TX and RX buffers
 */
void cc_handle_overflows(void) {
    uint8_t marc_state = cc_read_register(CC2500_MARCSTATE) & 0x1F;
    if (marc_state == 0x11)
        cc_strobe(CC2500_SFRX);
    else if (marc_state == 0x16)
        cc_strobe(CC2500_SFTX);
}