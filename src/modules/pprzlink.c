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

#include "pprzlink.h"
#include "cdcacm.h"
#include "ring.h"
#include "led.h"

struct pprzlink_t pprzlink;

bool pprzlink_check_free_space(struct pprzlink_t *link, long *fd, uint16_t len);
void pprzlink_put_byte(struct pprzlink_t *link, long fd, uint8_t data);
void pprzlink_put_buffer(struct pprzlink_t *link, long fd, const uint8_t *data, uint16_t len);
void pprzlink_send_message(struct pprzlink_t *link, long fd);
bool pprzlink_char_available(struct pprzlink_t *link);
uint8_t pprzlink_get_byte(struct pprzlink_t *link);

/**
 * Initialize the pprzlink protocol and devices
 */
void pprzlink_init(void) {
	pprz_transport_init(&pprzlink.tp);

  pprzlink.dev.check_free_space = (check_free_space_t) pprzlink_check_free_space;
  pprzlink.dev.put_byte = (put_byte_t) pprzlink_put_byte;
  pprzlink.dev.put_buffer = (put_buffer_t) pprzlink_put_buffer;
  pprzlink.dev.send_message = (send_message_t) pprzlink_send_message;
  pprzlink.dev.char_available = (char_available_t) pprzlink_char_available;
  pprzlink.dev.get_byte = (get_byte_t) pprzlink_get_byte;
  pprzlink.dev.periph = &pprzlink;

	pprzlink.r_rx = &cdcacm_data_rx;
	pprzlink.r_tx = &cdcacm_data_tx;
}

/**
 * Main pprzlink loop trying to parse messages from the cdcacm data endpoint
 */
void pprzlink_run(void) {
  // Parse pprzlink messages
  pprz_check_and_parse(&pprzlink.dev, &pprzlink.tp, pprzlink.recv_buf, &pprzlink.msg_received);
  if(pprzlink.msg_received) {
  	LED_TOGGLE(LED_RX);

  	uint8_t msg_id = IdOfPprzMsg(pprzlink.recv_buf);
  	if(pprzlink.msg_cb[msg_id] != NULL)
  		pprzlink.msg_cb[msg_id](pprzlink.recv_buf);

  	pprzlink.msg_received = false;
  }
}

/**
 * Add a message callback
 */
void pprzlink_register_cb(uint8_t msg_id, msg_cb_t cb) {
	pprzlink.msg_cb[msg_id] = cb;
}

bool pprzlink_check_free_space(struct pprzlink_t *link, long *fd __attribute__((unused)), uint16_t len) {
	return RING_FREE_SPACE(link->r_tx) > len;
}

void pprzlink_put_byte(struct pprzlink_t *link, long fd __attribute__((unused)), uint8_t data) {
	ring_write_ch(link->r_tx, data);
}

void pprzlink_put_buffer(struct pprzlink_t *link, long fd __attribute__((unused)), const uint8_t *data, uint16_t len) {
	ring_write(link->r_tx, data, len);
}

void pprzlink_send_message(struct pprzlink_t *link __attribute__((unused)), long fd __attribute__((unused))) {

}

bool pprzlink_char_available(struct pprzlink_t *link) {
	return !RING_EMPTY(link->r_rx);
}

uint8_t pprzlink_get_byte(struct pprzlink_t *link) {
	return ring_read_ch(link->r_rx, NULL);
}