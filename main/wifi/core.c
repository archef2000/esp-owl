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

#include "core.h"
#include "esp_event.h"

#include "log.h"
#include "wire.h"
#include "rx.h"
#include "tx.h"
#include "schedule.h"

#include <signal.h>
#include "esp_timer.h"
#include "esp_log.h"

ESP_EVENT_DEFINE_BASE(AWDL_EVENT_BASE);
#define READ_HOST 0x34987650

#define SIGSTATS SIGUSR1

#define ETHER_LENGTH 14
#define ETHER_DST_OFFSET 0
#define ETHER_SRC_OFFSET 6
#define ETHER_ETHERTYPE_OFFSET 12

#define POLL_NEW_UNICAST 0x1
#define POLL_NEW_MULTICAST 0x2

#define TAG "awdl core"

static void timer_rearm(esp_timer_handle_t *timer_handle, uint64_t usec) {
	esp_timer_start_once(*timer_handle, usec);
}

static int poll_host_device(struct daemon_state *state) {
	struct buf *buf = NULL;
	int result = 0;
	while (!state->next && !circular_buf_full(state->tx_queue_multicast)) {
		buf = buf_new_owned(ETHER_MAX_LEN);
		int len = buf_len(buf);
		if (host_recv(&state->io, (uint8_t *) buf_data(buf), &len) < 0) {
			goto wire_error;
		} else {
			bool is_multicast;
			struct ether_addr dst;
			buf_take(buf, buf_len(buf) - len);
			READ_ETHER_ADDR(buf, ETHER_DST_OFFSET, &dst);
			is_multicast = dst.ether_addr_octet[0] & 0x01;
			if (is_multicast) {
				circular_buf_put(state->tx_queue_multicast, buf);
				result |= POLL_NEW_MULTICAST;
			} else { /* unicast */
				state->next = buf;
				result |= POLL_NEW_UNICAST;
			}
		}
	}
	return result;
wire_error:
	if (buf)
		buf_free(buf);
	return result;
}

void host_device_ready(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
	struct timer_arg_t *timer = (struct timer_arg_t *)arg;
	struct daemon_state *state = (struct daemon_state *)timer->data;

	int poll_result = poll_host_device(state); /* fill TX queues */
	if (poll_result & POLL_NEW_MULTICAST)
		awdl_send_multicast(&state->timer_state.tx_mcast_timer);
	if (poll_result & POLL_NEW_UNICAST)
		awdl_send_unicast(&state->timer_state.tx_timer);
}

void timer_task(void *pvParameters) {
    struct timer_arg_t *arg = (struct timer_arg_t *)pvParameters;
	int64_t start_time = esp_timer_get_time();
	arg->cb(arg);
	ESP_LOGD("timer_task","finished: %s time: %lld", arg->name, esp_timer_get_time() - start_time);
}

void timer_task_trigger(struct timer_arg_t *arg) {
	if (arg->cb != NULL)
		xTaskNotifyGive(arg->task);
}

void timer_init_task(struct timer_arg_t *timer, timer_cb_t cb, uint64_t in, bool repeat, const char *name) {
	const esp_timer_create_args_t timer_args = {
		.callback = (esp_timer_cb_t)timer_task_trigger,
		.arg = timer,
	};
	timer->cb = cb;
	timer->name = name;
    xTaskCreate(timer_task, name, 20480, timer, 1, &timer->task);
	esp_timer_create(&timer_args,&timer->handle);
	if (repeat)
		esp_timer_start_periodic(timer->handle, in);
	else
		esp_timer_start_once(timer->handle, in);
}

void timer_init(struct timer_arg_t *timer, timer_cb_t cb, uint64_t in, bool repeat, const char *name) {
	const esp_timer_create_args_t timer_args = {
		.callback = (esp_timer_cb_t)timer_task,
		.arg = timer,
	};
	timer->cb = cb;
	timer->name = name;
	esp_timer_create(&timer_args,&timer->handle);
	if (repeat)
		esp_timer_start_periodic(timer->handle, in);
	else
		esp_timer_start_once(timer->handle, in);
}

