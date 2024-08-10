
#include "channel.h"
#include "ieee80211.h"

void awdl_chanseq_init_static(struct awdl_chan *seq, const struct awdl_chan *chan) {
	for (int i = 0; i < AWDL_CHANSEQ_LENGTH; i++, seq++) {
		*seq = *chan;
	}
}
#include "esp_log.h"
uint8_t awdl_chan_num(struct awdl_chan chan, enum awdl_chan_encoding enc) {
	//ESP_LOGI("awdl_chan_num", "chan=%d, enc=%d", chan.simple.chan_num, enc);
	switch (enc) {
		case AWDL_CHAN_ENC_SIMPLE:
			return chan.simple.chan_num;
		case AWDL_CHAN_ENC_LEGACY:
			return chan.legacy.chan_num;
		case AWDL_CHAN_ENC_OPCLASS:
			return chan.opclass.chan_num;
		default:
			return 0; /* unknown encoding */
	}
}

int awdl_chan_encoding_size(enum awdl_chan_encoding enc) {
	switch (enc) {
		case AWDL_CHAN_ENC_SIMPLE:
			return 1;
		case AWDL_CHAN_ENC_LEGACY:
		case AWDL_CHAN_ENC_OPCLASS:
			return 2;
		default:
			return -1; /* unknown encoding */
	}
}
