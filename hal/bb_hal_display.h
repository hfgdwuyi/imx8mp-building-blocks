#ifndef BB_HAL_DISPLAY_H
#define BB_HAL_DISPLAY_H
#include <stdint.h>

// DRM/KMS display HAL — zero dependencies, raw ioctl
// Supports DSI, LVDS, HDMI connectors on i.MX8MP

typedef enum {
    BB_DISP_DSI  = 0,
    BB_DISP_LVDS = 1,
    BB_DISP_HDMI = 2,
    BB_DISP_ANY  = 3
} bb_disp_type_t;

typedef struct {
    int          fd;            // DRM device fd
    uint32_t     connector_id;
    uint32_t     crtc_id;
    uint32_t     encoder_id;
    uint32_t     plane_id;
    uint32_t     fb_id;
    uint32_t     width;
    uint32_t     height;
    uint32_t     stride;        // bytes per line
    uint32_t     handle;        // GEM handle
    uint64_t     size;          // buffer size
    void        *map;           // mmap'd framebuffer
    int          restored;      // previous state saved for restore
    uint32_t     saved_fb;
    uint32_t     saved_crtc;
    int          is_master;     // we claimed DRM master
} bb_display_t;

// Open DRM device and find connector by type.
// device: "/dev/dri/card1", type: BB_DISP_DSI / BB_DISP_LVDS / BB_DISP_HDMI
// Sets preferred mode automatically from EDID/panel.
int  bb_display_open(bb_display_t *disp, const char *device, bb_disp_type_t type);

// Get mapped framebuffer pointer for direct pixel access.
// Format is XRGB8888 (32-bit, stride bytes per row).
// Returns: fb pointer, width, height, stride.
int  bb_display_get_fb(bb_display_t *disp, void **fb,
                       uint32_t *width, uint32_t *height, uint32_t *stride);

// Page flip: show the current framebuffer on screen.
int  bb_display_flip(bb_display_t *disp);

// Restore original CRTC state and release resources.
void bb_display_close(bb_display_t *disp);

#endif