int awdl_send_data(const struct buf *buf, const struct io_state *io_state,
                   struct awdl_state *awdl_state, struct ieee80211_state *ieee80211_state) {
	uint8_t awdl_data[65535];
	int awdl_data_len;
	uint16_t ethertype;
	struct ether_addr src, dst;
	uint64_t now;
	uint16_t period, slot, tu;

	READ_BE16(buf, ETHER_ETHERTYPE_OFFSET, &ethertype);
	READ_ETHER_ADDR(buf, ETHER_DST_OFFSET, &dst);
	READ_ETHER_ADDR(buf, ETHER_SRC_OFFSET, &src);

	buf_strip(buf, ETHER_LENGTH);
	awdl_data_len = awdl_init_full_data_frame(awdl_data, &src, &dst,
	                                          buf_data(buf), buf_len(buf),
	                                          awdl_state, ieee80211_state);
	now = clock_time_us();
	period = awdl_sync_current_eaw(now, &awdl_state->sync) / AWDL_CHANSEQ_LENGTH;
	slot = awdl_sync_current_eaw(now, &awdl_state->sync) % AWDL_CHANSEQ_LENGTH;
	tu = awdl_sync_next_aw_tu(now, &awdl_state->sync);
	log_trace("Send data (len %d) to %s (%u.%u.%u)", awdl_data_len,
	          ether_ntoa(&dst), period, slot, tu);
	awdl_state->stats.tx_data++;
	if (wlan_send(io_state, awdl_data, awdl_data_len) < 0)
		return TX_FAIL;
	return TX_OK;

wire_error:
	return TX_FAIL;
}

void awdl_send_action(struct daemon_state *state, enum awdl_action_type type) {
	int64_t start_time = esp_timer_get_time();
	int len;
	uint8_t buf[15535]; 
	len = awdl_init_full_action_frame(buf, &state->awdl_state, &state->ieee80211_state, type);
	if (len < 0){
		ESP_LOGE(TAG, "awdl_send_action awdl_init_full_action_frame error");
		return;
	}
	printf("\n%lld\n", esp_timer_get_time() - start_time);
	wlan_send(&state->io, buf, len);
	state->awdl_state.stats.tx_action++;
}

void awdl_switch_channel(struct timer_arg_t *timer) {
	uint64_t now, next_aw;
	int slot;
	struct awdl_chan chan_new;
	int chan_num_new, chan_num_old;
	struct daemon_state *state = timer->data;
	struct awdl_state *awdl_state = &state->awdl_state;

	chan_num_old = awdl_chan_num(awdl_state->channel.current, awdl_state->channel.enc);

	now = clock_time_us();
	slot = awdl_sync_current_eaw(now, &awdl_state->sync) % AWDL_CHANSEQ_LENGTH;
	chan_new = awdl_state->channel.sequence[slot];
	chan_num_new = awdl_chan_num(awdl_state->channel.sequence[slot], awdl_state->channel.enc);

	if (chan_num_new && (chan_num_new != chan_num_old)) {
		ESP_LOGI(TAG, "switch channel to %d (slot %d)", chan_num_new, slot);
		awdl_state->channel.current = chan_new;
	}

	next_aw = awdl_sync_next_aw_us(now, &awdl_state->sync);

	esp_timer_start_once(timer->handle, next_aw);
}

void awdl_neighbor_add(struct awdl_peer *p, void *_io_stat) {
	printf("awdl_neighbor_add: ");
	printf("p->name: %s; ", p->name);
	printf("country_code: %s\n", p->country_code);
	// TODO: add to ipv6 neigbor table
}

void awdl_neighbor_remove(struct awdl_peer *p, void *_io_state) {
	printf("awdl_neighbor_remove: ");
	printf("p->name: %s; ", p->name);
	printf("country_code: %s\n", p->country_code);
	// TODO: remove to ipv6 neigbor table
}

void awdl_clean_peers(struct timer_arg_t *arg) {
	ESP_LOGD("awdl core", "awdl_clean_peers");
    //int64_t start_time = esp_timer_get_time();

	uint64_t cutoff_time;
	struct daemon_state *state = arg->data;
	cutoff_time = clock_time_us() - state->awdl_state.peers.timeout;
	awdl_peers_remove(state->awdl_state.peers.peers, cutoff_time,
	                  state->awdl_state.peer_remove_cb, state->awdl_state.peer_remove_cb_data);

	/* TODO for now run election immediately after clean up; might consider seperate timer for this */
	awdl_election_run(&state->awdl_state.election, &state->awdl_state.peers);

	esp_timer_start_once(arg->handle, state->awdl_state.peers.clean_interval);
}

void awdl_send_psf(struct timer_arg_t *arg) {
	awdl_send_action((struct daemon_state *)arg->data, AWDL_ACTION_PSF);
}

void awdl_send_mif(struct timer_arg_t *timer) {
	struct daemon_state *state = timer->data;
	struct awdl_state *awdl_state = &state->awdl_state;
	uint64_t now, next_aw, eaw_len;

	now = clock_time_us();
	next_aw = awdl_sync_next_aw_us(now, &awdl_state->sync);
	eaw_len = awdl_state->sync.presence_mode * awdl_state->sync.aw_period;

	/* Schedule MIF in middle of sequence (if non-zero) */
	if (awdl_chan_num(awdl_state->channel.current, awdl_state->channel.enc) > 0) {
		awdl_send_action(state, AWDL_ACTION_MIF);
	}

	/* schedule next in the middle of EAW */
	esp_timer_start_once(timer->handle, next_aw + ieee80211_tu_to_usec(eaw_len / 2));
}

