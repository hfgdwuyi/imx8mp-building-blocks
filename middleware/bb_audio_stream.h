#ifndef BB_AUDIO_STREAM_H
#define BB_AUDIO_STREAM_H
/*
 * bb_audio_stream.h - Real-time audio stream manager
 *
 * Full-duplex audio pipeline on top of bb_hal_audio + bb_pool + bb_thread.
 * Supports PCM pass-through (codec_ops=NULL) or pluggable codec (Opus etc.).
 *
 * Thread architecture:
 *   capture_thread -> SPSC pool -> codec_thread -> MPSC pool -> playback_thread
 */
#include "bb_hal_audio.h"
#include "bb_pool.h"
#include "bb_thread.h"
#include <stdatomic.h>

// ---- Codec abstraction (pluggable, NULL = PCM pass-through) ----
typedef struct bb_audio_codec_ops {
	int  (*init)(void **ctx, uint32_t rate, uint32_t channels);
	void (*destroy)(void *ctx);
	int  (*encode)(void *ctx, const uint8_t *pcm, size_t pcm_len,
	               uint8_t *out, size_t out_cap, size_t *out_len);
	int  (*decode)(void *ctx, const uint8_t *enc, size_t enc_len,
	               uint8_t *pcm, size_t pcm_cap, size_t *pcm_len);
} bb_audio_codec_ops_t;

// ---- Stream configuration ----
typedef struct {
	int              card;          // ALSA card number (2 = NAU8822)
	int              dev;           // PCM device (0 = HiFi)
	uint32_t         rate;          // sample rate (48000)
	uint32_t         channels;      // 1 or 2
	bb_audio_fmt_t   fmt;           // sample format
	int              pool_frames;   // frame pool depth (default 4)
	int              cpu_capture;   // CPU affinity for capture thread
	int              cpu_codec;     // CPU affinity for codec thread
	int              cpu_playback;  // CPU affinity for playback thread
	int              rt_priority;   // SCHED_FIFO priority (default 80)
	bb_audio_codec_ops_t *codec_ops; // NULL = PCM pass-through
} bb_audio_stream_config_t;

#define BB_AUDIO_STREAM_CONFIG_DEFAULT { \
	.card = 2, .dev = 0, \
	.rate = 48000, .channels = 2, .fmt = BB_AUDIO_FMT_S16_LE, \
	.pool_frames = 4, \
	.cpu_capture = 0, .cpu_codec = 1, .cpu_playback = 2, \
	.rt_priority = 80, \
	.codec_ops = NULL, \
}

// ---- Stream instance ----
typedef struct {
	bb_audio_stream_config_t cfg;
	bb_audio_t        cap_dev;       // owned by capture thread
	bb_audio_t        pb_dev;        // owned by playback thread
	bb_frame_pool_t   sp_pool;       // SPSC: capture -> codec
	bb_frame_pool_t   mp_pool;       // MPSC: codec -> playback
	bb_thread_t       cap_thread;
	bb_thread_t       codec_thread;
	bb_thread_t       pb_thread;
	atomic_bool       running;
	atomic_bool       error;
	atomic_uint       cap_frames;    // total frames captured
	atomic_uint       pb_frames;     // total frames played
	atomic_uint       drop_frames;   // dropped due to pool full
	void             *codec_ctx;
} bb_audio_stream_t;

// ---- API ----
// Initialize stream: open devices, create pools.
// Returns 0 on success, negative errno on failure.
int  bb_audio_stream_init(bb_audio_stream_t *s, const bb_audio_stream_config_t *cfg);

// Start all three threads. Must be called after init.
int  bb_audio_stream_start(bb_audio_stream_t *s);

// Stop threads and drain. Can be called multiple times safely.
int  bb_audio_stream_stop(bb_audio_stream_t *s);

// Destroy stream: stop if running, close devices, destroy pools.
void bb_audio_stream_destroy(bb_audio_stream_t *s);

// Read atomic stats counters.
void bb_audio_stream_stats(const bb_audio_stream_t *s,
                           uint32_t *cap, uint32_t *pb, uint32_t *drop);

#endif // BB_AUDIO_STREAM_H
