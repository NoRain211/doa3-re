# Windows, macOS and Linux are all full targets

Status: accepted

DOA3 is feature complete only when the acceptance matrix passes on Windows,
macOS and Linux. Best-effort ports were the cheaper choice, and doaxbv-re's
roadmap (#1) still lists Linux as out of scope. We chose full targets because
the shared runtime (ADR 0001) already builds on all three, and PR #7 showed
DOA3 running on Metal and Vulkan. Every matrix row is now run three times.
