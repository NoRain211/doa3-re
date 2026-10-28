// SPDX-License-Identifier: GPL-3.0-or-later
// macOS launcher: pick resolution, anti-aliasing, volume, widescreen and the
// DOAXBV swimsuit switch (with the DOAXBV disc it builds swimsuits from),
// remember them in private/launcher.json, then exit 0 so launcher.sh starts
// the game. Mirrors tools/launcher.cs without SMAA (D3D11 only) or the build
// page: a page menu on the left over tools/launcher-background.jpg, the chosen
// page in a panel on the right. Keyboard and mouse. Exits 1 if the window is closed.
import AppKit

let root = URL(fileURLWithPath: CommandLine.arguments.count > 1
    ? CommandLine.arguments[1] : FileManager.default.currentDirectoryPath)
let settingsURL = root.appendingPathComponent("private/launcher.json")

let heights = [480, 720, 1080, 1440, 2160]
let samples = [1, 2, 4, 8]
let volumes = [100, 75, 50, 25, 0]
// The swimsuit switch builds DOAXBV swimsuits from the player's own DOAXBV
// disc: an .iso image or the extracted folder, remembered in
// private/doaxbv-path.txt.
let doaxbvPathURL = root.appendingPathComponent("private/doaxbv-path.txt")
var doaxbvPath: String = (try? String(contentsOf: doaxbvPathURL, encoding: .utf8))?
    .components(separatedBy: .newlines).first?.trimmingCharacters(in: .whitespaces) ?? ""

var settings: [String: Any] = [:]
if let data = try? Data(contentsOf: settingsURL),
   let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
    settings = json
}
func intSetting(_ key: String, _ fallback: Int) -> Int {
    (settings[key] as? NSNumber)?.intValue ?? fallback
}
func boolSetting(_ key: String, _ fallback: Bool = false) -> Bool {
    (settings[key] as? NSNumber)?.boolValue ?? fallback
}
let screenHeight: Int = {
    guard let screen = NSScreen.screens.first ?? NSScreen.main else { return 1080 }
    return Int(screen.frame.height * screen.backingScaleFactor)
}()

func rgb(_ r: Int, _ g: Int, _ b: Int, _ a: Int = 255) -> NSColor {
    NSColor(srgbRed: CGFloat(r) / 255, green: CGFloat(g) / 255, blue: CGFloat(b) / 255, alpha: CGFloat(a) / 255)
}
func font(_ name: String, _ size: CGFloat) -> NSFont { NSFont(name: name, size: size) ?? .boldSystemFont(ofSize: size) }

// A setting with its choices, or an action row when names is empty.
final class Row {
    let label: String
    var info: String
    let names: [String]
    var index: Int
    init(_ label: String, _ info: String, _ names: [String] = [], _ index: Int = 0) {
        self.label = label; self.info = info; self.names = names; self.index = max(0, index)
    }
}

struct Page { let name: String; let info: String; let rows: [Row] }

final class LauncherView: NSView {
    // Shoreline layout, as in tools/launcher.cs: menu on the left, the chosen page in a panel on the right.
    static let menuTop: CGFloat = 112, menuStep: CGFloat = 44, itemH: CGFloat = 38, menuW: CGFloat = 300
    static let panelX: CGFloat = 346, panelR: CGFloat = 912, panelTop: CGFloat = 160
    static let rowTop: CGFloat = 260, step: CGFloat = 34, rowH: CGFloat = 30, valueX: CGFloat = 712, valueW: CGFloat = 170
    static let playPage = 4, exitPage = 5

