#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
import protocol
import struct

class RFChip():

	def __init__(self):
		self.protocols = []

	def calc_scan_time(self):
		"""Calculate the scan time for the specific RF chip"""
		scan_time = 0
		for prot in self.protocols:
			scan_time += prot.get_scan_time()
		return scan_time

	def calc_scan_channels(self):
		"""Calculate the channels for scanning"""
		channels = set()
		for prot in self.protocols:
			channels.update(prot.get_channels())
		return channels

	def set(self, prot_name, val):
		"""Set the state of a protocol with a specific name"""
		for prot in self.protocols:
			if prot.name == prot_name:
				prot.state = val
				return prot
		return None

	def parse_recv_msg(self, msg):
		"""Parse a received message and return a possible TX object"""
		for prot in self.protocols:
			tx = prot.parse_recv_msg(msg)
			if tx != None:
				return tx
		return None


class CYRF6936(RFChip):
	NAME = "CYRF6936"
	ID = 0
	CRC_TABLE = [
		0x0000, 0x0580, 0x0F80, 0x0A00, 0x1B80, 0x1E00, 0x1400, 0x1180, 0x3380, 0x3600, 0x3C00, 0x3980, 0x2800, 0x2D80, 0x2780, 0x2200, 
		0x6380, 0x6600, 0x6C00, 0x6980, 0x7800, 0x7D80, 0x7780, 0x7200, 0x5000, 0x5580, 0x5F80, 0x5A00, 0x4B80, 0x4E00, 0x4400, 0x4180, 
		0xC380, 0xC600, 0xCC00, 0xC980, 0xD800, 0xDD80, 0xD780, 0xD200, 0xF000, 0xF580, 0xFF80, 0xFA00, 0xEB80, 0xEE00, 0xE400, 0xE180, 
		0xA000, 0xA580, 0xAF80, 0xAA00, 0xBB80, 0xBE00, 0xB400, 0xB180, 0x9380, 0x9600, 0x9C00, 0x9980, 0x8800, 0x8D80, 0x8780, 0x8200, 
		0x8381, 0x8601, 0x8C01, 0x8981, 0x9801, 0x9D81, 0x9781, 0x9201, 0xB001, 0xB581, 0xBF81, 0xBA01, 0xAB81, 0xAE01, 0xA401, 0xA181, 
		0xE001, 0xE581, 0xEF81, 0xEA01, 0xFB81, 0xFE01, 0xF401, 0xF181, 0xD381, 0xD601, 0xDC01, 0xD981, 0xC801, 0xCD81, 0xC781, 0xC201, 
		0x4001, 0x4581, 0x4F81, 0x4A01, 0x5B81, 0x5E01, 0x5401, 0x5181, 0x7381, 0x7601, 0x7C01, 0x7981, 0x6801, 0x6D81, 0x6781, 0x6201, 
		0x2381, 0x2601, 0x2C01, 0x2981, 0x3801, 0x3D81, 0x3781, 0x3201, 0x1001, 0x1581, 0x1F81, 0x1A01, 0x0B81, 0x0E01, 0x0401, 0x0181, 
		0x0383, 0x0603, 0x0C03, 0x0983, 0x1803, 0x1D83, 0x1783, 0x1203, 0x3003, 0x3583, 0x3F83, 0x3A03, 0x2B83, 0x2E03, 0x2403, 0x2183, 
		0x6003, 0x6583, 0x6F83, 0x6A03, 0x7B83, 0x7E03, 0x7403, 0x7183, 0x5383, 0x5603, 0x5C03, 0x5983, 0x4803, 0x4D83, 0x4783, 0x4203, 
		0xC003, 0xC583, 0xCF83, 0xCA03, 0xDB83, 0xDE03, 0xD403, 0xD183, 0xF383, 0xF603, 0xFC03, 0xF983, 0xE803, 0xED83, 0xE783, 0xE203, 
		0xA383, 0xA603, 0xAC03, 0xA983, 0xB803, 0xBD83, 0xB783, 0xB203, 0x9003, 0x9583, 0x9F83, 0x9A03, 0x8B83, 0x8E03, 0x8403, 0x8183, 
		0x8002, 0x8582, 0x8F82, 0x8A02, 0x9B82, 0x9E02, 0x9402, 0x9182, 0xB382, 0xB602, 0xBC02, 0xB982, 0xA802, 0xAD82, 0xA782, 0xA202, 
		0xE382, 0xE602, 0xEC02, 0xE982, 0xF802, 0xFD82, 0xF782, 0xF202, 0xD002, 0xD582, 0xDF82, 0xDA02, 0xCB82, 0xCE02, 0xC402, 0xC182, 
		0x4382, 0x4602, 0x4C02, 0x4982, 0x5802, 0x5D82, 0x5782, 0x5202, 0x7002, 0x7582, 0x7F82, 0x7A02, 0x6B82, 0x6E02, 0x6402, 0x6182, 
		0x2002, 0x2582, 0x2F82, 0x2A02, 0x3B82, 0x3E02, 0x3402, 0x3182, 0x1382, 0x1602, 0x1C02, 0x1982, 0x0802, 0x0D82, 0x0782]

	def __init__(self):
		"""Initialize the CYRF6936 chip"""
		self.name = CYRF6936.NAME
		self.id = CYRF6936.ID
		self.dmsx = protocol.DSMX()
		self.dsm2 = protocol.DSM2()
		self.protocols = [self.dmsx, self.dsm2]

	def start_scanning(self, devices):
		"""Start scanning and devide across the devices"""
		dev_cnt = len(devices)
		channels = self.divide_channels(dev_cnt)

		print(len(self.calc_scan_channels()))
		#print(self.divide_channels(3))

		for i in range(dev_cnt):
			print(len(channels[i]))
			scan_data = self.generate_scan_data(channels[i])
			devices[i].prot_exec(0, 1, scan_data) #FIXME ID

	def start_hacking(self, device, tx):
		"""Start hacking the transmitter"""
		hack_data = self.generate_hack_data(tx)
		device.prot_exec(1, 1, hack_data)

	def divide_channels(self, cnt):
		"""Divide the channels into cnt pieces for scanning"""
		channels = sorted(list(self.calc_scan_channels()))
		length = len(channels)
		return [ channels[i*length // cnt: (i+1)*length // cnt] for i in range(cnt) ]

	def generate_scan_data(self, channels):
		"""Generate packet data from channel list"""
		data = bytearray(len(channels)*2)
		idx = 0
		for channel in channels:
			# 0: channel, 1: row, 2: column
			struct.pack_into("<BB", data, idx, channel[0], channel[1] << 4 | channel[2])
			idx += 2
		return data

	def generate_hack_data(self, tx):
		"""Generate data for hacking"""
		data = bytearray(7)
		if not tx.dsmx:
			tx_chans = []
			for chan in tx.channels:
				tx_chans.append(chan)
			struct.pack_into("<BBBBBBB", data, 0, 0, tx.id[0], tx.id[1], tx.id[2], tx.id[3], tx_chans[0], tx_chans[1])
		else:
			struct.pack_into("<BBBBBBB", data, 0, 1, tx.id[0], tx.id[1], tx.id[2], tx.id[3], 0, 0)
		return data

	@staticmethod
	def reverse_bit(val):
		"""Reverse the order of the bits in 1 byte"""
		return int('{:08b}'.format(val)[::-1], 2)

	@staticmethod
	def calc_crc(data, crc):
		"""Calculate th CRC of the packet (append length in front of data)"""
		for d in data:
			crc = (crc >> 8) ^ CYRF6936.CRC_TABLE[(crc ^ CYRF6936.reverse_bit(d)) & 0xff];
		return crc

	@staticmethod
	def find_crc_seed(data, crc):
		"""Find the CRC seed based on the CRC and the packet data (with length append in front)"""
		local_crc = crc
		found_crcs = set()
		for i in range(len(CYRF6936.CRC_TABLE)):
			if (CYRF6936.CRC_TABLE[i] >> 8) == (local_crc >> 8):
				crc = ((local_crc ^ CYRF6936.CRC_TABLE[i]) << 8) | (i ^ CYRF6936.reverse_bit(data[-1]))
				
				if len(data) == 1:
					found_crcs.add(crc)
				else:
					found_crcs = found_crcs | CYRF6936.find_crc_seed(data[:-1], crc)
		return found_crcs

class CC2500(RFChip):
	NAME = "CC2500"
	ID = 1

	def __init__(self):
		"""Initialize the CC2500 chip"""
		self.name = CC2500.NAME
		self.id = CC2500.ID
		self.frskyx = protocol.FrSkyX()
		self.protocols = [self.frskyx]

	def start_scanning(self, devices):
		"""Start scanning and devide across the devices"""
		pass