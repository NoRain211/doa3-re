// SPDX-License-Identifier: GPL-3.0-or-later
// macOS launcher: pick the installed version to play, resolution, anti-aliasing,
// volume, widescreen and the
// DOAXBV swimsuit switch (with the DOAXBV disc it builds swimsuits from),
// remember them in private/launcher.json, then exit 0 so launcher.sh starts
// the game. Mirrors tools/launcher.cs without SMAA (D3D11 only); its BUILD GAME
// page builds the game from the player's disc: tools/launcher-background.jpg between letterbox bars, the page tabs and
// the chosen page's settings in the bottom one. Keyboard and mouse. Exits 1 if
// the window is closed.
import AppKit

let root = URL(fileURLWithPath: CommandLine.arguments.count > 1
    ? CommandLine.arguments[1] : FileManager.default.currentDirectoryPath)
let settingsURL = root.appendingPathComponent("private/launcher.json")

let heights = [480, 720, 1080, 1440, 2160]
let samples = [1, 2, 4, 8]
let screens = ["4:3", "16:9", "16:10", "Ultrawide"]
let volumes = [100, 75, 50, 25, 0]
// The swimsuit switch builds DOAXBV swimsuits from the player's own DOAXBV
// disc: an .iso image or the extracted folder, remembered in
// private/doaxbv-path.txt.
let doaxbvPathURL = root.appendingPathComponent("private/doaxbv-path.txt")
var doaxbvPath: String = (try? String(contentsOf: doaxbvPathURL, encoding: .utf8))?
    .components(separatedBy: .newlines).first?.trimmingCharacters(in: .whitespaces) ?? ""

// The BUILD GAME page builds from the player's own DOA3 disc: an .iso image or the
// extracted folder, remembered in private/game-source.txt.
let gameSourceURL = root.appendingPathComponent("private/game-source.txt")
var gameSource: String = (try? String(contentsOf: gameSourceURL, encoding: .utf8))?
    .components(separatedBy: .newlines).first?.trimmingCharacters(in: .whitespaces) ?? ""
let buildLogURL = root.appendingPathComponent("private/build.log")

// A version the build installed: each has its own runner folder, play folder
// (with the saves) and XBE name.
struct Install {
    let name: String, playDisc: String, xbe: String, buildDir: String
    var runner: URL { root.appendingPathComponent("\(buildDir)/recomp_program_runner") }
}

// The versions built here, most recently built last, that still have a runner and a
// play folder, from private/build.json's installed list. A record older than that
// list is the one version its top-level keys describe, in build/recomp-program.
func installedVersions() -> [Install] {
    let record = (try? Data(contentsOf: root.appendingPathComponent("private/build.json")))
        .flatMap { try? JSONSerialization.jsonObject(with: $0) as? [String: Any] } ?? [:]
    let entries = (record["installed"] as? [[String: Any]]) ?? [record]
    return entries.compactMap { entry in
        let install = Install(name: entry["version"] as? String ?? "USA 3.0",
                              playDisc: entry["play_disc"] as? String ?? "play-disc",
                              xbe: entry["xbe"] as? String ?? "default.xbe",
                              buildDir: entry["build_dir"] as? String ?? "build/recomp-program")
        return FileManager.default.isExecutableFile(atPath: install.runner.path)
            && FileManager.default.fileExists(
                atPath: root.appendingPathComponent("private/\(install.playDisc)").path) ? install : nil
    }
}

// A disc an earlier build extracted (private/imported-disc*/receipt.json) needs no source.
func isExtracted() -> Bool {
    ["imported-disc", "imported-disc-jp", "imported-disc-eur"].contains {
        FileManager.default.fileExists(atPath: root.appendingPathComponent("private/\($0)/receipt.json").path)
    }
}

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
// "4:3", "16:9", "16:10" or an ultrawide "W:H"; settings saved before 16:10
// hold a widescreen flag.
func screenSetting() -> String {
    settings["screen"] as? String ?? (boolSetting("widescreen") ? "16:9" : "4:3")
}
// The main screen's own shape when it is wider than 16:9, else 21:9 (2560x1080).
func ultrawideShape() -> String {
    guard let screen = NSScreen.screens.first else { return "64:27" }
    let w = Int(screen.frame.width * screen.backingScaleFactor)
    let h = Int(screen.frame.height * screen.backingScaleFactor)
    return w * 9 > h * 16 ? "\(min(w, h * 4)):\(h)" : "64:27" // the runtime takes up to 4:1
}
let screenHeight: Int = {
    guard let screen = NSScreen.screens.first ?? NSScreen.main else { return 1080 }
    return Int(screen.frame.height * screen.backingScaleFactor)
}()

