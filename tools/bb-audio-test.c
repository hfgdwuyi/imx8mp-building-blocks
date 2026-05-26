/*
 * bb-audio-test — Audio HAL test for NAU8822 on i.MX8MP
 *
 * Tests: capture (read PCM frames), playback (440Hz sine tone).
 * Usage: bb-audio-test [capture|playback|both] [duration_sec]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <poll.h>
#include <errno.h>
#include "bb_hal_audio.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CARD    2  // NAU8822
#define DEVICE  0  // HiFi
#define RATE    48000
#define CHANNELS 2
#define FMT     BB_AUDIO_FMT_S16_LE

// Generate 440Hz sine wave, S16_LE interleaved
static void gen_sine(int16_t *buf, uint32_t frames, uint32_t rate, uint32_t ch,
                     float freq, float amplitude) {
    for (uint32_t i = 0; i < frames; i++) {
        double t = (double)i / rate;
        int16_t s = (int16_t)(amplitude * 32767.0 * sin(2.0 * M_PI * freq * t));
        for (uint32_t c = 0; c < ch; c++)
            buf[i * ch + c] = s;
    }
}

static int test_playback(int duration_sec) {
    bb_audio_t a;
    if (bb_audio_open(&a, CARD, DEVICE, BB_AUDIO_PLAYBACK, RATE, CHANNELS, FMT) < 0) {
        fprintf(stderr, "Playback open failed\n");
        return -1;
    }

    uint32_t total_frames = a.rate * duration_sec;
    uint32_t period = a.period_frames;
    int16_t *buf = malloc(period * a.channels * sizeof(int16_t));
    if (!buf) { bb_audio_close(&a); return -1; }

    printf("[playback] %d sec of 440Hz tone @ %uHz %uch\n",
           duration_sec, a.rate, a.channels);

    uint32_t written = 0;
    while (written < total_frames) {
        uint32_t chunk = total_frames - written;
        if (chunk > period) chunk = period;
        gen_sine(buf, chunk, a.rate, a.channels, 440.0f, 0.5f);

        int n = bb_audio_write(&a, buf, chunk);
        if (n < 0) {
            fprintf(stderr, "write error at frame %u: %d\n", written, n);
            break;
        }
        written += n;
    }

    printf("[playback] wrote %u frames (%.1f sec)\n", written,
           (double)written / a.rate);
    free(buf);
    bb_audio_close(&a);
    return 0;
}

static int test_capture(int duration_sec) {
    bb_audio_t a;
    if (bb_audio_open(&a, CARD, DEVICE, BB_AUDIO_CAPTURE, RATE, CHANNELS, FMT) < 0) {
        fprintf(stderr, "Capture open failed\n");
        return -1;
    }

    uint32_t total_frames = a.rate * duration_sec;
    uint32_t period = a.period_frames;
    int16_t *buf = malloc(period * a.channels * sizeof(int16_t));
    if (!buf) { bb_audio_close(&a); return -1; }

    printf("[capture] %d sec @ %uHz %uch (polling with 1s timeout)\n",
           duration_sec, a.rate, a.channels);

    int64_t sum = 0, sum_sq = 0, nsamples = 0;
    int16_t peak = 0;
    uint32_t read_total = 0;

    while (read_total < total_frames) {
        // Poll with timeout — avoid hanging when no mic is connected
        struct pollfd pfd = { .fd = a.fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 1000);  // 1 second timeout
        if (ret == 0) {
            printf("[capture] timeout — no audio data (mic unplugged?)\n");
            break;
        }
        if (ret < 0) {
            fprintf(stderr, "poll error: %s\n", strerror(errno));
            break;
        }

        uint32_t chunk = total_frames - read_total;
        if (chunk > period) chunk = period;

        int n = bb_audio_read(&a, buf, chunk);
        if (n < 0) {
            fprintf(stderr, "read error at frame %u: %d\n", read_total, n);
            break;
        }
        if (n == 0) continue;

        for (uint32_t i = 0; i < (uint32_t)(n * a.channels); i++) {
            int16_t s = buf[i];
            if (s > peak) peak = s;
            if (-s > peak) peak = -s;
            sum += s;
            sum_sq += (int64_t)s * s;
            nsamples++;
        }
        read_total += n;
    }

    if (nsamples > 0) {
        double avg = (double)sum / nsamples;
        double rms = sqrt((double)sum_sq / nsamples);
        printf("[capture] %u frames (%.1f sec) | peak=%d avg=%.1f rms=%.1f\n",
               read_total, (double)read_total / a.rate, peak, avg, rms);
        if (peak < 10)
            printf("[capture] signal near zero — mic may be unplugged (expected)\n");
    } else {
        printf("[capture] no data captured\n");
    }

    free(buf);
    bb_audio_close(&a);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *mode = "both";
    int duration = 2;

    if (argc > 1) mode = argv[1];
    if (argc > 2) duration = atoi(argv[2]);

    printf("=== Audio HAL Test (NAU8822 card=%d dev=%d) ===\n", CARD, DEVICE);

    if (!strcmp(mode, "capture") || !strcmp(mode, "both"))
        test_capture(duration);

    if (!strcmp(mode, "playback") || !strcmp(mode, "both"))
        test_playback(duration);

    printf("=== Done ===\n");
    return 0;
}
