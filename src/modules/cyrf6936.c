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

#include <unistd.h>
#include <libopencm3/cm3/common.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/exti.h>

#include "cyrf6936.h"
#include "counter.h"
#include "config.h"
#include "timer.h"

/* The CYRF receive and send callbacks */
cyrf_on_event _cyrf_recv_callback = NULL;
cyrf_on_event _cyrf_send_callback = NULL;

/* The pin for selecting the device */
#define CYRF_CS_HI() gpio_set(CYRF_DEV_SS_PORT, CYRF_DEV_SS_PIN)
#define CYRF_CS_LO() gpio_clear(CYRF_DEV_SS_PORT, CYRF_DEV_SS_PIN)

/* Internal functions */
static void cyrf_process(void);

/**
 * Initialize the CYRF6936
 */
void cyrf_init(void) {
	DEBUG(cyrf6936, "Initializing");
	/* Initialize the clocks */
	rcc_periph_clock_enable(_SPI(CYRF_DEV_SPI, CLK));
	rcc_periph_clock_enable(CYRF_DEV_RST_CLK);

	/* Initialize the GPIO */
	gpio_set_mode(CYRF_DEV_RST_PORT, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, CYRF_DEV_RST_PIN); 						//RST
	gpio_set_mode(CYRF_DEV_SS_PORT, GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_PUSHPULL, CYRF_DEV_SS_PIN); 						//SS
	gpio_set_mode(_SPI(CYRF_DEV_SPI, SCK_PORT), GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, _SPI(CYRF_DEV_SPI, SCK_PIN)); 					//SCK
	gpio_set_mode(_SPI(CYRF_DEV_SPI, MISO_PORT), GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT,
			_SPI(CYRF_DEV_SPI, MISO_PIN)); 												//MISO
	gpio_set_mode(_SPI(CYRF_DEV_SPI, MOSI_PORT), GPIO_MODE_OUTPUT_50_MHZ,
			GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, _SPI(CYRF_DEV_SPI, MOSI_PIN)); 				//MOSI

#ifdef CYRF_DEV_IRQ_PORT
	/* Enable the IRQ */
	rcc_peripheral_enable_clock(&RCC_APB2ENR, CYRF_DEV_IRQ_CLK); //IRQ
	gpio_set_mode(CYRF_DEV_IRQ_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT,
			CYRF_DEV_IRQ_PIN);
	exti_select_source(CYRF_DEV_IRQ_EXTI, CYRF_DEV_IRQ_PORT);
	exti_set_trigger(CYRF_DEV_IRQ_EXTI, EXTI_TRIGGER_FALLING);
	exti_enable_request(CYRF_DEV_IRQ_EXTI);

	// Enable the IRQ NVIC
	nvic_set_priority(CYRF_DEV_IRQ_NVIC, 1);
	nvic_enable_irq(CYRF_DEV_IRQ_NVIC);
#endif

	/* Reset SPI, SPI_CR1 register cleared, SPI is disabled */
	spi_reset(_SPI(CYRF_DEV_SPI, BUS));

	/* Set up SPI in Master mode with:
	 * Clock baud rate: 1/64 of peripheral clock frequency
	 * Clock polarity: Idle High
	 * Clock phase: Data valid on 2nd clock pulse
	 * Data frame format: 8-bit
	 * Frame format: MSB First
	 */
	spi_init_master(_SPI(CYRF_DEV_SPI, BUS), SPI_CR1_BAUDRATE_FPCLK_DIV_64,
			SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_1,
			SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);

	/* Set NSS management to software. */
	spi_enable_software_slave_management(_SPI(CYRF_DEV_SPI, BUS));
	spi_set_nss_high(_SPI(CYRF_DEV_SPI, BUS));

	/* Enable SPI periph. */
	spi_enable(_SPI(CYRF_DEV_SPI, BUS));

	/* Reset the CYRF chip */
	gpio_set(CYRF_DEV_RST_PORT, CYRF_DEV_RST_PIN);
	counter_wait_poll(counter_get_ticks_of_ms(150));
	gpio_clear(CYRF_DEV_RST_PORT, CYRF_DEV_RST_PIN);
	counter_wait_poll(counter_get_ticks_of_ms(300));

	/* Also a software reset */
	cyrf_write_register(CYRF_MODE_OVERRIDE, CYRF_RST);
	DEBUG(cyrf6936, "Initializing done");
}

/**
 * Poll the status registers if the IRQ pin isn connected
 */
