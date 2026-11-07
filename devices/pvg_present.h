#ifndef HENGEVM_PVG_PRESENT_H
#define HENGEVM_PVG_PRESENT_H

#include <stdint.h>

/* Where a guest frame of one size lands on a screen of another: the drawable is the screen's
   own pixels (Apple: render drawables at the exact pixel size of the target display), the frame
   is aspect-fit inside it, and sampling is nearest only when every screen pixel is a whole number
   of frame pixels, 1:1 or an integer, so a crisp frame stays crisp; otherwise linear. */
typedef struct {
    uint32_t drawable_w, drawable_h; /* the layer's drawable */
    uint32_t x, y, w, h;             /* the viewport the frame fills, in drawable pixels */
    int nearest;                     /* 1: sample nearest; 0: linear */
} HvPresentPlan;

static inline HvPresentPlan hv_present_plan(uint32_t frame_w, uint32_t frame_h, uint32_t screen_w, uint32_t screen_h) {
    HvPresentPlan p = {0};
    if (!frame_w || !frame_h) return p;
    if (!screen_w || !screen_h) { /* no screen size known: the frame's own pixels, as before */
        p.drawable_w = p.w = frame_w, p.drawable_h = p.h = frame_h, p.nearest = 1;
        return p;
    }
    p.drawable_w = screen_w, p.drawable_h = screen_h;
    if (screen_w % frame_w == 0 && screen_h % frame_h == 0 && screen_w / frame_w == screen_h / frame_h) {
        p.w = screen_w, p.h = screen_h, p.nearest = 1; /* 1:1 or an integer multiple: every pixel whole */
        return p;
    }
    double sx = (double)screen_w / frame_w, sy = (double)screen_h / frame_h, s = sx < sy ? sx : sy;
    p.w = (uint32_t)(frame_w * s + 0.5), p.h = (uint32_t)(frame_h * s + 0.5);
    if (p.w > screen_w) p.w = screen_w;
    if (p.h > screen_h) p.h = screen_h;
    p.x = (screen_w - p.w) / 2, p.y = (screen_h - p.h) / 2;
    p.nearest = 0;
    return p;
}

/* Where the pointer's image lands on a guest frame, in frame pixels. macOS reports the pointer
   (display page +0xe00) in the pixels of the mode it accepted, which can be bigger than the frame
   it scans out: on a 4K monitor it accepts 3840 x 2160 and keeps a 1920 x 1080 desktop, so the
   position is taken from mode pixels to frame pixels. The glyph comes at the frame's own scale:
   on a monitor it is drawn as it is, on the phone phone_scale times bigger (a 1080p desktop shrunk
   onto a phone would make it a few dots across), the hot spot kept where it points. */
typedef struct {
    int x, y;              /* the image's top-left corner; may be off the frame */
    uint32_t w, h, scale;  /* its size, the glyph times scale */
} HvCursorPlan;

static inline HvCursorPlan hv_cursor_plan(uint32_t frame_w, uint32_t frame_h, uint32_t mode_w, uint32_t mode_h,
                                          uint32_t at_x, uint32_t at_y, uint32_t glyph_w, uint32_t glyph_h,
                                          int hot_x, int hot_y, int external, uint32_t phone_scale) {
    HvCursorPlan c;
    if (mode_w && mode_h) { /* unknown: already frame pixels */
        at_x = (uint32_t)((uint64_t)at_x * frame_w / mode_w);
        at_y = (uint32_t)((uint64_t)at_y * frame_h / mode_h);
    }
    c.scale = external ? 1 : phone_scale;
    c.x = (int)at_x - hot_x * (int)c.scale, c.y = (int)at_y - hot_y * (int)c.scale;
    c.w = glyph_w * c.scale, c.h = glyph_h * c.scale;
    return c;
}

#endif
