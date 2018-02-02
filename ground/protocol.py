#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
from enum import IntEnum
import rfchip
import struct
import transmitter

class ProtState(IntEnum):
	DISABLED = 0
	MINIMUM = 1
	AVERAGE = 2
	MAXIMUM = 3

class Protocol():

	def __init__(self, name):
		self.state = ProtState.AVERAGE
		self.name = name
		self.scan_times = {}
		self.channels = {}
		for state in ProtState:
			self.scan_times[state] = 0
			self.channels[state] = set()

	def get_scan_time(self):
		return self.scan_times[self.state]

	def get_channels(self):
		return self.channels[self.state]


class DSMX(Protocol):
	CHAN_TIME = 8500*23								# Amount of time before reapearance per channel (us)
	CHAN_USED = 23												# Amount of channels in used
	CHAN_MIN = 3													# Lowest occuring channel number
	CHAN_MAX = 76													# Highest occuring channel number
	CHAN_SEARCH_MIN = 24-8+1							# Minimum number of channel to search before single occurance
	CHAN_SEARCH_AVG = 24									# Scan the lowest range where at least 8 channels must occur
	CHAN_SEARCH_MAX = CHAN_MAX-CHAN_MIN		# Amount of channels to search when searching all channels
	DATA_CODES = 8												# Amount of data codes per channels to search

	def __init__(self):
		Protocol.__init__(self, "DSMX")
		self.scan_times[ProtState.MINIMUM] = DSMX.CHAN_TIME * DSMX.CHAN_SEARCH_MIN * DSMX.DATA_CODES
		self.scan_times[ProtState.AVERAGE] = DSMX.CHAN_TIME * DSMX.CHAN_SEARCH_AVG * DSMX.DATA_CODES
		self.scan_times[ProtState.MAXIMUM] = DSMX.CHAN_TIME * DSMX.CHAN_SEARCH_MAX * DSMX.DATA_CODES

		# Set the channels for the minimum scanning
		for channel in range(DSMX.CHAN_MIN, DSMX.CHAN_MIN+DSMX.CHAN_SEARCH_MIN):
			for pn_column in range(0, DSMX.DATA_CODES):
				pn_row = (channel-2)%5
				self.channels[ProtState.MINIMUM].add((channel, pn_row, pn_column))

		# Set the channels for the average scanning
		for channel in range(DSMX.CHAN_MIN, DSMX.CHAN_MIN+DSMX.CHAN_SEARCH_AVG):
			for pn_column in range(0, DSMX.DATA_CODES):
				pn_row = (channel-2)%5
				self.channels[ProtState.AVERAGE].add((channel, pn_row, pn_column))

		# Set the channels for the maximum scanning
		for channel in range(DSMX.CHAN_MIN, DSMX.CHAN_MIN+DSMX.CHAN_SEARCH_MAX):
			for pn_column in range(0, DSMX.DATA_CODES):
				pn_row = (channel-2)%5
				self.channels[ProtState.MAXIMUM].add((channel, pn_row, pn_column))

	def parse_recv_msg(self, msg):
		"""Parse a message from the CYRF6936 chip and return a possible TX"""
		channel = msg[19]
		pn_row = msg[20] >> 4
		pn_col = msg[20] & 0xF

		# Check if the pn_row belongs to a DSMX packet
		if (channel-2)%5 == pn_row:
			crc = (msg[17] << 8) | (msg[18] << 0)
			crc_seeds = rfchip.CYRF6936.find_crc_seed(msg[:-4], crc)
			if len(crc_seeds) == 1:
				crc_seed = crc_seeds.pop()
				id = [crc_seed & 0xFF, crc_seed >> 8, msg[1], msg[2]]
				return transmitter.DSMTransmitter(id, True, msg)
			elif len(crc_seeds) > 1:
				print "Multiple crc seeds " + str(len(crc_seeds))
		
		return None

	@staticmethod
	def calc_channels(mfg_id):
		"""Calculate the channels based on the chip ID"""
		channels = []
		cnt_3_27 = 0
		cnt_28_51 = 0
		cnt_52_76 = 0

		id = ~((mfg_id[0] << 24) | (mfg_id[1] << 16) | (mfg_id[2] << 8) | (mfg_id[3] << 0))
		rnd_id = id

		while len(channels) < DSMX.CHAN_USED:
			rnd_id = (rnd_id * 0x0019660D + 0x3C6EF35F) & 0xFFFFFFFF
			next_ch = ((rnd_id >> 8) % 0x49) + 3

			if ((next_ch ^ id) & 0x01 ) == 0 or next_ch in channels:
				continue

			if next_ch < 28 and cnt_3_27 < 8:
				cnt_3_27 += 1
				channels.append(next_ch)
			elif next_ch >= 28 and next_ch < 52 and cnt_28_51 < 7:
				cnt_28_51 +=1
				channels.append(next_ch)
			elif next_ch >= 52 and next_ch < 77 and cnt_52_76 < 8:
				cnt_52_76 += 1
				channels.append(next_ch)
		return channels