void awdl_send_unicast(struct timer_arg_t *timer) {
	struct daemon_state *state = timer->data;
	struct awdl_state *awdl_state = &state->awdl_state;
	uint64_t now = clock_time_us();
	double in = 0;

	if (state->next) { /* we have something to send */
		struct awdl_peer *peer;
		struct ether_addr dst;
		read_ether_addr(state->next, ETHER_DST_OFFSET, &dst);
		if (compare_ether_addr(&dst, &awdl_state->self_address) == 0) {
			/* send back to self */
			host_send(&state->io, buf_data(state->next), buf_len(state->next));
			buf_free(state->next);
			state->next = NULL;
		} else if (awdl_peer_get(awdl_state->peers.peers, &dst, &peer) < 0) {
			log_debug("Drop frame to non-peer %s", ether_ntoa(&dst));
			buf_free(state->next);
			state->next = NULL;
		} else {
			in = awdl_can_send_unicast_in(awdl_state, peer, now, AWDL_UNICAST_GUARD_TU);
			if (in == 0) { /* send now */
				awdl_send_data(state->next, &state->io, &state->awdl_state, &state->ieee80211_state);
				buf_free(state->next);
				state->next = NULL;
				state->awdl_state.stats.tx_data_unicast++;
			} else { /* try later */
				if (in < 0) /* we are at the end of slot but within guard */
					in = -in + usec_to_sec(ieee80211_tu_to_usec(AWDL_UNICAST_GUARD_TU));
			}
		}
	}

	/* rearm if more unicast frames available */
	if (state->next) {
		ESP_LOGW(TAG, "awdl_send_unicast: retry in %lu TU", ieee80211_usec_to_tu(sec_to_usec(in)));
		timer_rearm(&timer->handle, in);
	} else {
		/* poll for more frames to keep queue full */
		esp_event_post(AWDL_EVENT_BASE, READ_HOST, NULL, 0, 10);
	}
}

void awdl_send_multicast(struct timer_arg_t *timer) {
	struct daemon_state *state = timer->data;
	struct awdl_state *awdl_state = &state->awdl_state;
	uint64_t now = clock_time_us();
	double in = 0;

	if (!circular_buf_empty(state->tx_queue_multicast)) { /* we have something to send */
		in = awdl_can_send_in(awdl_state, now, AWDL_MULTICAST_GUARD_TU);
		if (awdl_is_multicast_eaw(awdl_state, now) && (in == 0)) { /* we can send now */
			void *next;
			circular_buf_get(state->tx_queue_multicast, &next, 0);
			awdl_send_data((struct buf *) next, &state->io, &state->awdl_state, &state->ieee80211_state);
			buf_free(next);
			state->awdl_state.stats.tx_data_multicast++;
		} else { /* try later */
			if (in == 0) /* try again next EAW */
				in = usec_to_sec(ieee80211_tu_to_usec(64));
			else if (in < 0) /* we are at the end of slot but within guard */
				in = -in + usec_to_sec(ieee80211_tu_to_usec(AWDL_MULTICAST_GUARD_TU));
		}
	}

	/* rearm if more multicast frames available */
	if (!circular_buf_empty(state->tx_queue_multicast)) {
		log_trace("awdl_send_multicast: retry in %lu TU", ieee80211_usec_to_tu(sec_to_usec(in)));
		timer_rearm(&timer->handle, in);
	} else {
		/* poll for more frames to keep queue full */
		esp_event_post(AWDL_EVENT_BASE, READ_HOST, NULL, 0, 10);
	}
}

void awdl_schedule(struct daemon_state *state) {
	state->timer_state.chan_timer.data = (void *)state;
	timer_init(&state->timer_state.chan_timer, awdl_switch_channel, 0, false, "chan_timer");
	
	state->timer_state.peer_timer.data = (void *)state;
	timer_init(&state->timer_state.peer_timer, awdl_clean_peers, usec_to_sec(state->awdl_state.peers.clean_interval), false, "peer_timer");
    
	state->timer_state.psf_timer.data = (void *)state;
	timer_init(&state->timer_state.psf_timer, awdl_send_psf, ieee80211_tu_to_usec(state->awdl_state.psf_interval), true, "psf_timer");
    printf("psf_timer interval us: %lu\n", ieee80211_tu_to_usec(state->awdl_state.psf_interval));
	
	state->timer_state.mif_timer.data = (void *)state;
	timer_init(&state->timer_state.mif_timer, awdl_send_mif, 0, false, "mif_timer");
	
	state->timer_state.tx_timer.data = (void *)state;
	timer_init(&state->timer_state.tx_timer, awdl_send_unicast, 0, false, "tx_timer");
    
	state->timer_state.tx_mcast_timer.data = (void *)state;
	timer_init(&state->timer_state.tx_mcast_timer, awdl_send_multicast, 0, false, "tx_mcast_timer");

}