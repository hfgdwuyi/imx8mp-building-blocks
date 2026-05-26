#ifndef BB_HAL_AUDIO_H
#define BB_HAL_AUDIO_H
#include <stdint.h>

// ALSA PCM HAL — raw ioctl, zero dependencies
// Tested on i.MX8MP: NAU8822 codec, capture + playback

typedef enum {
    BB_AUDIO_CAPTURE  = 0,
    BB_AUDIO_PLAYBACK = 1,
} bb_audio_dir_t;

typedef enum {
    BB_AUDIO_FMT_S16_LE = 0,
    BB_AUDIO_FMT_S24_LE = 1,
    BB_AUDIO_FMT_S32_LE = 2,
} bb_audio_fmt_t;

typedef struct {
    int              fd;
    bb_audio_dir_t   dir;
    uint32_t         rate;           // sample rate (8000–48000)
    uint32_t         channels;       // 1 or 2
    bb_audio_fmt_t   fmt;            // sample format
    uint32_t         period_frames;  // frames per period
    uint32_t         period_bytes;   // bytes per period
    uint32_t         buffer_frames;  // total buffer frames
} bb_audio_t;

// Open ALSA PCM device by card/device numbers.
//   card: ALSA card number (2 = NAU8822 on OK8MP-C)
//   dev:  PCM device number (0 = HiFi, 1 = ASRC-FE)
//   dir:  BB_AUDIO_CAPTURE or BB_AUDIO_PLAYBACK
//   rate/channels/fmt: desired stream parameters
int  bb_audio_open(bb_audio_t *a, int card, int dev, bb_audio_dir_t dir,
                   uint32_t rate, uint32_t channels, bb_audio_fmt_t fmt);

// Read captured PCM frames (blocking, returns frames read or -errno)
int  bb_audio_read(bb_audio_t *a, void *buf, uint32_t frames);

// Write PCM frames for playback (blocking, returns frames written or -errno)
int  bb_audio_write(bb_audio_t *a, const void *buf, uint32_t frames);

void bb_audio_close(bb_audio_t *a);

#endif
