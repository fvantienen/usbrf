/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2013 Freek van Tienen <freek.v.tienen@gmail.com>
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

#ifndef MODULES_CONFIG_H_
#define MODULES_CONFIG_H_

#include <libopencm3/cm3/common.h>

#ifndef DEBUG
#define DEBUG(a ,...) if(false){};
#endif

/**
 * The config structure based on the list
 */
#define CONFIG_ITEM(_name, _type, _parser, _default) _type _name;
#define CONFIG_ARRAY(_name, _type, _cnt, _parser, _default) _type _name[_cnt];
struct config_t {
	#include "modules/config_list.h"
};
#undef CONFIG_ITEM
#undef CONFIG_ARRAY
extern struct config_t config;

// The configure link structure between name and value
struct config_link_t {
	char *name;
	uint16_t cnt;
	uint8_t bytes_cnt;
	char *parser;
	void *value;
};

/**
 * External functions
 */
void config_init(void);
void config_store(void);
bool config_load(struct config_t *cfg);

#endif /* MODULES_CONFIG_H_ */
