# RamCpp4: In-Process Windows Taskbar System Monitor

**RamCpp4** is a lightweight, real-time hardware monitor widget designed for **Windows 10 and Windows 11**. It docks directly into the Windows Taskbar immediately to the left of the system tray (`TrayNotifyWnd`) or above it on vertical taskbars, displaying customizable real-time CPU, RAM, and Network metrics matching native taskbar typography.

Unlike external overlay widgets that float on top of the taskbar and can suffer from z-order conflicts, animation latency, and auto-hide desynchronization, **RamCpp4 uses native in-process DLL injection into `explorer.exe`**. The widget window is an authentic `WS_CHILD` window of `Shell_TrayWnd`, delivering perfect taskbar integration, automatic synchronization with taskbar auto-hide/fullscreen applications, and zero z-order fighting.

---

## Key Features

- **In-Process Taskbar Integration**: Injected into `explorer.exe` using `SetWindowsHookExW(WH_GETMESSAGE)` on Explorer's tray thread, unhooking immediately after initialization to ensure zero ongoing hook overhead.
- **True `WS_CHILD` Window**: Directly parented to `Shell_TrayWnd`. Automatically moves, hides, and scales with the Windows taskbar.
- **Zero-UAC & 100% Portable**: Requires **no Administrator privileges** and no installation. Runs from any folder without touching the Windows Registry.
- **Smooth Typography (No Fringing)**: Renders using double-buffered ClearType GDI that captures the native taskbar background under the widget, ensuring smooth anti-aliased text with an invisible background.
- **Thread Isolation**: All hardware polling (CPU via `GetSystemTimes`, RAM via `GlobalMemoryStatusEx`, Network via `GetIfTable2`) runs on a dedicated background worker thread inside the DLL, guaranteeing Explorer's UI thread is never blocked or stuttered.
- **Dynamic Orientation & Clamping**: Detects horizontal vs. vertical taskbar orientations dynamically. Widget coordinates are strictly clamped within the taskbar monitor screen boundaries (`rcMonitor`).
- **Resilient Lifecycle**: The loader process acts as a zero-CPU background watcher holding `RamCpp4_SingleInstance_Mutex`. It listens for `TaskbarCreated` to automatically re-inject if Explorer crashes or restarts.
- **Theme Auto-Detection**: Dynamically checks Windows dark/light theme (`SystemUsesLightTheme`) and live-reacts to `WM_THEMECHANGED` and `WM_SETTINGCHANGE`.
- **Customizable Templating**: Supports token substitution (`{cpu}`, `{ram_percent}`, `{net_down_mb_int}`, etc.), tabular alignment padding, custom fonts, sizes, and padding.
- **Clean Unload**: Exiting from the context menu or via `RamCpp4.exe --quit` removes window subclasses, shuts down worker threads, unloads `RamCpp4_Widget.dll` from Explorer, and closes the watcher process cleanly.

---

## Architecture Overview

```
+-------------------------------------------------------------+
|                      RamCpp4.exe (Loader)                   |
|  - Mutex: RamCpp4_SingleInstance_Mutex                      |
|  - Injects RamCpp4_Widget.dll via SetWindowsHookExW         |
|  - Immediate Unhook once widget window is active            |
|  - Background Watcher for TaskbarCreated message            |
+------------------------------+------------------------------+
                               | Injects via Hook
                               v
+-------------------------------------------------------------+
|                        explorer.exe                         |
|                                                             |
|   Shell_TrayWnd (Taskbar)                                   |
|   +-----------------------------------------------------+   |
|   |  RamCpp4_TaskbarWidget (WS_CHILD Window)             |   |
|   |  - Parent background capture + Double-buffered GDI   |   |
|   |  - Smooth ClearType typography                       |   |
|   |  - Subclasses Shell_TrayWnd for instant reposition   |   |
|   |  - Right-click context menu (Reload / Edit / Exit)   |   |
|   +-----------------------------------------------------+   |
|   |  TrayNotifyWnd (System Tray Notification Area)       |   |
|   +-----------------------------------------------------+   |
|                                                             |
|   Injected RamCpp4_Widget.dll                               |
|   +-----------------------------------------------------+   |
|   |  Dedicated Background Worker Thread                  |   |
|   |  - CPU: GetSystemTimes delta                         |   |
|   |  - RAM: GlobalMemoryStatusEx                         |   |
|   |  - Net: GetIfTable2 physical adapter auto-discovery  |   |
|   +-----------------------------------------------------+   |
+-------------------------------------------------------------+
```

---

## Project Structure