func rgb(_ r: Int, _ g: Int, _ b: Int, _ a: Int = 255) -> NSColor {
    NSColor(srgbRed: CGFloat(r) / 255, green: CGFloat(g) / 255, blue: CGFloat(b) / 255, alpha: CGFloat(a) / 255)
}
func font(_ name: String, _ size: CGFloat) -> NSFont { NSFont(name: name, size: size) ?? .boldSystemFont(ofSize: size) }

// A setting with its choices, or an action row: no names, an action, and the verb its button shows.
final class Row {
    let label: String
    var info: String
    var names: [String]
    var index: Int
    var action: (() -> Void)?
    var verb = "CHOOSE"
    init(_ label: String, _ info: String, _ names: [String] = [], _ index: Int = 0) {
        self.label = label; self.info = info; self.names = names; self.index = max(0, index)
    }
}

struct Page { let name: String; var info: String; let rows: [Row] }

// tools/update.py's exit codes; the last line it prints is the message to show.
enum UpdateResult: Int32 { case current = 0, failed = 1, available = 2, installed = 3 }

// Runs tools/update.py (check or install) off the main thread. done gets its result and its last stdout line,
// or its last stderr line if it crashed.
func runUpdate(_ command: String, done: @escaping (UpdateResult, String) -> Void) {
    let process = Process()
    let output = Pipe(), errors = Pipe()
    // The interpreter run_game.sh and build_game.sh prefer, else python3 on PATH.
    let script = root.appendingPathComponent("tools/update.py").path
    let venv = root.appendingPathComponent("private/venv/bin/python3").path
    if FileManager.default.isExecutableFile(atPath: venv) {
        process.executableURL = URL(fileURLWithPath: venv)
        process.arguments = [script, command]
    } else {
        process.executableURL = URL(fileURLWithPath: "/usr/bin/env")
        process.arguments = ["python3", script, command]
    }
    process.standardOutput = output
    process.standardError = errors
    func lastLine(_ pipe: Pipe) -> String? {
        String(data: pipe.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8)?
            .split(whereSeparator: \.isNewline).last.map { $0.trimmingCharacters(in: .whitespaces) }
    }
    process.terminationHandler = { finished in
        let status = finished.terminationStatus
        let message = lastLine(output) ?? lastLine(errors) ?? "update.py exited with \(status)"
        // After a signal, terminationStatus is the signal number, not an exit code.
        let result = finished.terminationReason == .exit ? UpdateResult(rawValue: status) ?? .failed : .failed
        DispatchQueue.main.async { done(result, message) }
    }
    do { try process.run() } catch { DispatchQueue.main.async { done(.failed, "Cannot run python3: \(error.localizedDescription)") } }
}

final class LauncherView: NSView {
    // Horizon Bar layout, as in tools/launcher.cs: letterbox bars, the settings and page tabs in the bottom one.
    static let topH: CGFloat = 44, side: CGFloat = 30, fadeY: CGFloat = 334
    static let infoY: CGFloat = 380, labelY: CGFloat = 414, valueY: CGFloat = 434, valueH: CGFloat = 30
    static let tabY: CGFloat = 494, tabH: CGFloat = 26
    static let buildPage = 4, playPage = 5, updatePage = 6, exitPage = 7

