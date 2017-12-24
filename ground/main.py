#!/usr/bin/env python
# Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>

import gi
import device
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GObject

class Ground(Gtk.Window):
    VERSION = 1.0

    def devices_page(self):
        self.dm = device.DeviceManager()
        self.dm.start()

        self.dm_page = Gtk.Box()
        self.dm_page.set_border_width(10)
        self.dm_page.add(Gtk.Label('TODO'))

        self.notebook.append_page(self.dm_page, Gtk.Label('Devices'))

    def __init__(self):
        Gtk.Window.__init__(self, title="USBRF Ground station")
        self.set_border_width(3)

        self.notebook = Gtk.Notebook()
        self.add(self.notebook)

        self.page1 = Gtk.Box()
        self.page1.set_border_width(10)
        self.page1.add(Gtk.Label('TODO'))
        self.notebook.append_page(self.page1, Gtk.Label('Overview'))

        self.devices_page()
        
    def on_delete(self, *args):
        self.dm.stop()
        Gtk.main_quit(*args)


if __name__ == "__main__":
    GObject.threads_init()
    win = Ground()
    win.connect("delete-event", win.on_delete)
    win.show_all()
    Gtk.main()