void cyrf_run(void) {
#ifndef CYRF_DEV_IRQ_ISR
	static uint32_t start_ticks = 0;
	if(start_ticks + counter_get_ticks_of_us(200) <= counter_get_ticks()) {
		start_ticks = counter_get_ticks();
		cyrf_process();
	}
#endif
}

#ifdef CYRF_DEV_IRQ_ISR
/**
 * On interrupt request do a process of the register
 */
void CYRF_DEV_IRQ_ISR(void) {
	cyrf_process();
	exti_reset_request(CYRF_DEV_IRQ_EXTI);
}
#endif

/**
 * Process the CYRF requests */
static void cyrf_process(void) {
	uint8_t tx_irq_status, rx_irq_status;

	// Read the transmit IRQ
	tx_irq_status = cyrf_read_register(CYRF_TX_IRQ_STATUS);
	if((tx_irq_status & 0x3) == 0x2)
		tx_irq_status |= cyrf_read_register(CYRF_TX_IRQ_STATUS);
	if (((tx_irq_status & CYRF_TXC_IRQ))
			&& _cyrf_send_callback != NULL) {
		_cyrf_send_callback((tx_irq_status & CYRF_TXE_IRQ) > 0x0);
	}

	// Read the read IRQ
	rx_irq_status = cyrf_read_register(CYRF_RX_IRQ_STATUS);
	if((rx_irq_status & 0x3) == 0x2)
		rx_irq_status |= cyrf_read_register(CYRF_RX_IRQ_STATUS);
	if (((rx_irq_status & CYRF_RXC_IRQ))
			&& _cyrf_recv_callback != NULL) {
		cyrf_write_register(CYRF_RX_IRQ_STATUS, 0x80); // need to set RXOW before data read
		_cyrf_recv_callback((rx_irq_status & CYRF_RXE_IRQ) > 0x0);
	}
}

/**
 * Register the receive callback
 * @param[in] callback The callback when it receives an interrupt for receive
 */
void cyrf_register_recv_callback(cyrf_on_event callback) {
	_cyrf_recv_callback = callback;
}

/**
 * Register the send callback
 * @param[in] callback The callback when it receives an interrupt for send
 */
void cyrf_register_send_callback(cyrf_on_event callback) {
	_cyrf_send_callback = callback;
}

/**
 * Write a byte to the register
 * @param[in] address The one byte address number of the register
 * @param[in] data The one byte data that needs to be written to the address
 */
void cyrf_write_register(const uint8_t address, const uint8_t data) {
	cm_disable_interrupts();
	CYRF_CS_LO();
	spi_xfer(_SPI(CYRF_DEV_SPI, BUS), CYRF_DIR | address);
	spi_xfer(_SPI(CYRF_DEV_SPI, BUS), data);
	CYRF_CS_HI();
	cm_enable_interrupts();
}

/**
 * Write a block to the register
 * @param[in] address The one byte address number of the register
 * @param[in] data The data that needs to be written to the address
 * @param[in] length The length in bytes of the data that needs to be written
 */
void cyrf_write_block(const uint8_t address, const uint8_t data[], const int length) {
	int i;
	cm_disable_interrupts();
	CYRF_CS_LO();
	spi_xfer(_SPI(CYRF_DEV_SPI, BUS), CYRF_DIR | address);

	for (i = 0; i < length; i++)
		spi_xfer(_SPI(CYRF_DEV_SPI, BUS), data[i]);

	CYRF_CS_HI();
	cm_enable_interrupts();
}

/**
 * Read a byte from the register
 * @param[in] The one byte address of the register
 * @return The one byte data of the register
 */
uint8_t cyrf_read_register(const uint8_t address) {
	uint8_t data;
	cm_disable_interrupts();
	CYRF_CS_LO();
	spi_xfer(_SPI(CYRF_DEV_SPI, BUS), address);
	data = spi_xfer(_SPI(CYRF_DEV_SPI, BUS), 0);
	CYRF_CS_HI();
	cm_enable_interrupts();
	return data;
}

/**
 * Read a block from the register
 * @param[in] address The one byte address of the register
 * @param[out] data The data that was received from the register
 * @param[in] length The length in bytes what needs to be read
 */
void cyrf_read_block(const uint8_t address, uint8_t data[], const int length) {
	int i;
	cm_disable_interrupts();
	CYRF_CS_LO();
	spi_xfer(_SPI(CYRF_DEV_SPI, BUS), address);

	for (i = 0; i < length; i++)
		data[i] = spi_xfer(_SPI(CYRF_DEV_SPI, BUS), 0);

	CYRF_CS_HI();
	cm_enable_interrupts();
}

/**
 * Read the MFG id from the register
 * @param[out] The MFG id from the device
 */
