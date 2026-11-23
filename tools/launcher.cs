// SPDX-License-Identifier: GPL-3.0-or-later
// Windows launcher, built twice by tools/package_test.py with the C# compiler
// every Windows ships (.NET Framework 4):
//   Launcher.exe builds DOA3 from the player's disc, picks resolution,
//     anti-aliasing, widescreen, renderer, volume, the free camera and the DOAXBV swimsuit switch,
//     remembers them in private\launcher.json, then plays. An ISO, disc folder
//     or default.xbe dropped on it opens the GAME page with that disc chosen.
//   DOA3.exe (PLAY_ONLY) plays with the saved settings.
// The window draws a menu over a generated beach (tools/launcher-background.jpg); no retail art is used.
// Keep to C# 5: the in-box compiler knows nothing newer.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Web.Script.Serialization;
using System.Windows.Forms;

// Unsigned exes without version details score as suspicious with antivirus heuristics (#57).
[assembly: AssemblyTitle("Dead or Alive 3 PC port launcher")]
[assembly: AssemblyDescription("Builds Dead or Alive 3 from the player's own Xbox disc and starts it.")]
[assembly: AssemblyProduct("doa3-re")]
[assembly: AssemblyCompany("NoRain211")]
[assembly: AssemblyCopyright("GPL-3.0-or-later, https://github.com/NoRain211/doa3-re")]

// A version the build installed: each has its own runner folder, play folder
// (with the saves) and XBE name.
class Install
{
    public string Name, PlayDisc, Xbe, BuildDir;
    public string Runner { get { return Path.Combine(Game.Root, BuildDir, "recomp_program_runner.exe"); } }
    // The same program with the SDL_GPU presenter on Vulkan.
    public string VulkanRunner { get { return Path.Combine(Game.Root, BuildDir, "recomp_program_runner_vulkan.exe"); } }
}

static class Game
{
    public static readonly string Root = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
    public static string Private(string name) { return Path.Combine(Root, "private", name); }

