/*
 * bb-display-test - Color bar test on i.MX8MP display
 *
 * Draws SMPTE-style color bars + geometric overlay, 5s display, then exit.
 * Tests bb_hal_display DSI/LVDS/HDMI HAL.
 *
 * Usage: bb-display-test [dsi|lvds|hdmi|any]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "bb_hal_display.h"

// XRGB8888 color constants
#define C_WHITE   0xFFFFFFFF
#define C_YELLOW  0xFFFFC000
#define C_CYAN    0xFF00FFFF
#define C_GREEN   0xFF00C000
#define C_MAGENTA 0xFFFF00FF
#define C_RED     0xFFFF0000
#define C_BLUE    0xFF0000FF
#define C_BLACK   0xFF000000
#define C_DKGREY  0xFF333333
#define C_LTGREY  0xFFC0C0C0

static const uint32_t smpte_colors[] = {
    C_LTGREY,  C_YELLOW,  C_CYAN,   C_GREEN,
    C_MAGENTA, C_RED,     C_BLUE,   C_BLACK,
};

static inline void put_pixel(uint32_t *fb, uint32_t stride_w,
                              int x, int y, uint32_t color) {
    if (x >= 0 && y >= 0) fb[y * stride_w + x] = color;
}

static void fill_rect(uint32_t *fb, uint32_t stride_w,
                      int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    for (int row = 0; row < h; row++) {
        uint32_t *line = &fb[(y + row) * stride_w + x];
        for (int col = 0; col < w; col++) line[col] = color;
    }
}

static void draw_rect(uint32_t *fb, uint32_t stride_w,
                      int x, int y, int w, int h, uint32_t color) {
    fill_rect(fb, stride_w, x, y, w, 1, color);        // top
    fill_rect(fb, stride_w, x, y + h - 1, w, 1, color); // bottom
    fill_rect(fb, stride_w, x, y, 1, h, color);        // left
    fill_rect(fb, stride_w, x + w - 1, y, 1, h, color); // right
}

static void draw_line(uint32_t *fb, uint32_t stride_w,
                      int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        put_pixel(fb, stride_w, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void draw_crosshair(uint32_t *fb, uint32_t stride_w,
                           int cx, int cy, int size, uint32_t color) {
    draw_line(fb, stride_w, cx - size, cy, cx + size, cy, color);
    draw_line(fb, stride_w, cx, cy - size, cx, cy + size, color);
    draw_rect(fb, stride_w, cx - size/2, cy - size/2, size, size, color);
}

int main(int argc, char *argv[]) {
    bb_disp_type_t type = BB_DISP_DSI;
    const char *device = "/dev/dri/card1";

    if (argc > 1) {
        if      (!strcmp(argv[1], "dsi"))  type = BB_DISP_DSI;
        else if (!strcmp(argv[1], "lvds")) type = BB_DISP_LVDS;
        else if (!strcmp(argv[1], "hdmi")) type = BB_DISP_HDMI;
        else if (!strcmp(argv[1], "any"))  type = BB_DISP_ANY;
        else { printf("Usage: %s [dsi|lvds|hdmi|any]\n", argv[0]); return 1; }
    }

    bb_display_t disp;
    if (bb_display_open(&disp, device, type) < 0) {
        fprintf(stderr, "Failed to open display\n");
        return 1;
    }

    void *fb_raw;
    uint32_t W, H, stride;
    if (bb_display_get_fb(&disp, &fb_raw, &W, &H, &stride) < 0) {
        fprintf(stderr, "Failed to get FB\n"); bb_display_close(&disp); return 1;
    }

    uint32_t *fb = (uint32_t *)fb_raw;
    uint32_t sw = stride / 4;  // stride in pixels

    printf("Display: %ux%u, stride=%u px\n", W, H, sw);

    // ---- Layer 1: 8 SMPTE color bars (top 75% of screen) ----
    int bar_w = W / 8;
    int bar_h = H * 3 / 4;
    for (int i = 0; i < 8; i++) {
        fill_rect(fb, sw, i * bar_w, 0, bar_w, bar_h, smpte_colors[i]);
        // thin white separator between bars
        if (i < 7) draw_line(fb, sw, (i+1)*bar_w, 0, (i+1)*bar_w, bar_h-1, 0xFF444444);
    }

    // ---- Layer 2: Bottom gradient strips (25% of screen) ----
    int grad_y = bar_h;
    int grad_h = H - bar_h;
    fill_rect(fb, sw, 0, grad_y, W/5, grad_h, 0xFF200080);      // purple-blue
    fill_rect(fb, sw, W/5, grad_y, W/5, grad_h, 0xFF802000);    // blue-purple
    fill_rect(fb, sw, 2*W/5, grad_y, W/5, grad_h, 0xFF200080);  // purple-blue
    fill_rect(fb, sw, 3*W/5, grad_y, W/5, grad_h, 0xFF802000);  // blue-purple
    fill_rect(fb, sw, 4*W/5, grad_y, W/5, grad_h, C_BLACK);     // black

    // ---- Layer 3: Grid overlay (white, semi-transparent via checker) ----
    for (uint32_t gx = 0; gx < W; gx += W/16) {
        for (uint32_t gy = 0; gy < (uint32_t)bar_h; gy += 1) {
            if ((gy / 4) % 2 == 0) {
                put_pixel(fb, sw, gx, gy, (fb[gy * sw + gx] & 0xFFFEFEFE) >> 1);
            }
        }
    }

    // ---- Layer 4: Crosshair at center of each bar ----
    for (int i = 0; i < 8; i++) {
        int cx = i * bar_w + bar_w / 2;
        int cy = bar_h / 2;
        draw_crosshair(fb, sw, cx, cy, 30, C_WHITE);
    }

    // ---- Layer 5: Outer border ----
    draw_rect(fb, sw, 0, 0, W, H, C_WHITE);
    draw_rect(fb, sw, 2, 2, W-4, H-4, C_WHITE);

    // ---- Layer 6: Center circle ----
    {
        int cx = W/2, cy = bar_h/2, r = (bar_h/4 < 100) ? bar_h/4 : 100;
        for (int dy = -r; dy <= r; dy++) {
            int dx = (int)sqrtf(r*r - dy*dy);
            put_pixel(fb, sw, cx + dx, cy + dy, C_WHITE);
            put_pixel(fb, sw, cx - dx, cy + dy, C_WHITE);
        }
    }

    // ---- Flip to show ----
    printf("Flipping to display...\n");
    bb_display_flip(&disp);

    printf("Showing 5 seconds (Ctrl+C to stop)...\n");
    fflush(stdout);
    sleep(5);

    printf("Restoring and exiting.\n");
    bb_display_close(&disp);
    return 0;
}
