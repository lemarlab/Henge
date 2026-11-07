import Foundation

/// The mode an external monitor gets, and the physical size that sets its density. Expected values
/// are worked by hand: mm = px / ppi * 25.4, at 220 ppi for a 2x (HiDPI) mode and 110 ppi for 1x
/// (Apple's Retina panels are 218-254 ppi; macOS treats about 192 ppi and up as HiDPI).
@main
struct DisplayModePlanTests {
    static var failures = 0
    static func check<T: Equatable>(_ what: String, _ got: T?, _ want: T?) {
        if got != want {
            print("  FAIL \(what): \(got.map { "\($0)" } ?? "nil"), expected \(want.map { "\($0)" } ?? "nil")")
            failures += 1
        }
    }

    static func main() {
        let plan = DisplayModePlan()
        // A MacBook over AirPlay: 3390 x 2160 fits the budget, so the guest renders every pixel, as
        // HiDPI (looks like 1695 x 1080): 3390 / 220 * 25.4 = 391.4 mm, 2160 / 220 * 25.4 = 249.4 mm.
        check("3390x2160 native, HiDPI", plan.mode(forScreenPixels: 3390, 2160, hz: 60),
              DisplayMode(width: 3390, height: 2160, hz: 60, widthMM: 391, heightMM: 249, scale: 2))
        // A 1080p monitor is a regular display: 1920 / 110 * 25.4 = 443.3 mm, 1080 / 110 * 25.4 = 249.4 mm.
        check("1920x1080 native, 1x", plan.mode(forScreenPixels: 1920, 1080, hz: 60),
              DisplayMode(width: 1920, height: 1080, hz: 60, widthMM: 443, heightMM: 249, scale: 1))
        // A 5K monitor (14.7 MP) is over the 4K budget: half of it, shown doubled (integer scale stays
        // crisp), as a regular display: 2560 / 110 * 25.4 = 591.1 mm, 1440 / 110 * 25.4 = 332.5 mm.
        check("5120x2880 halved to 2560x1440, 1x", plan.mode(forScreenPixels: 5120, 2880, hz: 60),
              DisplayMode(width: 2560, height: 1440, hz: 60, widthMM: 591, heightMM: 333, scale: 1))
        // 8K halves to 4K, which fits and is HiDPI: 3840 / 220 * 25.4 = 443.3 mm, 2160 / 220 * 25.4 = 249.4 mm.
        check("7680x4320 halved to 3840x2160, HiDPI", plan.mode(forScreenPixels: 7680, 4320, hz: 60),
              DisplayMode(width: 3840, height: 2160, hz: 60, widthMM: 443, heightMM: 249, scale: 2))
        // A 120 Hz monitor is driven at 60: the guest draws on demand and the phone's GPU and battery pay for every frame.
        check("120 Hz capped to 60", plan.mode(forScreenPixels: 1920, 1080, hz: 120)?.hz, 60)
        // Odd sizes lose a pixel: surfaces are even. Tiny or absurd screens get no mode (the Mac keeps its own).
        check("odd 3391x2161 rounds to 3390x2160", plan.mode(forScreenPixels: 3391, 2161, hz: 60).map { ($0.width, $0.height) == (3390, 2160) }, true)
        check("320x240 is no screen for a Mac", plan.mode(forScreenPixels: 320, 240, hz: 60), nil)
        check("0x0 is no screen", plan.mode(forScreenPixels: 0, 0, hz: 0), nil)
        print(failures == 0 ? "Display mode plan: all checks passed" : "Display mode plan: \(failures) check(s) failed")
        exit(failures == 0 ? 0 : 1)
    }
}
