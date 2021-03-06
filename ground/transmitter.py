#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
import protocol
import json
import os.path
from collections import deque
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GObject

class Transmitter(GObject.GObject):

	def __init__(self):
		GObject.GObject.__init__(self)
		self.name = "UNK"
		self.prot_name = "UNK"
		self.hackable = 0
		self.do_hack = False
		self.recv_data = deque([], 20)
		self.recv_cnt = 0
		self.channel_values = {}

		for i in range(20):
			self.channel_values[i] = 0

	def get_id_str(self):
		"""Print the ID as a string for the GUI"""
		return "".join(("%02X" % i) for i in self.id)

	def merge(self, other):
		"""Merge and parse the data from another transmitter together"""
		self.rfchip = other.rfchip
		for data in other.recv_data:
			self.parse_data(data)

		# Check if we can attack
		self.check_hackable()

	def start_hacking(self, device):
		"""Start hacking the transmitter"""
		if self.rfchip != None:
			self.rfchip.start_hacking(device, self)

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
		self.name = "UNK " + self.get_id_str()

		if data != None:
			self.parse_data(data)

		# Check if we can attack
		self.check_hackable()

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

	def is_same(self, other):
		"""Check if the transmitter is similar or not based on ID or inverse ID"""
		if isinstance(other, DSMTransmitter) and other.dsmx == self.dsmx and (other.id == self.id or other.id == self.inverse_id()):
			return True
		return False

	def inverse_id(self):
		"""Return the inverse of the current id (only bytes 0, 1 because of checksum)"""
		return [(~self.id[0]) & 0xFF, (~self.id[1]) & 0xFF, self.id[2], self.id[3]]

	def check_hackable(self):
		"""Check if we are able to attack the device and thus have enough information"""
		if not self.dsmx and len(self.channels) == 2:
			# Check the latest message and verify the sop_column calculation
			sop_col_n = (self.id[0] + self.id[1] + self.id[2] + 2) & 0x7
			sop_col_i = (~self.id[0] + ~self.id[1] + self.id[2] + 2) & 0x7
			last_sop_col = self.recv_data[-1][20] & 0xF if len(self.recv_data) > 0 else -1

			# Check if we can make a decision or not based on the analyzed data
			if (sop_col_n != last_sop_col and sop_col_i == last_sop_col):
				self.id = self.inverse_id()
				self.hackable = 100
			elif (sop_col_n == last_sop_col and sop_col_i != last_sop_col):
				self.hackable = 100
			else:
				self.hackable = 80
		elif not self.dsmx:
			self.hackable = 20 * len(self.channels)
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
			last_sop_col = self.recv_data[-1][20] & 0xF if len(self.recv_data) > 0 else -1

			# Check if we can make a decision or not based on the analyzed data
			if (sop_col_n != last_sop_col and sop_col_i == last_sop_col) or (len(diff_channels_n) > 0 and len(diff_channels_i) == 0):
				self.id = self.inverse_id()
				self.hackable = 100
			elif (sop_col_n == last_sop_col and sop_col_i != last_sop_col) or (len(diff_channels_n) == 0 and len(diff_channels_i) > 0):
				self.hackable = 100
			else:
				self.hackable = len(self.channels)

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

	def to_obj(self):
		obj = {
			'name': self.name,
			'id': self.id,
			'dsmx': self.dsmx,
			'resolution': self.resolution,
			'channels': list(self.channels),
			'do_hack': self.do_hack
		}
		return obj

	@classmethod
	def from_obj(cls, obj):
		tx = DSMTransmitter(obj['id'], obj['dsmx'])
		tx.name = obj['name']
		tx.resolution = obj['resolution']
		if tx.dsmx:
			tx.channels = set(obj['channels'])
		tx.do_hack = obj['do_hack']
		tx.check_hackable()
		return tx


