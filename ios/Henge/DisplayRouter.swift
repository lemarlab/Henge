import SwiftUI
import QuartzCore

/// A screen as the mode plan sees it: its pixels and refresh rate.
struct ScreenPixels: Equatable {
    var width: Int
    var height: Int
    var hz: Int
}

/// Owned by SessionView, and shared with its display accessory. Only one layer
/// receives frames; the phone's input views stay alive when the monitor takes over.
/// A monitor that takes over also asks the Mac for its own mode; the last one going
/// asks for the Mac's boot mode back.
@MainActor
final class DisplayRouter: ObservableObject {
    @Published private(set) var isExternal = false

    /// The mode the Mac booted in (SessionView sets it): what the engine shows until a monitor asks for more.
    var bootMode: DisplayMode? {
        didSet { askMode() } // the boot configuration itself, or a monitor's mode if one is already showing
    }
    var plan = DisplayModePlan()

    private var targets = DisplayTargetSelection()
    private var layers: [UUID: CAMetalLayer] = [:]
    private var screens: [UUID: ScreenPixels] = [:]
    private var selected: UUID?
    /// The mode the engine was last given, so the same one is never asked for twice.
    private var engineMode: DisplayMode?

    func connect(_ id: UUID, layer: CAMetalLayer, role: DisplayTargetSelection.Role, screen: ScreenPixels? = nil) {
        layers[id] = layer
        if let screen { screens[id] = screen }
        targets.connect(id, role: role)
        updateTarget()
    }

    func disconnect(_ id: UUID) {
        targets.disconnect(id)
        updateTarget()
        layers.removeValue(forKey: id)
        screens.removeValue(forKey: id)
    }

    /// Re-present the latest frame after layout or a return from the background,
    /// even when macOS is idle and isn't submitting another frame.
    func refresh(_ id: UUID? = nil) {
        guard id == nil || id == selected else { return }
        sendTarget()
    }

    private func updateTarget() {
        let next = targets.selected
        if next != selected {
            selected = next
            sendTarget()
            askMode()
        }
        if isExternal != targets.isExternal { isExternal = targets.isExternal }
    }

    private func sendTarget() {
        let layer = selected.flatMap { layers[$0] }
        henge_vm_set_display_layer(layer.map { Unmanaged.passUnretained($0).toOpaque() })
    }

    /// The selected screen's mode, or the boot mode when the phone shows the Mac again.
    private func askMode() {
        var wanted = bootMode
        if targets.isExternal, let selected, let screen = screens[selected],
           let mode = plan.mode(forScreenPixels: screen.width, screen.height, hz: screen.hz) {
            wanted = mode
        }
        guard let wanted, wanted != engineMode else { return }
        engineMode = wanted
        henge_vm_request_display_mode(UInt32(wanted.width), UInt32(wanted.height), UInt32(wanted.widthMM), UInt32(wanted.heightMM), UInt32(wanted.hz))
    }
}
