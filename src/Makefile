##
## This file is part of the libopencm3 project.
##
## Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>
##
## This library is free software: you can redistribute it and/or modify
## it under the terms of the GNU Lesser General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This library is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Lesser General Public License for more details.
##
## You should have received a copy of the GNU Lesser General Public License
## along with this library.  If not, see <http://www.gnu.org/licenses/>.
##

BINARY = usbrf

# The modules and helpers used for the usbrf module
OBJS += modules/led.o modules/spi.o modules/button.o modules/timer.o modules/cdcacm.o modules/cyrf6936.o modules/cc2500.o modules/config.o
OBJS += modules/console.o modules/ring.o modules/counter.o modules/ant_switch.o modules/pprzlink.o modules/protocol.o helper/crc.o helper/dsm.o helper/frsky.o

# The different kind of protocols available
OBJS += protocol/cyrf_scanner.o protocol/dsm_hack.o protocol/cc_scanner.o protocol/frsky_hack.o protocol/frsky_receiver.o

# Enable pprzlink
PPRZLINK = 1

include ../Makefile.include
