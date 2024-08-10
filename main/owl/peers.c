
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "peers.h"
#include "esp_log.h"
#include "hashmap.h"
#include "election.h"
#include "channel.h"

#define PEERS_DEFAULT_TIMEOUT        2000000 /* in ms */
#define PEERS_DEFAULT_CLEAN_INTERVAL 1000000 /* in ms */

void awdl_peer_state_init(struct awdl_peer_state *state) {
	state->peers = awdl_peers_init();
	state->timeout = PEERS_DEFAULT_TIMEOUT;
	state->clean_interval = PEERS_DEFAULT_CLEAN_INTERVAL;
}

awdl_peers_t awdl_peers_init() {
	return (awdl_peers_t) hashmap_new(sizeof(struct ether_addr));
}
struct awdl_peer *awdl_peer_new(const struct ether_addr *addr) {
	struct awdl_peer *peer = (struct awdl_peer *) malloc(sizeof(struct awdl_peer));
	*(struct ether_addr *) &peer->addr = *addr;
	peer->last_update = 0;
	awdl_election_state_init(&peer->election, addr);
	awdl_chanseq_init_static(peer->sequence, &CHAN_NULL);
	peer->sync_offset = 0;
	peer->devclass = 0;
	peer->version = 0;
	peer->supports_v2 = 0;
	peer->sent_mif = 0;
	strcpy(peer->name, "");
	strcpy(peer->country_code, "NA");
	peer->is_valid = 0;
	return peer;
}

static int awdl_peer_is_valid(const struct awdl_peer *peer) {
	return peer->sent_mif && peer->devclass && peer->version;
}

enum peers_status awdl_peer_get(awdl_peers_t peers, const struct ether_addr *_addr, struct awdl_peer **peer) {
	int status;
	map_t map = (map_t) peers;
	mkey_t addr = (mkey_t) _addr;
	status = hashmap_get(map, addr, (any_t *) peer, 0 /* keep */);
	if (status == MAP_MISSING)
		return PEERS_MISSING;
	return PEERS_OK;
}

enum peers_status
awdl_peer_add(awdl_peers_t peers, const struct ether_addr *_addr, uint64_t now, awdl_peer_cb cb, void *arg) {
	int status, result;
	map_t map = (map_t) peers;
	mkey_t addr = (mkey_t) _addr;
	struct awdl_peer *peer;
	status = hashmap_get(map, addr, (any_t *) &peer, 0 /* do not remove */);
	if (status == MAP_MISSING) {
		printf("awdl_peer_add: missing peer %s\n", ether_ntoa(_addr));
		peer = awdl_peer_new(_addr); /* create new entry */
	}

	/* update */
	peer->last_update = now;

	if (status == MAP_OK) {
		result = PEERS_UPDATE;
		goto out;
	}

	status = hashmap_put(map, (mkey_t) &peer->addr, peer);
	if (status != MAP_OK) {
		free(peer);
		return PEERS_INTERNAL;
	}
	result = PEERS_OK;
out:
	if (!peer->is_valid && awdl_peer_is_valid(peer)) {
		/* peer has turned valid */
		peer->is_valid = 1;
		//log_info("add peer %s (%s)", ether_ntoa(&peer->addr), peer->name);
		ESP_LOGI("awdl", "add peer %s (%s)", ether_ntoa(&peer->addr), peer->name);
		if (cb)
			cb(peer, arg);
	}
	return result;
}


void awdl_peers_remove(awdl_peers_t peers, uint64_t before, awdl_peer_cb cb, void *arg) {
	map_t map = (map_t) peers;
	map_it_t it = hashmap_it_new(map);
	struct awdl_peer *peer;
	while (hashmap_it_next(it, NULL, (any_t *) &peer) == MAP_OK) {
		if (peer->last_update < before) {
			if (peer->is_valid) {
				ESP_LOGI("awdl", "remove peer %s (%s)", ether_ntoa(&peer->addr), peer->name);
				if (cb)
					cb(peer, arg);
			}
			hashmap_it_remove(it);
			free(peer);
		}
	}
	hashmap_it_free(it);
}

awdl_peers_it_t awdl_peers_it_new(awdl_peers_t in) {
	return (awdl_peers_it_t) hashmap_it_new((map_t) in);
}

enum peers_status awdl_peers_it_next(awdl_peers_it_t it, struct awdl_peer **peer) {
	int s = hashmap_it_next((map_it_t) it, 0, (any_t *) peer);
	if (s == MAP_OK)
		return PEERS_OK;
	else
		return PEERS_MISSING;
}
