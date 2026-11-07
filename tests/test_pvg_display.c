/* The virtual display's shared state page (devices/pvg_lite.c): the mode list the guest reads
   at boot, and the hot-plug handshake that offers a monitor's mode while it runs, each offer a
   new sequence number the guest acks. Offsets are the guest driver's (see pvg_lite.c's header);
   expected values are worked by hand. */
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "machine.h"
#include "gicv3.h"
#include "pvg_lite.h"

#define MEM 0x70000000ull
#define RAM (1u << 20)

static HvMachine m;
static int checks, failures;

static void check(const char *what, uint64_t got, uint64_t want) {
    checks++;
    if (got != want) {
        printf("  FAIL %s: %llx, expected %llx\n", what, (unsigned long long)got, (unsigned long long)want);
        failures++;
    }
}

static uint32_t rd32(uint64_t pa) { uint32_t v = 0; hv_phys_read(&m, pa, &v, 4); return v; }
static uint8_t rd8(uint64_t pa) { uint8_t v = 0; hv_phys_read(&m, pa, &v, 1); return v; }
static float rdf(uint64_t pa) { float v = 0; hv_phys_read(&m, pa, &v, 4); return v; }

/* Op 1 names the display page (its page number, 16 KB pages); op 2 acks a sequence number. */
static void name_page(HvPvgLite *p, uint64_t pa) { uint32_t c[5] = {1, 20, 1, 0, (uint32_t)(pa >> 14)}; hv_pvg_lite_command(p, c, 20); }
static void ack(HvPvgLite *p, uint32_t seq) { uint32_t c[5] = {2, 20, 2, 0, seq}; hv_pvg_lite_command(p, c, 20); }