class FrSkyXTransmitter(Transmitter):

	def __init__(self, id, eu = False, data = None):
		Transmitter.__init__(self)
		self.id = id
		self.eu = eu
		self.channels = {}
		self.prot_name = "FrSkyXEU" if eu else "FrSkyX"
		self.name = "UNK " + self.get_id_str()

		for i in range(protocol.FrSkyX.CHAN_USED):
			self.channels[i] = (-1, 128)

		if data != None:
			self.parse_data(data)

		self.check_hackable()

	def is_same(self, other):
		"""Check if the transmitter is similar or not based on the ID"""
		if isinstance(other, FrSkyXTransmitter) and self.eu == other.eu and other.id == self.id:
			return True
		return False

	def parse_data(self, data):
		self.recv_data.append(data)
		self.recv_cnt += 1

		# Get information from the CC2500
		rssi = (data[-4]-256)/2-72 if data[-4] >= 128 else data[-4]/2-72
		lqi = data[-3] & 0x7F

		# Update the channel map
		idx = data[4] & 0x3F
		channel = data[-2]
		if lqi < self.channels[idx][1]:
			self.channels[idx] = (channel, lqi)

		# Parse the RC channels (Check if not failsafe values)
		if data[7] == 0:
			for i in range(0, 12, 3):
				chan0 = data[i+9] + ((data[i+10] & 0x0F) << 8)
				chan1 = (data[i+10] >> 4) + (data[i+11] << 4)

				idx = i/3*2
				if chan0 & 0x800:
					self.channel_values[idx+8] = float(chan0 - 0x800) / 2047 * 100
				else:
					self.channel_values[idx] = float(chan0) / 2047 * 100

				if chan1 & 0x800:
					self.channel_values[idx+9] = float(chan1 - 0x800) / 2047 * 100
				else:
					self.channel_values[idx+1] = float(chan1) /2047 * 100

	def check_hackable(self):
		"""Check if we can hack the device and have the full hopping table"""
		not_found = 0
		for i in range(protocol.FrSkyX.CHAN_USED):
			if self.channels[i][0] == -1:
				not_found = not_found + 1

		if not_found == 0:
			self.hackable = 100
			# test = {0: 3, 1: 183, 2: 128, 3: 73, 4: 18, 5: 198, 6: 143, 7: 90, 8: 33, 9: 213, 10: 158, 11: 103, 12: 48, 13: 228, 14: 175, 15: 118, 16: 63, 17: 8, 18: 188, 19: 133, 20: 78, 21: 23, 22: 203, 23: 148, 24: 93, 25: 38, 26: 221, 27: 163, 28: 108, 29: 53, 30: 233, 31: 178, 32: 123, 33: 68, 34: 13, 35: 193, 36: 138, 37: 83, 38: 28, 39: 208, 40: 153, 41: 98, 42: 45, 43: 223, 44: 168, 45: 113, 46: 58}
			# wrong = 0
			# for i in test:
			# 	if self.channels[i][0] != test[i]:
			# 		wrong += 1
			# 		print('W ' + str(i) + ': ' + str(self.channels[i][0]) + ' ' + str(test[i]))
			# print(self.channels)
			# print('WRONG: ' + str(wrong));
		else:
			self.hackable = int(100.0-(100.0/protocol.FrSkyX.CHAN_USED*not_found))

	def to_obj(self):
		obj = {
			'name': self.name,
			'id': self.id,
			'eu': self.eu,
			'channels': self.channels,
			'do_hack': self.do_hack
		}
		return obj

	@classmethod
	def from_obj(cls, obj):
		tx = FrSkyXTransmitter(obj['id'], obj['eu'])
		tx.name = obj['name']
		tx.do_hack = obj['do_hack']
		for c in obj['channels']:
			tx.channels[int(c)] = (obj['channels'][c][0], 128)

		tx.check_hackable()
		return tx


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
				return tx

		self.transmitters.append(new_tx)
		self.on_change()
		return new_tx

	def clear(self):
		self.transmitters = []
		self.on_change()

	def save(self):
		json_data = []
		for tx in self.transmitters:
			json_tx = {'cls': tx.__class__.__name__, 'data': tx.to_obj()}
			json_data.append(json_tx)

		with open('transmitters.json', 'w') as outfile:
			json.dump(json_data, outfile)
			
	def load(self):
		if os.path.isfile('transmitters.json'):
			with open('transmitters.json') as infile:
				json_data = json.load(infile)

				for json_tx in json_data:
					new_tx = globals()[json_tx['cls']].from_obj(json_tx['data'])
					for tx in self.transmitters:
						if tx.is_same(new_tx):
							self.transmitters.remove(tx)

					self.transmitters.append(new_tx)

				self.on_change()

	def on_change(self):
		if self._on_change != None:
			self._on_change()

	def set_on_change(self, on_change):
		self._on_change = on_change