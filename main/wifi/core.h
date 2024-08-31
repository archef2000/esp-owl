/*
 * OWL: an open Apple Wireless Direct Link (AWDL) implementation
 * Copyright (C) 2018  The Open Wireless Link Project (https://owlink.org)
 * Copyright (C) 2018  Milan Stute
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef OWL_CORE_H
#define OWL_CORE_H

#include <stdbool.h>

#include "state.h"
#include "circular_buffer.h"
#include "io.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "lwip/inet.h"
#include <stdbool.h>

#define ETHER_LENGTH 6
#define ETHER_DST_OFFSET 0

struct in6_addr ether_addr_to_in6_addr(struct ether_addr *addr);

struct ether_addr in6_addr_to_ether_addr(struct in6_addr *addr);

void in6_addr_to_string(char *buf, struct in6_addr addr);

void ether_addr_to_string(char *buf, struct ether_addr addr);

struct timer_arg_t;

/**
 * @brief Timer callback function type
 * @param arg pointer to timer_arg_t
 */
typedef void (*timer_cb_t)(struct timer_arg_t* arg);

struct timer_arg_t {
	esp_timer_handle_t handle;
	TaskHandle_t task;
	timer_cb_t cb;
	const char *name;
	void *data;
};

struct timer_state {
	struct ev_loop *loop;
	struct timer_arg_t mif_timer, psf_timer, tx_timer, tx_mcast_timer, chan_timer, peer_timer;
};

struct daemon_state {
	struct io_state io;
	struct awdl_state awdl_state;
	struct ieee80211_state ieee80211_state;
	struct timer_state timer_state;
	struct buf *next;
	cbuf_handle_t tx_queue_multicast;
	const char *dump;
};

void timer_init(struct timer_arg_t *timer, timer_cb_t cb, uint64_t in, bool repeat, const char *name);

int awdl_init(struct daemon_state *state, const char *wlan, const char *host, struct awdl_chan chan, const char *dump);

void awdl_free(struct daemon_state *state);

void awdl_schedule(struct daemon_state *state);
/*

void wlan_device_ready(struct ev_loop *loop, ev_io *handle, int revents);

*/

void awdl_send_action(struct daemon_state *state, enum awdl_action_type type);

void awdl_send_psf(struct timer_arg_t *timer);

void awdl_send_mif(struct timer_arg_t *timer);

void awdl_send_unicast(struct timer_arg_t *timer);

void awdl_send_multicast(struct timer_arg_t *timer);

void awdl_switch_channel(struct timer_arg_t *timer);
/*

void awdl_clean_peers(struct ev_loop *loop, ev_timer *timer, int revents);

void awdl_print_stats(struct ev_loop *loop, ev_signal *handle, int revents);
*/
int awdl_send_data(const struct buf *buf, const struct io_state *io_state,
                   struct awdl_state *awdl_state, struct ieee80211_state *ieee80211_state);

void awdl_neighbor_add(struct awdl_peer *p, void *_io_stat);

void awdl_neighbor_remove(struct awdl_peer *p, void *_io_state);

#endif /* OWL_CORE_H */
