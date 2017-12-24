#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
import threading
import sys
import time
import serial.tools.list_ports
sys.path.append('../lib/pprzlink/lib/v1.0/python')
from pprzlink.serial import SerialMessagesInterface
from pprzlink.message import PprzMessage
from main import Ground

# The USB device
class Device():

	def __init__(self, port):
		self.port = port
		self.running = True

		# Open the device
		self.smi = SerialMessagesInterface(self.on_recv, False, port, 115200, 'usbrf')
		self.smi.start()

		# Request information
		pprz_msg = PprzMessage('usbrf', 'REQ_INFO')
		pprz_msg.version = Ground.VERSION * 1000
		self.smi.send(pprz_msg, 0)

	def stop(self):
		self.smi.stop()

	def on_recv(self, sender_id, msg):
		print('Received message ' + str(sender_id))
		print(msg.id)
		print(msg.board)
		print(msg.version)

	def __contains__(self, port):
		return self.port == port

# The USB device manager
class DeviceManager(threading.Thread):
	idVendor = 0x0484
	idProduct = 0x5741
	interface = 'SuperbitRF data port'

	def __init__(self):
		threading.Thread.__init__(self)
		self.running = True
		self.devices = []

	def run(self):
		try:
			while self.running:
				self.find_devices()
				time.sleep(1)
		except StopIteration:
			pass

		for dev in self.devices:
			dev.stop()
			self.devices.remove(dev)


	def stop(self):
		self.running = False

	def shutdown(self):
		self.stop()

	def is_registered(self, port):
		for dev in self.devices:
			if dev.port == port:
				return True
		return False

	def find_devices(self):
		ports = serial.tools.list_ports.comports()
		for p in ports:
			if p.interface == self.interface and not self.is_registered(p.device):
				self.devices.append(Device(p.device))