    static Dictionary<string, object> ReadJson(string path)
    {
        try { return new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(path)) ?? new Dictionary<string, object>(); }
        catch (Exception) { return new Dictionary<string, object>(); }
    }

    public static Dictionary<string, object> LoadSettings() { return ReadJson(Private("launcher.json")); }

    // The build records the versions it installed, most recently built last, in
    // private\build.json; a build older than that list is USA 3.0 in play-disc.
    static Dictionary<string, object> BuildRecord() { return ReadJson(Private("build.json")); }

    static Install Entry(Dictionary<string, object> record)
    {
        return new Install {
            Name = Str(record, "version", "USA 3.0"), PlayDisc = Str(record, "play_disc", "play-disc"),
            Xbe = Str(record, "xbe", "default.xbe"), BuildDir = Str(record, "build_dir", @"build\recomp-program") };
    }

    // Every version built here, most recently built last, that still has its runner
    // and its play folder.
    public static List<Install> Installed()
    {
        var record = BuildRecord();
        var installs = new List<Install>();
        object listed;
        // The serializer reads a JSON array as an ArrayList.
        if (record.TryGetValue("installed", out listed) && listed is IList)
            foreach (object each in (IList)listed)
            {
                var entry = each as Dictionary<string, object>;
                if (entry != null) installs.Add(Entry(entry));
            }
        else installs.Add(Entry(record));
        return installs.Where(i => File.Exists(i.Runner) && Directory.Exists(Private(i.PlayDisc))).ToList();
    }

    // START GAME plays the remembered version while it is installed, else the one built last.
    public static Install Chosen(Dictionary<string, object> settings)
    {
        var installs = Installed();
        string remembered = Str(settings, "version", "");
        return installs.FirstOrDefault(i => i.Name == remembered) ?? installs.LastOrDefault();
    }

    public static void SaveSettings(Dictionary<string, object> settings)
    {
        Directory.CreateDirectory(Path.Combine(Root, "private"));
        // UTF-8 without a byte order mark keeps a non-ASCII disc path intact.
        File.WriteAllText(Private("launcher.json"), new JavaScriptSerializer().Serialize(settings));
    }

    public static int Int(Dictionary<string, object> s, string key, int fallback)
    {
        object v;
        try { return s.TryGetValue(key, out v) && v != null ? Convert.ToInt32(v, CultureInfo.InvariantCulture) : fallback; }
        catch (Exception) { return fallback; }
    }

    public static bool Bool(Dictionary<string, object> s, string key, bool fallback)
    {
        object v;
        return s.TryGetValue(key, out v) && v is bool ? (bool)v : fallback;
    }

    public static string Str(Dictionary<string, object> s, string key, string fallback)
    {
        object v;
        return s.TryGetValue(key, out v) && v is string ? (string)v : fallback;
    }

    // "4:3", "16:9", "16:10" or an ultrawide "W:H"; settings saved before
    // 16:10 hold a widescreen flag.
    public static string ScreenShape(Dictionary<string, object> s)
    {
        return Str(s, "screen", Bool(s, "widescreen", false) ? "16:9" : "4:3");
    }

    // The primary screen's own shape when it is wider than 16:9 (at most the
    // runtime's 4:1), else 21:9 (2560x1080).
    public static string UltrawideShape()
    {
        var bounds = Screen.PrimaryScreen.Bounds;
        return bounds.Width * 9 > bounds.Height * 16
            ? Math.Min(bounds.Width, bounds.Height * 4) + ":" + bounds.Height : "64:27";
    }

    public static string FirstLine(string path)
    {
        if (!File.Exists(path)) return null;
        string line = File.ReadLines(path).FirstOrDefault();
        return line == null ? null : line.Trim().TrimStart('\uFEFF');
    }

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    static extern bool CreateHardLink(string link, string target, IntPtr security);
    [DllImport("user32.dll")] static extern bool AllowSetForegroundWindow(int processId);

    // A hard-linked view of the extracted disc keeps the game's cache and saves
    // (.recomp-storage) out of the source disc. The first boots fill the cache.
    // The build records the disc it built from; copies cover other drives.
    static string PlayDisc(string folder)
    {
        string disc = Private(folder);
        if (Directory.Exists(disc)) return disc;
        string source = FirstLine(Private("disc-path.txt")) ?? Private(@"imported-disc\disc");
        // Keep one trailing separator so a drive root (a mounted disc) stays D:\ rather than D:.
        source = Path.GetFullPath(source).TrimEnd('\\') + "\\";
        if (!Directory.Exists(source)) throw new IOException("The extracted disc is missing: " + source + ". Build the game again.");
        // Fill a staging folder so an interrupted copy is never taken as complete.
        string staging = disc + ".tmp";
        if (Directory.Exists(staging)) Directory.Delete(staging, true);
        foreach (string file in Directory.EnumerateFiles(source, "*", SearchOption.AllDirectories))
        {
            string relative = file.Substring(source.Length);
            if (relative.StartsWith(@".recomp-storage\", StringComparison.OrdinalIgnoreCase)) continue;
            string target = Path.Combine(staging, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target));
            if (!CreateHardLink(target, file, IntPtr.Zero)) File.Copy(file, target);
        }
        Directory.Move(staging, disc);
        return disc;
    }

    public static bool IsDoaxbvImage(string path)
    {
        string name = Path.GetFileName(path).ToLowerInvariant();
        return name.EndsWith(".iso") && (name.Contains("xtreme") || name.Contains("doax") || name.Contains("beach"));
    }
    public static bool IsDoaxbvFolder(string path)
    {
        return File.Exists(Path.Combine(path, "datahdd.afs")) && File.Exists(Path.Combine(path, "datadvd.afs"));
    }

    // None chosen: a DOAXBV .iso or extracted folder (datahdd.afs and datadvd.afs) in private\,
    // or the disc of a doaxbv-re checkout beside this one.
    static string FindDoaxbv()
    {
        string priv = Path.Combine(Root, "private");
        string sister = Path.Combine(Path.GetDirectoryName(Root) ?? Root, @"doaxbv-re\private\disc");
        var folders = new List<string>();
        if (Directory.Exists(priv))
        {
            string image = Directory.EnumerateFiles(priv, "*.iso").FirstOrDefault(IsDoaxbvImage);
            if (image != null) return image;
            foreach (string d in Directory.GetDirectories(priv)) { folders.Add(d); folders.AddRange(Directory.GetDirectories(d)); }
        }
        if (Directory.Exists(sister)) { folders.Add(sister); folders.AddRange(Directory.GetDirectories(sister)); }
        return folders.FirstOrDefault(IsDoaxbvFolder);
    }

    // The swimsuit switch: DOAXBV swimsuits built from the player's DOAXBV disc
    // (an .iso or its extracted folder), remembered in private\doaxbv-path.txt.
    static void SwimsuitEnv(StringDictionary env, Dictionary<string, object> settings)
    {
        if (string.IsNullOrEmpty(env["RECOMP_DOAXBV_DISC"]))
        {
            string chosen = FirstLine(Private("doaxbv-path.txt"));
            if (chosen == null || !(File.Exists(chosen) || Directory.Exists(chosen)))
            {
                chosen = FindDoaxbv();
                if (chosen == null) return;
                File.WriteAllText(Private("doaxbv-path.txt"), chosen);
            }
            env["RECOMP_DOAXBV_DISC"] = chosen;
        }
        string packed = Path.Combine(Root, @"recipes\doaxbv-suits.pack.gz");
        if (!File.Exists(packed)) packed = Private("doaxbv-suits.pack.gz");
        if (!File.Exists(packed)) return;
        // The runtime reads the suit recipes unpacked, so unpack once into private\.
        string pack = Private("doaxbv-suits.pack");
        if (!File.Exists(pack) || File.GetLastWriteTimeUtc(pack) < File.GetLastWriteTimeUtc(packed))
        {
            using (var input = new GZipStream(File.OpenRead(packed), CompressionMode.Decompress))
            using (var output = File.Create(pack + ".tmp"))
                input.CopyTo(output);
            File.Delete(pack);
            File.Move(pack + ".tmp", pack);
        }
        env["RECOMP_COSTUME_PACK"] = pack;
        if (Bool(settings, "swimsuit_start_favorite", false)) env["RECOMP_COSTUME_START_FAVORITE"] = "1";
    }

    // Starts the chosen version with these settings; returns why it failed, or null.
    public static string Play(Dictionary<string, object> settings)
    {
        Install install = Chosen(settings);
        if (install == null) return "The game is not built yet. Build it in Launcher.exe.";
        string runner = Bool(settings, "vulkan", false) ? install.VulkanRunner : install.Runner;
        if (!File.Exists(runner)) return "This build has no Vulkan renderer. Build the game again in Launcher.exe, or choose Direct3D 11.";
        string keymap;
        string refused = KeyMap(out keymap);
        if (refused != null) return refused;
        string disc = PlayDisc(install.PlayDisc);
        var start = new ProcessStartInfo("cmd.exe") { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = Root };
        var env = start.EnvironmentVariables;
        if (keymap.Length > 0 && string.IsNullOrEmpty(env["RECOMP_KEYMAP"])) env["RECOMP_KEYMAP"] = keymap;
        // The runner parses these with atof, so always write a '.' decimal point.
        env["RECOMP_D3D_SCALE"] = (Int(settings, "height", 480) / 480.0).ToString(CultureInfo.InvariantCulture);
        env["RECOMP_D3D_MSAA"] = Int(settings, "msaa", 1).ToString(CultureInfo.InvariantCulture);
        env["RECOMP_D3D_SMAA"] = Bool(settings, "smaa", false) ? "1" : "0";
        string shape = ScreenShape(settings);
        env["RECOMP_D3D_WIDESCREEN"] = shape == "4:3" ? "0" : shape;
        env["RECOMP_FREE_CAMERA"] = Bool(settings, "free_camera", false) ? "1" : "0";
        env["RECOMP_AUDIO_GAIN"] = (Int(settings, "volume", 100) / 100.0).ToString(CultureInfo.InvariantCulture);
        if (Bool(settings, "swimsuits", true)) SwimsuitEnv(env, settings);
        else { env.Remove("RECOMP_DOAXBV_DISC"); env.Remove("RECOMP_COSTUME_PACK"); env.Remove("RECOMP_COSTUME_START_FAVORITE"); }
        // Test-harness settings would speed the game up or stop it after a timeout.
        env.Remove("RECOMP_UNPACED");
        env.Remove("RECOMP_WATCHDOG_MS");
        string logs = Private("play-logs");
        Directory.CreateDirectory(logs);
        string log = Path.Combine(logs, DateTime.Now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + ".log");
        start.Arguments = string.Format("/c \"\"{0}\" --xbe \"{1}\" 2>\"{2}\" >\"{2}.out\"\"", runner, Path.Combine(disc, install.Xbe), log);
        // The launcher holds the foreground, so let the game's window take it: an
        // unfocused game window gets no pad input from Windows.
        try { AllowSetForegroundWindow(-1); } catch (EntryPointNotFoundException) { }
        using (var game = Process.Start(start))
        {
            // A runner gone within seconds failed to start.
            if (!game.WaitForExit(5000)) return null;
        }
        string tail = "";
        try { tail = string.Join("\n", File.ReadAllLines(log).Reverse().Take(20).Reverse()); } catch (IOException) { }
        return "The game exited during startup:\n\n" + tail + "\n\nFull log: " + log;
    }

    // The pad buttons and keys private\keyboard.json may bind, as in tools/run_game.py. The codes
    // are virtual keys; F10 is a Windows system key and F11 is reserved.
    static readonly string[] KeyActions = { "up", "down", "left", "right", "start", "back", "a", "b", "x", "y", "white", "black", "ltrig", "rtrig", "l3" };

    static Dictionary<string, int> KeyCodes()
    {
        var codes = new Dictionary<string, int> { { "BACKSPACE", 0x08 }, { "TAB", 0x09 }, { "ENTER", 0x0d }, { "SHIFT", 0x10 },
            { "CTRL", 0x11 }, { "ESC", 0x1b }, { "SPACE", 0x20 }, { "LEFT", 0x25 }, { "UP", 0x26 }, { "RIGHT", 0x27 }, { "DOWN", 0x28 } };
        for (char c = 'A'; c <= 'Z'; ++c) codes[c.ToString()] = c;
        for (char c = '0'; c <= '9'; ++c) codes[c.ToString()] = c;
        for (int n = 1; n <= 12; ++n) if (n != 10 && n != 11) codes["F" + n.ToString(CultureInfo.InvariantCulture)] = 0x6f + n;
        return codes;
    }

    // private\keyboard.json, e.g. {"a": "Space"}, as RECOMP_KEYMAP's "button=hexVK,..." ("" without
    // the file); returns why the file cannot be used, or null.
    static string KeyMap(out string keymap)
    {
        keymap = "";
        string path = Private("keyboard.json");
        if (!File.Exists(path)) return null;
        object parsed;
        try { parsed = new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path)); }
        catch (Exception e) { return @"private\keyboard.json is not valid JSON: " + e.Message; }
        var bindings = parsed as Dictionary<string, object>;
        if (bindings == null) return @"private\keyboard.json must be an object such as {""a"": ""Space""}";
        var codes = KeyCodes();
        var entries = new List<string>();
        foreach (var binding in bindings)
        {
            string key = binding.Value as string;
            int code;
            if (Array.IndexOf(KeyActions, binding.Key) < 0 || key == null || !codes.TryGetValue(key.ToUpperInvariant(), out code))
                return @"private\keyboard.json: cannot bind """ + binding.Key + @""" to " + new JavaScriptSerializer().Serialize(binding.Value) +
                    "; buttons are " + string.Join(", ", KeyActions) +
                    "; keys are A-Z, 0-9, F1-F9, F12, Up, Down, Left, Right, Enter, Backspace, Space, Tab, Shift, Ctrl and Esc";
            entries.Add(binding.Key + "=" + code.ToString("x", CultureInfo.InvariantCulture));
        }
        keymap = string.Join(",", entries);
        return null;
    }
}

// Xbox controllers through XInput.
static class Pad
{
    [StructLayout(LayoutKind.Sequential)]
    struct State { public uint Packet; public ushort Buttons; public byte LT, RT; public short LX, LY, RX, RY; }
    [DllImport("xinput1_4.dll")] static extern int XInputGetState(int user, out State state);
    // Buttons of every connected controller, with the left stick folded into the D-pad bits.
    public static int Buttons()
    {
        int all = 0;
        for (int i = 0; i < 4; i++)
        {
            State s;
            if (XInputGetState(i, out s) != 0) continue;
            int b = s.Buttons;
            if (s.LY > 16000) b |= 1;
            if (s.LY < -16000) b |= 2;
            if (s.LX < -16000) b |= 4;
            if (s.LX > 16000) b |= 8;
            all |= b;
        }
        return all;
    }
}

class Row
{
    public string Label, Info, Action;
    public string[] Names;
    public int Index, Default;
}

class Page
{
    public string Name, Info;
    public Row[] Rows;
}

// The UPDATE page lives in tools/launcher_update.cs.
partial class LauncherForm : Form
{
    static readonly int[] Heights = { 480, 720, 1080, 1440, 2160 };
    static readonly int[] Samples = { 1, 2, 4, 8 };
    static readonly string[] Screens = { "4:3", "16:9", "16:10", "Ultrawide" };
    static readonly int[] Volumes = { 0, 25, 50, 75, 100 };
    // GAME's rows: Start Game, Version, DOA3 Disc, Movies, Build.
    const int GamePage = 0, UpdatePage = 4, GameDiscRow = 2, GameBuildRow = 4;
    // Horizon Bar layout on a 960x540 window: the top letterbox bar, then a panel on the left with
    // the page tabs, the chosen page's rows and its information text.
    const int W = 960, H = 540, TopH = 44, Side = 30, FadeY = 334;
    const int PanelW = 480, Inset = 20, TabY = TopH + 12, TabH = 26, ListY = TabY + TabH + 18, RowH = 36, InfoY = H - 64;

    readonly Row resolution, msaa, smaa, screen, renderer, volume, freeCamera, swimsuits, startFavorite, chooseDisc, chooseGame, movies, chooseVersion, updateRow, checkAtStart;
    readonly Page[] pages;
    // The menu cursor picks a page, the row cursor a row on it; focus is on that row, or on the tabs when not inPage.
    int menu, row;
    bool inPage = true;
    // A notice (build progress, a refused action) replaces the information text until the next input.
    string notice;
    string gameSource;
    // The saved pick, kept while no version is installed.
    string savedVersion;
    readonly string savedShape; // a saved ultrawide "W:H", or null
    // The versions built here, most recently built last, as START GAME's Version row
    // lists them and private\launcher.json remembers the pick.
    List<Install> versions = new List<Install>();
    Process build;
    readonly string buildLog = Game.Private("build.log");
    readonly string doaxbvFile = Game.Private("doaxbv-path.txt");
    readonly Timer timer = new Timer { Interval = 500 };
    readonly Timer padTimer = new Timer { Interval = 50 };
    int padHeld;
    DateTime padRepeat = DateTime.MaxValue;
    Point? drag;
    RectangleF[] tabs, values;
    // Caption pills in the top bar: minimize, fullscreen, close.
    RectangleF[] caps;
    int hoverCap = -1;
    // Fullscreen fills the screen with the fixed scene scaled up, centred on black.
    bool fullscreen;
    Rectangle restoreBounds;
    // The window snaps fullscreen at once (without the pixel copy that would
    // park the old frame in the corner) and the scene zooms into place; easing
    // the bounds themselves fights the DWM's resize throttling near full size.
    readonly Timer animTimer = new Timer { Interval = 16 };
    DateTime animStart;
    float sceneScale = 1, animFrom, animTo;
    bool animMax;
    const int AnimMs = 200;

    [DllImport("user32.dll")]
    static extern bool SetWindowPos(IntPtr window, IntPtr after, int x, int y, int width, int height, uint flags);
    // SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOOWNERZORDER.
    void MoveWindowNoCopy(Rectangle r)
    {
        SetWindowPos(Handle, IntPtr.Zero, r.X, r.Y, r.Width, r.Height, 0x0004 | 0x0010 | 0x0100 | 0x0200);
    }

    readonly Bitmap background = new Bitmap(W, H);
    readonly Bitmap animFrame = new Bitmap(W, H);
    readonly Font itemFont = new Font("Arial", 15, FontStyle.Bold | FontStyle.Italic, GraphicsUnit.Pixel);
    readonly Font menuFont = new Font("Arial", 19, FontStyle.Bold | FontStyle.Italic, GraphicsUnit.Pixel);
    readonly Font tabFont = new Font("Arial", 14, FontStyle.Bold | FontStyle.Italic, GraphicsUnit.Pixel);
    readonly Font infoFont = new Font("Arial", 14, FontStyle.Italic, GraphicsUnit.Pixel);
    readonly Font smallFont = new Font("Arial", 12, FontStyle.Bold, GraphicsUnit.Pixel);
    readonly Font logoFont = new Font("Arial Black", 38, FontStyle.Italic, GraphicsUnit.Pixel);
    readonly Font bigFont = new Font("Arial Black", 66, FontStyle.Italic, GraphicsUnit.Pixel);
    readonly StringFormat far = new StringFormat { Alignment = StringAlignment.Far, LineAlignment = StringAlignment.Center };
    readonly StringFormat near = new StringFormat { Alignment = StringAlignment.Near, LineAlignment = StringAlignment.Center };
    readonly StringFormat center = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
    readonly StringFormat wrap = new StringFormat { Trimming = StringTrimming.EllipsisWord, FormatFlags = StringFormatFlags.LineLimit };

    static int Nearest(int h) { return Math.Max(0, Heights.Count(x => x <= h) - 1); }
    static Row Toggle(string label, string info, bool on, int fallback)
    {
        return new Row { Label = label, Info = info, Names = new[] { "Off", "On" }, Index = on ? 1 : 0, Default = fallback };
    }

    public LauncherForm(string disc)
    {
        var settings = Game.LoadSettings();
        int screenHeight = Screen.PrimaryScreen.Bounds.Height;
        resolution = new Row { Label = "Resolution", Info = "Sets the render resolution. 480p opens in a window",
            Names = Heights.Select(h => h + "p").ToArray(), Index = Nearest(Game.Int(settings, "height", screenHeight)), Default = Nearest(screenHeight) };
        msaa = new Row { Label = "MSAA", Info = "Smooths polygon edges with multisampling", Names = new[] { "Off", "2x", "4x", "8x" },
            Index = Math.Max(0, Array.IndexOf(Samples, Game.Int(settings, "msaa", 1))) };
        smaa = Toggle("SMAA", "Smooths edges with a post-process filter", Game.Bool(settings, "smaa", false), 0);
        int savedScreen = Array.IndexOf(Screens, Game.ScreenShape(settings)); // -1: an ultrawide "W:H"
        screen = new Row { Label = "Screen", Info = "Wider 3D view. The HUD and menus stay 4:3", Names = Screens,
            Index = savedScreen >= 0 ? savedScreen : 3 };
        savedShape = savedScreen >= 0 ? null : Game.ScreenShape(settings);
        renderer = new Row { Label = "Renderer", Info = "Vulkan may stutter less on some GPUs. SMAA needs Direct3D 11",
            Names = new[] { "Direct3D 11", "Vulkan" }, Index = Game.Bool(settings, "vulkan", false) ? 1 : 0 };
        int savedVolume = Array.IndexOf(Volumes, Game.Int(settings, "volume", 100));
        volume = new Row { Label = "Volume", Info = "Sets the master volume", Names = Volumes.Select(v => v.ToString()).ToArray(),
            Index = savedVolume < 0 ? Volumes.Length - 1 : savedVolume, Default = Volumes.Length - 1 };
        // In a fight: click L3, then LB / LT (Player 1) or RB / RT (Player 2); X or B marks a favorite.
        freeCamera = Toggle("Free Camera", "In game, F1 or a right-stick click flies the camera", Game.Bool(settings, "free_camera", false), 0);
        swimsuits = Toggle("Swimsuits", "In a fight, click L3: LB / LT P1, RB / RT P2, X / B favorite, Y favorites only", Game.Bool(settings, "swimsuits", true), 1);
        startFavorite = Toggle("Start In Favorite", "Each fighter starts in her first favorite (L3 + X / B)", Game.Bool(settings, "swimsuit_start_favorite", false), 0);
        chooseDisc = new Row { Label = "DOAXBV Disc", Info = DiscName(), Action = "disc" };
        object source;
        gameSource = settings.TryGetValue("game_source", out source) ? source as string : null;
        chooseGame = new Row { Label = "DOA3 Disc", Info = SourceName(), Action = "game" };
        movies = Toggle("Movies", "Off saves 3.4 GB; the game then skips every movie", Game.Bool(settings, "movies", true), 1);
        chooseVersion = new Row { Label = "Version", Info = "The version Start Game plays; each keeps its own saves" };
        savedVersion = Game.Str(settings, "version", "");
        RefreshVersions(savedVersion);
        var buildRow = new Row { Label = "Build", Info = "Lifts and compiles the game; takes 10-20 minutes", Action = "build" };
        updateRow = new Row { Label = "Update", Info = UpdateStatus(), Action = "update" };
        checkAtStart = Toggle("Check At Start", "Looks for a newer release when the launcher opens", Game.Bool(settings, "check_updates", true), 1);
        pages = new[] {
            new Page { Name = "GAME", Info = BuildStatus(), Rows = new[] {
                new Row { Label = "Start Game", Info = PlayInfo(), Action = "play" }, chooseVersion, chooseGame, movies, buildRow } },
            new Page { Name = "VIDEO", Info = "Sets resolution, anti-aliasing, screen shape and the renderer",
                Rows = PageRows(resolution, msaa, smaa, screen, renderer) },
            new Page { Name = "AUDIO", Info = "Sets the master volume", Rows = PageRows(volume) },
            new Page { Name = "GAMEPLAY", Info = "Chooses the free camera and the DOAXBV swimsuits",
                Rows = PageRows(freeCamera, swimsuits, startFavorite, chooseDisc) },
            new Page { Name = "UPDATE", Info = UpdateStatus(), Rows = PageRows(updateRow, checkAtStart) },
        };
        row = versions.Count > 0 ? 0 : GameDiscRow;

        Text = "Dead or Alive 3";
        // The exe's own icon, when the build embedded private\launcher-icon.ico, also marks the window and taskbar.
        // .NET Framework refuses a network-share path; the window then keeps the default icon.
        try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); }
        catch (ArgumentException) { }
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.CenterScreen;
        KeyPreview = true;
        BackColor = Color.Black;
        ClientSize = new Size(W, H);
        DoubleBuffered = true;
        AllowDrop = true;
        DrawBackground();
        timer.Tick += delegate { BuildTick(); };
        padTimer.Tick += delegate { PadTick(); };
        animTimer.Tick += delegate { AnimTick(); };
        // Buttons already down at start (the press that opened the launcher) wait for a release.
        try { padHeld = Pad.Buttons(); padTimer.Start(); } catch (DllNotFoundException) { }
        if (disc != null && DiscPath(disc) != null) UseDisc(disc);
    }

    static Row[] PageRows(params Row[] rows)
    {
        return rows.Concat(new[] {
            new Row { Label = "Default Setting", Info = "Restores the defaults on this page", Action = "default" } }).ToArray();
    }

    string DiscName()
    {
        string path = Game.FirstLine(doaxbvFile);
        return path != null ? "Disc: " + Path.GetFileName(path.TrimEnd('\\')) : @"Found at start in private\ or doaxbv-re; or choose or drop it here";
    }
    // Whether a disc was already extracted (private\imported-disc*\receipt.json).
    static bool Extracted()
    {
        string priv = Path.Combine(Game.Root, "private");
        return Directory.Exists(priv) && Directory.GetDirectories(priv, "imported-disc*").Any(d => File.Exists(Path.Combine(d, "receipt.json")));
    }
    string SourceName()
    {
        if (!string.IsNullOrEmpty(gameSource)) return "Disc: " + Path.GetFileName(gameSource.TrimEnd('\\'));
        return Extracted() ? "Disc: the one already extracted" : "No DOA3 disc chosen";
    }
    string BuildStatus()
    {
        string names = VersionNames();
        return names != null ? "Installed: " + names + ". Build again after an update or for another disc" : "Not built yet: choose your DOA3 disc (Japan 3.1, USA or Europe), then Build";
    }
    // Every installed version, most recently built last; null without one.
    string VersionNames() { return versions.Count > 0 ? string.Join(", ", versions.Select(v => v.Name)) : null; }
    // The version row's pick, saved as private\launcher.json's "version".
    string ChosenName { get { return versions.Count > 0 ? chooseVersion.Names[chooseVersion.Index] : null; } }
    Install Chosen { get { return versions.Count > 0 ? versions[Math.Min(chooseVersion.Index, versions.Count - 1)] : null; } }

    // The versions installed now, and the row that picks one: the remembered choice
    // while it is installed, else the one built last.
    void RefreshVersions(string remember)
    {
        versions = Game.Installed();
        // Before a build the row shows None, a value, rather than a button that does nothing.
        chooseVersion.Names = versions.Count > 0 ? versions.Select(v => v.Name).ToArray() : new[] { "None" };
        int index = versions.FindIndex(v => v.Name == remember);
        chooseVersion.Index = index >= 0 ? index : Math.Max(versions.Count - 1, 0);
    }

    string PlayInfo()
    {
        if (Chosen == null) return "Saves these settings and starts the game";
        return "Starts Dead or Alive 3 " + Chosen.Name + " with these settings";
    }

    public Dictionary<string, object> Settings()
    {
        return new Dictionary<string, object> {
            { "height", Heights[resolution.Index] }, { "msaa", Samples[msaa.Index] }, { "smaa", smaa.Index == 1 },
            { "screen", screen.Index == 3 ? Game.UltrawideShape() : Screens[screen.Index] },
            { "vulkan", renderer.Index == 1 },
            { "volume", Volumes[volume.Index] },
            { "free_camera", freeCamera.Index == 1 },
            { "swimsuits", swimsuits.Index == 1 }, { "swimsuit_start_favorite", startFavorite.Index == 1 },
            { "game_source", gameSource }, { "movies", movies.Index == 1 }, { "check_updates", checkAtStart.Index == 1 },
            { "version", ChosenName ?? savedVersion } };
    }

    static Color Rgb(int r, int g, int b, int a = 255) { return Color.FromArgb(a, r, g, b); }
    static Brush Gradient(float x, float y, float w, float h, Color c1, Color c2) { return new LinearGradientBrush(new RectangleF(x, y, w, h), c1, c2, 90f); }
    string Verb(string action)
    {
        return action == "build" ? "BUILD" : action == "default" ? "RESET" : action == "play" ? "START"
            : action == "update" ? (pending ? "INSTALL" : "CHECK") : "CHOOSE";
    }

    // The generated beach between letterbox bars. It stays a loose file: a large compressed
    // resource makes antivirus heuristics read the exe as packed (#57).
    void DrawBackground()
    {
        string file = Path.Combine(Game.Root, @"tools\launcher-background.jpg");
        using (var bg = Graphics.FromImage(background))
        {
            bg.InterpolationMode = InterpolationMode.HighQualityBicubic;
            if (File.Exists(file)) using (var image = Image.FromFile(file)) bg.DrawImage(image, 0, 0, W, H);
            else bg.Clear(Rgb(74, 60, 51));
            bg.FillRectangle(new SolidBrush(Rgb(13, 10, 9)), 0, 0, W, TopH);
            var fade = new LinearGradientBrush(new Rectangle(0, FadeY - 1, W, H - FadeY + 2), Color.Black, Color.Black, 90f);
            fade.InterpolationColors = new ColorBlend { Colors = new[] { Rgb(13, 10, 9, 0), Rgb(13, 10, 9, 230), Rgb(13, 10, 9) }, Positions = new[] { 0f, .26f, 1f } };
            bg.FillRectangle(fade, 0, FadeY, W, H - FadeY);
            bg.FillRectangle(new SolidBrush(Rgb(13, 10, 9, 200)), 0, TopH, PanelW, H - TopH);
        }
    }

    // Brushed silver, as on the title logo.
    static Brush Chrome(float y, float h)
    {
        var brush = new LinearGradientBrush(new RectangleF(0, y, 10, h), Color.White, Color.Gray, 90f);
        brush.InterpolationColors = new ColorBlend {
            Colors = new[] { Rgb(251, 251, 249), Rgb(212, 215, 216), Rgb(138, 143, 146), Rgb(201, 204, 204), Rgb(240, 240, 238) },
            Positions = new[] { 0f, .38f, .52f, .7f, 1f } };
        return brush;
    }

    static void Shadowed(Graphics g, string text, Font font, Brush brush, RectangleF rect, StringFormat format, int d)
    {
        g.DrawString(text, font, new SolidBrush(Rgb(30, 22, 18, 220)), new RectangleF(rect.X + d, rect.Y + d, rect.Width, rect.Height), format);
        g.DrawString(text, font, brush, rect, format);
    }

    // A triangle pointing along dx, dy from its base centred on x, y.
    static void Arrow(Graphics g, float x, float y, int dx, int dy, Color color)
    {
        g.FillPolygon(new SolidBrush(color), new[] { new PointF(x + 7 * dx, y + 7 * dy), new PointF(x - 5 * dy, y - 5 * dx), new PointF(x + 5 * dy, y + 5 * dx) });
    }

    static GraphicsPath Pill(float x, float y, float w, float h)
    {
        var path = new GraphicsPath();
        path.AddArc(x, y, h, h, 90, 180);
        path.AddArc(x + w - h, y, h, h, 270, 180);
        path.CloseFigure();
        return path;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        Graphics g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = TextRenderingHint.AntiAlias;
        // Plain Bilinear upscaling is a slow GDI+ path (~250ms a frame at 2.7x); this one is not.
        g.InterpolationMode = InterpolationMode.HighQualityBilinear;
        if (animTimer.Enabled)
        {
            g.DrawImage(animFrame, (ClientSize.Width - W * sceneScale) / 2f, (ClientSize.Height - H * sceneScale) / 2f, W * sceneScale, H * sceneScale);
            return;
        }
        if (fullscreen)
        {
            g.TranslateTransform((ClientSize.Width - W * sceneScale) / 2f, (ClientSize.Height - H * sceneScale) / 2f);
            g.ScaleTransform(sceneScale, sceneScale);
        }
        g.DrawImage(background, 0, 0);
        Color sand = Rgb(241, 197, 143), dark = Rgb(42, 29, 21), muted = Rgb(191, 178, 165);

        // Top letterbox bar: caption pills (minimize, fullscreen, close) at the right edge, button prompts left of
        // them, then title and installed versions in the space left of those.
        caps = new RectangleF[3];
        float cx = W - Side;
        for (int c = 2; c >= 0; c--)
        {
            cx -= 22;
            if (c < 2) cx -= 10; // a gap between the pills
            caps[c] = new RectangleF(cx, TopH / 2 - 10, 22, 20);
            using (var fill = new SolidBrush(hoverCap == c ? sand : Rgb(242, 227, 207))) g.FillPath(fill, Pill(cx, TopH / 2 - 10, 22, 20));
            using (var pen = new Pen(dark, 2f) { StartCap = LineCap.Round, EndCap = LineCap.Round })
            {
                float px = cx + 11, py = TopH / 2f;
                if (c == 0) g.DrawLine(pen, px - 4, py + 4, px + 4, py + 4);
                else if (c == 2) { g.DrawLine(pen, px - 4, py - 3, px + 4, py + 3); g.DrawLine(pen, px - 4, py + 3, px + 4, py - 3); }
                else
                {
                    g.DrawLines(pen, new[] { new PointF(px - 5, py - 2), new PointF(px - 5, py - 5), new PointF(px - 2, py - 5) });
                    g.DrawLines(pen, new[] { new PointF(px + 2, py - 5), new PointF(px + 5, py - 5), new PointF(px + 5, py - 2) });
                    g.DrawLines(pen, new[] { new PointF(px - 5, py + 2), new PointF(px - 5, py + 5), new PointF(px - 2, py + 5) });
                    g.DrawLines(pen, new[] { new PointF(px + 2, py + 5), new PointF(px + 5, py + 5), new PointF(px + 5, py + 2) });
                }
            }
        }
        float x = cx - 14;
        foreach (var hint in new[] { new[] { "B", "BACK" }, new[] { "A", "SELECT" }, new[] { "", "CHANGE" }, new[] { "LB/RB", "PAGE" } })
        {
            float textW = g.MeasureString(hint[1], smallFont).Width;
            x -= textW;
            g.DrawString(hint[1], smallFont, new SolidBrush(muted), new RectangleF(x, 0, textW + 4, TopH), near);
            float w = hint[0] == "" ? 26 : hint[0].Length > 1 ? g.MeasureString(hint[0], smallFont).Width + 8 : 20;
            x -= w + 6;
            g.FillPath(new SolidBrush(Rgb(242, 227, 207)), Pill(x, TopH / 2 - 10, w, 20));
            if (hint[0] == "") { Arrow(g, x + 11, TopH / 2, -1, 0, dark); Arrow(g, x + 15, TopH / 2, 1, 0, dark); }
            else g.DrawString(hint[0], smallFont, new SolidBrush(dark), new RectangleF(x, TopH / 2 - 10, w, 20), center);
            x -= 22;
        }
        string names = VersionNames();
        string state = (names != null ? "INSTALLED: " + names.ToUpperInvariant() : "NOT BUILT YET")
            + (pending ? "  \u00B7  UPDATE AVAILABLE" : "");
        // The title goes when the versions would not fit on one line beside the prompts.
        string header = "DEAD OR ALIVE 3  \u00B7  " + state;
        if (g.MeasureString(header, smallFont).Width > x - Side) header = state;
        g.DrawString(header, smallFont, new SolidBrush(muted), new RectangleF(Side, 0, x - Side, TopH), near);

        // Logo over the cliff.
        Shadowed(g, "3", bigFont, Chrome(58, 84), new RectangleF(0, 58, W - 44, 90), far, 3);
        Shadowed(g, "DEAD OR ALIVE", logoFont, Chrome(82, 50), new RectangleF(0, 82, W - 118, 56), far, 3);

        // The left panel: page tabs spread across it, the current one underlined in sunset gold and lit while focused.
        var divider = new Pen(Rgb(255, 255, 255, 36));
        tabs = new RectangleF[pages.Length];
        Font tabSize = tabFont;
        float[] widths = new float[pages.Length];
        float total = 0, room = PanelW - 2 * Inset;
        // A smaller size keeps at least 12 pixels between them when they would not fit.
        for (int size = 14; ; size--)
        {
            total = 0;
            for (int m = 0; m < pages.Length; m++) total += widths[m] = g.MeasureString(pages[m].Name, tabSize).Width;
            if (total + 12 * (pages.Length - 1) <= room || size <= 10) break;
            if (tabSize != tabFont) tabSize.Dispose();
            tabSize = new Font("Arial", size - 1, FontStyle.Bold | FontStyle.Italic, GraphicsUnit.Pixel);
        }
        float gap = pages.Length > 1 ? (room - total) / (pages.Length - 1) : 0;
        float tx = Inset;
        for (int m = 0; m < pages.Length; m++)
        {
            float w = widths[m];
            tabs[m] = new RectangleF(tx, TabY, w, TabH);
            if (m == menu && !inPage) g.FillRectangle(new SolidBrush(Rgb(232, 191, 138, 70)), tx - 6, TabY, w + 12, TabH + 3);
            g.DrawString(pages[m].Name, tabSize, new SolidBrush(m == menu ? (inPage ? Rgb(255, 243, 228) : Color.White) : Rgb(169, 156, 144)), tabs[m], near);
            if (m == menu) g.FillRectangle(new LinearGradientBrush(new RectangleF(tx - 1, 0, w + 2, 1), Rgb(247, 220, 181), Rgb(201, 138, 85), 0f), tx, TabY + TabH, w, 3);
            tx += w + gap;
        }
        if (tabSize != tabFont) tabSize.Dispose();
        g.DrawLine(divider, Inset, ListY - 9, PanelW - Inset, ListY - 9);

        // The page's rows: label on the left, the value as < VALUE > or the action's verb in a pill on the right.
        Page page = pages[menu];
        Row[] rows = page.Rows;
        values = new RectangleF[rows.Length];
        const float valueRight = PanelW - Inset - 12;
        for (int i = 0; i < rows.Length; i++)
        {
            Row r = rows[i];
            float y = ListY + i * RowH, cy = y + RowH / 2f;
            bool selected = inPage && i == row;
            if (selected) g.FillRectangle(new SolidBrush(Rgb(232, 191, 138, 45)), Inset, y, PanelW - 2 * Inset, RowH);
            g.DrawString(r.Label.ToUpperInvariant(), itemFont, new SolidBrush(selected ? Color.White : Rgb(198, 182, 166)), new RectangleF(Inset + 12, y, PanelW - 2 * Inset - 24, RowH), near);
            if (r.Names == null)
            {
                string verb = Verb(r.Action);
                float pw = g.MeasureString(verb, smallFont).Width + 18;
                var pill = new RectangleF(valueRight - pw, cy - 11, pw, 22);
                g.FillPath(new SolidBrush(selected ? sand : Rgb(241, 197, 143, 40)), Pill(pill.X, pill.Y, pill.Width, pill.Height));
                g.DrawString(verb, smallFont, new SolidBrush(selected ? dark : sand), pill, center);
                continue;
            }
            string value = r.Names[r.Index].ToUpperInvariant();
            float textRight = valueRight - 13, textLeft = textRight - g.MeasureString(value, menuFont).Width;
            Color arrow = selected ? sand : Rgb(241, 197, 143, 130);
            if (r.Index > 0) Arrow(g, textLeft - 6, cy, -1, 0, arrow);
            if (r.Index < r.Names.Length - 1) Arrow(g, valueRight - 7, cy, 1, 0, arrow);
            g.DrawString(value, menuFont, new SolidBrush(selected ? Color.White : Rgb(232, 222, 212)), new RectangleF(textLeft, y, textRight - textLeft, RowH), near);
            values[i] = new RectangleF(textLeft - 17, y, valueRight - textLeft + 17, RowH);
        }

        // The focused row's information, the page's while the tabs have focus, or a notice: two lines at most.
        string info = notice ?? (inPage ? rows[row].Info : page.Info);
        g.DrawLine(divider, Inset, InfoY - 10, PanelW - Inset, InfoY - 10);
        g.DrawString(info, infoFont, new SolidBrush(Rgb(216, 202, 188)), new RectangleF(Inset, InfoY, PanelW - 2 * Inset, 2 * infoFont.GetHeight(g) + 2), wrap);
    }

    static readonly string BundledPython = Path.Combine(Game.Root, @"tools\python\python.exe");

    // The release ZIP's bundled Python, else Python 3.12 or newer through the py launcher or PATH.
    internal static string[] FindPython()
    {
        if (File.Exists(BundledPython)) return new[] { BundledPython };
        foreach (var python in new[] { new[] { "py", "-3" }, new[] { "python" } })
        {
            string args = string.Join(" ", python.Skip(1)) + " -c \"import sys; sys.exit(sys.version_info < (3, 12))\"";
            try
            {
                using (var check = Process.Start(new ProcessStartInfo(python[0], args) { UseShellExecute = false, CreateNoWindow = true }))
                    if (check.WaitForExit(30000) && check.ExitCode == 0) return python;
            }
            catch (Win32Exception) { }
        }
        return null;
    }

    // Python's environment for start; an installed Python's settings or user packages must not redirect the bundled one.
    internal static void SetPythonEnvironment(ProcessStartInfo start, string[] python)
    {
        start.EnvironmentVariables["PYTHONIOENCODING"] = "utf-8";
        if (python[0] != BundledPython) return;
        start.EnvironmentVariables.Remove("PYTHONHOME");
        start.EnvironmentVariables.Remove("PYTHONPATH");
        start.EnvironmentVariables["PYTHONNOUSERSITE"] = "1";
    }

    // A trailing backslash would escape the closing quote.
    static string Quote(string path) { return "\"" + (path.EndsWith("\\") ? path + "." : path) + "\""; }

    void StartBuild()
    {
        if (build != null) return;
        if (Installing) { notice = "Wait for the update to finish"; return; }
        if (string.IsNullOrEmpty(gameSource) && !Extracted()) { notice = "Choose your DOA3 ISO or disc folder first"; return; }
        string[] python = FindPython();
        if (python == null)
        {
            MessageBox.Show("Install Python 3.12 or newer from https://www.python.org/downloads/windows/ and add it to PATH, then build again.", "DOA3 launcher");
            return;
        }
        Game.SaveSettings(Settings());
        var command = new List<string> { Quote(python[0]) }; // the bundled python.exe path may hold spaces
        command.AddRange(python.Skip(1));
        command.Add("-u");
        command.Add(Quote(Path.Combine(Game.Root, @"tools\build_game.py")));
        if (!string.IsNullOrEmpty(gameSource)) command.Add(Quote(gameSource));
        command.Add(movies.Index == 1 ? "--movies" : "--no-movies");
        command.Add(">" + Quote(buildLog));
        command.Add("2>" + Quote(buildLog + ".err"));
        var start = new ProcessStartInfo("cmd.exe", "/c \"" + string.Join(" ", command) + "\"")
            { UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = Game.Root };
        // Build tools print any character; the log is read back as UTF-8.
        SetPythonEnvironment(start, python);
        build = Process.Start(start);
        notice = "Starting the build";
        timer.Start();
    }

    static string Tail(string path, int count)
    {
        try
        {
            using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            {
                stream.Seek(Math.Max(0, stream.Length - 8192), SeekOrigin.Begin);
                var lines = new StreamReader(stream, Encoding.UTF8).ReadToEnd().Split('\n').Select(l => l.Trim()).Where(l => l.Length > 0).ToArray();
                return string.Join("\n", lines.Skip(Math.Max(0, lines.Length - count)));
            }
        }
        catch (IOException) { return ""; }
    }

    void BuildTick()
    {
        string line = Tail(buildLog, 1);
        if (line.Length > 0) notice = line;
        if (build.HasExited) { timer.Stop(); FinishBuild(); }
        Invalidate();
    }

    void FinishBuild()
    {
        int code = build.ExitCode;
        build.Dispose();
        build = null;
        RefreshVersions(ChosenName ?? savedVersion);
        pages[GamePage].Info = BuildStatus();
        pages[GamePage].Rows[0].Info = PlayInfo();
        chooseGame.Info = SourceName();
        if (code == 0) { notice = "Build complete. Choose Start Game to play"; menu = GamePage; inPage = true; row = 0; return; }
        notice = @"Build failed; see private\build.log";
        MessageBox.Show("The build failed:\n\n" + Tail(buildLog + ".err", 8) + "\n\nFull output: " + buildLog, "DOA3 launcher");
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        base.OnFormClosing(e);
        if (Installing) { notice = "Wait for the update to finish"; e.Cancel = true; Invalidate(); return; }
        if (build != null && !build.HasExited)
        {
            if (MessageBox.Show("Stop the build?", "DOA3 launcher", MessageBoxButtons.YesNo) != DialogResult.Yes) { e.Cancel = true; return; }
            timer.Stop();
            using (var kill = Process.Start(new ProcessStartInfo("taskkill", "/T /F /PID " + build.Id) { UseShellExecute = false, CreateNoWindow = true }))
                kill.WaitForExit();
        }
        KeepSettings();
    }

    // Settings hold after a plain exit too, so DOA3.exe plays with them. An exit keeps
    // the saved ultrawide shape; play sizes it to the current screen.
    void KeepSettings()
    {
        var kept = Settings();
        if (screen.Index == 3 && savedShape != null) kept["screen"] = savedShape;
        try { Game.SaveSettings(kept); } catch (IOException) { } catch (UnauthorizedAccessException) { }
    }

    static int Clamp(int value, int max) { return Math.Min(Math.Max(value, 0), max); }
    Row[] Rows { get { return pages[menu].Rows; } }

    // Keeps the focus where it is; a focused row moves to the new page's first.
    void SwitchPage(int delta)
    {
        int m = Clamp(menu + delta, pages.Length - 1);
        if (m != menu) { menu = m; row = 0; }
    }

    void ChangeValue(int delta)
    {
        if (!inPage) return;
        Row r = Rows[row];
        if (r.Names != null) r.Index = Clamp(r.Index + delta, r.Names.Length - 1);
        if (r == chooseVersion) pages[GamePage].Rows[0].Info = PlayInfo();
    }

    static string Choose(string title, string filter)
    {
        using (var dialog = new OpenFileDialog { Title = title, Filter = filter })
            return dialog.ShowDialog() == DialogResult.OK ? dialog.FileName : null;
    }

    // Start Game, and the pad's START from any page.
    void PlayGame()
    {
        if (Installing) notice = "Wait for the update to finish";
        else if (build != null) notice = "Wait for the build to finish";
        else if (Chosen == null)
        {
            menu = GamePage; inPage = true;
            row = string.IsNullOrEmpty(gameSource) && !Extracted() ? GameDiscRow : GameBuildRow;
            notice = "Build the game first";
        }
        else if (renderer.Index == 1 && !File.Exists(Chosen.VulkanRunner))
        {
            menu = GamePage; inPage = true; row = GameBuildRow;
            notice = "Build again to add Vulkan, or choose Direct3D 11";
        }
        else { Tag = "play"; Close(); }
    }

    void Accept()
    {
        Page page = pages[menu];
        if (!inPage) { inPage = true; row = 0; }
        else
        {
            Row r = page.Rows[row];
            if (r.Action == "default") { foreach (Row each in page.Rows) if (each.Names != null) each.Index = each.Default; }
            else if (r.Action == "play") PlayGame();
            else if (r.Action == "disc")
            {
                string file = Choose("DOAXBV disc image (.iso), or datahdd.afs in its extracted folder", "DOAXBV disc|*.iso;datahdd.afs|All files|*.*");
                if (file != null)
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(doaxbvFile));
                    File.WriteAllText(doaxbvFile, file.EndsWith(".afs", StringComparison.OrdinalIgnoreCase) ? Path.GetDirectoryName(file) : file);
                    r.Info = DiscName();
                }
            }
            else if (r.Action == "game")
            {
                string file = Choose("Dead or Alive 3 disc image (.iso), or default.xbe in its extracted folder", "DOA3 disc|*.iso;default.xbe|All files|*.*");
                if (file != null)
                {
                    gameSource = file.EndsWith(".xbe", StringComparison.OrdinalIgnoreCase) ? Path.GetDirectoryName(file) : file;
                    r.Info = SourceName();
                }
            }
            else if (r.Action == "build") StartBuild();
            else if (r.Action == "update") Update(false);
        }
        Invalidate();
    }

    void Back()
    {
        if (inPage) inPage = false;
        else Close();
    }

    // Up and Down move between the tabs and the rows, Left and Right change a value or the page.
    void Press(Keys key)
    {
        if (build == null) notice = null;
        switch (key)
        {
            case Keys.Left: case Keys.Right:
                if (inPage) ChangeValue(key == Keys.Left ? -1 : 1); else SwitchPage(key == Keys.Left ? -1 : 1);
                break;
            case Keys.Q: case Keys.PageUp: SwitchPage(-1); break;
            case Keys.E: case Keys.PageDown: SwitchPage(1); break;
            case Keys.Up: if (inPage) { if (row == 0) inPage = false; else row--; } break;
            case Keys.Down: if (!inPage) { inPage = true; row = 0; } else row = Clamp(row + 1, Rows.Length - 1); break;
            case Keys.Return: Accept(); break;
            case Keys.Play: PlayGame(); break; // the pad's START only
            case Keys.Escape: case Keys.Back: Back(); break;
        }
        Invalidate();
    }

    // A form without controls would otherwise spend the arrow keys on focus moves.
    protected override bool ProcessDialogKey(Keys keyData) { return false; }
    protected override void OnKeyDown(KeyEventArgs e) { base.OnKeyDown(e); if (e.KeyCode != Keys.Play) Press(e.KeyCode); }

    // A dropped DOAXBV disc (a folder with datahdd.afs, that file, or an ISO named for the game) becomes the
    // swimsuit disc; any other ISO, disc folder or default.xbe becomes the DOA3 disc and selects Build.
    static string DiscPath(string path)
    {
        return new[] { ".iso", ".xbe", ".afs" }.Any(e => path.EndsWith(e, StringComparison.OrdinalIgnoreCase)) || Directory.Exists(path) ? path : null;
    }
    static string DoaxbvPath(string path)
    {
        if (path.EndsWith(".afs", StringComparison.OrdinalIgnoreCase)) return Path.GetDirectoryName(path);
        if (File.Exists(Path.Combine(path, "datahdd.afs")) || Game.IsDoaxbvImage(path)) return path;
        return null;
    }

    void UseDisc(string path)
    {
        string doaxbv = DoaxbvPath(path);
        if (doaxbv != null)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(doaxbvFile));
            File.WriteAllText(doaxbvFile, doaxbv);
            chooseDisc.Info = DiscName();
            notice = "Swimsuits: " + DiscName();
            return;
        }
        if (build != null) { notice = "Wait for the build to finish"; return; }
        gameSource = path.EndsWith(".xbe", StringComparison.OrdinalIgnoreCase) ? Path.GetDirectoryName(path) : path;
        chooseGame.Info = SourceName();
        menu = GamePage; inPage = true; row = GameBuildRow;
        notice = SourceName() + ". Press Build";
    }

    static string Dropped(DragEventArgs e)
    {
        var files = e.Data.GetData(DataFormats.FileDrop) as string[];
        return files != null && files.Length > 0 ? DiscPath(files[0]) : null;
    }
    protected override void OnDragEnter(DragEventArgs e) { if (Dropped(e) != null) e.Effect = DragDropEffects.Copy; }
    protected override void OnDragDrop(DragEventArgs e)
    {
        string path = Dropped(e);
        if (path != null) { UseDisc(path); Activate(); Invalidate(); }
    }

    // D-pad or left stick, A, START plays, B or BACK, LB and RB switch pages. Directions repeat while held.
    static readonly int[] PadBits = { 0x1, 0x2, 0x4, 0x8, 0x1000, 0x10, 0x2000, 0x20, 0x100, 0x200 };
    static readonly Keys[] PadKeys = { Keys.Up, Keys.Down, Keys.Left, Keys.Right, Keys.Return, Keys.Play, Keys.Escape, Keys.Escape, Keys.PageUp, Keys.PageDown };
    void PadTick()
    {
        // Only while the launcher has focus, so a controller used elsewhere during a build is ignored.
        if (ActiveForm != this) return;
        int buttons = Pad.Buttons();
        DateTime now = DateTime.Now;
        int held = padHeld;
        padHeld = buttons;
        for (int i = 0; i < PadBits.Length; i++)
        {
            int bit = PadBits[i];
            if ((buttons & bit) == 0) continue;
            bool pressed = (held & bit) == 0;
            if (!pressed && (bit > 0x8 || now < padRepeat)) continue;
            padRepeat = now.AddMilliseconds(pressed ? 400 : 120);
            Press(PadKeys[i]);
            return;
        }
    }

    // The window keeps its fixed 960x540 scene; fullscreen scales it, so map a
    // mouse point back into the scene before any hit test.
    PointF LayoutPoint(Point p)
    {
        if (!fullscreen) return new PointF(p.X, p.Y);
        return new PointF((p.X - (ClientSize.Width - W * sceneScale) / 2f) / sceneScale,
            (p.Y - (ClientSize.Height - H * sceneScale) / 2f) / sceneScale);
    }

    int HitCap(float x, float y)
    {
        if (caps == null) return -1;
        for (int c = 0; c < caps.Length; c++) if (caps[c].Contains(x, y)) return c;
        return -1;
    }

    void PressCap(int c)
    {
        // While animating, the window shape belongs to the animation.
        if (c == 0 && animTimer.Enabled) return;
        if (c == 0) WindowState = FormWindowState.Minimized;
        else if (c == 1) ToggleFullscreen();
        else Close();
    }

    // A borderless window covering the whole monitor is fullscreen, taskbar included.
    // The bounds snap there without copying pixels, and the scene zooms in.
    void ToggleFullscreen()
    {
        if (animTimer.Enabled) return;
        if (!fullscreen)
        {
            fullscreen = true;
            restoreBounds = Bounds;
            Rectangle target = Screen.FromControl(this).Bounds;
            StartZoom(1, Math.Min(target.Width / (float)W, target.Height / (float)H), true);
            MoveWindowNoCopy(target);
        }
        // Stays fullscreen (scaled hit tests, no drag) until AnimTick snaps the window back.
        else StartZoom(sceneScale, 1, false);
    }

    void StartZoom(float from, float to, bool enter)
    {
        // GDI+ re-rasterizes text and paths at every new scale (over 200ms a frame
        // near full size), so the zoom draws one 1x snapshot of the scene, scaled.
        bool full = fullscreen;
        fullscreen = false;
        using (Graphics g = Graphics.FromImage(animFrame)) OnPaint(new PaintEventArgs(g, new Rectangle(0, 0, W, H)));
        fullscreen = full;
        animFrom = from; animTo = to; animMax = enter; animStart = DateTime.Now;
        animTimer.Start();
    }

    void AnimTick()
    {
        float t = (float)(DateTime.Now - animStart).TotalMilliseconds / AnimMs;
        if (t >= 1)
        {
            animTimer.Stop();
            sceneScale = animTo;
            // Leaving fullscreen ends by snapping the window back where it was.
            if (!animMax) { fullscreen = false; MoveWindowNoCopy(restoreBounds); }
        }
        else sceneScale = animFrom + (animTo - animFrom) * (t * t * (3 - 2 * t));
        Invalidate();
    }

    int HitTab(int x, int y)
    {
        if (tabs == null) return -1;
        for (int m = 0; m < tabs.Length; m++) if (tabs[m].Contains(x, y)) return m;
        return -1;
    }
    int HitRow(float x, float y)
    {
        if (y < ListY || x < Inset || x >= PanelW - Inset) return -1;
        int i = (int)((y - ListY) / RowH);
        return i < Rows.Length ? i : -1;
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        // Borderless, so a drag on the top bar moves the window; not while fullscreen.
        if (drag.HasValue && !fullscreen) { Location = new Point(Left + e.X - drag.Value.X, Top + e.Y - drag.Value.Y); return; }
        PointF p = LayoutPoint(e.Location);
        int cap = HitCap(p.X, p.Y);
        if (cap != hoverCap) { hoverCap = cap; Invalidate(); }
        int m = HitTab((int)p.X, (int)p.Y), i = HitRow(p.X, p.Y);
        if (m >= 0 && (inPage || m != menu)) { menu = m; inPage = false; Invalidate(); }
        if (i >= 0 && (!inPage || i != row)) { inPage = true; row = i; Invalidate(); }
    }
    protected override void OnMouseDown(MouseEventArgs e)
    {
        if (fullscreen) return;
        PointF p = LayoutPoint(e.Location);
        if (e.Y < TopH && HitCap(p.X, p.Y) < 0) drag = e.Location;
    }
    protected override void OnMouseUp(MouseEventArgs e) { drag = null; }
    protected override void OnMouseLeave(EventArgs e)
    {
        base.OnMouseLeave(e);
        if (hoverCap >= 0) { hoverCap = -1; Invalidate(); }
    }
    protected override void OnResize(EventArgs e) { base.OnResize(e); if (fullscreen) Invalidate(); }

    protected override void OnMouseClick(MouseEventArgs e)
    {
        if (build == null) notice = null;
        PointF p = LayoutPoint(e.Location);
        if (e.Button == MouseButtons.Left && HitCap(p.X, p.Y) >= 0) { PressCap(HitCap(p.X, p.Y)); return; }
        int m = HitTab((int)p.X, (int)p.Y), i = HitRow(p.X, p.Y);
        if (e.Button == MouseButtons.Right) { Back(); Invalidate(); return; }
        if (m >= 0) { menu = m; inPage = false; Invalidate(); return; }
        if (i < 0) return;
        inPage = true; row = i;
        if (Rows[i].Action != null) { Accept(); Invalidate(); return; }
        // The value's left half steps back, its right half forward.
        RectangleF value = values != null && i < values.Length ? values[i] : RectangleF.Empty;
        if (value.Contains((float)p.X, (float)p.Y)) ChangeValue((float)p.X < value.X + value.Width / 2 ? -1 : 1);
        Invalidate();
    }
}

static class Program
{
    [STAThread]
    static void Main(string[] args)
    {
        Application.EnableVisualStyles();
        try
        {
#if PLAY_ONLY
            if (Game.Chosen(Game.LoadSettings()) == null) { Process.Start(Path.Combine(Game.Root, "Launcher.exe")); return; }
            Dictionary<string, object> settings = Game.LoadSettings();
#else
            var form = new LauncherForm(args.Length > 0 ? args[0] : null);
            Application.Run(form);
            if (form.Tag as string != "play") return;
            Dictionary<string, object> settings = form.Settings();
            Game.SaveSettings(settings);
#endif
            string error = Game.Play(settings);
            if (error != null) MessageBox.Show(error, "Dead or Alive 3");
        }
        catch (Exception e) { MessageBox.Show(e.Message, "Dead or Alive 3"); }
    }
}
