// SPDX-License-Identifier: GPL-3.0-or-later
// The launcher's UPDATE page. tools\update.py, which the other launchers share, does the work:
// the last line it prints is the message to show, and its exit code says what happened.
// Keep to C# 5, as in tools/launcher.cs.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;

// tools\update.py's exit codes.
enum UpdateResult { Current = 0, Failed = 1, Available = 2, Installed = 3 }

// The package's VERSION file says what is installed; a source checkout has none and is never updated.
static class Updater
{
    public static readonly bool Available = File.Exists(Path.Combine(Game.Root, "VERSION"));

    // Runs update.py check or install; message is its last stdout line, or its last stderr line if it crashed.
    public static UpdateResult Run(string command, out string message)
    {
        string[] python = LauncherForm.FindPython();
        if (python == null) { message = "Install Python 3.12 or newer and add it to PATH to update"; return UpdateResult.Failed; }
        var args = python.Skip(1).Concat(new[] { "-u", "\"" + Path.Combine(Game.Root, @"tools\update.py") + "\"", command });
        var start = new ProcessStartInfo(python[0], string.Join(" ", args)) {
            UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = Game.Root,
            RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8 };
        start.EnvironmentVariables["PYTHONIOENCODING"] = "utf-8";
        var output = new List<string>();
        var errors = new List<string>();
        Func<List<string>, DataReceivedEventHandler> collect = lines => delegate (object sender, DataReceivedEventArgs e)
        {
            if (!string.IsNullOrWhiteSpace(e.Data)) lock (lines) lines.Add(e.Data.Trim());
        };
        using (Process update = Process.Start(start))
        {
            update.OutputDataReceived += collect(output);
            update.ErrorDataReceived += collect(errors);
            update.BeginOutputReadLine();
            update.BeginErrorReadLine();
            // The no-argument wait also waits for the redirected streams to drain.
            update.WaitForExit();
            lock (output) lock (errors)
            {
                message = output.LastOrDefault() ?? errors.LastOrDefault() ?? "update.py exited with " + update.ExitCode;
                return Enum.IsDefined(typeof(UpdateResult), update.ExitCode) ? (UpdateResult)update.ExitCode : UpdateResult.Failed;
            }
        }
    }
}

partial class LauncherForm
{
    enum UpdateWork { Idle, Checking, Installing }

    // Whether a check found a newer release, and what update.py is doing; play and exit wait for an install.
    bool pending;
    UpdateWork work;
    bool Installing { get { return work == UpdateWork.Installing; } }

    string UpdateStatus()
    {
        return !Updater.Available ? "Updates are for the release package; this is a source checkout"
            : pending ? "A newer version is available. Choose Install"
            : "Choose Check to look for a newer release";
    }

    protected override void OnShown(EventArgs e)
    {
        base.OnShown(e);
        if (Updater.Available && checkAtStart.Index == 1) Update(true);
    }

    // Checks for a newer release, or installs the one found, on a worker thread. A quiet check shows nothing unless it finds one.
    void Update(bool quiet)
    {
        if (work != UpdateWork.Idle) return;
        // A quiet start-up check only reads, so it may run beside a build started from a dropped disc.
        if (build != null && !quiet) { notice = "Wait for the build to finish"; return; }
        if (!Updater.Available) { notice = UpdateStatus(); return; }
        work = pending ? UpdateWork.Installing : UpdateWork.Checking;
        if (!quiet) notice = Installing ? "Downloading the update" : "Checking for updates";
        string command = Installing ? "install" : "check";
        new Thread(delegate ()
        {
            UpdateResult result;
            string message;
            try { result = Updater.Run(command, out message); }
            catch (Exception e) { result = UpdateResult.Failed; message = e.Message; }
            if (IsHandleCreated) BeginInvoke(new Action(() => FinishUpdate(result, message, quiet)));
        }) { IsBackground = true }.Start();
    }

    void FinishUpdate(UpdateResult result, string message, bool quiet)
    {
        work = UpdateWork.Idle;
        if (result != UpdateResult.Failed) pending = result == UpdateResult.Available;
        updateRow.Info = pages[UpdatePage].Info = UpdateStatus();
        if (result == UpdateResult.Installed)
        {
            MessageBox.Show(message, "DOA3 launcher");
            // The new launcher reads private\launcher.json as it starts, so save this one's choices first.
            KeepSettings();
            Process.Start(Path.Combine(Game.Root, "Launcher.exe"));
            Close();
            return;
        }
        if (!quiet || result == UpdateResult.Available) notice = message;
        Invalidate();
    }
}
