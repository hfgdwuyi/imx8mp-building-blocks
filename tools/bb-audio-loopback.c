/*
 * bb-audio-loopback — Full-duplex audio loopback test
 *
 * Captures from mic, passes through PCM pipeline, plays to speaker.
 * Usage: bb-audio-loopback [duration_sec] [rate] [channels]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "bb_audio_stream.h"
#include "bb_log.h"

static volatile int g_running = 1;

static void sig_handler(int sig) {
	(void)sig;
	g_running = 0;
}

int main(int argc, char *argv[]) {
	int duration = 5;
	uint32_t rate = 48000;
	uint32_t channels = 2;

	if (argc > 1) duration = atoi(argv[1]);
	if (argc > 2) rate     = (uint32_t)atoi(argv[2]);
	if (argc > 3) channels = (uint32_t)atoi(argv[3]);

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	bb_log_init("bb-audio-loopback", BB_LOG_TO_STDERR, NULL, 0);
	bb_log_info("Audio loopback test: %uHz %uch %d sec",
	            rate, channels, duration);

	bb_audio_stream_config_t cfg = BB_AUDIO_STREAM_CONFIG_DEFAULT;
	cfg.rate     = rate;
	cfg.channels = channels;
	cfg.pool_frames = 4;
	cfg.codec_ops   = NULL;  // PCM pass-through

	bb_audio_stream_t stream;
	if (bb_audio_stream_init(&stream, &cfg) < 0) {
		fprintf(stderr, "Stream init failed\n");
		return 1;
	}

	if (bb_audio_stream_start(&stream) < 0) {
		fprintf(stderr, "Stream start failed\n");
		bb_audio_stream_destroy(&stream);
		return 1;
	}

	printf("\n=== Loopback running (%d sec) ===\n", duration);
	printf("Speak into the mic — you should hear yourself.\n");
	printf("Press Ctrl+C to stop early.\n\n");

	// Print stats every second
	uint32_t last_cap = 0, last_pb = 0;
	for (int t = 0; t < duration && g_running; t++) {
		sleep(1);

		uint32_t cap, pb, drop;
		bb_audio_stream_stats(&stream, &cap, &pb, &drop);

		printf("[t=%d] cap=%u pb=%u drop=%u | cap_delta=%u pb_delta=%u\n",
		       t + 1, cap, pb, drop, cap - last_cap, pb - last_pb);
		last_cap = cap;
		last_pb = pb;

		if (drop > 0)
			printf("  *** WARNING: %u frames dropped ***\n", drop);
	}

	bb_audio_stream_stop(&stream);

	uint32_t cap, pb, drop;
	bb_audio_stream_stats(&stream, &cap, &pb, &drop);
	printf("\n=== Final ===\n");
	printf("Captured : %u frames\n", cap);
	printf("Played   : %u frames\n", pb);
	printf("Dropped  : %u frames\n", drop);
	if (cap > 0)
		printf("Drop rate: %.2f%%\n", 100.0 * drop / cap);

	bb_audio_stream_destroy(&stream);

	if (atomic_load(&stream.error))
		printf("Stream reported errors during run.\n");

	return 0;
}
