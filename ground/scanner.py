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
				if rf_time[0] == rfchip.name and len(rf_time[3]) != 0:
					rfchip.start_scanning(rf_time[3])
				elif rf_time[0] == rfchip.name:
					print('Not enough devices for ' + rfchip.name)

		print('Starting scanner...')

	def hack(self):
		devices = list(self.dm.devices)
		tms = []
		for tx in self.tm.transmitters:
			if tx.hackable and tx.do_hack:
				tms.append(tx)

		tms.sort(key=lambda tx: tx.recv_cnt, reverse = True)

		while len(tms) != 0:
			tx = tms[0]
			for dev in devices:
				if tx.rfchip.__class__ in dev.get_chips():
					tx.start_hacking(dev)
					devices.remove(dev)
					break
			tms.remove(tx)

		print('Starting hacking...')

	def on_recv_data(self, msg):
		"""When we receive data from a scanning chip"""
		for rfchip in self.rfchips:
			if rfchip.id == msg.chip_id:
				tx = rfchip.parse_recv_msg(msg.data)
				if tx != None:
					self.tm.add_or_merge(tx, rfchip)

	def set(self, prot_name, val):
		"""Set the state of a protocol"""
		for rfchip in self.rfchips:
			prot = rfchip.set(prot_name, val)
			if prot != None:
				return prot

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
				self.prot_liststore.append([prot.name, prot.state.name, int(prot.get_scan_time()/1000000)])

	def create_gui(self):
		"""Create the GUI"""
		self.grid = Gtk.Grid()
		self.add(self.grid)

		self.clear_but = Gtk.Button(label='Clear')
		self.clear_but.connect("clicked", self.on_clear)
		self.grid.attach(self.clear_but, 1, 0, 1, 1)

		self.scan_but = Gtk.Button(label='Scan')
		self.scan_but.connect("clicked", self.on_scan)
		self.grid.attach(self.scan_but, 2, 0, 1, 1)

		self.hack_but = Gtk.Button(label='Hack')
		self.hack_but.connect("clicked", self.on_hack)
		self.grid.attach(self.hack_but, 3, 0, 1, 1)

		# Create the sub GUI's
		self.create_gui_transmitters()
		self.create_gui_protocols()

	def create_gui_transmitters(self):
		"""Create a GUI for the transmitter overview"""
		self.tx_liststore = Gtk.ListStore(str, str, bool, int, bool, int, int, int, int)
		self.tx_treeview = Gtk.TreeView(model=self.tx_liststore)
		self.grid.attach(self.tx_treeview, 0, 0, 1, 20)

		# Add the protocol name column
		column = Gtk.TreeViewColumn("Protocol", Gtk.CellRendererText(), text=0)
		column.set_min_width(50)
		column.set_sort_column_id(0)
		self.tx_treeview.append_column(column)

		# Add the transmitter id column
		column = Gtk.TreeViewColumn("ID", Gtk.CellRendererText(), text=1)
		column.set_min_width(100)
		column.set_sort_column_id(1)
		self.tx_treeview.append_column(column)

		# Add the hackable information
		column = Gtk.TreeViewColumn("Hackable", Gtk.CellRendererText(), text=2)
		column.set_sort_column_id(2)
		self.tx_treeview.append_column(column)

		# Add the received packets information
		column = Gtk.TreeViewColumn("Rcnt", Gtk.CellRendererText(), text=3)
		column.set_sort_column_id(3)
		self.tx_treeview.append_column(column)

		# Show channels
		column = Gtk.TreeViewColumn("C0", Gtk.CellRendererProgress(), value=5)
		self.tx_treeview.append_column(column)
		column = Gtk.TreeViewColumn("C1", Gtk.CellRendererProgress(), value=6)
		self.tx_treeview.append_column(column)
		column = Gtk.TreeViewColumn("C2", Gtk.CellRendererProgress(), value=7)
		self.tx_treeview.append_column(column)
		column = Gtk.TreeViewColumn("C3", Gtk.CellRendererProgress(), value=8)
		self.tx_treeview.append_column(column)

		# Add the enable/disable hacking
		hack_toggle = Gtk.CellRendererToggle()
		hack_toggle.connect("toggled", self.on_hack_toggle)
		column = Gtk.TreeViewColumn("HACK", hack_toggle, active=4)
		self.tx_treeview.append_column(column)
		
	def create_gui_protocols(self):
		"""Create the GUI for the protocols overview"""
		# Add a treeview containing all protocols
		self.prot_liststore = Gtk.ListStore(str, str, int)
		self.prot_treeview = Gtk.TreeView(model=self.prot_liststore)
		self.grid.attach(self.prot_treeview, 1, 1, 3, 1)

		# Add the first Name column
		column = Gtk.TreeViewColumn("Name", Gtk.CellRendererText(), text=0)
		self.prot_treeview.append_column(column)

		# Add the channels column
		state_list = Gtk.ListStore(str)
		for state in protocol.ProtState:
			state_list.append([state.name])

		state = Gtk.CellRendererCombo()
		state.set_property("model", state_list)
		state.set_property("editable", True)
		state.set_property("text-column", 0)
		state.set_property("has-entry", False)
		state.connect("edited", self.on_prot_state_change)
		column = Gtk.TreeViewColumn("Channels", state, text=1)
		self.prot_treeview.append_column(column)

		# Add the calculated time column
		column = Gtk.TreeViewColumn("Time(s)", Gtk.CellRendererText(), text=2)
		self.prot_treeview.append_column(column)


	def on_txm_change(self):
		"""Whenever a TX is changed or added in the list"""
		GObject.idle_add(self._on_txm_change)
	
	def _on_txm_change(self):
		"""Called by GTK main thread to show new transmitters"""
		self.tx_liststore.clear()
		for tx in self.scanner.tm.transmitters:
			self.tx_liststore.append([tx.prot_name, tx.get_id_str(), tx.hackable, tx.recv_cnt, tx.do_hack, int(tx.channel_values[0]), int(tx.channel_values[1]), int(tx.channel_values[2]), int(tx.channel_values[3])])

	def on_hack_toggle(self, widget, path):
		self.scanner.tm.hack_toggle(self.tx_liststore[path][0], self.tx_liststore[path][1])
		self.tx_liststore[path][4] = not self.tx_liststore[path][4]

	def on_prot_state_change(self, widget, path, text):
		"""Whenever the protocol state is changed"""
		prot = self.scanner.set(self.prot_liststore[path][0], protocol.ProtState[text])
		self.prot_liststore[path][1] = text
		self.prot_liststore[path][2] = int(prot.get_scan_time()/1000000)

	def on_scan(self, widget):
		"""Start scanning"""
		self.scanner.start()

	def on_hack(self, widget):
		"""Start hacking"""
		self.scanner.hack()

	def on_clear(self, widget):
		"""Clear the scanned information"""
		self.scanner.tm.clear()