    let resolution = Row("Resolution", "Sets the render resolution. 480p opens in a window",
                         heights.map { "\($0)p" }, heights.lastIndex(where: { $0 <= intSetting("height", screenHeight) }) ?? 0)
    let msaa = Row("MSAA", "Smooths polygon edges with multisampling", samples.map { $0 == 1 ? "Off" : "\($0)x" },
                   samples.firstIndex(of: intSetting("msaa", 1)) ?? 0)
    let screen = Row("Screen", "Wider 3D view. The HUD and menus are stretched", ["4:3", "16:9"], boolSetting("widescreen") ? 1 : 0)
    let volume = Row("Volume", "Sets the master volume", volumes.map { $0 == 0 ? "Mute" : "\($0)%" },
                     volumes.firstIndex(of: intSetting("volume", 100)) ?? 0)
    let freeCamera = Row("Free Camera", "F1, \u{60} or a right-stick click flies the camera; P freezes the game",
                         ["Off", "On"], boolSetting("free_camera") ? 1 : 0)
    let swimsuits = Row("Swimsuits", "In a fight, click L3: LB / LT P1, RB / RT P2, X / B favorite",
                        ["Off", "On"], boolSetting("swimsuits", true) ? 1 : 0)
    let startFavorite = Row("Start In Favorite", "Each fighter starts in her first favorite (L3 + X / B)",
                            ["Off", "On"], boolSetting("swimsuit_start_favorite") ? 1 : 0)
    let disc = Row("DOAXBV Disc", "")
    lazy var pages = [
        Page(name: "VIDEO SETTINGS", info: "Sets resolution, anti-aliasing and screen shape", rows: [resolution, msaa, screen]),
        Page(name: "AUDIO SETTINGS", info: "Sets the master volume", rows: [volume]),
        Page(name: "FREE CAMERA", info: "Fly the camera through fights and cutscenes, freeze them and save screenshots", rows: [freeCamera]),
        Page(name: "SWIMSUITS", info: "DOAXBV swimsuits; needs your own DOAXBV disc (.iso or folder) to build them from",
             rows: [swimsuits, startFavorite, disc]),
        Page(name: "START GAME", info: "Saves these settings and starts the game", rows: []),
        Page(name: "EXIT", info: "Closes the launcher", rows: []),
    ]
    // The menu cursor picks a page; inside a page the row cursor picks a setting.
    var menu = LauncherView.playPage, row = 0, inPage = false
    var installed = "INSTALLED: USA 3.0"
    var onPlay: () -> Void = {}
    let background = NSImage(contentsOf: root.appendingPathComponent("tools/launcher-background.jpg"))

