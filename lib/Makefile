##
## This file is part of the superbitrf project.
##
## Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
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

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
endif

all: locm3 ppzlink

locm3:
	$(Q)if [ ! "`ls -A libopencm3`" ] ; then \
	echo "######## ERROR ########"; \
	echo "\tlibopencm3 is not initialized."; \
	echo "\tPlease run:"; \
	echo "\t$$ git submodule init"; \
	echo "\t$$ git submodule update"; \
	echo "\tbefore running make."; \
	echo "######## ERROR ########"; \
	exit 1; \
	fi
	$(Q)$(MAKE) -C libopencm3 lib TARGETS="stm32/f1"

ppzlink:
	$(Q)if [ ! "`ls -A pprzlink`" ] ; then \
	echo "######## ERROR ########"; \
	echo "\pprzlink is not initialized."; \
	echo "\tPlease run:"; \
	echo "\t$$ git submodule init"; \
	echo "\t$$ git submodule update"; \
	echo "\tbefore running make."; \
	echo "######## ERROR ########"; \
	exit 1; \
	fi
	$(Q)$(MAKE) -C pprzlink pymessages

clean:
	$(Q)$(MAKE) -C libopencm3 clean
	$(Q)$(MAKE) -C pprzlink uninstall