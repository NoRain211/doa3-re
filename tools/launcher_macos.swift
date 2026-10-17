// SPDX-License-Identifier: GPL-3.0-or-later
// macOS launcher: pick resolution, anti-aliasing, volume, widescreen and the
// DOAXBV swimsuit switch (its bounce, and the DOAXBV disc it builds swimsuits from),
// remember them in private/launcher.json, then exit 0 so launcher.sh starts
// the game. Mirrors tools/launcher.ps1; SMAA needs the D3D11 presenter.
// Exits 1 if the window is closed.
import AppKit

let root = URL(fileURLWithPath: CommandLine.arguments.count > 1
    ? CommandLine.arguments[1] : FileManager.default.currentDirectoryPath)
let settingsURL = root.appendingPathComponent("private/launcher.json")

let heights = [480, 720, 1080, 1440, 2160]
let samples = [1, 2, 4, 8]
let volumes = [100, 75, 50, 25, 0]
// Chest physics strength on DOAXBV swimsuits, in percent of the default.
let bounces = [0, 25, 50, 75, 100, 125, 150, 175, 200, 250, 300, 350, 400]
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
func boolSetting(_ key: String) -> Bool {
    (settings[key] as? NSNumber)?.boolValue ?? false
}
let screenHeight: Int = {
    guard let screen = NSScreen.screens.first ?? NSScreen.main else { return 1080 }
    return Int(screen.frame.height * screen.backingScaleFactor)
}()

final class Launcher: NSObject, NSApplicationDelegate, NSWindowDelegate {
    let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 260, height: 346),
                          styleMask: [.titled, .closable], backing: .buffered, defer: false)
    let resolution = NSPopUpButton()
    let msaa = NSPopUpButton()
    let volume = NSPopUpButton()
    let swimsuits = NSButton(checkboxWithTitle: "DOAXBV swimsuits", target: nil, action: nil)
    let startFavorite = NSButton(checkboxWithTitle: "Start in a favorite", target: nil, action: nil)
    let bounce = NSPopUpButton()
    let disc = NSTextField(labelWithString: "")
    let smaa = NSButton(checkboxWithTitle: "SMAA", target: nil, action: nil)
    let widescreen = NSButton(checkboxWithTitle: "Widescreen (16:9)", target: nil, action: nil)
    var launched = false

    func addRow(_ title: String, _ popup: NSPopUpButton, _ items: [String], _ index: Int, _ y: CGFloat) {
        let label = NSTextField(labelWithString: title)
        label.frame = NSRect(x: 16, y: y + 3, width: 84, height: 20)
        popup.frame = NSRect(x: 104, y: y, width: 140, height: 26)
        popup.addItems(withTitles: items)
        popup.selectItem(at: max(0, index))
        window.contentView!.addSubview(label)
        window.contentView!.addSubview(popup)
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        window.title = "Dead or Alive 3"
        window.delegate = self
        swimsuits.frame = NSRect(x: 16, y: 312, width: 228, height: 20)
        startFavorite.frame = NSRect(x: 32, y: 290, width: 212, height: 20)
        startFavorite.state = boolSetting("swimsuit_start_favorite") ? .on : .off
        startFavorite.toolTip = "Each fighter starts in her first favorite swimsuit (Back + Y marks a favorite)."
        swimsuits.state = (settings["swimsuits"] as? NSNumber)?.boolValue ?? true ? .on : .off
        swimsuits.toolTip = "Every DOA3 woman can wear her DOAXBV swimsuits, built from your DOAXBV disc. In a fight: hold Back, then RB / LB (yours) or RT / LT (opponent)."
        let choose = NSButton(title: "Choose DOAXBV disc…", target: self, action: #selector(chooseDisc))
        choose.frame = NSRect(x: 12, y: 230, width: 236, height: 28)
        disc.frame = NSRect(x: 16, y: 210, width: 228, height: 16)
        disc.font = NSFont.systemFont(ofSize: 10)
        disc.lineBreakMode = .byTruncatingMiddle
        showDisc()
        for view in [swimsuits, startFavorite, choose, disc] { window.contentView!.addSubview(view) }
        addRow("  Bounce", bounce, bounces.map { $0 == 0 ? "Off" : "\($0)%" },
               bounces.firstIndex(of: intSetting("swimsuit_bounce", 100)) ?? 4, 260)
        bounce.toolTip = "Chest physics on DOAXBV swimsuits. 100% is the default; Off keeps them still."
        let savedHeight = intSetting("height", screenHeight)
        let nearest = heights.lastIndex(where: { $0 <= savedHeight }) ?? 0
        addRow("Resolution", resolution,
               heights.map { $0 == 480 ? "480p (windowed)" : "\($0)p" }, nearest, 172)
        let savedMsaa = intSetting("msaa", 1)
        addRow("MSAA", msaa, samples.map { $0 == 1 ? "Off" : "\($0)x" },
               samples.firstIndex(of: savedMsaa) ?? 0, 140)
        smaa.frame = NSRect(x: 104, y: 114, width: 140, height: 20)
        smaa.title = "SMAA (unsupported)"
        smaa.state = .off
        smaa.isEnabled = false
        addRow("Volume", volume, volumes.map { $0 == 0 ? "Mute" : "\($0)%" },
               volumes.firstIndex(of: intSetting("volume", 100)) ?? 0, 80)
        widescreen.frame = NSRect(x: 104, y: 54, width: 150, height: 20)
        widescreen.state = boolSetting("widescreen") ? .on : .off
        widescreen.toolTip = "Wider 3D view. The HUD and menus are stretched."

        let play = NSButton(title: "Play", target: self, action: #selector(play))
        play.frame = NSRect(x: 160, y: 12, width: 88, height: 30)
        play.keyEquivalent = "\r"
        for view in [smaa, widescreen, play] { window.contentView!.addSubview(view) }

        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    func showDisc() {
        disc.stringValue = doaxbvPath.isEmpty ? "No DOAXBV disc chosen" : (doaxbvPath as NSString).lastPathComponent
        disc.toolTip = doaxbvPath
    }

    @objc func chooseDisc() {
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

    @objc func play() {
        let chosen: [String: Any] = [
            "swimsuits": swimsuits.state == .on,
            "swimsuit_start_favorite": startFavorite.state == .on,
            "swimsuit_bounce": bounces[bounce.indexOfSelectedItem],
            "height": heights[resolution.indexOfSelectedItem],
            "msaa": samples[msaa.indexOfSelectedItem],
            "smaa": false,
            "volume": volumes[volume.indexOfSelectedItem],
            "widescreen": widescreen.state == .on,
        ]
        do {
            try FileManager.default.createDirectory(
                at: settingsURL.deletingLastPathComponent(), withIntermediateDirectories: true)
            let data = try JSONSerialization.data(
                withJSONObject: chosen, options: [.prettyPrinted, .sortedKeys])
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
}

let app = NSApplication.shared
app.setActivationPolicy(.regular)
let launcher = Launcher()
app.delegate = launcher
app.run()
exit(launcher.launched ? 0 : 1)
