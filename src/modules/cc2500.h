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

#ifndef MODULES_CC2500_H_
#define MODULES_CC2500_H_

// Include the board specifications for the CC2500 defines
#include "board.h"

enum {
    CC2500_IOCFG2           = 0x00,        // GDO2 output pin configuration
    CC2500_IOCFG1           = 0x01,        // GDO1 output pin configuration
    CC2500_IOCFG0           = 0x02,        // GDO0 output pin configuration
    CC2500_FIFOTHR          = 0x03,        // RX FIFO and TX FIFO thresholds
    CC2500_SYNC1            = 0x04,        // Sync word, high byte
    CC2500_SYNC0            = 0x05,        // Sync word, low byte
    CC2500_PKTLEN           = 0x06,        // Packet length
    CC2500_PKTCTRL1         = 0x07,        // Packet automation control
    CC2500_PKTCTRL0         = 0x08,        // Packet automation control
    CC2500_ADDR             = 0x09,        // Device address
    CC2500_CHANNR           = 0x0A,        // Channel number
    CC2500_FSCTRL1          = 0x0B,        // Frequency synthesizer control
    CC2500_FSCTRL0          = 0x0C,        // Frequency synthesizer control
    CC2500_FREQ2            = 0x0D,        // Frequency control word, high byte
    CC2500_FREQ1            = 0x0E,        // Frequency control word, middle byte
    CC2500_FREQ0            = 0x0F,        // Frequency control word, low byte
    CC2500_MDMCFG4          = 0x10,        // Modem configuration
    CC2500_MDMCFG3          = 0x11,        // Modem configuration
    CC2500_MDMCFG2          = 0x12,        // Modem configuration
    CC2500_MDMCFG1          = 0x13,        // Modem configuration
    CC2500_MDMCFG0          = 0x14,        // Modem configuration
    CC2500_DEVIATN          = 0x15,        // Modem deviation setting
    CC2500_MCSM2            = 0x16,        // Main Radio Cntrl State Machine config
    CC2500_MCSM1            = 0x17,        // Main Radio Cntrl State Machine config
    CC2500_MCSM0            = 0x18,        // Main Radio Cntrl State Machine config
    CC2500_FOCCFG           = 0x19,        // Frequency Offset Compensation config
    CC2500_BSCFG            = 0x1A,        // Bit Synchronization configuration
    CC2500_AGCCTRL2         = 0x1B,        // AGC control
    CC2500_AGCCTRL1         = 0x1C,        // AGC control
    CC2500_AGCCTRL0         = 0x1D,        // AGC control
    CC2500_WOREVT1          = 0x1E,        // High byte Event 0 timeout
    CC2500_WOREVT0          = 0x1F,        // Low byte Event 0 timeout
    CC2500_WORCTRL          = 0x20,        // Wake On Radio control
    CC2500_FREND1           = 0x21,        // Front end RX configuration
    CC2500_FREND0           = 0x22,        // Front end TX configuration
    CC2500_FSCAL3           = 0x23,        // Frequency synthesizer calibration
    CC2500_FSCAL2           = 0x24,        // Frequency synthesizer calibration
    CC2500_FSCAL1           = 0x25,        // Frequency synthesizer calibration
    CC2500_FSCAL0           = 0x26,        // Frequency synthesizer calibration
    CC2500_RCCTRL1          = 0x27,        // RC oscillator configuration
    CC2500_RCCTRL0          = 0x28,        // RC oscillator configuration
    CC2500_FSTEST           = 0x29,        // Frequency synthesizer cal control
    CC2500_PTEST            = 0x2A,        // Production test
    CC2500_AGCTEST          = 0x2B,        // AGC test
    CC2500_TEST2            = 0x2C,        // Various test settings
    CC2500_TEST1            = 0x2D,        // Various test settings
    CC2500_TEST0            = 0x2E,        // Various test settings

// Status registers
    CC2500_PARTNUM          = 0x70,        // Part number
    CC2500_VERSION          = 0x71,        // Current version number
    CC2500_FREQEST          = 0x72,        // Frequency offset estimate
    CC2500_LQI              = 0x73,        // Demodulator estimate for link quality
    CC2500_RSSI             = 0x74,        // Received signal strength indication
    CC2500_MARCSTATE        = 0x75,        // Control state machine state
    CC2500_WORTIME1         = 0x76,        // High byte of WOR timer
    CC2500_WORTIME0         = 0x77,        // Low byte of WOR timer
    CC2500_PKTSTATUS        = 0x78,        // Current GDOx status and packet status
    CC2500_VCO_VC_DAC       = 0x79,        // Current setting from PLL cal module
    CC2500_TXBYTES          = 0x7A,        // Underflow and # of bytes in TXFIFO
    CC2500_RXBYTES          = 0x7B,        // Overflow and # of bytes in RXFIFO

// Multi byte memory locations
    CC2500_PATABLE          = 0x3E,
    CC2500_TXFIFO           = 0x3F,
    CC2500_RXFIFO           = 0x3F,
    CC2500_FIFO             = 0x3F,
};

