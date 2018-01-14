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

# The USB device
class Device():

	def __init__(self, dm, port):
		self.dm = dm
		self.port = port
		self.name = "Unknown"
		self.id = -1
		self.running = True
		self.recv_cb = {}

		# Open the device
		self.smi = SerialMessagesInterface(self.on_recv, self.on_disconnect, False, port, 115200, 'usbrf')
		self.smi.start()

		# Request information
		msg = PprzMessage('usbrf', 'REQ_INFO')
		msg['version'] = Ground.VERSION * 1000
		self.smi.send(msg, 0)

	def stop(self):
		self.smi.stop()

	def prot_exec(self, prot_id, start, data):
		data_len = len(data)
		offset = 0

		# Split in messages of data length 200 maximum
		while offset < data_len:
			offset_end = offset+200 if offset+200 < data_len else data_len
			msg = PprzMessage('usbrf', 'PROT_EXEC')
			msg['id'] = prot_id
			msg['type'] = start #FIXME
			msg['arg_offset'] = offset
			msg['arg_size'] = data_len
			msg['arg_data'] = data[offset:offset_end]
			self.smi.send(msg, 0)
			offset += 200
			time.sleep(0.02)

	def register_recv(self, name, func):
		self.recv_cb[name] = func

	def on_recv(self, sender_id, msg):
		if msg._name == 'INFO':
			self.on_msg_info(msg)
		elif msg._name in self.recv_cb:
			self.recv_cb[msg._name](msg)

	def on_disconnect(self):
		self.dm.on_disconnect(self)

	def on_msg_info(self, msg):
		self.id = msg.id
		self.board = msg.board
		self.version = msg.version
		self.name = self.get_name()
		self.chips = self.get_chips()
		self.dm.on_info(self)

	def get_chips(self):
		if self.board <= 1:
			return [rfchip.CYRF6936]
		elif self.board == 2:
			return [rfchip.CYRF6936, rfchip.CC2500]
		return []

	def get_name(self):
		return "USBRF " + "-".join(("%04X" % id) for id in self.id)

# The USB device manager
class DeviceManager(threading.Thread):
	idVendor = 0x0484
	idProduct = 0x5741
	interface = 'SuperbitRF data port'

	def __init__(self, on_info=None, on_disconnect=None):
		threading.Thread.__init__(self)
		self.running = True
		self.devices = []
		self._on_info = on_info
		self._on_disconnect = on_disconnect

	# Main thread searching for devices
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

		for dev in self.dm.get_devices():
			self.on_info(dev)

	def create_gui(self):
		"""Create the GUI"""
		# Add a treeview containing all devices
		self.liststore = Gtk.ListStore(str, str, str)
		self.treeview = Gtk.TreeView(model=self.liststore)
		self.add(self.treeview)

		# Add the first Name column
		column = Gtk.TreeViewColumn("Name", Gtk.CellRendererText(), text=1)
		column.set_min_width(200)
		self.treeview.append_column(column)

		# Add the protocol
		protocol_list = Gtk.ListStore(str)
		protocol_list.append(["None"])
		protocol_list.append(["DSM2"])
		protocol_list.append(["DSMX"])
		protocol = Gtk.CellRendererCombo()
		protocol.set_property("model", protocol_list)
		protocol.set_property("editable", True)
		protocol.set_property("text-column", 0)
		protocol.set_property("has-entry", False)
		protocol.connect("edited", self.on_protocol_change)
		column = Gtk.TreeViewColumn("Protocol", protocol, text=2)
		self.treeview.append_column(column)


	def on_protocol_change(self, widget, path, text):
		self.liststore[path][2] = text
		#

	def on_info(self, dev):
		"""When a device reports his information add it to the list"""
		GObject.idle_add(self._on_info, dev)

	def _on_info(self, dev):
		self.liststore.append([dev.port, dev.name, "None"])

	def on_disconnect(self, dev):
		"""When a device disconnects remove it from the list"""
		GObject.idle_add(self._on_disconnect, dev)

	def _on_disconnect(self, dev):
		for d in self.liststore:
			if d[0] == dev.port:
				self.liststore.remove(d.iter)
