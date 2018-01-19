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
	CHAN_USED = 2													# Amount of channels in used
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

		self.channels_min = []
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

	def __init__(self):
		Protocol.__init__(self, "FrSkyX")