void cyrf_get_mfg_id(uint8_t *mfg_id) {
	cyrf_write_register(CYRF_MFG_ID, 0xFF);
	cyrf_read_block(CYRF_MFG_ID, mfg_id, 6);
	cyrf_write_register(CYRF_MFG_ID, 0x00);
	DEBUG(cyrf6936, "READ MFG_ID: 0x%02X%02X%02X%02X%02X%02X", mfg_id[0], mfg_id[1], mfg_id[2], mfg_id[3], mfg_id[4], mfg_id[5]);
}

/**
 * Get the RSSI (signal strength) of the last received packet
 * @return The RSSI of the last received packet
 */
uint8_t cyrf_get_rssi(void) {
	return cyrf_read_register(CYRF_RSSI) & 0x0F;
}

/**
 * Get the RX status
 * @return The RX status register
 */
uint8_t cyrf_get_rx_status(void) {
	return cyrf_read_register(CYRF_RX_STATUS);
}

/**
 * Set multiple (config) values at once
 * @param[in] config An array of len by 2, consisting the register address and the values
 * @param[in] length The length of the config array
 */
void cyrf_set_config_len(const uint8_t config[][2], const uint8_t length) {
	int i;
	for (i = 0; i < length; i++) {
		cyrf_write_register(config[i][0], config[i][1]);
		DEBUG(cyrf6936, "WRITE 0x%02X: 0x%02X", config[i][0], config[i][1]);
	}
}

/**
 * Set the RF channel
 * @param[in] chan The channel needs to be set
 */
void cyrf_set_channel(const uint8_t chan) {
	cyrf_write_register(CYRF_CHANNEL, chan);
	DEBUG(cyrf6936, "WRITE CHANNEL: 0x%02X", chan);
}

/**
 * Set the power
 * @param[in] power The power that needs to be set
 */
void cyrf_set_power(const uint8_t power) {
	uint8_t tx_cfg = cyrf_read_register(CYRF_TX_CFG) & (0xFF - CYRF_PA_4);
	cyrf_write_register(CYRF_TX_CFG, tx_cfg | power);
	DEBUG(cyrf6936, "WRITE POWER: 0x%02X (0x%02X)", power, tx_cfg);
}

/**
 * Set the mode
 * @param[in] mode The mode that the chip needs to be set to
 * @param[in] force Force the mode switch
 */
void cyrf_set_mode(const uint8_t mode, const bool force) {
	if (force)
		cyrf_write_register(CYRF_XACT_CFG, mode | CYRF_FRC_END);
	else
		cyrf_write_register(CYRF_XACT_CFG, mode);

	DEBUG(cyrf6936, "WRITE MODE: 0x%02X (0x%02X)", mode, force);
}

/**
 * Set the CRC seed
 * @param[in] crc The 16-bit CRC seed
 */
void cyrf_set_crc_seed(const uint16_t crc) {
	cyrf_write_register(CYRF_CRC_SEED_LSB, crc & 0xff);
	cyrf_write_register(CYRF_CRC_SEED_MSB, crc >> 8);

	DEBUG(cyrf6936, "WRITE CRC: 0x%02X LSB 0x%02X MSB", crc & 0xff, crc >> 8);
}

/**
 * Set the SOP code
 * @param[in] sopcode The 8 bytes SOP code
 */
void cyrf_set_sop_code(const uint8_t *sopcode) {
	cyrf_write_block(CYRF_SOP_CODE, sopcode, 8);

	DEBUG(cyrf6936, "WRITE SOP_CODE: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
			sopcode[0], sopcode[1], sopcode[2], sopcode[3], sopcode[4], sopcode[5], sopcode[6], sopcode[7]);
}

/**
 * Set the data code
 * @param[in] datacode The 16 bytes data code
 */
void cyrf_set_data_code(const uint8_t *datacode) {
	cyrf_write_block(CYRF_DATA_CODE, datacode, 16);

	DEBUG(cyrf6936, "WRITE DATA_CODE: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
			datacode[0], datacode[1], datacode[2], datacode[3], datacode[4], datacode[5], datacode[6], datacode[7],
			datacode[8], datacode[9], datacode[10], datacode[11], datacode[12], datacode[13], datacode[14], datacode[15]);
}

/**
 * Set the 8 byte data code
 * @param[in] datacode The 16 bytes data code
 */
void cyrf_set_data_code_small(const uint8_t *datacode) {
	cyrf_write_block(CYRF_DATA_CODE, datacode, 8);
	DEBUG(cyrf6936, "WRITE DATA_CODE: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
			datacode[0], datacode[1], datacode[2], datacode[3], datacode[4], datacode[5], datacode[6], datacode[7]);
}

