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

#ifndef HELPER_FRSKY_H_
#define HELPER_FRSKY_H_

#include <stdint.h>
#include <stdbool.h>

/* All times are in microseconds divided by 10 */
#define FRSKY_RECV_TIME			1000				//*< Time to wait for an FrSky packet */
#define FRSKY_SEND_TIME			900					//*< Time between 2 consecutive packets */ 
#define FRSKYX_USED_CHAN		47					//*< Amount of channels used by FrSkyX */
#define FRSKY_MAX_CHANNEL		235					//*< The highest channel number */

/* The different FrSky protocols */
enum frsky_protocol_t {
	FRSKYV = 0,
	FRSKYD,
	FRSKYX,
	FRSKYX_EU
};

/* External functions */
void frsky_set_config(enum frsky_protocol_t protocol);
void frsky_tune_channel(uint8_t ch);

#endif /* HELPER_FRSKY_H_ */