// Definitions for burst/single access to registers
#define CC2500_WRITE_SINGLE     0x00
#define CC2500_WRITE_BURST      0x40
#define CC2500_READ_SINGLE      0x80
#define CC2500_READ_BURST       0xC0

// Strobe commands
#define CC2500_SRES             0x30        // Reset chip.
#define CC2500_SFSTXON          0x31        // Enable and calibrate frequency synthesizer (if MCSM0.FS_AUTOCAL=1).
                                            // If in RX/TX: Go to a wait state where only the synthesizer is
                                            // running (for quick RX / TX turnaround).
#define CC2500_SXOFF            0x32        // Turn off crystal oscillator.
#define CC2500_SCAL             0x33        // Calibrate frequency synthesizer and turn it off
                                            // (enables quick start).
#define CC2500_SRX              0x34        // Enable RX. Perform calibration first if coming from IDLE and
                                            // MCSM0.FS_AUTOCAL=1.
#define CC2500_STX              0x35        // In IDLE state: Enable TX. Perform calibration first if
                                            // MCSM0.FS_AUTOCAL=1. If in RX state and CCA is enabled:
                                            // Only go to TX if channel is clear.
#define CC2500_SIDLE            0x36        // Exit RX / TX, turn off frequency synthesizer and exit
                                            // Wake-On-Radio mode if applicable.
#define CC2500_SAFC             0x37        // Perform AFC adjustment of the frequency synthesizer
#define CC2500_SWOR             0x38        // Start automatic RX polling sequence (Wake-on-Radio)
#define CC2500_SPWD             0x39        // Enter power down mode when CSn goes high.
#define CC2500_SFRX             0x3A        // Flush the RX FIFO buffer.
#define CC2500_SFTX             0x3B        // Flush the TX FIFO buffer.
#define CC2500_SWORRST          0x3C        // Reset real time clock.
#define CC2500_SNOP             0x3D        // No operation. May be used to pad strobe commands to two
                                            // bytes for simpler software.
//----------------------------------------------------------------------------------
// Chip Status Byte
//----------------------------------------------------------------------------------

// Bit fields in the chip status byte
#define CC2500_STATUS_CHIP_RDYn_BM             0x80
#define CC2500_STATUS_STATE_BM                 0x70
#define CC2500_STATUS_FIFO_BYTES_AVAILABLE_BM  0x0F

// Chip states
#define CC2500_STATE_IDLE                      0x00
#define CC2500_STATE_RX                        0x10
#define CC2500_STATE_TX                        0x20
#define CC2500_STATE_FSTXON                    0x30
#define CC2500_STATE_CALIBRATE                 0x40
#define CC2500_STATE_SETTLING                  0x50
#define CC2500_STATE_RX_OVERFLOW               0x60
#define CC2500_STATE_TX_UNDERFLOW              0x70

//----------------------------------------------------------------------------------
// Other register bit fields
//----------------------------------------------------------------------------------
#define CC2500_LQI_CRC_OK_BM                   0x80
#define CC2500_LQI_EST_BM                      0x7F

// adress checks
#define CC2500_PKTCTRL1_FLAG_ADR_CHECK_00 ((0<<1) | (0<<0))
#define CC2500_PKTCTRL1_FLAG_ADR_CHECK_01 ((0<<1) | (1<<0))
#define CC2500_PKTCTRL1_FLAG_ADR_CHECK_10 ((1<<1) | (0<<0))
#define CC2500_PKTCTRL1_FLAG_ADR_CHECK_11 ((1<<1) | (1<<0))
#define CC2500_PKTCTRL1_APPEND_STATUS     (1<<2)
#define CC2500_PKTCTRL1_CRC_AUTOFLUSH     (1<<3)

enum cc2500_mode_t {
    CC2500_TXRX_OFF = 0,
    CC2500_TXRX_TX,
    CC2500_TXRX_RX
};

/* External functions and variables */
void cc_init(void);
void cc_run(void);

typedef void (*cc_on_event)(uint8_t len);
void cc_register_recv_callback(cc_on_event callback);
void cc_register_send_callback(cc_on_event callback);

void cc_write_register(const uint8_t address, const uint8_t data);
void cc_write_block(const uint8_t address, const uint8_t data[], const int length);
uint8_t cc_read_register(const uint8_t address);
void cc_read_block(const uint8_t address, uint8_t *data, const int length);

bool cc_reset(void);
void cc_get_mfg_id(uint8_t *mfg_id);
void cc_strobe(uint8_t cmd);
void cc_write_data(uint8_t *packet, uint8_t length);
void cc_read_data(uint8_t *packet, uint8_t length);
void cc_set_mode(enum cc2500_mode_t mode);
void cc_set_power(uint8_t power);
void cc_handle_overflows(void);

#endif /* MODULES_CC2500_H_ */
