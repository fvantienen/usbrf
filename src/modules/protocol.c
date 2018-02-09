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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "protocol.h"
#include "modules/console.h"
#include "modules/pprzlink.h"
#include "protocol/cyrf_scanner.h"
#include "protocol/dsm_hack.h"
#include "protocol/cc_scanner.h"
#include "protocol/frsky_hack.h"
#include "protocol/frsky_receiver.h"
#include "protocol/frsky_transmitter.h"

/* All protocol information */
static struct protocol_t *protocols[] = {
	&protocol_cyrf_scanner,
	&protocol_dsm_hack,
	&protocol_cc_scanner,
	&protocol_frsky_hack,
	&protocol_frsky_receiver,
	&protocol_frsky_transmitter,
};
static const int protocols_nb = sizeof(protocols) / sizeof(protocols[0]);
static int protocol_cur_idx;
static bool protocol_running;
uint16_t protocol_rc_chan[16];					//*< The received rc channels */
uint8_t protocol_rc_chan_nb;						//*< The amount of received rc channels */ 

/* Console commands */
static void protocol_cmd_list(char *cmdLine);
static void protocol_cmd_set(char *cmdLine);
static void protocol_cmd_start(char *cmdLine);
static void protocol_cmd_stop(char *cmdLine);
static void protocol_cmd_status(char *cmdLine);

/* PPRZ bindings */
static void protocol_pprz_exec(uint8_t *data);
static void protocol_pprz_rc_data(uint8_t *data);

/**
 * Initialize all protocols
 */
void protocol_init(void) {
  // Set the current protocol
  protocol_cur_idx = -1;
  protocol_running = false;
  protocol_rc_chan_nb = 0;

  // Add console commands
  console_cmd_add("plist", "", protocol_cmd_list);
  console_cmd_add("pset", "[protocol_id]", protocol_cmd_set);
  console_cmd_add("start", "", protocol_cmd_start);
  console_cmd_add("stop", "", protocol_cmd_stop);
  console_cmd_add("status", "", protocol_cmd_status);

  // Add PPRZ bindings
  pprzlink_register_cb(PPRZ_MSG_ID_PROT_EXEC, protocol_pprz_exec);
  pprzlink_register_cb(PPRZ_MSG_ID_RC_DATA, protocol_pprz_rc_data);
}

/**
 * Run the protocol
 */
void protocol_run(void) {
  if(protocol_cur_idx >= 0 && protocol_running)
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

	// Check if we were not running
	if(protocol_running) {
		console_print("\r\nCan't change the protocol because the current protocol is running");
		return;
	}

	// Parse the set command
	if(sscanf(cmdLine, "%d", &value) != 1) {
		if(protocol_cur_idx >= 0)
			protocols[protocol_cur_idx]->deinit();
		protocol_cur_idx = -1;
		console_print("\r\nThe current protocol is changed to NONE");
	}
	else if(value >= 0 && value < protocols_nb) {
		if(protocol_cur_idx >= 0)
			protocols[protocol_cur_idx]->deinit();
		protocol_cur_idx = value;
		protocols[protocol_cur_idx]->init();
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
	if(protocol_cur_idx >= 0 && !protocol_running) {
		protocols[protocol_cur_idx]->start();
		protocol_running = true;
		console_print("\r\nStarted protocol %s.", protocols[protocol_cur_idx]->name);
	} else if(protocol_running) {
		console_print("\r\nProtocol already started");
	} else {
		console_print("\r\nNo protocol selected.");
	}
}

/**
 * Stop the current protocol
 */
static void protocol_cmd_stop(char *cmdLine __attribute__((unused))) {
	if(protocol_cur_idx >= 0 && protocol_running) {
		protocols[protocol_cur_idx]->stop();
		protocol_running = false;
		console_print("\r\nStopped protocol %s.", protocols[protocol_cur_idx]->name);
	} else if(!protocol_running) {
		console_print("\r\nProtocol already stopped");
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
	  console_print("\r\n\tRunning: %s", protocol_running? "yes":"no");
	  protocols[protocol_cur_idx]->status();
	} else {
		console_print("\r\nNo protocol selected.");
	}
}

/**
 * Execute a protocol command through pprzlink
 */
static void protocol_pprz_exec(uint8_t *data) {
	// Check if we need to change protocol
	int8_t prot_id = DL_PROT_EXEC_id(data);
	if(prot_id != protocol_cur_idx) {
		if(protocol_cur_idx >= 0) {
			if(protocol_running)
				protocols[protocol_cur_idx]->stop();

			protocols[protocol_cur_idx]->deinit();
		}

		if(prot_id < protocols_nb)
			protocol_cur_idx = prot_id;

		// Return if we change to inactive
		if(prot_id < 0)
			return;

		protocols[protocol_cur_idx]->init();
		protocol_running = false;
	}

	// Check if the protocol is running
	uint8_t type = DL_PROT_EXEC_type(data);
	if((type == PROTOCOL_START || type == PROTOCOL_STOP) && protocol_running) {
		protocols[protocol_cur_idx]->stop();
		protocol_running = false;
	}

	// Parse the arguments
	uint16_t arg_offset = DL_PROT_EXEC_arg_offset(data);
	uint16_t arg_size = DL_PROT_EXEC_arg_size(data);
	uint8_t arg_len = DL_PROT_EXEC_arg_data_length(data);
	if((arg_size-arg_offset) > 0 && protocols[protocol_cur_idx]->parse_arg != NULL)
		protocols[protocol_cur_idx]->parse_arg(type, DL_PROT_EXEC_arg_data(data), arg_len, arg_offset, arg_size);

	// Start the protocol if the arguments are succesfully received
	if(type == PROTOCOL_START && arg_offset+arg_len >= arg_size) {
		protocols[protocol_cur_idx]->start();
		protocol_running = true;
	}
}

/**
 * Whenever we receive rc data through pprzlink
 */
static void protocol_pprz_rc_data(uint8_t *data) {
	uint8_t chan_nb = DL_RC_DATA_data_length(data);
	uint16_t *channels = DL_RC_DATA_data(data);
	if(chan_nb > 16)
		chan_nb = 16;

	protocol_rc_chan_nb = chan_nb;
	memcpy(protocol_rc_chan, channels, chan_nb*sizeof(uint16_t));
}