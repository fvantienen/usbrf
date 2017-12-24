/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2017 Freek van Tienen <freek.v.tienen@gmail.com>
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

#ifndef PPRZLINK_H_
#define PPRZLINK_H_

#include "pprzlink/pprz_transport.h"

/* Main paparazzi link variables */
struct pprzlink_t {
	struct link_device dev;		///< The communication device
	struct pprz_transport tp;	///< The transport layer state
	uint8_t recv_buf[256];		///< The receive message buffer
	bool msg_received;				///< Whether a message is received or not
	struct ring *r_rx;				///< The receive ring buffer
	struct ring *r_tx;				///< The transmit ring buffer
};

/* Extern variables and functions */
extern struct pprzlink_t pprzlink;
void pprzlink_init(void);
void pprzlink_run(void);

#endif /* PPRZLINK_H_ */
