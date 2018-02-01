/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2018 Freek van Tienen <freek.v.tienen@gmail.com>
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

#ifndef DSM_HACK_H_
#define DSM_HACK_H_

#include "modules/protocol.h"

extern struct protocol_t protocol_dsm_hack;

/* The internal status of the DSM hacking protocol */
enum dsm_hack_status_t {
	DSM_HACK_SYNC,				/**< The receiver is syncing with the TX */
	DSM_HACK_RECV_A,			/**< The receiver is receiving channel A */
	DSM_HACK_RECV_B,			/**< The receiver is receiving channel B */
	DSM_HACK_SEND_A,			/**< The receiver is taking over control channel A */
	DSM_HACK_SEND_B,			/**< The receiver is taking over control channel B */
};

#endif /* DSM_HACK_H_ */