class DSM2(Protocol):
	CHAN_TIME = 19500*1.5									# Amount of time before reapearance per channel (us)
	CHAN_USED = 2													# Amount of channels in use
	CHAN_MIN = 0													# Lowest occuring channel number
	CHAN_MAX = 79													# Highest occuring channel number
	CHAN_SEARCH_MIN = CHAN_MAX-CHAN_MIN		# Minimum number of channel to search before single occurance
	CHAN_SEARCH_AVG = CHAN_MAX-CHAN_MIN		# Minimum number of channel to search before single occurance
	CHAN_SEARCH_MAX = CHAN_MAX-CHAN_MIN		# Amount of channels to search when searching all channels
	DATA_CODES = 8												# Amount of data codes per channels to search

	def __init__(self):
		Protocol.__init__(self, "DSM2")
		self.scan_times[ProtState.MINIMUM] = DSM2.CHAN_TIME * DSM2.CHAN_SEARCH_MIN * DSM2.DATA_CODES
		self.scan_times[ProtState.AVERAGE] = DSM2.CHAN_TIME * DSM2.CHAN_SEARCH_AVG * DSM2.DATA_CODES
		self.scan_times[ProtState.MAXIMUM] = DSM2.CHAN_TIME * DSM2.CHAN_SEARCH_MAX * DSM2.DATA_CODES

		for channel in range(DSM2.CHAN_MIN, DSM2.CHAN_MIN+DSM2.CHAN_SEARCH_MIN):
			for pn_column in range(0, DSM2.DATA_CODES):
				pn_row = channel%5
				self.channels[ProtState.MINIMUM].add((channel, pn_row, pn_column))

		self.channels[ProtState.AVERAGE] = self.channels[ProtState.MINIMUM]
		self.channels[ProtState.MAXIMUM] = self.channels[ProtState.MINIMUM]

	def parse_recv_msg(self, msg):
		"""Parse a message from the CYRF6936 chip and return a possible TX"""
		channel = msg[19]
		pn_row = msg[20] >> 4
		pn_col = msg[20] & 0xF

		# Check if the pn_row belongs to a DSM2 packet
		if channel%5 == pn_row:
			crc = (msg[17] << 8) | (msg[18] << 0)
			crc_seeds = rfchip.CYRF6936.find_crc_seed(msg[:-4], crc)
			if len(crc_seeds) == 1:
				crc_seed = crc_seeds.pop()
				id = [crc_seed & 0xFF, crc_seed >> 8, (~msg[1]) & 0xFF, (~msg[2]) & 0xFF]
				return transmitter.DSMTransmitter(id, False, msg)
			elif len(crc_seeds) > 1:
				print "Multiple crc seeds " + str(len(crc_seeds))

		return None

class FrSkyX(Protocol):
	CHAN_TIME = 9000*47																# Amount of time before reapearance per channel (us)
	CHAN_USED = 47																		# Amount of channels in use
	CHAN_MIN = 1 																			# Lowest occuring channel number
	CHAN_MAX = 235																		# Highest occuring channel number
	CHAN_SEARCH_MIN = CHAN_MAX-CHAN_MIN-CHAN_USED+1 	# Minimum amount of channels to search
	CHAN_SEARCH_AVG = CHAN_MAX-CHAN_MIN 							# Average amount of channels to search
	CHAN_SEARCH_MAX = CHAN_MAX-CHAN_MIN 							# Maximum amount of channels to search
	FSCTRL0_NUM = 8 																	# Frequency offsets

	CRC_TABLE = [
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
		0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78]

	def __init__(self, name = "FrSkyX", eu = False, packet_len = 29):
		Protocol.__init__(self, name)
		self.packet_len = packet_len
		self.eu = eu
		self.id = 2

		self.scan_times[ProtState.MINIMUM] = FrSkyX.CHAN_TIME * FrSkyX.CHAN_SEARCH_MIN
		self.scan_times[ProtState.AVERAGE] = FrSkyX.CHAN_TIME * FrSkyX.CHAN_SEARCH_AVG
		self.scan_times[ProtState.MAXIMUM] = FrSkyX.CHAN_TIME * FrSkyX.CHAN_SEARCH_MAX * FrSkyX.FSCTRL0_NUM

		for channel in range(FrSkyX.CHAN_MIN, FrSkyX.CHAN_MIN + FrSkyX.CHAN_SEARCH_MIN):
			self.channels[ProtState.MINIMUM].add((channel, 0))

		for channel in range(FrSkyX.CHAN_MIN, FrSkyX.CHAN_MAX):
			self.channels[ProtState.AVERAGE].add((channel, 0))

		for channel in range(FrSkyX.CHAN_MIN, FrSkyX.CHAN_MAX):
			for fsctrl0 in range(FrSkyX.FSCTRL0_NUM):
				self.channels[ProtState.MAXIMUM].add((channel, fsctrl0))

	def parse_recv_msg(self, msg):
		"""Parse a message from the CC2500 and return a possible TX"""

		# Check if the length is correct
		if msg[0] != self.packet_len:
			return None

		# Check if the CC2500 CRC is correct
		if (msg[self.packet_len+2] & 0x80) != 0x80:
			return None

		# Check the inner CRC
		calc_crc = self.crc(msg[3:self.packet_len-1])
		packet_crc = (msg[self.packet_len-1] << 8) | msg[self.packet_len]
		if calc_crc != packet_crc:
			return None

		id = msg[1:3]
		return transmitter.FrSkyXTransmitter(id, self.eu, msg)

	def crc(self, data):
		"""Calculate the internal CRC"""
		crc = 0
		for d in data:
			crc = ((crc << 8) & 0xFF00) ^ FrSkyX.CRC_TABLE[((crc >> 8) ^ d)]
		return crc

class FrSkyXEU(FrSkyX):

	def __init__(self):
		FrSkyX.__init__(self, "FrSkyXEU", True, 32)
		self.id = 3