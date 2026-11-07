import Foundation

/// What macOS is told about a screen: a mode and the physical size that sets its pixel density,
/// which is how it decides between 1x and HiDPI (2x) rendering.
struct DisplayMode: Equatable {
    var width: Int
    var height: Int
    var hz: Int
    var widthMM: Int
    var heightMM: Int
    /// The scale the density asks macOS for: 2 means it shows the mode as HiDPI (looks like half the pixels).
    var scale: Int
}

/// The mode an external monitor gets, from its pixel size and refresh rate, within a budget.
struct DisplayModePlan {
    /// The most pixels the guest renders. A 4K RGBA16Float surface is about 66 MB, and macOS keeps
    /// two or three of them, so this is where a phone running a many-GB Mac stops.
    var maxPixels = 3840 * 2160
    /// The density macOS reads as a regular display, and the one it reads as Retina: Apple's own
    /// panels are 218-254 ppi, and about 192 ppi and up gets HiDPI.
    static let ppi1x = 110.0, ppi2x = 220.0

    func mode(forScreenPixels width: Int, _ height: Int, hz: Int) -> DisplayMode? {
        var w = width & ~1, h = height & ~1
        guard w >= 640, h >= 480, w <= 16384, h <= 16384 else { return nil }
        while w * h > maxPixels { w = (w / 2) & ~1; h = (h / 2) & ~1 }
        guard w >= 640, h >= 480 else { return nil }
        let scale = h >= 2160 ? 2 : 1
        let ppi = scale == 2 ? Self.ppi2x : Self.ppi1x
        return DisplayMode(width: w, height: h, hz: min(max(hz, 30), 60),
                           widthMM: Int((Double(w) / ppi * 25.4).rounded()),
                           heightMM: Int((Double(h) / ppi * 25.4).rounded()), scale: scale)
    }
}