/**
 * Set the preamble
 * @param[in] preamble The 3 bytes preamble
 */
void cyrf_set_preamble(const uint8_t *preamble) {
	cyrf_write_block(CYRF_PREAMBLE, preamble, 3);
	DEBUG(cyrf6936, "WRITE PREAMBLE: 0x%02X 0x%02X 0x%02X",
			preamble[0], preamble[1], preamble[2]);
}

/**
 * Set the Framing config
 * @param[in] config The framing config register
 */
void cyrf_set_framing_cfg(const uint8_t config) {
	cyrf_write_register(CYRF_FRAMING_CFG, config);
	DEBUG(cyrf6936, "WRITE FRAMING: 0x%02X", config);
}

/**
 * Set the RX config
 * @param[in] config The RX config register
 */
void cyrf_set_rx_cfg(const uint8_t config) {
	cyrf_write_register(CYRF_RX_CFG, config);
	DEBUG(cyrf6936, "WRITE RX_CFG: 0x%02X", config);
}

/**
 * Set the TX config
 * @param[in] config The TX config register
 */
void cyrf_set_tx_cfg(const uint8_t config) {
	cyrf_write_register(CYRF_TX_CFG, config);
	DEBUG(cyrf6936, "WRITE TX_CFG: 0x%02X", config);
}

/*
 * Set the RX override
 * @param[in] override The RX override register
 */
void cyrf_set_rx_override(const uint8_t override) {
	cyrf_write_register(CYRF_RX_OVERRIDE, override);
	DEBUG(cyrf6936, "WRITE RX_OVERRIDE: 0x%02X", override);
}

/*
 * Set the TX override
 * @param[in] override The TX override register
 */
void cyrf_set_tx_override(const uint8_t override) {
	cyrf_write_register(CYRF_TX_OVERRIDE, override);
	DEBUG(cyrf6936, "WRITE TX_OVERRIDE: 0x%02X", override);
}

/*
 * Send a data packet with length
 * @param[in] data The data of the packet
 * @param[in] length The length of the data
 */
void cyrf_send_len(const uint8_t *data, const uint8_t length) {
	cyrf_write_register(CYRF_TX_LENGTH, length);
	cyrf_write_register(CYRF_TX_CTRL, CYRF_TX_CLR);
	cyrf_write_block(CYRF_TX_BUFFER, data, length);
	cyrf_write_register(CYRF_TX_CTRL, CYRF_TX_GO | CYRF_TXC_IRQEN | CYRF_TXE_IRQEN);
}

/**
 * Send a 16 byte data packet
 * @param[in] data The 16 byte data of the packet
 */
void cyrf_send(const uint8_t *data) {
	cyrf_send_len(data, 16);
	DEBUG(cyrf6936, "SEND");
}

/**
 * Resends the previous packet
 */
void cyrf_resend(void) {
	cyrf_write_register(CYRF_TX_CTRL, CYRF_TX_GO | CYRF_TXC_IRQEN | CYRF_TXE_IRQEN);
	DEBUG(cyrf6936, "RESEND");
}

/**
 * Start receiving mode and set IRQ
 */
void cyrf_start_recv(void) {
	cyrf_write_register(CYRF_RX_IRQ_STATUS, CYRF_RXOW_IRQ); // Clear the RX overwrite
	cyrf_write_register(CYRF_RX_CTRL, CYRF_RX_GO | CYRF_RXC_IRQEN | CYRF_RXE_IRQEN); // Start receiving and set the IRQ
	DEBUG(cyrf6936, "START RECEIVE");
}

/**
 * Abort the receiving
 */
void cyrf_abort_recv(void) {
	cyrf_set_mode(CYRF_MODE_SYNTH_RX, true);
	cyrf_write_register(CYRF_RX_ABORT, 0x00);
}

/**
 * Start transmitting mode
 */
void cyrf_start_transmit(void) {
	cyrf_set_mode(CYRF_MODE_SYNTH_TX, 1);
	DEBUG(cyrf6936, "START TRANSMIT");
}

/**
 * Receive the packet from the RX buffer with length
 * @param[out] data The data from the RX buffer
 * @param[in] length The length of data that is received from the RX buffer
 */
void cyrf_recv_len(uint8_t *data, const uint8_t length) {
	cyrf_read_block(CYRF_RX_BUFFER, data, length);
}

/**
 * Receive a 16 byte packet from the RX buffer
 * @param[out] data The 16 byte data from the RX buffer
 */
void cyrf_recv(uint8_t *data) {
	cyrf_recv_len(data, 16);
}
