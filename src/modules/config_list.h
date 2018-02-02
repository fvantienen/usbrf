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

#ifndef CONFIG_ITEM
#define CONFIG_ITEM(...) {}
#endif
#ifndef CONFIG_ARRAY
#define CONFIG_ARRAY(...) {}
#endif
#define _A(...) __VA_ARGS__

// General items
CONFIG_ITEM(version, float, "%0.3f", 2.001)
CONFIG_ITEM(debug, bool, "%d", false)

// CYRF6936 items
CONFIG_ARRAY(spektrum_bind_id, uint8_t, 4, "%02X", _A({0, 0, 0, 0}))

// CC2500 items
CONFIG_ITEM(cc_tuned, bool, "%d", false)
CONFIG_ITEM(cc_fsctrl0, int8_t, "%d", 0)
CONFIG_ARRAY(frsky_bind_id, uint8_t, 2, "%02X", _A({0, 0}))
CONFIG_ARRAY(frsky_hop_table, uint8_t, 50, "%03d ", _A({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}))
CONFIG_ITEM(frsky_bound, bool, "%d", false)