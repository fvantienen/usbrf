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

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

struct protocol_t {
  char *name;
  void (*init)(void);
  void (*deinit)(void);
  void (*start)(void);
  void (*stop)(void);
  void (*run)(void);
  void (*status)(void);
  void (*parse_arg)(uint8_t, uint8_t*, uint16_t, uint16_t, uint16_t);
};

enum protocol_exec_type_t {
	PROTOCOL_STOP = 0,
	PROTOCOL_START,
	PROTOCOL_EXTRA,
};
extern uint16_t protocol_rc_chan[16];
extern uint8_t protocol_rc_chan_nb;

void protocol_init(void);
void protocol_run(void);

#endif /* PROTOCOL_H_ */