    let resolution = Row("Resolution", "Sets the render resolution. 480p opens in a window",
                         heights.map { "\($0)p" }, heights.lastIndex(where: { $0 <= intSetting("height", screenHeight) }) ?? 0)
    let msaa = Row("MSAA", "Smooths polygon edges with multisampling", samples.map { $0 == 1 ? "Off" : "\($0)x" },
                   samples.firstIndex(of: intSetting("msaa", 1)) ?? 0)
    let screen = Row("Screen", "Wider 3D view. The HUD and menus stay 4:3", screens,
                     screens.firstIndex(of: screenSetting()) ?? 3)
    let volume = Row("Volume", "Sets the master volume", volumes.map { $0 == 0 ? "Mute" : "\($0)%" },
                     volumes.firstIndex(of: intSetting("volume", 100)) ?? 0)
    let freeCamera = Row("Free Camera", "F1, \u{60} or a right-stick click flies the camera; P freezes the game",
                         ["Off", "On"], boolSetting("free_camera") ? 1 : 0)
    let swimsuits = Row("Swimsuits", "In a fight, click L3: LB / LT P1, RB / RT P2, X / B favorite",
                        ["Off", "On"], boolSetting("swimsuits", true) ? 1 : 0)
    let startFavorite = Row("Start In Favorite", "Each fighter starts in her first favorite (L3 + X / B)",
                            ["Off", "On"], boolSetting("swimsuit_start_favorite") ? 1 : 0)
    let disc = Row("DOAXBV Disc", "")
    let chooseGame = Row("DOA3 Disc", "")
    let movies = Row("Movies", "Off saves 3.4 GB; the game then skips every movie", ["Off", "On"], boolSetting("movies", true) ? 1 : 0)
    let versionRow = Row("Version", "The version START GAME plays; each keeps its own saves")
    let buildRow = Row("Build", "Builds the game from your disc; takes 10-20 minutes")
    let updateRow = Row("Update", "Choose Check to look for a newer release")
    let checkAtStart = Row("Check At Start", "Looks for a newer release when the launcher opens", ["Off", "On"],
                           boolSetting("check_updates", true) ? 1 : 0)
    lazy var pages = [
        Page(name: "VIDEO SETTINGS", info: "Sets resolution, anti-aliasing and screen shape", rows: [resolution, msaa, screen]),
        Page(name: "AUDIO SETTINGS", info: "Sets the master volume", rows: [volume]),
        Page(name: "FREE CAMERA", info: "Fly the camera through fights and cutscenes, freeze them and save screenshots", rows: [freeCamera]),
        Page(name: "SWIMSUITS", info: "DOAXBV swimsuits; needs your own DOAXBV disc (.iso or folder) to build them from",
             rows: [swimsuits, startFavorite, disc]),
        Page(name: "BUILD GAME", info: "Builds the game from your own DOA3 disc; takes 10-20 minutes",
             rows: [chooseGame, movies, buildRow]),
        Page(name: "START GAME", info: "Saves these settings and starts the game", rows: [versionRow]),
        Page(name: "UPDATE", info: "Checks GitHub for a newer release and installs it", rows: [updateRow, checkAtStart]),
        Page(name: "EXIT", info: "Closes the launcher", rows: []),
    ]
    // The tab cursor picks a page; inside a page the row cursor picks a setting column.
    // Not "menu": NSView inherits menu: NSMenu? from NSResponder.
    var tab = installedVersions().isEmpty ? LauncherView.buildPage : LauncherView.playPage, row = 0, inPage = false
    // A held Down that entered a page waits for release before it changes a value.
    var downEntered = false
    // Whether a check found a newer release, and what update.py is doing.
    // Play and quit wait for an install, which replaces the tools.
    enum UpdateWork { case idle, checking, installing }
    var pending = false
    var work = UpdateWork.idle
    var tabs: [NSRect] = []
    // The installed versions, most recently built last, which the START GAME row lists.
    var installs = installedVersions()
    // A line that replaces the information text until the next input: build progress or a refused action.
    var notice: String?
    // The build runs build_game.sh with its output in private/build.log. The first exit request during a
    // build asks for a second.
    var buildProcess: Process?
    var buildTimer: Timer?
    var stopArmed = false
    var onPlay: () -> Void = {}
    let background = NSImage(contentsOf: root.appendingPathComponent("tools/launcher-background.jpg"))

