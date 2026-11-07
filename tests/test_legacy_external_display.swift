import Foundation
import QuartzCore

/// What the engine was last told to draw into.
nonisolated(unsafe) var engineLayer: UnsafeMutableRawPointer?

@_cdecl("henge_vm_set_display_layer")
func recordDisplayLayer(_ layer: UnsafeMutableRawPointer?) { engineLayer = layer }

/// Every mode the engine was asked for, in order.
nonisolated(unsafe) var modesAsked: [String] = []

@_cdecl("henge_vm_request_display_mode")
func recordDisplayMode(_ w: UInt32, _ h: UInt32, _ mmW: UInt32, _ mmH: UInt32, _ hz: UInt32) { modesAsked.append("\(w)x\(h) \(mmW)x\(mmH)mm \(hz)Hz") }

/// An external display scene as the app has it: records what it was told. Showing the Mac
/// attaches a layer the way the real screen view does; stopping it never detaches, because
/// iOS may never tear the view down once the monitor is gone.
@MainActor
final class FakeScreen: ExternalScreen {
    let surface = UUID()
    let layer = CAMetalLayer()
    var told: [String] = []
    /// The monitor's pixels, as the real scene delegate's view reports them.
    var screen = ScreenPixels(width: 3390, height: 2160, hz: 60)

    init(screen: ScreenPixels? = nil) { if let screen { self.screen = screen } }

    func showMac(_ presentation: MacPresentation) {
        told.append("show")
        presentation.display.connect(surface, layer: layer, role: .external, screen: screen)
    }

    func stopMac() { told.append("stop") }
}

@main
struct LegacyExternalDisplayTests {
    @MainActor static func main() {
        let phoneLayer = CAMetalLayer()
        let router = DisplayRouter()
        func engineShows(_ layer: CAMetalLayer) -> Bool { engineLayer == Unmanaged.passUnretained(layer).toOpaque() }

        // Two external displays (USB-C and AirPlay) plugged in, then the session begins.
        router.bootMode = DisplayMode(width: 1920, height: 1080, hz: 60, widthMM: 0, heightMM: 0, scale: 1)
        router.connect(UUID(), layer: phoneLayer, role: .device)
        let a = FakeScreen(screen: ScreenPixels(width: 1920, height: 1080, hz: 60)), b = FakeScreen() // a 1080p USB-C monitor, a MacBook over AirPlay
        let legacy = LegacyExternalDisplay()
        legacy.connect(a)
        legacy.connect(b)
        legacy.begin(MacPresentation(display: router, aspect: 16.0 / 9.0))
        precondition(a.told == ["show"] && b.told == ["show"])
        precondition(engineShows(b.layer))
        // Each monitor taking over asked for its own mode: the 1080p one as a regular display (443 x 249 mm), then the MacBook's.
        precondition(modesAsked == ["1920x1080 0x0mm 60Hz", "1920x1080 443x249mm 60Hz", "3390x2160 391x249mm 60Hz"], "\(modesAsked)")

        // Unplugging A leaves B alone and still showing the Mac, not the phone.
        legacy.disconnect(a)
        precondition(a.told == ["show", "stop"])
        precondition(b.told == ["show"])
        precondition(engineShows(b.layer))

        // The session ending stops only the display still showing the Mac, and the phone gets it back.
        legacy.end(MacPresentation(display: router, aspect: 16.0 / 9.0))
        precondition(a.told == ["show", "stop"])
        precondition(b.told == ["show", "stop"])
        precondition(engineShows(phoneLayer) && !router.isExternal)
        // Unplugging A changed nothing (B still showed the Mac); the session's end asked for the phone's mode back.
        precondition(modesAsked.count == 4 && modesAsked.last == "1920x1080 0x0mm 60Hz", "\(modesAsked)")
        print("Legacy external display: all checks passed")
    }
}