```
RamCpp4/
├── RamCpp4.slnx                   # Visual Studio 2026 Solution
├── build.bat                      # One-click Release x64 compilation script
├── settings.txt                   # Default configuration file
├── .gitignore
├── README.md
│
├── RamCpp4/                       # Loader Application (EXE)
│   ├── RamCpp4.vcxproj
│   ├── RamCpp4.vcxproj.filters
│   └── src/
│       ├── Main.cpp               # Single instance, CLI, TaskbarCreated watcher
│       ├── Injector.h             # Injection interface
│       └── Injector.cpp           # SetWindowsHookExW injection & immediate unhook
│
└── RamCpp4_Widget/                # Injected DLL (Taskbar Widget)
    ├── RamCpp4_Widget.vcxproj
    ├── RamCpp4_Widget.vcxproj.filters
    └── src/
        ├── DllMain.cpp            # DLL entry, exports RamCpp4_HookProc
        ├── WidgetWindow.h/cpp     # WS_CHILD window, double-buffering, GDI ClearType
        ├── TaskbarManager.h/cpp   # Shell_TrayWnd subclassing, coordinates, theme
        ├── SystemMonitor.h/cpp    # CPU/RAM/Net metrics engine & adapter filtering
        ├── TemplateEngine.h/cpp   # Token parser & tabular space formatter
        └── Config.h/cpp           # UTF-8/UTF-16 settings.txt parser & defaults generator
```

---

## Building

### Requirements
- **Visual Studio 2026** (or Visual Studio 2022 with MSVC `v145` or `v143` toolset).
- **Windows 10 / 11 SDK** (10.0.19041.0 or higher).
- **Target Platform**: `x64` (Explorer on 64-bit Windows is a 64-bit process; the DLL must be x64).

### Compilation
Double-click or run `build.bat` from a terminal:
```cmd
build.bat
```
Or use MSBuild directly:
```cmd
"D:\MyPrograms\VisualStudio\MSBuild\Current\Bin\MSBuild.exe" RamCpp4.slnx /p:Configuration=Release /p:Platform=x64
```
Output binaries will be placed in `x64\Release\`:
- `RamCpp4.exe` (17 KB)
- `RamCpp4_Widget.dll` (100 KB)
- `settings.txt`

---

## Usage

### Running
Simply run `RamCpp4.exe`:
- If not running, it injects `RamCpp4_Widget.dll` into `explorer.exe` and docks the widget next to your system tray.
- If already running, launching `RamCpp4.exe` sends a reload signal to refresh `settings.txt` and exits immediately.

### Command Line Flags
| Flag | Description |
| :--- | :--- |
| *(none)* | Launches the widget and starts the background watcher. |
| `--reload`, `-r` | Signals the running widget to reload `settings.txt`. |
| `--quit`, `-q` | Unloads the widget from Explorer and closes all associated processes. |

### Context Menu (Right-Click)
Right-click the widget in the taskbar to summon the native context menu:
- **Reload settings**: Re-reads `settings.txt` from disk and updates metrics immediately.
- **Edit settings.txt**: Opens `settings.txt` in Notepad.
- **Exit**: Destroys the widget, removes window subclasses, terminates worker threads, unloads `RamCpp4_Widget.dll` from Explorer, and shuts down the loader.

*(Note: Left-clicks and double-clicks are intentionally suppressed to prevent accidental interaction).*

---

## Configuration Reference (`settings.txt`)

| Key | Default | Constraints | Description |
| :--- | :--- | :--- | :--- |
| `UpdateInterval` | `2000` | Min `900` ms | Polling interval for hardware metrics in milliseconds. |
| `TextColor` | `Auto` | `Auto`, `#RRGGBB` | Font color (`Auto` checks Windows taskbar theme). |
| `FontFamily` | `Segoe UI` | Any font name | Font typeface name. |
| `FontSize` | `9` | `6` - `36` pt | Font size in points. |
| `FontBold` | `0` | `0` or `1` | Bold font weight (`1` = Bold). |
| `Alignment` | `Center` | `Left`, `Center`, `Right` | Text alignment. |
| `HorizontalTemplate`| `CPU: {cpu}%\nRAM: {ram_percent}%` | String | Template for horizontal taskbar. |
| `VerticalTemplate`  | `C:{cpu}%\nR:{ram_percent}%` | String | Template for vertical taskbar. |
| `HorizontalWidth`   | `90` | `30` - `400` px | Width of widget on horizontal taskbar. |
| `VerticalHeight`    | `40` | `20` - `300` px | Height of widget (or clamp on horizontal). |
| `PaddingX` / `PaddingY` | `4` / `2` | $\ge 0$ px | Horizontal and vertical text padding. |
| `OffsetX` / `OffsetY` | `0` / `0` | Negative / Positive | Manual fine-tuning offsets in pixels. |
| `EnablePadding`     | `1` | `0` or `1` | Space-padding for uniform number column widths. |
| `NetworkAdapter`    | `Auto` | `Auto`, Name, IPv4 | Network adapter to monitor. |
| `ShowDebugBorder`   | `0` | `0` or `1` | Draws high-contrast pink 1px border for sizing. |

### Available Template Tokens
- `{cpu}`: Total system CPU usage % ($0 - 99$).
- `{ram_percent}`: Physical RAM used %.
- `{ram_free_percent}`: Physical RAM free %.
- `{ram_used_gb}`, `{ram_free_gb}`, `{ram_total_gb}`: RAM in GB ($1\text{ decimal place}$).
- `{net_down}`, `{net_up}`: Network download / upload speed in Mbps ($1\text{ decimal place}$).
- `{net_down_mb}`, `{net_up_mb}`: Network speed in MB/s ($1\text{ decimal place}$).
- `{net_down_int}`, `{net_up_int}`: Network speed in Mbps (integer, rounded).
- `{net_down_mb_int}`, `{net_up_mb_int}`: Network speed in MB/s (integer, rounded).
