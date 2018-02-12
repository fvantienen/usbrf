#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
import rfchip
import protocol
import transmitter
import gi
import sys
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GObject
from enum import IntEnum

class CellRendererClickablePixbuf(Gtk.CellRendererPixbuf):
  __gsignals__ = {'clicked': (GObject.SIGNAL_RUN_LAST, GObject.TYPE_NONE,
                              (GObject.TYPE_STRING,))
                 }
  def do_activate(self, event, widget, path, background_area, cell_area, flags):
    self.emit('clicked', path)
  def __init__(self, *args, **kwargs):
    Gtk.CellRendererPixbuf.__init__(self, *args, **kwargs)
    self.set_property('mode', Gtk.CellRendererMode.ACTIVATABLE)

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
			if tx.do_hack:
				tms.append(tx)

		tms.sort(key=lambda tx: tx.recv_cnt, reverse = True)

		while len(tms) != 0:
			tx = tms[0]
			done = False
			for dev in devices:
				if tx.rfchip.__class__ in dev.get_chips():
					tx.start_hacking(dev)
					devices.remove(dev)
					done = True
					break

			if not done:
				print('Could not start hacking on ' + tx.get_id_str() + ', not enough devices!')
			tms.remove(tx)

		print('Starting hacking...')

	def stop(self):
		for dev in self.dm.devices:
			dev.stop_prot()

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

	class TxFields(IntEnum):
		cls = 0
		name = 1
		prot_name = 2
		id_str = 3
		hackable = 4
		recv_cnt = 5
		do_hack = 6
		chan0 = 7
		chan1 = 8
		chan2 = 9
		chan3 = 10

	def __init__(self, scanner):
		self.scanner = scanner
		self.pause = False
		self.rcnt_min = 0

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

		self.scan_but = Gtk.Button(label='Scan')
		self.scan_but.connect("clicked", self.on_scan)
		self.grid.attach(self.scan_but, 11, 1, 1, 1)

		self.hack_but = Gtk.Button(label='Hack')
		self.hack_but.connect("clicked", self.on_hack)
		self.grid.attach(self.hack_but, 12, 1, 1, 1)

		self.stop_but = Gtk.Button(label='Stop')
		self.stop_but.connect("clicked", self.on_stop)
		self.grid.attach(self.stop_but, 13, 1, 1, 1)

		# Create the sub GUI's
		self.create_gui_transmitters()
		self.create_gui_protocols()

	def create_gui_transmitters(self):
		"""Create a GUI for the transmitter overview"""
		# Add save, load and clear buttons
		self.save_but = Gtk.Button(label='Save')
		self.save_but.connect("clicked", self.on_save)
		self.grid.attach(self.save_but, 0, 0, 1, 1)

		self.save_but = Gtk.Button(label='Load')
		self.save_but.connect("clicked", self.on_load)
		self.grid.attach(self.save_but, 1, 0, 1, 1)

		self.clear_but = Gtk.Button(label='Clear')
		self.clear_but.connect("clicked", self.on_clear)
		self.grid.attach(self.clear_but, 2, 0, 1, 1)

		self.pause_but = Gtk.ToggleButton(label='Pause')
		self.pause_but.connect("toggled", self.on_pause)
		self.grid.attach(self.pause_but, 3, 0, 1, 1)

		# Add the RCNT filter
		self.rcnt_spinb = Gtk.SpinButton()
		self.rcnt_spinb.set_adjustment(Gtk.Adjustment(0, 0, 500, 1, 5, 0))
		self.rcnt_spinb.connect("value-changed", self.on_rcnt_change)
		self.grid.attach(self.rcnt_spinb, 4, 0, 1, 1)

		# Create the treeview and liststore
		self.tx_liststore = Gtk.ListStore(transmitter.Transmitter, str, str, str, int, int, bool, int, int, int, int)
		self.rcnt_filter = self.tx_liststore.filter_new()
		self.rcnt_filter.set_visible_func(self.filter_rcnt)
		self.tx_treeview = Gtk.TreeView(model=self.rcnt_filter)
		self.grid.attach(self.tx_treeview, 0, 1, 10, 20)

		# Add the protocol name column
		cell = Gtk.CellRendererText()
		cell.set_property("editable", True)
		cell.connect("edited", self.item_change_name)
		column = Gtk.TreeViewColumn("Name", cell, text=self.TxFields.name)
		column.set_min_width(200)
		column.set_sort_column_id(self.TxFields.name)
		self.tx_treeview.append_column(column)

		# Add the protocol name column
		column = Gtk.TreeViewColumn("Protocol", Gtk.CellRendererText(), text=self.TxFields.prot_name)
		column.set_min_width(50)
		column.set_sort_column_id(self.TxFields.prot_name)
		self.tx_treeview.append_column(column)

		# Add the transmitter id column
		column = Gtk.TreeViewColumn("ID", Gtk.CellRendererText(), text=self.TxFields.id_str)
		column.set_min_width(100)
		column.set_sort_column_id(self.TxFields.id_str)
		self.tx_treeview.append_column(column)

		# Add the hackable information
		column = Gtk.TreeViewColumn("Hackable", Gtk.CellRendererProgress(), value=self.TxFields.hackable)
		column.set_sort_column_id(self.TxFields.hackable)
		self.tx_treeview.append_column(column)

		# Add the received packets information
		column = Gtk.TreeViewColumn("Rcnt", Gtk.CellRendererText(), text=self.TxFields.recv_cnt)
		column.set_sort_column_id(self.TxFields.recv_cnt)
		self.tx_treeview.append_column(column)

		# Show channels
		column = Gtk.TreeViewColumn("C0", Gtk.CellRendererProgress(), value=self.TxFields.chan0)
		self.tx_treeview.append_column(column)
		column = Gtk.TreeViewColumn("C1", Gtk.CellRendererProgress(), value=self.TxFields.chan1)
		self.tx_treeview.append_column(column)
		column = Gtk.TreeViewColumn("C2", Gtk.CellRendererProgress(), value=self.TxFields.chan2)
		self.tx_treeview.append_column(column)
		column = Gtk.TreeViewColumn("C3", Gtk.CellRendererProgress(), value=self.TxFields.chan3)
		self.tx_treeview.append_column(column)

		# Add the enable/disable hacking
		hack_toggle = Gtk.CellRendererToggle()
		hack_toggle.connect("toggled", self.item_hack_toggle)
		column = Gtk.TreeViewColumn("HACK", hack_toggle, active=self.TxFields.do_hack)
		self.tx_treeview.append_column(column)

		# Action buttons
		column = Gtk.TreeViewColumn("Actions")

		but_del = CellRendererClickablePixbuf(icon_name = "edit-delete")
		but_del.connect('clicked', self.item_on_delete)
		column.pack_start(but_del, False)

		but_hack = CellRendererClickablePixbuf(icon_name = "go-next")
		but_hack.connect('clicked', self.item_on_hack)
		column.pack_start(but_hack, True)

		self.tx_treeview.append_column(column)
		
	def create_gui_protocols(self):
		"""Create the GUI for the protocols overview"""
		# Add a treeview containing all protocols
		self.prot_liststore = Gtk.ListStore(str, str, int)
		self.prot_treeview = Gtk.TreeView(model=self.prot_liststore)
		self.grid.attach(self.prot_treeview, 10, 2, 4, 1)

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

	def filter_rcnt(self, model, iter, data):
		"""Filter based on the receive count"""
		rcnt = self.tx_liststore[iter][self.TxFields.recv_cnt]
		return rcnt >= self.rcnt_min

	def on_rcnt_change(self, widget):
		"""When the RCNT filter has changed value"""
		self.rcnt_min = widget.get_value_as_int()
		self.rcnt_filter.refilter()

	def on_txm_change(self):
		"""Whenever a TX is changed or added in the list"""
		if not self.pause:
			GObject.idle_add(self._on_txm_change)
	
	def _on_txm_change(self):
		"""Called by GTK main thread to show new transmitters"""
		#TODO: do not clear the whole list!
		#self.tx_liststore.clear()
		txs = list(self.scanner.tm.transmitters)
		for txl in self.tx_liststore:
			tx = txl[self.TxFields.cls]
			if tx in txs:
				txl[self.TxFields.name] = tx.name
				txl[self.TxFields.id_str] = tx.get_id_str()
				txl[self.TxFields.hackable] = tx.hackable
				txl[self.TxFields.recv_cnt] = tx.recv_cnt
				txl[self.TxFields.do_hack] = tx.do_hack
				txl[self.TxFields.chan0] = int(tx.channel_values[0])
				txl[self.TxFields.chan1] = int(tx.channel_values[1])
				txl[self.TxFields.chan2] = int(tx.channel_values[2])
				txl[self.TxFields.chan3] = int(tx.channel_values[3])
				txs.remove(tx)
			else:
				self.tx_liststore.remove(txl.iter)
		
		# Append all new transmitters
		for tx in txs:
			self.tx_liststore.append([tx, tx.name, tx.prot_name, tx.get_id_str(), tx.hackable, tx.recv_cnt, tx.do_hack, int(tx.channel_values[0]), int(tx.channel_values[1]), int(tx.channel_values[2]), int(tx.channel_values[3])])

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

	def on_stop(self, widget):
		"""Stop all activities"""
		self.scanner.stop()

	def on_clear(self, widget):
		"""Clear the transmitters information"""
		self.scanner.tm.clear()

	def on_save(self, widget):
		"""Save the transmitters information to json"""
		self.scanner.tm.save()

	def on_load(self, widget):
		"""Load the transmitters information from json"""
		self.scanner.tm.load()

	def on_pause(self, widget):
		"""Pause or unpause the interface"""
		if widget.get_active():
			self.pause = True
		else:
			self.pause = False
			self.on_txm_change()

	def item_change_name(self, widget, path, text):
		"""Change the name of the transmitter in the list"""
		tx = self.tx_liststore[path][self.TxFields.cls]
		tx.name = text
		self.tx_liststore[path][self.TxFields.cls.name] = text

	def item_hack_toggle(self, widget, path):
		"""Enable or disable hacking when using generic actions"""
		tx = self.tx_liststore[path][self.TxFields.cls]
		tx.do_hack = not self.tx_liststore[path][self.TxFields.cls.do_hack]
		self.tx_liststore[path][self.TxFields.cls.do_hack] = tx.do_hack

	def item_on_delete(self, widget, path):
		"""Delete a specific transmitter from the list"""
		tx = self.tx_liststore[path][self.TxFields.cls]
		self.scanner.tm.transmitters.remove(tx)
		self.tx_liststore.remove(self.tx_liststore[path].iter)

	def item_on_hack(self, widget, path):
		"""Start hacking this specific transmitter from the list"""
		tx = self.tx_liststore[path][self.TxFields.cls]

		done = False
		for dev in self.scanner.dm.devices:
			if not dev.is_running() or dev.is_scanning():
				tx.start_hacking(dev)
				done = True
				break
		
		if not done:
			print("Not enough devices to attack " + tx.get_id_str())
