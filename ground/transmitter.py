#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
import protocol

class Transmitter():

	def __init__(self):
		self.prot_name = "UNK"
		self.hackable = False
		self.do_hack = True
		self.recv_data = []
		self.recv_cnt = 0
		self.channel_values = {}

		for i in range(20):
			self.channel_values[i] = 0

	def get_id_str(self):
		"""Print the ID as a string for the GUI"""
		return "".join(("%02X" % i) for i in self.id)

	def merge(self, other):
		"""Merge and parse the data from another transmitter together"""
		for data in other.recv_data:
			self.parse_data(data)

class DSMTransmitter(Transmitter):

	def __init__(self, id, dsmx = False, data = None):
		Transmitter.__init__(self)
		self.id = id
		self.dsmx = dsmx
		self.resolution = None
		self.bm_10bit = 0
		self.bm_11bit = 0
		self.channels = set()
		self.prot_name = "DSMX" if dsmx else "DSM2"

		if data != None:
			self.parse_data(data)

	def parse_data(self, data):
		"""Parse an incoming CYRF6936 data packet"""
		self.recv_data.append(data)
		self.recv_cnt += 1
		self.channels.add(data[19])

		# Try to guess the resolution (10/11 bits)
		for i in range(0, 7):
			channel_data = data[3+i*2 : 5+i*2] # Skip length and ID
			dec_10bit = self.decode_channel(channel_data, 10)
			dec_11bit = self.decode_channel(channel_data, 11)
			if dec_10bit != None:
				self.bm_10bit |= (1 << dec_10bit[0])
			if dec_11bit != None:
				self.bm_11bit |= (1 << dec_11bit[0])

		# Update the last known channel values
		self.update_channels(data)

		# Check if we can attack
		self.check_hackable()

	def is_same(self, other):
		"""Check if the transmitter is similar or not based on ID or inverse ID"""
		if isinstance(other, DSMTransmitter) and (other.id == self.id or other.id == self.inverse_id()):
			return True
		return False

	def inverse_id(self):
		"""Return the inverse of the current id (only bytes 0, 1 because of checksum)"""
		return [(~self.id[0]) & 0xFF, (~self.id[1]) & 0xFF, self.id[2], self.id[3]]

	def start_hacking(self, device):
		"""Start hacking the transmitter"""
		self.rfchip.start_hacking(device, self)

	def check_hackable(self):
		"""Check if we are able to attack the device and thus have enough information"""
		if not self.dsmx and len(self.channels) == 2:
			# Check the latest message and verify the sop_column calculation
			sop_col_n = (self.id[0] + self.id[1] + self.id[2] + 2) & 0x7
			sop_col_i = (~self.id[0] + ~self.id[1] + self.id[2] + 2) & 0x7
			last_sop_col = self.recv_data[-1][20] & 0xF

			# Check if we can make a decision or not based on the analyzed data
			if (sop_col_n != last_sop_col and sop_col_i == last_sop_col):
				self.id = self.inverse_id()
				self.hackable = True
			elif (sop_col_n == last_sop_col and sop_col_i != last_sop_col):
				self.hackable = True

		elif self.dsmx:
			# Check if the channels are correct for the ID
			calc_channels_n = set(protocol.DSMX.calc_channels(self.id))
			calc_channels_i = set(protocol.DSMX.calc_channels(self.inverse_id()))
			diff_channels_n = self.channels - calc_channels_n
			diff_channels_i = self.channels - calc_channels_i
			#print calc_channels_n
			#print calc_channels_i

			# Check the latest message and verify the sop_column calculation
			sop_col_n = (self.id[0] + self.id[1] + self.id[2] + 2) & 0x7
			sop_col_i = (~self.id[0] + ~self.id[1] + self.id[2] + 2) & 0x7
			last_sop_col = self.recv_data[-1][20] & 0xF

			# Check if we can make a decision or not based on the analyzed data
			if (sop_col_n != last_sop_col and sop_col_i == last_sop_col) or (len(diff_channels_n) > 0 and len(diff_channels_i) == 0):
				self.id = self.inverse_id()
				self.hackable = True
			elif (sop_col_n == last_sop_col and sop_col_i != last_sop_col) or (len(diff_channels_n) == 0 and len(diff_channels_i) > 0):
				self.hackable = True

	def get_resolution(self):
		"""Get the resolution of set, else return a guess based on received values"""
		if self.resolution != None:
			return self.resolution

		# Use the guessed resolution now
		# TODO make better ( we now only check if at least we received the lowest 6 channels in 10bit)
		if (self.bm_10bit & 0x3f) ^ 0x3f == 0:
			return 11 #FIXME
		else:
			return 11

	def decode_channel(self, data, resolution):
		"""Decode the channel with a specific resolution"""
		raw = data[0] << 8 | data[1]

		# This is not valid channel data
		if raw == 0xFFFF:
			return None

		channel = (raw >> resolution) & 0xF
		data_mask = (1 << resolution) - 1
		value = raw & data_mask
		return (channel, value)

	def update_channels(self, data):
		"""Return the channel values based on the received data"""
		resolution = self.get_resolution()

		# Go through the possible channels
		for i in range(0, 7):
			channel_data = data[3+i*2 : 5+i*2] # Skip length and ID
			channel = self.decode_channel(channel_data, resolution)
			if channel != None:
				self.channel_values[channel[0]] = float(channel[1]) / (1 << resolution)*100


class FrSkyXTransmitter(Transmitter):

	def __init__(self, id, eu = False, data = None):
		Transmitter.__init__(self)
		self.id = id
		self.eu = eu
		self.channels = {}
		self.prot_name = "FrSkyXEU" if eu else "FrSkyX"

		for i in range(protocol.FrSkyX.CHAN_USED):
			self.channels[i] = -1

		if data != None:
			self.parse_data(data)

	def is_same(self, other):
		"""Check if the transmitter is similar or not based on the ID"""
		if isinstance(other, FrSkyXTransmitter) and self.eu == other.eu and other.id == self.id:
			return True
		return False

	def parse_data(self, data):
		self.recv_data.append(data)
		self.recv_cnt += 1

		# Update the channel map
		idx = data[4] & 0x3F
		self.channels[idx] = data[-2]

		rssi = data[-4]
		lqi = data[-3] & 0x7F
		#if rssi < 200:
		#print(data)
		#print(str(idx) + ": " +str(data[-2]) + " (" + str(rssi) + ", " + str(lqi) + ")")
		print(self.channels)

		# Check if we can hack the device and have the full hopping table
		self.hackable = True
		for i in range(protocol.FrSkyX.CHAN_USED):
			if self.channels[i] == -1:
				self.hackable = False
				break


class TransmitterManager():

	def __init__(self):
		self.transmitters = []
		self._on_change = None

	def add_or_merge(self, new_tx, rfchip):
		"""Add or merge a new transmitter"""
		new_tx.rfchip = rfchip
		for tx in self.transmitters:
			if tx.is_same(new_tx):
				tx.merge(new_tx)
				self.on_change()
				return

		self.transmitters.append(new_tx)
		self.on_change()

	def clear(self):
		self.transmitters = []
		self.on_change()

	def hack_toggle(self, prot_name, txid_str):
		for tx in self.transmitters:
			if tx.prot_name == prot_name and tx.get_id_str() == txid_str:
				tx.do_hack = not tx.do_hack

	def on_change(self):
		if self._on_change != None:
			self._on_change()

	def set_on_change(self, on_change):
		self._on_change = on_change