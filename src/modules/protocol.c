/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2015 Freek van Tienen <freek.v.tienen@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>
#include "protocol.h"
#include "console.h"
#include "protocol/dsm_hack.h"

/* All protocols */
static struct protocol_t *protocols[] = { &protocol_dsm_hack };
static const int protocols_nb = sizeof(protocols) / sizeof(protocols[0]);
static int protocol_cur_idx;

/* Console commands */
static void protocol_cmd_list(char *cmdLine);
static void protocol_cmd_set(char *cmdLine);
static void protocol_cmd_start(char *cmdLine);
static void protocol_cmd_stop(char *cmdLine);
static void protocol_cmd_status(char *cmdLine);

/**
 * Initialize all protocols
 */
void protocol_init(void) {
  // Set the current protocol
  protocol_cur_idx = -1;

  // Add console commands
  console_cmd_add("plist", "", protocol_cmd_list);
  console_cmd_add("pset", "[protocol_id]", protocol_cmd_set);
  console_cmd_add("start", "", protocol_cmd_start);
  console_cmd_add("stop", "", protocol_cmd_stop);
  console_cmd_add("status", "", protocol_cmd_status);
}

/**
 * Run the protocol
 */
void protocol_run(void) {
  if(protocol_cur_idx >= 0)
    protocols[protocol_cur_idx]->run();
}

/**
 * List all available protocols
 */
static void protocol_cmd_list(char *cmdLine __attribute__((unused))) {
	uint8_t i;
	console_print("\r\nAvailable protocols:");
	for(i = 0; i < protocols_nb; i++) {
		console_print("\r\n\t%d: %s", i, protocols[i]->name);
	}
}

/**
 * Set the current protocol
 */
static void protocol_cmd_set(char *cmdLine) {
	int value = -1;
	if(sscanf(cmdLine, "%d", &value) != 1) {
		protocol_cur_idx = -1;
		console_print("\r\nThe current protocol is changed to NONE");
	}
	else if(value >= 0 && value < protocols_nb) {
		protocol_cur_idx = value;
		console_print("\r\nThe current protocol is changed to %s", protocols[value]->name);
	}
	else{
		console_print("\r\nThe protocol number needs to between 0 and %d", protocols_nb);
	}
}

/**
 * Start the current protocol
 */
static void protocol_cmd_start(char *cmdLine __attribute__((unused))) {
	if(protocol_cur_idx >= 0) {
		protocols[protocol_cur_idx]->start();
		console_print("\r\nStarted protocol %s.", protocols[protocol_cur_idx]->name);
	} else {
		console_print("\r\nNo protocol selected.");
	}
}

/**
 * Stop the current protocol
 */
static void protocol_cmd_stop(char *cmdLine __attribute__((unused))) {
	if(protocol_cur_idx >= 0) {
		protocols[protocol_cur_idx]->stop();
		console_print("\r\nStopped protocol %s.", protocols[protocol_cur_idx]->name);
	} else {
		console_print("\r\nNo protocol selected.");
	}
}

/**
 * Status of the current protocol
 */
static void protocol_cmd_status(char *cmdLine __attribute__((unused))) {
	if(protocol_cur_idx >= 0) {
	  console_print("\r\nProtocol");
	  console_print("\r\n\tCurrent: %s", protocols[protocol_cur_idx]->name);
	  protocols[protocol_cur_idx]->status();
	} else {
		console_print("\r\nNo protocol selected.");
	}
}