    let itemFont = font("HelveticaNeue-BoldItalic", 15), menuFont = font("HelveticaNeue-BoldItalic", 19)
    let infoFont = font("HelveticaNeue-Italic", 14), smallFont = font("HelveticaNeue-Bold", 12)
    let titleFont = font("HelveticaNeue-BoldItalic", 25), logoFont = font("Arial-Black", 38), bigFont = font("Arial-Black", 66)

    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }

    override init(frame: NSRect) {
        super.init(frame: frame)
        showDisc()
        addTrackingArea(NSTrackingArea(rect: .zero, options: [.mouseMoved, .activeInKeyWindow, .inVisibleRect], owner: self, userInfo: nil))
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) is not used") }

    var rows: [Row] { pages[menu].rows }

    func showDisc() {
        disc.info = doaxbvPath.isEmpty ? "No DOAXBV disc chosen; choose its .iso or folder"
            : "Disc: " + (doaxbvPath as NSString).lastPathComponent
    }

    func chooseDisc() {
        let panel = NSOpenPanel()
        panel.message = "Choose your DOAXBV disc image (.iso) or its extracted folder"
        panel.canChooseFiles = true
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        guard panel.runModal() == .OK, let url = panel.url else { return }
        doaxbvPath = url.path
        try? FileManager.default.createDirectory(at: doaxbvPathURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? (doaxbvPath + "\n").write(to: doaxbvPathURL, atomically: true, encoding: .utf8)
        showDisc()
    }

    var chosen: [String: Any] {
        [
            "swimsuits": swimsuits.index == 1,
            "swimsuit_start_favorite": startFavorite.index == 1,
            "height": heights[resolution.index],
            "msaa": samples[msaa.index],
            "smaa": false,
            "volume": volumes[volume.index],
            "widescreen": screen.index == 1,
            "free_camera": freeCamera.index == 1,
        ]
    }

    func change(_ delta: Int) {
        let r = rows[row]
        if !r.names.isEmpty { r.index = min(max(r.index + delta, 0), r.names.count - 1) }
    }

    func accept() {
        if !inPage {
            if menu == LauncherView.playPage { onPlay(); return }
            if menu == LauncherView.exitPage { NSApp.terminate(nil); return }
            inPage = true
            row = 0
        } else if rows[row].names.isEmpty {
            chooseDisc()
        } else {
            change(1)
        }
    }

    func back() {
        if inPage { inPage = false } else { NSApp.terminate(nil) }
    }

    override func keyDown(with event: NSEvent) {
        switch event.keyCode {
        case 126: if inPage { row = max(row - 1, 0) } else { menu = max(menu - 1, 0) }
        case 125: if inPage { row = min(row + 1, rows.count - 1) } else { menu = min(menu + 1, pages.count - 1) }
        case 123: if inPage { change(-1) }
        case 124: if inPage { change(1) } else if !rows.isEmpty { accept() }
        case 36, 76, 49: accept()
        case 53, 51: back()
        default: super.keyDown(with: event); return
        }
        needsDisplay = true
    }

    func hitMenu(_ p: NSPoint) -> Int {
        let m = Int(((p.y - LauncherView.menuTop) / LauncherView.menuStep).rounded(.down))
        return p.x < LauncherView.menuW && m >= 0 && m < pages.count
            && p.y - LauncherView.menuTop - CGFloat(m) * LauncherView.menuStep < LauncherView.itemH ? m : -1
    }
    func hitRow(_ p: NSPoint) -> Int {
        let i = Int(((p.y - LauncherView.rowTop) / LauncherView.step).rounded(.down))
        return p.x > LauncherView.panelX && p.x < LauncherView.panelR && i >= 0 && i < rows.count
            && p.y - LauncherView.rowTop - CGFloat(i) * LauncherView.step < LauncherView.rowH ? i : -1
    }

    override func mouseMoved(with event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        let m = hitMenu(p), i = hitRow(p)
        if !inPage && m >= 0 && m != menu { menu = m; needsDisplay = true }
        if inPage && i >= 0 && i != row { row = i; needsDisplay = true }
    }

    override func mouseDown(with event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        let m = hitMenu(p), i = hitRow(p)
        defer { needsDisplay = true }
        if m >= 0 { menu = m; inPage = false; accept(); return }
        guard i >= 0 else { return }
        // From the menu, the first click only enters the page and selects the row.
        let entering = !inPage
        inPage = true
        row = i
        if entering { return }
        if rows[i].names.isEmpty { chooseDisc() }
        else if p.x >= LauncherView.valueX - 12 { change(p.x < LauncherView.valueX + LauncherView.valueW / 2 ? -1 : 1) }
    }

    override func rightMouseDown(with event: NSEvent) { back(); needsDisplay = true }

    // Text vertically centred in rect; shadowed text gets the logo's dark drop shadow.
    func text(_ s: String, _ f: NSFont, _ color: NSColor, _ rect: NSRect, _ align: NSTextAlignment = .left,
              shadow: Bool = false, oblique: CGFloat = 0) {
        let style = NSMutableParagraphStyle()
        style.alignment = align
        var attributes: [NSAttributedString.Key: Any] = [.font: f, .foregroundColor: color, .paragraphStyle: style, .obliqueness: oblique]
        if shadow {
            let drop = NSShadow()
            drop.shadowColor = rgb(30, 22, 18, 220)
            drop.shadowOffset = NSSize(width: 2, height: -2)
            attributes[.shadow] = drop
        }
        let string = NSAttributedString(string: s, attributes: attributes)
        let height = min(rect.height, string.boundingRect(with: rect.size, options: [.usesLineFragmentOrigin]).height)
        string.draw(in: NSRect(x: rect.minX, y: rect.minY + (rect.height - height) / 2, width: rect.width, height: height))
    }

    func arrow(_ x: CGFloat, _ cy: CGFloat, _ dir: CGFloat, _ color: NSColor) {
        let path = NSBezierPath()
        path.move(to: NSPoint(x: x + 7 * dir, y: cy))
        path.line(to: NSPoint(x: x, y: cy - 5))
        path.line(to: NSPoint(x: x, y: cy + 5))
        path.close()
        color.setFill()
        path.fill()
    }

    override func draw(_ dirtyRect: NSRect) {
        let w = bounds.width, h = bounds.height
        let sand = rgb(241, 197, 143), dark = rgb(42, 29, 21), light = rgb(216, 207, 198), chrome = rgb(226, 228, 228)
        rgb(74, 60, 51).setFill()
        bounds.fill()
        background?.draw(in: bounds, from: .zero, operation: .copy, fraction: 1, respectFlipped: true, hints: nil)
        let shadeStops: [CGFloat] = [0, 0.28, 0.52, 0.66, 1]
        NSGradient(colors: [rgb(22, 16, 14, 217), rgb(22, 16, 14, 153), rgb(22, 16, 14, 25), rgb(22, 16, 14, 0), rgb(22, 16, 14, 0)],
                   atLocations: shadeStops, colorSpace: .sRGB)?.draw(in: bounds, angle: 0)

        // Logo, top right.
        text("3", bigFont, chrome, NSRect(x: 0, y: 14, width: w - 34, height: 90), .right, shadow: true, oblique: 0.2)
        text("DEAD OR ALIVE", logoFont, chrome, NSRect(x: 0, y: 38, width: w - 112, height: 56), .right, shadow: true, oblique: 0.2)

        // Menu down the left; the current page sits on a slanted sunset bar.
        for (m, page) in pages.enumerated() {
            let y = LauncherView.menuTop + CGFloat(m) * LauncherView.menuStep
            if m == menu {
                let bar = NSBezierPath()
                bar.move(to: NSPoint(x: 0, y: y))
                bar.line(to: NSPoint(x: LauncherView.menuW, y: y))
                bar.line(to: NSPoint(x: LauncherView.menuW - 18, y: y + LauncherView.itemH))
                bar.line(to: NSPoint(x: 0, y: y + LauncherView.itemH))
                bar.close()
                let barStops: [CGFloat] = [0, 0.7, 1]
                NSGradient(colors: [rgb(246, 223, 189), rgb(220, 174, 124), rgb(185, 134, 87)], atLocations: barStops,
                           colorSpace: .sRGB)?.draw(in: bar, angle: 0)
            }
            text(page.name, menuFont, m == menu ? dark : light,
                 NSRect(x: 40, y: y, width: LauncherView.menuW - 40, height: LauncherView.itemH))
        }
        text(installed, smallFont, rgb(188, 174, 159), NSRect(x: 40, y: h - 46, width: 300, height: 24))

        // The chosen page in a panel over the beach. From the menu its rows show as a preview.
        let page = pages[menu]
        let top = LauncherView.panelTop, left = LauncherView.panelX, panelW = LauncherView.panelR - LauncherView.panelX
        let bottom = LauncherView.rowTop + CGFloat(max(page.rows.count, 1)) * LauncherView.step + 12
        rgb(25, 18, 15, 165).setFill()
        NSRect(x: left, y: top, width: panelW, height: bottom - top).fill(using: .sourceOver)
        rgb(232, 191, 138).setFill()
        NSRect(x: left, y: top, width: 4, height: bottom - top).fill()
        text(page.name, titleFont, chrome, NSRect(x: left + 22, y: top + 12, width: 500, height: 34), shadow: true)
        text(inPage ? page.rows[row].info : page.info, infoFont, rgb(233, 221, 208),
             NSRect(x: left + 22, y: top + 50, width: panelW - 44, height: 40))
        if menu == LauncherView.playPage {
            text("PRESS START BUTTON", titleFont, chrome,
                 NSRect(x: left, y: LauncherView.rowTop, width: panelW, height: LauncherView.step), .center, shadow: true)
        }
        for (i, r) in page.rows.enumerated() {
            let y = LauncherView.rowTop + CGFloat(i) * LauncherView.step
            let selected = inPage && i == row
            if i > 0 {
                rgb(255, 255, 255, 30).setFill()
                NSRect(x: left + 22, y: y - 2, width: panelW - 44, height: 1).fill(using: .sourceOver)
            }
            if selected {
                rgb(232, 191, 138, 60).setFill()
                NSRect(x: left + 4, y: y, width: panelW - 4, height: LauncherView.rowH).fill(using: .sourceOver)
            }
            text(r.label.uppercased(), itemFont, selected ? .white : light, NSRect(x: left + 22, y: y, width: 300, height: LauncherView.rowH))
            guard !r.names.isEmpty else { continue }
            let cy = y + LauncherView.rowH / 2
            let arrowColor = selected ? sand : sand.withAlphaComponent(0.5)
            arrow(LauncherView.valueX, cy, -1, arrowColor)
            arrow(LauncherView.valueX + LauncherView.valueW, cy, 1, arrowColor)
            text(r.names[r.index].uppercased(), itemFont, .white,
                 NSRect(x: LauncherView.valueX, y: y, width: LauncherView.valueW, height: LauncherView.rowH), .center)
        }

        // Key prompts, bottom right.
        var x = w - 36
        for (key, label) in [("", "CHANGE"), ("Esc", "BACK"), ("Return", "SELECT")] {
            x -= NSAttributedString(string: label, attributes: [.font: smallFont]).size().width
            text(label, smallFont, rgb(243, 238, 232), NSRect(x: x, y: h - 45, width: 120, height: 20))
            let pillW: CGFloat = key.isEmpty ? 38 : NSAttributedString(string: key, attributes: [.font: smallFont]).size().width + 14
            x -= pillW + 6
            rgb(242, 227, 207).setFill()
            NSBezierPath(roundedRect: NSRect(x: x, y: h - 45, width: pillW, height: 20), xRadius: 10, yRadius: 10).fill()
            if key.isEmpty { arrow(x + 16, h - 35, -1, dark); arrow(x + 22, h - 35, 1, dark) }
            else { text(key, smallFont, dark, NSRect(x: x, y: h - 45, width: pillW, height: 20), .center) }
            x -= 22
        }
    }
}

