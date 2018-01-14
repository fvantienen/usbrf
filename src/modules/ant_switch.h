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

#ifndef MODULES_ANT_SWITCH_H_
#define MODULES_ANT_SWITCH_H_

#include <stdbool.h>

/* External functions for the antenna swither */
void ant_switch_init(void);
void ant_switch(bool *state);

#endif /* MODULES_ANT_SWITCH_H_ */
