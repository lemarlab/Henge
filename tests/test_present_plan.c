/* How a guest frame lands on a screen: the drawable is the screen's own pixels, the frame is
   aspect-fit inside it, and the filter is nearest only when every screen pixel is a whole
   number of frame pixels (1:1 or integer), linear otherwise. Expected values are worked by hand. */
#include <stdio.h>

#include "pvg_present.h"

static int checks, failures;

static void check(const char *what, long got, long want) {
    checks++;
    if (got != want) {
        printf("  FAIL %s: %ld, expected %ld\n", what, got, want);
        failures++;
    }
}

static void expect(const char *what, HvPresentPlan p, uint32_t dw, uint32_t dh, uint32_t x, uint32_t y, uint32_t w, uint32_t h, int nearest) {
    char buf[96];
    snprintf(buf, sizeof buf, "%s drawable", what); check(buf, (long)p.drawable_w << 16 | p.drawable_h, (long)dw << 16 | dh);
    snprintf(buf, sizeof buf, "%s viewport origin", what); check(buf, (long)p.x << 16 | p.y, (long)x << 16 | y);
    snprintf(buf, sizeof buf, "%s viewport size", what); check(buf, (long)p.w << 16 | p.h, (long)w << 16 | h);
    snprintf(buf, sizeof buf, "%s nearest", what); check(buf, p.nearest, nearest);
}

static void expect_cursor(const char *what, HvCursorPlan c, int x, int y, uint32_t w, uint32_t h, uint32_t scale) {
    char buf[96];
    snprintf(buf, sizeof buf, "%s pointer x", what); check(buf, c.x, x);
    snprintf(buf, sizeof buf, "%s pointer y", what); check(buf, c.y, y);
    snprintf(buf, sizeof buf, "%s pointer size", what); check(buf, (long)c.w << 16 | c.h, (long)w << 16 | h);
    snprintf(buf, sizeof buf, "%s pointer scale", what); check(buf, c.scale, scale);
}

/* The pointer: macOS reports where it is in the pixels of the mode it accepted, which on a 4K
   monitor is 3840 x 2160 while it scans out a 1920 x 1080 frame. A 32 x 32 glyph, hot spot (5, 5). */
static void cursor_plans(void) {
    /* 4K monitor, 1x desktop: reported (680, 20) of 3840 x 2160 is (340, 10) of the 1920 x 1080 frame;
       at the guest's own size (1x) the image starts 5 up and left of it: (335, 5), 32 x 32. */
    expect_cursor("4K monitor, 1080p frame", hv_cursor_plan(1920, 1080, 3840, 2160, 680, 20, 32, 32, 5, 5, 1, 3), 335, 5, 32, 32, 1);
    /* The photo's second case: reported (390, 40) -> (195, 20) on the frame, image at (190, 15). */
    expect_cursor("4K monitor, File menu", hv_cursor_plan(1920, 1080, 3840, 2160, 390, 40, 32, 32, 5, 5, 1, 3), 190, 15, 32, 32, 1);
    /* AirPlay, HiDPI: the frame is the mode, 3390 x 2160, so (1000, 500) stays; 1x: (995, 495). */
    expect_cursor("AirPlay 3390x2160", hv_cursor_plan(3390, 2160, 3390, 2160, 1000, 500, 32, 32, 5, 5, 1, 3), 995, 495, 32, 32, 1);
    /* The phone, as today: frame == mode, three times the glyph, the hot spot kept where it points:
       (680 - 15, 20 - 15) = (665, 5), 96 x 96. */
    expect_cursor("phone 1080p", hv_cursor_plan(1920, 1080, 1920, 1080, 680, 20, 32, 32, 5, 5, 0, 3), 665, 5, 96, 96, 3);
    /* No mode known yet (0 x 0): the frame's own pixels. */
    expect_cursor("phone, no mode", hv_cursor_plan(1920, 1080, 0, 0, 680, 20, 32, 32, 5, 5, 0, 3), 665, 5, 96, 96, 3);
}

int main(void) {
    cursor_plans();
    /* A 1080p frame on a MacBook's 3390 x 2160 AirPlay screen: the width limits, 3390 / 1920 = 1.7656,
       so 1080 * 1.7656 = 1906.9 -> 1907 tall, (2160 - 1907) / 2 = 126 down. Not a whole number: linear. */
    expect("1080p on 3390x2160", hv_present_plan(1920, 1080, 3390, 2160), 3390, 2160, 0, 126, 3390, 1907, 0);
    /* The frame is the screen's own size: every pixel lands on its own, nearest. */
    expect("3390x2160 on 3390x2160", hv_present_plan(3390, 2160, 3390, 2160), 3390, 2160, 0, 0, 3390, 2160, 1);
    /* Half a 4K screen (the budget's choice for 5K, or a 2x mode): each frame pixel is exactly 2 x 2 screen pixels, nearest. */
    expect("1920x1080 on 3840x2160", hv_present_plan(1920, 1080, 3840, 2160), 3840, 2160, 0, 0, 3840, 2160, 1);
    /* The phone itself (iPhone 17 Pro on its side, 2622 x 1206): the height limits, 1206 / 1080 = 1.11667,
       so 1920 * 1.11667 = 2144 wide, (2622 - 2144) / 2 = 239 in. Linear. */
    expect("1080p on 2622x1206", hv_present_plan(1920, 1080, 2622, 1206), 2622, 1206, 239, 0, 2144, 1206, 0);
    /* No screen size known (a layer before layout, the colour probe): the frame's own pixels, as before. */
    expect("1080p on an unknown screen", hv_present_plan(1920, 1080, 0, 0), 1920, 1080, 0, 0, 1920, 1080, 1);
    printf("%s: %d checks, %d failures\n", failures ? "Present and cursor plan: FAILED" : "Present and cursor plan: all checks passed", checks, failures);
    return failures != 0;
}
