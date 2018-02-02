/*
 * This file is part of the superbitrf project.
 *
 * Copyright (C) 2013-2015 Freek van Tienen <freek.v.tienen@gmail.com>
 * Copyright (C) 2014 Piotr Esden-Tempski <piotr@esden.net>
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

#include <stdio.h>
#include <string.h>
#include <libopencm3/stm32/flash.h>

#include "config.h"
#include "modules/console.h"
#include "modules/cdcacm.h"
#include "helper/crc.h"

/* console commands */
static void config_cmd_version(char *cmdLine);
static void config_cmd_load(char *cmdLine);
static void config_cmd_save(char *cmdLine);
static void config_cmd_set(char *cmdLine);
static void config_cmd_list(char *cmdLine);
static void config_cmd_reset(char *cmdLine);

/* We are assuming we are using the STM32F103TBU6.
 * Flash: 128 * 1kb pages
 * We want to store the config in the last flash sector.
 */
#define CONFIG_ADDR 0x0801FC00
#define CONFIG_SEED 0x1221					 //*< The config CRC seed */

/* Default configuration settings.
 * The version allways needs to be at the first place
 */
#define CONFIG_ITEM(_name, _type, _parser, _default) ._name = _default,
#define CONFIG_ARRAY(_name, _type, _cnt, _parser, _default) ._name = _default,
struct config_t config = {
	#include "modules/config_list.h"
};
#undef CONFIG_ITEM
#undef CONFIG_ARRAY

#define CONFIG_ITEM(_name, _type, _parser, _default) { .name = #_name, .cnt = 1, .bytes_cnt = sizeof(_type), .parser = _parser, .value = &config._name },
#define CONFIG_ARRAY(_name, _type, _cnt, _parser, _default) { .name = #_name, .cnt = _cnt, .bytes_cnt = sizeof(_type), .parser = _parser, .value = &config._name },
static struct config_link_t config_links[] = {
	#include "modules/config_list.h"
};
#undef CONFIG_ITEM
#undef CONFIG_ARRAY
static uint16_t config_links_cnt = sizeof(config_links) / sizeof(config_links[0]);

/**
 * Initializes the configuration
 * When the version of the 'Default configuration' is different the one from flash gets updated.
 * Else the one from flash gets loaded.
 */
void config_init(void) {
	struct config_t flash_cfg;
	bool valid_cfg = config_load(&flash_cfg);

	/* Check if the version stored in flash is the same as the one we have set
	   by default. Otherwise the config is very likely outdated and we will have to
	   discard it. */
	if (valid_cfg && flash_cfg.version == config.version) {
		memcpy(&config, &flash_cfg, sizeof(struct config_t));
	} else {
		config_store();
	}

	/* Add some commands to the console */
	console_cmd_add("version", "", config_cmd_version);
	console_cmd_add("load", "", config_cmd_load);
	console_cmd_add("save", "", config_cmd_save);
	console_cmd_add("list", "", config_cmd_list);
	console_cmd_add("set", "[name] [value]", config_cmd_set);
	console_cmd_add("reset", "", config_cmd_reset);
}

/**
 * Stores the current config in flash
 */
void config_store(void) {
	uint16_t size = sizeof(struct config_t);
	uint32_t addr = CONFIG_ADDR;
	uint8_t  *byte_config = (uint8_t *)&config;

	/* Unlock flash. */
	flash_unlock();

	/* Erase the config storage page. */
	flash_erase_page(CONFIG_ADDR);

	/* Write config struct to flash. */
	uint16_t write_word = 0xFFFF;
	for (uint16_t i = 0; i < size; i++) {
		write_word = (write_word << 8) | (*(byte_config++));
		if ((i % 2) == 1) {
			flash_program_half_word(addr, write_word);
			addr += 2;
		}
	}

	/* Write last byte if the size is uneven */
	if ((size % 2) == 1) {
		write_word = (write_word << 8) | 0xFF;
		flash_program_half_word(addr, write_word);
		addr += 2;
	}

	/* Write config CRC to flash. */
	uint16_t crc = crc16(CONFIG_SEED, (uint8_t *)&config, size);
	flash_program_half_word(addr, crc);

	/* Lock flash. */
	flash_lock();
}

/**
 * Load the config from flash.
 * This is definitely not the most efficient way of reading out the data. But
 * as we do it only once it probably does not matter much. (esden)
 */
