#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
import threading
import sys
import time
import gi
import rfchip
import serial.tools.list_ports
sys.path.append('../lib/pprzlink/lib/v1.0/python')
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GObject
from pprzlink.serial import SerialMessagesInterface
from pprzlink.message import PprzMessage
from main import Ground
from enum import IntEnum

# The USB device
class Device(GObject.GObject):

	class Prot(IntEnum):
		NONE = -1
		CYRF_SCANNER = 0
		DSM_HACK = 1
		CC_SCANNER = 2
		FRSKY_HACK = 3
		FRSKY_RECEIVER = 4
		FRSKY_TRANSMITTER = 5

	class State(IntEnum):
		STOP = 0
		START = 1

	def __init__(self, dm, port):
		GObject.GObject.__init__(self)
		self.dm = dm
		self.port = port
		self.name = "Unknown"
		self.prot = self.Prot.NONE
		self.state = self.State.STOP
		self.id = -1
		self.recv_cb = {}

		# Open the device
		self.smi = SerialMessagesInterface(self.on_recv, self.on_disconnect, False, port, 115200, 'usbrf')
		self.smi.start()

		# Request information
		msg = PprzMessage('usbrf', 'REQ_INFO')
		msg['version'] = Ground.VERSION * 1000
		self.smi.send(msg, 0)
		time.sleep(1)

	def stop(self):
		"""Stop the connection to the device"""
		self.smi.stop()

	def prot_exec(self, prot, state, data):
		"""Execute a protocol on the device (can be start or stopped)"""
		data_len = len(data)
		offset = 0

		# Split in messages of data length 200 maximum
		while offset < data_len+1:
			offset_end = offset+200 if offset+200 < data_len else data_len
			msg = PprzMessage('usbrf', 'PROT_EXEC')
			msg['id'] = int(prot)
			msg['type'] = int(state)
			msg['arg_offset'] = offset
			msg['arg_size'] = data_len
			msg['arg_data'] = data[offset:offset_end]
			self.smi.send(msg, 0)
			offset += 200
			time.sleep(0.03)

		# Update the state
		self.prot = prot
		self.state = state
		self.dm.on_update()

	def is_scanning(self):
		"""Whether the device is in one of the scanning protocols"""
		return (self.state == self.State.START and (self.prot == self.Prot.CYRF_SCANNER or self.prot == self.Prot.CC_SCANNER))

	def is_hacking(self):
		"""Whether the device is in hacking mode"""
		return (self.state == self.State.START and (self.prot == self.Prot.DSM_HACK or self.prot == self.Prot.FRSKY_HACK))

	def is_running(self):
		"""Whether the device is running a protocol or not"""
		return self.state == self.State.START

	def stop_prot(self):
		"""Stop the current protocol"""
		if self.is_running():
			self.prot_exec(self.prot, self.State.STOP, [])

	def register_recv(self, name, func):
		"""Register receive callback"""
		self.recv_cb[name] = func

	def on_recv(self, sender_id, msg):
		"""When the device received a valid PPRZLINK message handle it"""
		if msg._name == 'INFO':
			self.on_msg_info(msg)
		elif msg._name in self.recv_cb:
			self.recv_cb[msg._name](msg)

	def on_disconnect(self):
		"""Whenever the device disconnects from the computer"""
		self.dm.on_disconnect(self)

	def on_msg_info(self, msg):
		"""Update device information based on received INFO message"""
		self.id = msg.id
		self.board = msg.board
		self.version = msg.version
		self.name = self.get_name()
		self.chips = self.get_chips()
		self.dm.on_info(self)

	def get_chips(self):
		"""Get the chips that are available on the board based on the hardware ID"""
		if self.board <= 1:
			return [rfchip.CYRF6936]
		elif self.board == 2:
			return [rfchip.CYRF6936, rfchip.CC2500]
		return []

	def get_name(self):
		"""Return a name containing all device identifiers"""
		return "USBRF" + str(self.board) + " " + "-".join(("%04X" % id) for id in self.id) + " V" + str(self.version)

