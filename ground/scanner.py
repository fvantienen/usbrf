#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
import rfchip
import protocol
import gi
import sys
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GObject

# The TX device scanner
class Scanner():

	def __init__(self, dm, tm):
		"""Intialize the scanner with the device manager and transmitter manager"""
		self.dm = dm
		self.tm = tm
		self.rfchips = [rfchip.CYRF6936(), rfchip.CC2500()]

	def start(self):
		"""Start scanning for TX Devices"""
		dev_cnt = len(self.dm.devices)
		devices = list(self.dm.devices)
		rf_times = []

		# Calculate the scanning time per chip
		for rfchip in self.rfchips:
			rf_times.append([rfchip.name, rfchip.calc_scan_time(), 0, []])
			print(rfchip.name + ': ' + str(rfchip.calc_scan_time()))

		# Add devices with only a single chip
		for dev in devices:
			if len(dev.chips) == 1:
				for rf_time in rf_times:
					if rf_time[0] == dev.chips[0].NAME:
						rf_time[3].append(dev)
						rf_time[2] += 1
						devices.remove(dev)

		# Sort the rf_times
		for rf_time in rf_times:
			if rf_time[1] == 0:
				rf_times.remove(rf_time)
		rf_times.sort(key=lambda rf_time: sys.maxint if rf_time[2] == 0 else (rf_time[1]/rf_time[2]), reverse=True)

		# For al the other devices make a smart choice
		while len(devices) != 0:
			dev = devices[0]
			rf_times[0][3].append(dev)
			rf_times[0][2] += 1
			devices.remove(dev)
			rf_times.sort(key=lambda rf_time: sys.maxint if rf_time[2] == 0 else (rf_time[1]/rf_time[2]), reverse=True)

		# Register the receive on all devices
		self.dm.register_recv("RECV_DATA", self.on_recv_data)

		# Start scanning on all devices
		for rfchip in self.rfchips:
			for rf_time in rf_times:
				if rf_time[0] == rfchip.name:
					rfchip.start_scanning(rf_time[3])

		print('Start')

	def on_recv_data(self, msg):
		"""When we receive data from a scanning chip"""
		for rfchip in self.rfchips:
			if rfchip.id == msg.chip_id:
				tx = rfchip.parse_recv_msg(msg.data)
				if tx != None:
					self.tm.add_or_merge(tx)

	def set(self, prot_name, val):
		"""Set the state of a protocol"""
		for rfchip in self.rfchips:
			rfchip.set(prot_name, val)

# The TX Device Scanner overview
class ScannerManagerBox(Gtk.Box):

	def __init__(self, scanner):
		self.scanner = scanner

		# Create the GUI
		Gtk.Box.__init__(self)
		self.create_gui()

		# Bind to tx list changes
		self.scanner.tm.set_on_change(self.on_txm_change)

		# Append all protocols
		for rfchip in self.scanner.rfchips:
			for prot in rfchip.protocols:
				self.prot_liststore.append([prot.name, prot.state.name])

	def create_gui(self):
		"""Create the GUI"""
		self.grid = Gtk.Grid()
		self.add(self.grid)

		self.clear_but = Gtk.Button(label='Clear')
		self.clear_but.connect("clicked", self.on_clear)
		self.grid.attach(self.clear_but, 1, 0, 1, 1)

		self.start_but = Gtk.Button(label='Start')
		self.start_but.connect("clicked", self.on_start)
		self.grid.attach(self.start_but, 2, 0, 1, 1)

		# Create the sub GUI's
		self.create_gui_transmitters()
		self.create_gui_protocols()

	def create_gui_transmitters(self):
		"""Create a GUI for the transmitter overview"""
		self.tx_liststore = Gtk.ListStore(str, str, bool, int)
		self.tx_treeview = Gtk.TreeView(model=self.tx_liststore)
		self.grid.attach(self.tx_treeview, 0, 0, 1, 2)

		# Add the protocol name column
		column = Gtk.TreeViewColumn("Protocol", Gtk.CellRendererText(), text=0)
		column.set_min_width(50)
		self.tx_treeview.append_column(column)

		# Add the transmitter id column
		column = Gtk.TreeViewColumn("ID", Gtk.CellRendererText(), text=1)
		column.set_min_width(100)
		self.tx_treeview.append_column(column)

		# Add the hackable information
		column = Gtk.TreeViewColumn("Hackable", Gtk.CellRendererText(), text=2)
		self.tx_treeview.append_column(column)

		# Add the received packets information
		column = Gtk.TreeViewColumn("Recv packets", Gtk.CellRendererText(), text=3)
		self.tx_treeview.append_column(column)
		
	def create_gui_protocols(self):
		"""Create the GUI for the protocols overview"""
		# Add a treeview containing all protocols
		self.prot_liststore = Gtk.ListStore(str, str)
		self.prot_treeview = Gtk.TreeView(model=self.prot_liststore)
		self.grid.attach(self.prot_treeview, 1, 1, 2, 1)

		# Add the first Name column
		column = Gtk.TreeViewColumn("Name", Gtk.CellRendererText(), text=0)
		self.prot_treeview.append_column(column)

		# Add the state column
		state_list = Gtk.ListStore(str)
		for state in protocol.ProtState:
			state_list.append([state.name])

		state = Gtk.CellRendererCombo()
		state.set_property("model", state_list)
		state.set_property("editable", True)
		state.set_property("text-column", 0)
		state.set_property("has-entry", False)
		state.connect("edited", self.on_prot_state_change)
		column = Gtk.TreeViewColumn("State", state, text=1)
		self.prot_treeview.append_column(column)

	def on_txm_change(self):
		"""Whenever a TX is changed or added in the list"""
		GObject.idle_add(self._on_txm_change)
	
	def _on_txm_change(self):
		"""Called by GTK main thread to show new transmitters"""
		self.tx_liststore.clear()
		for tx in self.scanner.tm.transmitters:
			self.tx_liststore.append([tx.prot_name, tx.get_id_str(), tx.attackable, tx.recv_cnt])

	def on_prot_state_change(self, widget, path, text):
		"""Whenever the protocol state is changed"""
		self.scanner.set(self.prot_liststore[path][0], protocol.ProtState[text])
		self.prot_liststore[path][1] = text

	def on_start(self, widget):
		"""Start or stop scanning"""
		self.scanner.start()

	def on_clear(self, widget):
		"""Clear the scanned information"""
		self.scanner.tm.clear()