bool config_load(struct config_t *cfg) {
	uint16_t size = sizeof(struct config_t);
	uint16_t *flash_data = (uint16_t *)CONFIG_ADDR;
	uint8_t *byte_config = (uint8_t *)cfg;

	/* Read the config data from the flash */
	for (uint16_t i = 0; i < size; i++) {
		if ((i % 2) == 0) {
			*byte_config = (*flash_data) >> 8;
			byte_config++;
		} else {
			*byte_config = (*flash_data) & 0xFF;
			byte_config++;
			flash_data++;
		}
	}

	/* Go to next word if we ended uneven */
	if ((size % 2) == 1)
		flash_data++;

	/* Check the CRC from flash */
	uint16_t flash_crc = *flash_data;
	uint16_t calc_crc = crc16(CONFIG_SEED, (uint8_t *)cfg, size);
	if(flash_crc != calc_crc)
		return false;

	return true;
}

/**
 * Show the current version and information
 */
static void config_cmd_version(char *cmdLine __attribute((unused))) {
	console_print("\r\nCurrent version: %.3f", config.version);
	console_print("\r\nMade by Freek van Tienen and Piotr Esden-Tempski");
	console_print("\r\nLGPL V3");
}

/**
 * Load the config from flash
 */
static void config_cmd_load(char *cmdLine __attribute((unused))) {
	struct config_t flash_cfg;
	bool valid = config_load(&flash_cfg);

	/* Check if the version is the same, else show error */
	if (valid && flash_cfg.version == flash_cfg.version) {
		memcpy(&config, &flash_cfg, sizeof(struct config_t));
		console_print("\r\nSuccessfully loaded config from the memory!");
	} else {
		console_print("\r\nThere is no loadable config found.");
	}
}

/**
 * Save the config to flash
 */
static void config_cmd_save(char *cmdLine __attribute((unused))) {
	config_store();
	console_print("\r\nSuccessfully saved config to the memory!");
}


/**
 * Set a value
 */
static void config_cmd_set(char *cmdLine) {
	char name[16], format[64];
	int offset;

	if (sscanf(cmdLine, "%16s %n", name, &offset) != 1) {
		console_print("\r\nThis function needs a name!");
	} else {
		for(uint16_t i = 0; i < config_links_cnt; i++) {
			if(!strncasecmp(config_links[i].name, name, strlen(config_links[i].name))) {
				uint8_t value[config_links[i].bytes_cnt * config_links[i].cnt];
				snprintf(format, 64, "%s%%n", config_links[i].parser);

				if(config_links[i].cnt == 1) {
					if(sscanf(cmdLine + offset, format, value, &offset) != 1) {
						console_print("\r\nInvalid value");
						return;
					}

					memcpy(config_links[i].value, value, config_links[i].bytes_cnt);
					console_print("\r\n %s = ", config_links[i].name);
					console_print(config_links[i].parser, *(uint8_t*)config_links[i].value);
				}
				else {
					int offset_cnt = offset;
					for(uint16_t j = 0; j < config_links[i].cnt; j++) {
						if(sscanf(cmdLine + offset_cnt, format, &value[j * config_links[i].bytes_cnt], &offset) != 1) {
							console_print("\r\nInvalid value");
							return;
						}
						offset_cnt += offset;
					}

					memcpy(config_links[i].value, value, config_links[i].bytes_cnt * config_links[i].cnt);
					console_print("\r\n %s = ", config_links[i].name);
					uint8_t *value_byte = (uint8_t *)config_links[i].value;
					for(uint16_t j = 0; j < config_links[i].cnt; j++)
  					console_print(config_links[i].parser, value_byte[j * config_links[i].bytes_cnt]);
				}
				return;
			}
		}

		console_print("\r\nConfig value not found!");
	}
}

/**
 * List all values
 */
static void config_cmd_list(char *cmdLine __attribute((unused))) {
	// Loop trough all config items
	for(uint16_t i = 0; i < config_links_cnt; i++) {
 		console_print("\r\n%s = ", config_links[i].name);
 		if(config_links[i].cnt == 1)
  		console_print(config_links[i].parser, *(uint8_t*)config_links[i].value);
  	else {
  		uint8_t *value_byte = (uint8_t *)config_links[i].value;
  		for(uint16_t j = 0; j < config_links[i].cnt; j++)
  			console_print(config_links[i].parser, value_byte[j * config_links[i].bytes_cnt]);
  	}
		
		cdcacm_run(); // Fix for large transmits
	}

}

/**
 * Reset to initial settings
 */
static void config_cmd_reset(char *cmdLine __attribute((unused))) {
	//memcpy(usbrf_config, init_config, sizeof(init_config));
	console_print("\r\nSuccessfully reset to default settings.");
}