final class Launcher: NSObject, NSApplicationDelegate, NSWindowDelegate {
    let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 960, height: 540),
                          styleMask: [.titled, .closable], backing: .buffered, defer: false)
    let view = LauncherView(frame: NSRect(x: 0, y: 0, width: 960, height: 540))
    var launched = false

    func applicationDidFinishLaunching(_ notification: Notification) {
        // The installed version, from the build's record; older builds are USA 3.0.
        let build = (try? Data(contentsOf: root.appendingPathComponent("private/build.json")))
            .flatMap { try? JSONSerialization.jsonObject(with: $0) as? [String: Any] }
        view.installed = "INSTALLED: " + ((build?["version"] as? String) ?? "USA 3.0").uppercased()
        view.onPlay = { [unowned self] in self.play() }
        window.title = "Dead or Alive 3"
        window.delegate = self
        window.contentView = view
        window.acceptsMouseMovedEvents = true
        window.makeFirstResponder(view)
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func play() {
        do {
            try FileManager.default.createDirectory(
                at: settingsURL.deletingLastPathComponent(), withIntermediateDirectories: true)
            let data = try JSONSerialization.data(
                withJSONObject: view.chosen, options: [.prettyPrinted, .sortedKeys])
            try data.write(to: settingsURL)
        } catch {
            NSAlert(error: error).runModal()
            return
        }
        launched = true
        NSApp.terminate(nil)
    }

    func windowWillClose(_ notification: Notification) {
        if !launched { exit(1) }
    }

    // AppKit exits 0 after terminate, so a Dock or Cmd-Q quit would start the game.
    func applicationWillTerminate(_ notification: Notification) {
        exit(launched ? 0 : 1)
    }
}

let app = NSApplication.shared
app.setActivationPolicy(.regular)
let launcher = Launcher()
app.delegate = launcher
app.run()
exit(launcher.launched ? 0 : 1)