/* The accepted mode as one number, monitor << 32 | height << 16 | width, to compare at once. */
static uint64_t accepted(HvPvgLite *p) {
    uint32_t w = 0, h = 0;
    int monitor = -1;
    hv_pvg_lite_accepted_mode(p, &w, &h, &monitor);
    return (uint64_t)(uint32_t)monitor << 32 | (uint64_t)h << 16 | w;
}
#define MODE(w, h, monitor) ((uint64_t)(monitor) << 32 | (uint64_t)(h) << 16 | (w))

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (hv_machine_init(&m, 4 << 20) != 0) return 1;
    void *ram = mmap(NULL, RAM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    hv_machine_add_ram(&m, MEM, RAM, ram, 0);
    HvGic *gic = hv_gicv3_create(&m, 0x60000000, 0x60010000);
    HvPvgLite *p = hv_pvg_lite_create(&m, gic, 0x61000000, 0x62000000, 32 + 0x11, 32 + 0x10);
    uint64_t page = MEM + 0x40000;

    /* Boot, as today: the page the guest names carries one 1920 x 1080 mode at 60 Hz, 400 mm wide
       (and 400 * 1080 / 1920 = 225 mm tall), sequence 1, ack pending, the mode-change flag (bit 4) up. */
    name_page(p, page);
    check("one mode", rd32(page + 0x208), 1);
    check("mode 1920x1080", rd32(page + 0x210), 1080u << 16 | 1920);
    check("60 Hz", rd32(page + 0x214), 60u << 16);
    check("400 x 225 mm", rd32(page + 0x14), 225u << 16 | 400);
    check("width 400 mm as float", rdf(page + 0x24) == 400.0f, 1);
    check("sequence 1", rd32(page + 0x200), 1);
    check("ack pending", rd8(page + 0xfa9), 1);
    check("state: told", rd32(page + 0xfc0), 1);
    check("mode change flagged", rd32(page + 0x100) & 4, 4);
    check("nothing acked yet", hv_pvg_lite_mode_acked(p), 0);
    check("before any ack, the boot mode is the accepted one", accepted(p), MODE(1920, 1080, 0));
    ack(p, 1);
    check("boot mode accepted", accepted(p), MODE(1920, 1080, 0));
    check("acked sequence recorded", rd32(page + 0xfbc), 1);
    check("ack no longer pending", rd8(page + 0xfa9), 0);
    check("state: acked", rd32(page + 0xfc0), 2);
    check("sequence 1 acked", hv_pvg_lite_mode_acked(p), 1);

    /* A monitor arrives: 3390 x 2160 at 391 x 249 mm (HiDPI density) is offered as sequence 2,
       flagged, ack pending again, and the graphics interrupt wakes the guest to read it. */
    hv_pvg_lite_request_mode(p, 3390, 2160, 391, 249, 60);
    check("offered 3390x2160", rd32(page + 0x210), 2160u << 16 | 3390);
    check("still one mode", rd32(page + 0x208), 1);
    check("391 x 249 mm", rd32(page + 0x14), 249u << 16 | 391);
    check("width 391 mm as float", rdf(page + 0x24) == 391.0f, 1);
    check("height 249 mm as float", rdf(page + 0x28) == 249.0f, 1);
    check("sequence 2", rd32(page + 0x200), 2);
    check("ack pending for the offer", rd8(page + 0xfa9), 1);
    check("state: told again", rd32(page + 0xfc0), 1);
    check("mode change flagged again", rd32(page + 0x100) & 4, 4);
    check("graphics interrupt pended", gic->latch[32 + 0x11], 1);
    check("acked stays at 1 until the guest answers", hv_pvg_lite_mode_acked(p), 1);
    check("offered, not yet accepted: still the boot mode", accepted(p), MODE(1920, 1080, 0));
    ack(p, 2);
    check("sequence 2 acked", hv_pvg_lite_mode_acked(p), 2);
    check("the monitor's mode accepted", accepted(p), MODE(3390, 2160, 1));

    /* Unplugged: the phone's own mode back, with its own density rule (0 mm), as sequence 3. */
    hv_pvg_lite_request_mode(p, 1920, 1080, 0, 0, 60);
    check("back to 1920x1080", rd32(page + 0x210), 1080u << 16 | 1920);
    check("back to 400 x 225 mm", rd32(page + 0x14), 225u << 16 | 400);
    check("sequence 3", rd32(page + 0x200), 3);
    check("until the guest acks, the monitor's mode stays accepted", accepted(p), MODE(3390, 2160, 1));
    ack(p, 3);
    check("the fallback 1920x1080 accepted on unplug", accepted(p), MODE(1920, 1080, 0));

    /* A 4K monitor over USB-C: 3840 x 2160 at 443 x 249 mm, sequence 4. */
    hv_pvg_lite_request_mode(p, 3840, 2160, 443, 249, 60);
    check("4K offered, not yet accepted", accepted(p), MODE(1920, 1080, 0));
    ack(p, 4);
    check("4K accepted", accepted(p), MODE(3840, 2160, 1));

    /* Asked before the guest named a page (a monitor plugged in before boot): the next page carries it. */
    hv_pvg_lite_request_mode(p, 2560, 1440, 591, 333, 60);
    HvPvgLite *fresh = hv_pvg_lite_create(&m, gic, 0x63000000, 0x64000000, 32 + 0x13, 32 + 0x12);
    name_page(fresh, MEM + 0x80000);
    check("a fresh device boots in the asked mode", rd32(MEM + 0x80000 + 0x210), 1440u << 16 | 2560);
    check("with its density", rd32(MEM + 0x80000 + 0x14), 333u << 16 | 591);
    check("as its first sequence", rd32(MEM + 0x80000 + 0x200), 1);
    check("which it has accepted before any ack", accepted(fresh), MODE(2560, 1440, 1));
    check("the first device keeps its own accepted mode", accepted(p), MODE(3840, 2160, 1));

    printf("%s: %d checks, %d failures\n", failures ? "PVG display page: FAILED" : "PVG display page: all checks passed", checks, failures);
    return failures != 0;
}
