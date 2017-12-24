##
## This file is part of the superbitrf project.
##
## Copyright (C) 2013 Piotr Esden-Tempski <piotr@esden.net>
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

TEST_TARGETS := test/blink test/usb_cdcacm test/transfer test/multi_usb_cdcacm test/console test/config

# Be silent per default, but 'make V=1' will show all compiler calls.
ifneq ($(V),1)
Q := @
endif

all: lib main tests

$(TEST_TARGETS): lib
	@printf "  BUILD   $@\n";
	$(Q)$(MAKE) --directory=$@

tests: $(TEST_TARGETS)
	$(Q)true

main: lib
	@printf "  BUILD   main\n";
	$(Q)$(MAKE) --directory=src

lib:
	$(Q)$(MAKE) -C lib

flash: main
	$(Q)$(MAKE) -C src flash

clean:
	@printf "  CLEAN   lib\n"
	$(Q)$(MAKE) -C lib clean
	@printf "  CLEAN   src\n"
	$(Q)$(MAKE) -C src clean
	$(Q)for i in $(TEST_TARGETS); do \
		if [ -d $$i ]; then \
			printf "  CLEAN   test/$$i\n"; \
			$(MAKE) -C $$i clean SRCLIBDIR=$(SRCLIBDIR) || exit $?; \
		fi; \
	done

.PHONY: all lib