    let itemFont = font("HelveticaNeue-BoldItalic", 15), menuFont = font("HelveticaNeue-BoldItalic", 19)
    let tabFont = font("HelveticaNeue-BoldItalic", 14), infoFont = font("HelveticaNeue-Italic", 14), smallFont = font("HelveticaNeue-Bold", 12)
    let titleFont = font("HelveticaNeue-BoldItalic", 25), logoFont = font("Arial-Black", 38), bigFont = font("Arial-Black", 66)

    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }

    override init(frame: NSRect) {
        super.init(frame: frame)
        showDisc()
        showGame()
        refreshInstalled(remember: settings["version"] as? String)
        registerForDraggedTypes([.fileURL])
        disc.action = { [unowned self] in self.chooseDisc() }
        chooseGame.action = { [unowned self] in self.chooseGameDisc() }
        buildRow.action = { [unowned self] in self.startBuild() }
        buildRow.verb = "BUILD"
        updateRow.action = { [unowned self] in self.update(quiet: false) }
        updateRow.verb = "CHECK"
        addTrackingArea(NSTrackingArea(rect: .zero, options: [.mouseMoved, .activeInKeyWindow, .inVisibleRect], owner: self, userInfo: nil))
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) is not used") }

    var rows: [Row] { pages[tab].rows }
    var columnWidth: CGFloat { (bounds.width - 2 * LauncherView.side) / CGFloat(max(rows.count, 1)) }

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

    func showGame() {
        chooseGame.info = !gameSource.isEmpty ? "Disc: " + (gameSource as NSString).lastPathComponent
            : isExtracted() ? "Disc: the one already extracted" : "No DOA3 disc chosen; choose its .iso or folder, or drop it here"
    }

    // The installed versions, and the Version row's pick: the remembered one while it
    // is installed, else the one built last.
    func refreshInstalled(remember: String? = nil) {
        let keep = remember ?? versionName
        installs = installedVersions()
        versionRow.names = installs.map { $0.name }
        versionRow.index = installs.firstIndex { $0.name == keep } ?? max(installs.count - 1, 0)
        pages[LauncherView.playPage].info = playInfo
    }

    // The version START GAME plays, saved as private/launcher.json's "version".
    var versionName: String {
        versionRow.names.isEmpty ? "" : versionRow.names[min(versionRow.index, versionRow.names.count - 1)]
    }
    var picked: Install? { installs.isEmpty ? nil : installs[min(versionRow.index, installs.count - 1)] }

    // "INSTALLED: USA 3.0, JAPAN 3.1", or NOT BUILT.
    var installedText: String {
        installs.isEmpty ? "NOT BUILT"
            : "INSTALLED: " + installs.map { $0.name.uppercased() }.joined(separator: ", ")
    }
    var playInfo: String {
        guard let version = picked else { return "Saves these settings and starts the game" }
        return "Starts Dead or Alive 3 \(version.name) with these settings"
            + (installs.count > 1 ? "; down picks the version" : "")
    }

    func chooseGameDisc() {
        let panel = NSOpenPanel()
        panel.message = "Choose your Dead or Alive 3 disc image (.iso) or its extracted folder"
        panel.canChooseFiles = true
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        guard panel.runModal() == .OK, let url = panel.url else { return }
        useGame(url.path)
    }

    // Takes an ISO, a disc folder or its default.xbe from the panel or a drop; anything else is refused.
    func useGame(_ chosenPath: String) {
        guard buildProcess == nil else { notice = "Wait for the build to finish"; return }
        var path = chosenPath
        var isDirectory: ObjCBool = false
        guard FileManager.default.fileExists(atPath: path, isDirectory: &isDirectory) else { return }
        let name = (path as NSString).lastPathComponent.lowercased()
        if !isDirectory.boolValue && name == "default.xbe" {
            path = (path as NSString).deletingLastPathComponent
        } else if !isDirectory.boolValue && (path as NSString).pathExtension.lowercased() != "iso" {
            notice = "Choose an Xbox .iso, or a disc folder with default.xbe"
            tab = LauncherView.buildPage
            return
        }
        gameSource = path
        try? FileManager.default.createDirectory(at: gameSourceURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? (gameSource + "\n").write(to: gameSourceURL, atomically: true, encoding: .utf8)
        showGame()
        tab = LauncherView.buildPage
        inPage = true
        row = 2 // Build
        notice = "Disc: " + (gameSource as NSString).lastPathComponent + ". Press Build"
    }

    override func draggingEntered(_ sender: NSDraggingInfo) -> NSDragOperation { .copy }
    override func performDragOperation(_ sender: NSDraggingInfo) -> Bool {
        let urls = sender.draggingPasteboard.readObjects(forClasses: [NSURL.self],
                                                         options: [.urlReadingFileURLsOnly: true]) as? [URL]
        guard let url = urls?.first else { return false }
        useGame(url.path)
        needsDisplay = true
        return true
    }

    func startBuild() {
        guard buildProcess == nil else { return }
        if work == .installing { notice = "Wait for the update to finish"; return }
        if gameSource.isEmpty && !isExtracted() { notice = "Choose your DOA3 ISO or disc folder first"; return }
        let manager = FileManager.default
        try? manager.createDirectory(at: buildLogURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        manager.createFile(atPath: buildLogURL.path, contents: nil)
        guard let log = try? FileHandle(forWritingTo: buildLogURL) else { notice = "Cannot write private/build.log"; return }
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/bin/bash")
        process.arguments = [root.appendingPathComponent("build_game.sh").path]
            + (gameSource.isEmpty ? [] : [gameSource]) + [movies.index == 1 ? "--movies" : "--no-movies"]
        process.currentDirectoryURL = root
        var environment = ProcessInfo.processInfo.environment
        environment["PYTHONUNBUFFERED"] = "1"
        environment["PYTHONIOENCODING"] = "utf-8"
        process.environment = environment
        process.standardOutput = log
        process.standardError = log
        process.standardInput = FileHandle.nullDevice
        process.terminationHandler = { [weak self] finished in
            let status = finished.terminationStatus, exited = finished.terminationReason == .exit
            DispatchQueue.main.async { self?.buildFinished(status: status, exited: exited) }
        }
        do { try process.run() } catch { notice = "Cannot start the build: \(error.localizedDescription)"; return }
        buildProcess = process
        stopArmed = false
        buildRow.verb = "WORKING"
        notice = "Starting the build"
        buildTimer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            guard let self = self, self.buildProcess != nil, !self.stopArmed else { return }
            let line = self.logTail()
            if !line.isEmpty { self.notice = line }
            self.needsDisplay = true
        }
    }

    // The last non-empty line of the build log.
    func logTail() -> String {
        guard let handle = try? FileHandle(forReadingFrom: buildLogURL) else { return "" }
        defer { try? handle.close() }
        let size = (try? handle.seekToEnd()) ?? 0
        try? handle.seek(toOffset: size > 4096 ? size - 4096 : 0)
        let data = (try? handle.readToEnd()) ?? Data()
        return String(decoding: data, as: UTF8.self).split(whereSeparator: \.isNewline).last
            .map { $0.trimmingCharacters(in: .whitespaces) } ?? ""
    }

    func buildFinished(status: Int32, exited: Bool) {
        buildTimer?.invalidate()
        buildTimer = nil
        buildProcess = nil
        buildRow.verb = "BUILD"
        refreshInstalled(remember: versionName)
        showGame()
        if exited && status == 0 {
            notice = "Build complete. Choose START GAME to play"
            tab = LauncherView.playPage
            inPage = false
        } else {
            notice = "Build failed (see private/build.log): " + logTail()
            tab = LauncherView.buildPage
        }
        needsDisplay = true
    }

    // Stops the build and everything it started: the children are found through their parents.
    func stopBuild() {
        guard let process = buildProcess, process.isRunning else { return }
        let tree = Process()
        tree.executableURL = URL(fileURLWithPath: "/bin/sh")
        tree.arguments = ["-c", "kill_tree() { for c in $(pgrep -P $1); do kill_tree $c; done; kill -TERM $1; }; kill_tree $0",
                          String(process.processIdentifier)]
        try? tree.run()
        tree.waitUntilExit()
        process.waitUntilExit()
    }

    // Leaving needs an install to finish, and a running build to be stopped, which takes a second request.
    func exitBlocked() -> Bool {
        if waitForInstall() { return true }
        guard buildProcess != nil else { return false }
        if stopArmed { stopBuild(); return false }
        stopArmed = true
        tab = LauncherView.buildPage
        inPage = false
        notice = "A build is running. Quit again to stop it"
        needsDisplay = true
        return true
    }

    var chosen: [String: Any] {
        [
            "movies": movies.index == 1,
            "swimsuits": swimsuits.index == 1,
            "swimsuit_start_favorite": startFavorite.index == 1,
            "height": heights[resolution.index],
            "msaa": samples[msaa.index],
            "smaa": false,
            "volume": volumes[volume.index],
            "screen": screen.index == 3 ? ultrawideShape() : screens[screen.index],
            "free_camera": freeCamera.index == 1,
            "check_updates": checkAtStart.index == 1,
            // Without an installed version the saved pick stays.
            "version": versionName.isEmpty ? settings["version"] as? String ?? "" : versionName,
        ]
    }

    // Checks for a newer release, or installs the one found. A quiet check says nothing unless it finds one.
    func update(quiet: Bool) {
        guard work == .idle else { return }
        work = pending ? .installing : .checking
        updateRow.verb = "WORKING"
        if !quiet { updateRow.info = work == .installing ? "Downloading the update" : "Checking for updates" }
        runUpdate(work == .installing ? "install" : "check") { [self] result, message in
            work = .idle
            if result != .failed { pending = result == .available }
            if !quiet || result == .available { updateRow.info = message }
            updateRow.verb = pending ? "INSTALL" : "CHECK"
            needsDisplay = true
        }
    }

    // Shows the update page with why play or quit has to wait; false when nothing is installing.
    func waitForInstall() -> Bool {
        guard work == .installing else { return false }
        tab = LauncherView.updatePage
        inPage = true
        row = 0
        updateRow.info = "Wait for the update to finish"
        needsDisplay = true
        return true
    }

    func change(_ delta: Int) {
        let r = rows[row]
        if !r.names.isEmpty { r.index = min(max(r.index + delta, 0), r.names.count - 1) }
        if r === versionRow { pages[LauncherView.playPage].info = playInfo }
    }

    func accept() {
        if inPage && tab == LauncherView.playPage { inPage = false } // Return plays from the Version row too
        if !inPage {
            if (tab == LauncherView.playPage || tab == LauncherView.exitPage) && waitForInstall() { return }
            if tab == LauncherView.playPage {
                if buildProcess != nil { tab = LauncherView.buildPage; notice = "Wait for the build to finish"; return }
                if picked == nil { tab = LauncherView.buildPage; notice = "Build the game first"; return }
                onPlay()
                return
            }
            if tab == LauncherView.exitPage { NSApp.terminate(nil); return }
            inPage = true
            row = 0
        } else if let action = rows[row].action {
            action()
        } else {
            change(1)
        }
    }

    func back() {
        if inPage { inPage = false } else { NSApp.terminate(nil) }
    }

    override func keyDown(with event: NSEvent) {
        if buildProcess == nil { notice = nil }
        switch event.keyCode {
        case 123: if inPage { row = max(row - 1, 0) } else { tab = max(tab - 1, 0) }
        case 124: if inPage { row = min(row + 1, rows.count - 1) } else { tab = min(tab + 1, pages.count - 1) }
        case 126: if inPage { change(1) }
        case 125:
            if !event.isARepeat { downEntered = false }
            // Down enters a page's settings, so START GAME's Version row is reachable;
            // Return plays whatever page is showing.
            if !inPage { if !rows.isEmpty { inPage = true; row = 0; downEntered = true } }
            else if !(event.isARepeat && downEntered) { change(-1) }
        case 36, 76, 49: accept()
        case 53, 51: back()
        default: super.keyDown(with: event); return
        }
        needsDisplay = true
    }

    func hitTab(_ p: NSPoint) -> Int { tabs.firstIndex { $0.contains(p) } ?? -1 }
    func hitColumn(_ p: NSPoint) -> Int {
        guard p.y >= LauncherView.labelY - 6, p.y <= LauncherView.valueY + LauncherView.valueH, p.x >= LauncherView.side else { return -1 }
        let i = Int((p.x - LauncherView.side) / columnWidth)
        return i < rows.count ? i : -1
    }

    override func mouseMoved(with event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        let m = hitTab(p), i = hitColumn(p)
        if !inPage && m >= 0 && m != tab { tab = m; needsDisplay = true }
        if inPage && i >= 0 && i != row { row = i; needsDisplay = true }
    }

    override func mouseDown(with event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        let m = hitTab(p), i = hitColumn(p)
        if buildProcess == nil { notice = nil }
        defer { needsDisplay = true }
        if m >= 0 { tab = m; inPage = false; accept(); return }
        guard i >= 0 else { return }
        // From the tabs, the first click only enters the page and selects the setting.
        let entering = !inPage
        inPage = true
        row = i
        if entering { return }
        if let action = rows[i].action { action() }
        else { change(p.x < LauncherView.side + (CGFloat(i) + 0.5) * columnWidth ? -1 : 1) }
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

    // A triangle pointing along dx, dy from its base centred on x, y.
    func arrow(_ x: CGFloat, _ y: CGFloat, _ dx: CGFloat, _ dy: CGFloat, _ color: NSColor) {
        let path = NSBezierPath()
        path.move(to: NSPoint(x: x + 7 * dx, y: y + 7 * dy))
        path.line(to: NSPoint(x: x - 5 * dy, y: y - 5 * dx))
        path.line(to: NSPoint(x: x + 5 * dy, y: y + 5 * dx))
        path.close()
        color.setFill()
        path.fill()
    }

    func width(_ s: String, _ f: NSFont) -> CGFloat { NSAttributedString(string: s, attributes: [.font: f]).size().width }

    override func draw(_ dirtyRect: NSRect) {
        let w = bounds.width, h = bounds.height, side = LauncherView.side, topH = LauncherView.topH
        let labelY = LauncherView.labelY, valueY = LauncherView.valueY, valueH = LauncherView.valueH
        let tabY = LauncherView.tabY, tabH = LauncherView.tabH
        let sand = rgb(241, 197, 143), dark = rgb(42, 29, 21), muted = rgb(191, 178, 165), chrome = rgb(226, 228, 228)
        rgb(74, 60, 51).setFill()
        bounds.fill()
        background?.draw(in: bounds, from: .zero, operation: .copy, fraction: 1, respectFlipped: true, hints: nil)

        // Letterbox: a solid top bar, and the beach fading into the same dark under the settings.
        rgb(13, 10, 9).setFill()
        NSRect(x: 0, y: 0, width: w, height: topH).fill()
        let fadeStops: [CGFloat] = [0, 0.26, 1]
        NSGradient(colors: [rgb(13, 10, 9, 0), rgb(13, 10, 9, 230), rgb(13, 10, 9)], atLocations: fadeStops, colorSpace: .sRGB)?
            .draw(in: NSRect(x: 0, y: LauncherView.fadeY, width: w, height: h - LauncherView.fadeY), angle: 90)

        // Top bar: key prompts on the right, then title and installed versions in the space left of them.
        var x = w - side
        for (key, label) in [("", "CHANGE"), ("Esc", "BACK"), ("Return", "SELECT")] {
            x -= width(label, smallFont)
            text(label, smallFont, muted, NSRect(x: x, y: 0, width: width(label, smallFont) + 4, height: topH))
            let pillW: CGFloat = key.isEmpty ? 26 : width(key, smallFont) + 14
            x -= pillW + 6
            let pill = NSRect(x: x, y: topH / 2 - 10, width: pillW, height: 20)
            rgb(242, 227, 207).setFill()
            NSBezierPath(roundedRect: pill, xRadius: 10, yRadius: 10).fill()
            if key.isEmpty { arrow(x + 9, topH / 2 + 2, 0, -1, dark); arrow(x + 17, topH / 2 - 2, 0, 1, dark) }
            else { text(key, smallFont, dark, pill, .center) }
            x -= 22
        }
        let state = installedText + (pending ? "  \u{00B7}  UPDATE AVAILABLE" : "")
        // The title goes when the versions would not fit on one line beside the prompts.
        let full = "DEAD OR ALIVE 3  \u{00B7}  " + state
        text(width(full, smallFont) > x - side ? state : full,
             smallFont, muted, NSRect(x: side, y: 0, width: x - side, height: topH))

        // Logo over the cliff.
        text("3", bigFont, chrome, NSRect(x: 0, y: 58, width: w - 44, height: 90), .right, shadow: true, oblique: 0.2)
        text("DEAD OR ALIVE", logoFont, chrome, NSRect(x: 0, y: 82, width: w - 122, height: 56), .right, shadow: true, oblique: 0.2)

        // The chosen page in the bottom bar: its information line, then its settings side by side.
        // START GAME shows its prompt until the player enters it to pick a version.
        let page = pages[tab]
        let prompt = tab == LauncherView.playPage && !inPage
        text(notice ?? (inPage ? page.rows[row].info : page.info), infoFont, rgb(216, 202, 188),
             NSRect(x: side, y: LauncherView.infoY, width: w - 2 * side, height: 22))
        if prompt {
            text("PRESS START BUTTON", titleFont, chrome, NSRect(x: side, y: valueY - 8, width: w - 2 * side, height: 40), shadow: true)
        }
        let colW = columnWidth
        let shownRows: [Row] = prompt ? [] : page.rows
        for (i, r) in shownRows.enumerated() {
            let left = side + CGFloat(i) * colW
            let selected = inPage && i == row
            if selected {
                rgb(232, 191, 138, 45).setFill()
                NSRect(x: left, y: labelY - 6, width: colW, height: valueY + valueH + 6 - labelY).fill(using: .sourceOver)
            }
            if i > 0 {
                rgb(255, 255, 255, 36).setFill()
                NSRect(x: left, y: labelY - 2, width: 1, height: valueY + valueH + 2 - labelY).fill(using: .sourceOver)
            }
            text(r.label.uppercased(), smallFont, selected ? .white : rgb(198, 182, 166), NSRect(x: left + 12, y: labelY, width: colW - 16, height: 18))
            guard !r.names.isEmpty else {
                text(r.verb, itemFont, sand, NSRect(x: left + 12, y: valueY, width: colW - 16, height: valueH))
                continue
            }
            let value = r.names[r.index].uppercased()
            let arrowColor = selected ? sand : rgb(241, 197, 143, 130)
            arrow(left + 19, valueY + valueH / 2, -1, 0, arrowColor)
            arrow(left + 30 + width(value, menuFont), valueY + valueH / 2, 1, 0, arrowColor)
            text(value, menuFont, selected ? .white : rgb(232, 222, 212), NSRect(x: left + 26, y: valueY, width: colW - 30, height: valueH))
        }

        // Page tabs along the bottom, the current one underlined in sunset gold.
        rgb(255, 255, 255, 36).setFill()
        NSRect(x: side, y: tabY - 8, width: w - 2 * side, height: 1).fill(using: .sourceOver)
        var tx = side
        tabs = pages.enumerated().map { m, p in
            let tw = width(p.name, tabFont)
            let rect = NSRect(x: tx, y: tabY, width: tw, height: tabH)
            text(p.name, tabFont, m == tab ? rgb(255, 243, 228) : rgb(169, 156, 144), NSRect(x: tx, y: tabY, width: tw + 4, height: tabH))
            if m == tab {
                NSGradient(starting: rgb(247, 220, 181), ending: rgb(201, 138, 85))?.draw(in: NSRect(x: tx, y: tabY + tabH, width: tw, height: 3), angle: 0)
            }
            tx += tw + 18
            return rect
        }
    }
}

// Keeps the fixed 960x540 scene centred and scaled on black when the window grows or goes fullscreen.
final class Letterbox: NSView {
    override func draw(_ dirtyRect: NSRect) {
        NSColor.black.setFill()
        bounds.fill()
    }

    override func resizeSubviews(withOldSize oldSize: NSSize) {
        guard let scene = subviews.first else { return }
        let s = min(bounds.width / 960, bounds.height / 540)
        scene.frame = NSRect(x: (bounds.width - 960 * s) / 2, y: (bounds.height - 540 * s) / 2,
                             width: 960 * s, height: 540 * s)
        scene.setBoundsSize(NSSize(width: 960, height: 540))
    }
}

final class Launcher: NSObject, NSApplicationDelegate, NSWindowDelegate {
    let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 960, height: 540),
                          // The traffic lights give close, minimize and fullscreen.
                          styleMask: [.titled, .closable, .miniaturizable, .resizable],
                          backing: .buffered, defer: false)
    let view = LauncherView(frame: NSRect(x: 0, y: 0, width: 960, height: 540))
    var launched = false

    func applicationDidFinishLaunching(_ notification: Notification) {
        view.onPlay = { [unowned self] in self.play() }
        window.title = "Dead or Alive 3"
        window.delegate = self
        let letterbox = Letterbox(frame: view.frame)
        letterbox.addSubview(view)
        window.contentView = letterbox
        window.contentAspectRatio = view.frame.size
        window.acceptsMouseMovedEvents = true
        window.makeFirstResponder(view)
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        if view.checkAtStart.index == 1 { view.update(quiet: true) }
    }

    func play() {
        do {
            try FileManager.default.createDirectory(
                at: settingsURL.deletingLastPathComponent(), withIntermediateDirectories: true)
            // Keys this launcher has no row for, such as game_source, stay as saved.
            settings.merge(view.chosen) { $1 }
            let data = try JSONSerialization.data(
                withJSONObject: settings, options: [.prettyPrinted, .sortedKeys])
            try data.write(to: settingsURL)
        } catch {
            NSAlert(error: error).runModal()
            return
        }
        launched = true
        NSApp.terminate(nil)
    }

    // Settings hold after a plain quit too, so the game plays with them later.
    func keepSettings() {
        guard !launched else { return }
        // A quit keeps an untouched saved ultrawide; play saves what the row shows.
        var kept = view.chosen
        if view.screen.index == screens.firstIndex(of: screenSetting()) ?? 3 { kept["screen"] = settings["screen"] }
        settings.merge(kept) { $1 }
        try? FileManager.default.createDirectory(at: settingsURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        try? JSONSerialization.data(withJSONObject: settings, options: [.prettyPrinted, .sortedKeys]).write(to: settingsURL)
    }

    // An install replaces the tools unattended, so closing or quitting waits for it; a build asks to be stopped.
    func windowShouldClose(_ sender: NSWindow) -> Bool { !view.exitBlocked() }

    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        view.exitBlocked() ? .terminateCancel : .terminateNow
    }

    func windowWillClose(_ notification: Notification) {
        keepSettings()
        if !launched { exit(1) }
    }

    // AppKit exits 0 after terminate, so a Dock or Cmd-Q quit would start the game.
    func applicationWillTerminate(_ notification: Notification) {
        keepSettings()
        exit(launched ? 0 : 1)
    }
}

let app = NSApplication.shared
app.setActivationPolicy(.regular)
let launcher = Launcher()
app.delegate = launcher
app.run()
exit(launcher.launched ? 0 : 1)
