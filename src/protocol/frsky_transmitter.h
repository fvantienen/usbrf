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

#ifndef FRSKY_TRANSMITTER_H_
#define FRSKY_TRANSMITTER_H_

#include "modules/protocol.h"

extern struct protocol_t protocol_frsky_transmitter;

/* The internal states of the FrSky transmitter protocol */
enum frsky_transmitter_state_t {
	FRSKY_TRX_SEND,			/**< The transmitter is sending the data packet */
//	FRSKY_TRX_TELEM,		/**< The transmitter is waiting for a telemetry packet */
};

#endif /* FRSKY_TRANSMITTER_H_ */
