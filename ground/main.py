#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>

import gi
import device
import transmitter
import scanner
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GObject

class Ground(Gtk.Window):
    VERSION = 1.0

    def __init__(self):
        # Initialize the device manager
        self.dm = device.DeviceManager()
        self.dm.start()

        # Initialize the scanner and transmitter manager
        self.tm = transmitter.TransmitterManager()
        self.scanner = scanner.Scanner(self.dm, self.tm)

        # Create the GUI
        self.create_gui()

    def create_gui(self):
        Gtk.Window.__init__(self, title="USBRF Ground station")
        self.set_border_width(3)
        self.set_default_size(800, 600)

        self.notebook = Gtk.Notebook()
        self.add(self.notebook)

        self.overview_page = scanner.ScannerManagerBox(self.scanner)
        self.notebook.append_page(self.overview_page, Gtk.Label('Scanner'))

        self.dm_page = device.DeviceManagerBox(self.dm)
        self.notebook.append_page(self.dm_page, Gtk.Label('Devices'))

    def on_delete(self, *args):
        self.dm.stop()
        Gtk.main_quit(*args)



if __name__ == "__main__":
    GObject.threads_init()
    win = Ground()
    win.connect("delete-event", win.on_delete)
    win.show_all()
    Gtk.main()