# The USB device manager
class DeviceManager(threading.Thread):
	idVendor = 0x0484
	idProduct = 0x5741
	interface = 'SuperbitRF data port'

	def __init__(self, on_info=None, on_disconnect=None, on_update=None):
		threading.Thread.__init__(self)
		self.running = True
		self.devices = []
		self._on_info = on_info
		self._on_disconnect = on_disconnect
		self._on_update = on_update

	def run(self):
		"""Main thread searching for devices"""
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
		"""Stop searching for devices"""
		for dev in self.devices:
			dev.stop()
		self.running = False

	def shutdown(self):
		"""Whenever the thread is shutdown stop it to make sure it is closed correctly"""
		self.stop()

	def get_device(self, port):
		"""Get the device by port identifier"""
		for dev in self.devices:
			if dev.port == port:
				return dev
		return None

	def get_devices(self):
		"""Return the devices that respond with their ID"""
		devs = []
		for d in self.devices:
			if d.id != -1:
				devs.append(d)
		return devs

	def find_devices(self):
		"""Find connected USB devcies which aren't added yet"""
		ports = serial.tools.list_ports.comports()
		for p in ports:
			if p.interface == self.interface and self.get_device(p.device) == None:
				self.devices.append(Device(self, p.device))

	def register_recv(self, name, func):
		"""Register receive callback"""
		for d in self.devices:
			d.register_recv(name, func)

	def on_info(self, dev):
		"""When a device reports his information"""
		if self._on_info != None:
			self._on_info(dev)

	def on_disconnect(self, dev):
		"""When a device gets disconnected"""
		self.devices.remove(dev)
		if dev.id != -1 and self._on_disconnect != None:
			self._on_disconnect(dev)

	def on_update(self):
		"""When a device changes state"""
		if self._on_update != None:
			self._on_update()


# The Device manager overview
class DeviceManagerBox(Gtk.Box):

	def __init__(self, dm):
		# Create the GUI
		Gtk.Box.__init__(self)
		self.create_gui()

		# Initialize the device manager
		self.dm = dm
		self.dm._on_info = self.on_info
		self.dm._on_disconnect = self.on_disconnect
		self.dm._on_update = self.on_update

		for dev in self.dm.get_devices():
			self.on_info(dev)

	def create_gui(self):
		"""Create the GUI"""
		# Add a treeview containing all devices
		self.liststore = Gtk.ListStore(Device, str, str, str)
		self.treeview = Gtk.TreeView(model=self.liststore)
		self.add(self.treeview)

		# Add the first Name column
		column = Gtk.TreeViewColumn("Name", Gtk.CellRendererText(), text=1)
		column.set_min_width(250)
		self.treeview.append_column(column)

		# Add the protocol
		column = Gtk.TreeViewColumn("Protocol", Gtk.CellRendererText(), text=2)
		column.set_min_width(100)
		self.treeview.append_column(column)

		# Add the State
		column = Gtk.TreeViewColumn("State", Gtk.CellRendererText(), text=3)
		self.treeview.append_column(column)

	def on_info(self, dev):
		"""When a device reports his information add it to the list"""
		GObject.idle_add(self._on_info, dev)

	def _on_info(self, dev):
		"""When a new device presents it's information append it to the list"""
		self.liststore.append([dev, dev.name, dev.prot.name, dev.state.name])

	def on_disconnect(self, dev):
		"""When a device disconnects remove it from the list"""
		GObject.idle_add(self._on_disconnect, dev)

	def _on_disconnect(self, dev):
		for d in self.liststore:
			if d[0] == dev:
				self.liststore.remove(d.iter)

	def on_update(self):
		"""When a device updates, update the list"""
		GObject.idle_add(self._on_update)

	def _on_update(self):
		for lt in self.liststore:
			dev = lt[0]
			lt[2] = dev.prot.name
			lt[3] = dev.state.name
