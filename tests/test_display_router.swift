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

@main
struct DisplayRouterTests {
    @MainActor static func main() {
        let phoneLayer = CAMetalLayer(), monitorLayer = CAMetalLayer()
        let phone = UUID(), monitor = UUID()
        let router = DisplayRouter()
        func engineShows(_ layer: CAMetalLayer) -> Bool { engineLayer == Unmanaged.passUnretained(layer).toOpaque() }

        router.connect(phone, layer: phoneLayer, role: .device)
        router.connect(monitor, layer: monitorLayer, role: .external)
        precondition(engineShows(monitorLayer) && router.isExternal)

        // Two monitors (USB-C and AirPlay): the later one shows the Mac. Disconnecting one
        // monitor's surface leaves the other showing it, not the phone.
        let airPlayLayer = CAMetalLayer(), airPlay = UUID()
        router.connect(airPlay, layer: airPlayLayer, role: .external)
        precondition(engineShows(airPlayLayer))
        router.disconnect(airPlay)
        precondition(engineShows(monitorLayer) && router.isExternal)

        // The last monitor going gives the Mac back to the phone, and a late repeat changes nothing.
        router.disconnect(monitor)
        precondition(engineShows(phoneLayer) && !router.isExternal)
        router.disconnect(monitor)
        precondition(engineShows(phoneLayer) && !router.isExternal)

        // A monitor connecting asks the Mac for its mode (3390 x 2160 HiDPI: 391 x 249 mm); the last one
        // going asks for the phone's boot mode back (its own density rule: 0 mm). Nothing else asks.
        let modeRouter = DisplayRouter()
        modeRouter.bootMode = DisplayMode(width: 1920, height: 1080, hz: 60, widthMM: 0, heightMM: 0, scale: 1)
        precondition(modesAsked == ["1920x1080 0x0mm 60Hz"], "the boot mode is the boot configuration: \(modesAsked)")
        modeRouter.connect(phone, layer: phoneLayer, role: .device)
        precondition(modesAsked.count == 1, "the phone's own surface asks for nothing new: \(modesAsked)")
        modeRouter.connect(monitor, layer: monitorLayer, role: .external, screen: ScreenPixels(width: 3390, height: 2160, hz: 60))
        precondition(modesAsked == ["1920x1080 0x0mm 60Hz", "3390x2160 391x249mm 60Hz"], "\(modesAsked)")
        modeRouter.refresh(monitor) // a layout pass asks for nothing new
        precondition(modesAsked.count == 2)
        modeRouter.disconnect(monitor)
        precondition(modesAsked == ["1920x1080 0x0mm 60Hz", "3390x2160 391x249mm 60Hz", "1920x1080 0x0mm 60Hz"], "\(modesAsked)")
        // A monitor plugged in before the Mac boots: the boot configuration is the monitor's mode, not the phone's.
        let early = DisplayRouter()
        modesAsked = []
        early.connect(phone, layer: phoneLayer, role: .device)
        early.connect(monitor, layer: monitorLayer, role: .external, screen: ScreenPixels(width: 3840, height: 2160, hz: 120))
        early.bootMode = DisplayMode(width: 1920, height: 1080, hz: 60, widthMM: 0, heightMM: 0, scale: 1)
        precondition(modesAsked == ["3840x2160 443x249mm 60Hz"], "\(modesAsked)")
        print("Display router: all checks passed")
    }
}
