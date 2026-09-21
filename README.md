# RamCpp4: In-Process Windows Taskbar System Monitor

**RamCpp4** is a lightweight, real-time hardware monitor widget designed for **Windows 10 and Windows 11**. It docks directly into the Windows Taskbar immediately to the left of the system tray (`TrayNotifyWnd`) or above it on vertical taskbars, displaying customizable real-time CPU, RAM, and Network metrics matching native taskbar typography.

Unlike external overlay widgets that float on top of the taskbar and can suffer from z-order conflicts, animation latency, and auto-hide desynchronization, **RamCpp4 uses native in-process DLL injection into `explorer.exe`**. The widget window is an **owned layered window** (`WS_POPUP` owned by `Shell_TrayWnd`) utilizing true 32-bit hardware-composited premultiplied alpha rendering (`UpdateLayeredWindow`), delivering perfect taskbar integration, automatic synchronization with taskbar auto-hide/fullscreen applications, zero z-order fighting, and flawless transparent typography without edge fringing.

---

## Key Features

- **In-Process Taskbar Integration**: Injected into `explorer.exe` using `SetWindowsHookExW(WH_GETMESSAGE)` on Explorer's tray thread, unhooking immediately after initialization to ensure zero ongoing hook overhead.
- **Owned Layered Window Architecture**: Owned directly by `Shell_TrayWnd` with `WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`. Automatically synchronizes position, auto-hide, and visibility with the Windows taskbar, eliminating z-order conflicts and child-window clipping.
- **True 32-bit Premultiplied Alpha**: Direct per-pixel alpha rendering via `UpdateLayeredWindow` with `AC_SRC_ALPHA`. Completely eliminates background snapshotting, screen tearing, and color-fringing across transparent, Mica, and Acrylic taskbars.
- **Clock-Matched Typography & Anti-Swelling**:
  - **Rendering Engines**: Supports smooth grayscale `AntiAliased` or subpixel LCD `ClearType` rendering.
  - **Font Weight Precision**: Supports `FontWeight` presets (`Thin`, `ExtraLight`, `Light`, `SemiLight`, `Normal`, `Medium`, `SemiBold`, `Bold` or `100`–`900`), allowing exact visual alignment with Windows 10/11 taskbar clock numerals.
  - **Perceptual Alpha Thinning (`TextThinning`)**: Uses a gamma lookup table to counteract Desktop Window Manager (DWM) alpha edge swelling and font dilation on layered windows.
- **Ultra-Low CPU & Battery Optimization**:
  - **Persistent GDI Memory DC & 32-bit DIB Sections**: Reallocated only when widget dimensions change, eliminating per-tick allocation churn.
  - **Power & Session Awareness**: Listens for `GUID_CONSOLE_DISPLAY_STATE` and `WTS_SESSION_LOCK` to halt metric polling and rendering completely when the display turns off or the workstation is locked.
  - **Zero-Allocation Formatting**: Precomputed 2-digit lookup tables for instant CPU and RAM percentage formatting without string formatting overhead.
  - **Network Adapter Caching**: Caches physical network interfaces with throttled re-enumeration to minimize kernel querying.
  - **Thread Isolation & Lower Priority**: All hardware polling runs on a dedicated background worker thread at `THREAD_PRIORITY_BELOW_NORMAL`.
- **Zero-UAC & 100% Portable**: Requires **no Administrator privileges** and no installation. Statically compiled (`/MT`) with zero runtime dependencies.
- **Smart Editor Integration**: Right-click "Edit settings.txt" automatically launches **Notepad++** if installed (`Program Files` or `Program Files (x86)`), seamlessly falling back to standard `notepad.exe`.
- **Dynamic Orientation & Clamping**: Detects horizontal vs. vertical taskbar orientations dynamically. Widget coordinates are strictly clamped within the taskbar monitor screen boundaries (`rcMonitor`).
- **Resilient Lifecycle**: The loader process acts as a zero-CPU background watcher holding `RamCpp4_SingleInstance_Mutex`. It listens for `TaskbarCreated` to automatically re-inject if Explorer crashes or restarts.
- **Clean Unload & Ejection**: Exiting from the context menu or via `RamCpp4.exe --quit` safely destroys the layered window, restores taskbar subclasses, terminates worker threads, unloads `RamCpp4_Widget.dll` from Explorer, and closes the watcher cleanly.
- **Theme Auto-Detection**: Dynamically checks Windows dark/light theme (`SystemUsesLightTheme`) and live-reacts to `WM_THEMECHANGED` and `WM_SETTINGCHANGE` with cached registry reads.
- **Customizable Templating**: Supports token substitution (`{cpu}`, `{ram_free_gb}`, `{net_down_int}`, etc.), token aliases, Unicode arrows/symbols, tabular alignment padding, custom fonts, sizes, and colors.
- **Self-Contained Release Packaging**: Release builds automatically export portable, ready-to-run binaries into `ReleaseFiles/`.

---

## Architecture Overview

```
+-----------------------------------------------------------------------------+
|                            RamCpp4.exe (Loader)                             |
|  - Mutex: RamCpp4_SingleInstance_Mutex                                      |
|  - Injects RamCpp4_Widget.dll via SetWindowsHookExW                         |
|  - Immediate Unhook once widget window is active                            |
|  - Background Watcher for TaskbarCreated message                            |
+--------------------------------------+--------------------------------------+
                                       | Injects via Hook
                                       v
+-----------------------------------------------------------------------------+
|                                explorer.exe                                 |
|                                                                             |
|   Shell_TrayWnd (Taskbar)                                                   |
|   +---------------------------------------------------------------------+   |
|   |  RamCpp4_TaskbarWidget (Owned Layered Window)                       |   |
|   |  - Style: WS_POPUP | WS_VISIBLE                                     |   |
|   |  - ExStyle: WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE         |   |
|   |  - Persistent 32-bit DIB Section + Memory DC                        |   |
|   |  - True Premultiplied Alpha via UpdateLayeredWindow                 |   |
|   |  - Anti-Swelling Gamma LUT (TextThinning) + AntiAliased/ClearType   |   |
|   |  - Power & Session Awareness (Sleep / Lock pause)                   |   |
|   |  - Subclasses Shell_TrayWnd for instant reposition / auto-hide      |   |
|   |  - Right-click context menu (Reload / Edit [Notepad++] / Exit)      |   |
|   +---------------------------------------------------------------------+   |
|   |  TrayNotifyWnd (System Tray Notification Area)                       |   |
|   +---------------------------------------------------------------------+   |
|                                                                             |
|   Injected RamCpp4_Widget.dll                                               |
|   +---------------------------------------------------------------------+   |
|   |  Dedicated Background Worker Thread (THREAD_PRIORITY_BELOW_NORMAL)  |   |
|   |  - CPU: GetSystemTimes delta                                        |   |
|   |  - RAM: GlobalMemoryStatusEx                                        |   |
|   |  - Net: GetIfTable2 physical adapter auto-discovery (cached)        |   |
|   |  - Zero-allocation 2-digit lookup tables for instant formatting     |   |
|   +---------------------------------------------------------------------+   |
+-----------------------------------------------------------------------------+
```

---

## Project Structure

```
RamCpp4/
├── RamCpp4.slnx                   # Visual Studio 2026 Solution
├── build.bat                      # One-click Release x64 compilation script (/MT)
├── settings.txt                   # Default configuration file
├── .gitignore
├── README.md
│
├── ReleaseFiles/                  # Automated portable distribution directory
│   ├── RamCpp4.exe                # Standalone loader & watcher (~108 KB)
│   ├── RamCpp4_Widget.dll         # Injected taskbar widget DLL (~357 KB)
│   └── settings.txt               # User settings file
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
        ├── WidgetWindow.h/cpp     # Owned layered window, persistent DIB, gamma LUT, power awareness
        ├── TaskbarManager.h/cpp   # Shell_TrayWnd subclassing, coordinates, cached theme
        ├── SystemMonitor.h/cpp    # CPU/RAM/Net metrics engine & adapter caching
        ├── TemplateEngine.h/cpp   # Token parser, 2-digit lookup tables & tabular space formatter
        └── Config.h/cpp           # UTF-8/UTF-16 settings parser & robust default generator
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

### Static Runtime & Distribution
The Release configuration uses `/MT` (Multi-Threaded static CRT), producing completely self-contained binaries that do not require any Visual C++ Redistributables to be installed.

Build outputs are automatically populated in:
- `x64\Release\`
- `ReleaseFiles\` (portable bundle containing `RamCpp4.exe`, `RamCpp4_Widget.dll`, and `settings.txt`)

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
- **Edit settings.txt**: Opens `settings.txt` in **Notepad++** (if installed in 64-bit or 32-bit Program Files) or standard Notepad.
- **Exit**: Destroys the widget window, removes taskbar subclasses, terminates worker threads, unloads `RamCpp4_Widget.dll` from Explorer, and shuts down the loader.

*(Note: Left-clicks and double-clicks are intentionally suppressed to prevent accidental interaction).*

---

## Configuration Reference (`settings.txt`)

| Key | Default | Constraints | Description |
| :--- | :--- | :--- | :--- |
| `UpdateInterval` | `1000` | Min `900` ms | Polling and UI refresh interval in milliseconds. |
| `TextColor` | `Auto` | `Auto`, `#RRGGBB`, `RRGGBB` | Font color (`Auto` automatically matches dark/light taskbar). |
| `FontFamily` | `Consolas` | Any font name | Font typeface name (e.g. `Consolas`, `Segoe UI`, `Cascadia Code`). |
| `FontSize` | `11` | `6` - `36` pt | Font size in points. |
| `FontBold` | `0` | `0` or `1` | Bold font weight (`1` = Bold, overrides `FontWeight` to 700). |
| `FontWeight` | `Normal` | Preset or `100` - `900` | Precise stroke weight (`Thin`, `ExtraLight`, `Light`, `SemiLight`, `Normal`, `Medium`, `SemiBold`, `Bold`). |
| `TextThinning` | `1.4` | `0.5` - `3.0` | Perceptual alpha thinning exponent to eliminate DWM layered window edge swelling. |
| `FontQuality` | `AntiAliased` | `AntiAliased`, `ClearType` | Font smoothing engine (`AntiAliased` for smooth text, `ClearType` for subpixel stems). |
| `Alignment` | `Center` | `Left`, `Center`, `Right` | Text alignment within the widget boundary. |
| `NetworkAdapter` | `Wi-Fi` | `Auto`, Name/Desc, IP | Network adapter to monitor (`Auto` sums across all active physical adapters). |
| `HorizontalTemplate`| *See below* | String (`\n` for newline) | Display template for horizontal taskbars. |
| `VerticalTemplate` | *See below* | String (`\n` for newline) | Display template for vertical taskbars. |
| `HorizontalWidth` | `220` | `30` - `400` px | Width of widget on horizontal taskbars. |
| `VerticalHeight` | `120` | `20` - `300` px | Height of widget on vertical taskbars. |
| `PaddingX` / `PaddingY` | `1` / `1` | $\ge 0$ px | Horizontal and vertical inner padding in pixels. |
| `OffsetX` / `OffsetY` | `0` / `0` | Negative / Positive | Manual dock placement fine-tuning offsets in pixels. |
| `EnablePadding` | `1` | `0` or `1` | Space-padding for single-digit numbers for uniform column widths. |
| `ShowDebugBorder` | `0` | `0` or `1` | Draws a 1-pixel high-contrast border for sizing and alignment diagnosis. |

### Default Templates
- **Horizontal**:
  ```ini
  HorizontalTemplate=Free: {ram_free_gb} GB ({ram_free_percent}%) 🠉{net_up_int}\nCPU : {cpu}   %        🠋{net_down_int}
  ```
- **Vertical**:
  ```ini
  VerticalTemplate=C:{cpu}%\nFree:\n{ram_free_gb} GB\n({ram_free_percent}%)\n🠉{net_up_int}\n🠋{net_down_int}
  ```

---

## Available Template Tokens

### CPU
- `{cpu}`: System CPU usage % ($0 - 99$, fixed 2-digit width via space padding if `EnablePadding=1`).

### RAM
- `{ram_percent}`: Physical RAM used % ($0 - 99$).
- `{ram_free_percent}`: Physical RAM free % ($0 - 99$).
- `{ram_used_gb}`: Physical RAM used in GB (4 characters, e.g. ` 7.8`).
- `{ram_free_gb}`: Physical RAM free in GB (4 characters, e.g. ` 9.5`).
- `{ram_total_gb}`: Total physical RAM in GB (e.g. `16.0`).

### Network (Mbps - Megabits per second)
- `{net_down}`: Download speed in Mbps with 1 decimal place (e.g. `15.2`).
- `{net_up}`: Upload speed in Mbps with 1 decimal place (e.g. ` 2.1`).
- `{net_down_int}`: Download speed whole integer in Mbps (e.g. `15`). Aliases: `{net_down_0}`, `{net_down_round}`.
- `{net_up_int}`: Upload speed whole integer in Mbps (e.g. `2`). Aliases: `{net_up_0}`, `{net_up_round}`.

### Network (MB/s - Megabytes per second)
- `{net_down_mb}`: Download speed in MB/s with 1 decimal place (e.g. `1.9`).
- `{net_up_mb}`: Upload speed in MB/s with 1 decimal place (e.g. `0.3`).
- `{net_down_mb_int}`: Download speed whole integer in MB/s (e.g. `2`). Aliases: `{net_down_mb_0}`.
- `{net_up_mb_int}`: Upload speed whole integer in MB/s (e.g. `1`). Aliases: `{net_up_mb_0}`.

### Unicode Symbols & Arrows
Literal Unicode characters and escape sequences are supported in templates:
- **Barb Arrows**: `🠉`, `🠋`, `🠈`, `🠊`, `🡩`, `🡫`, `🡰`, `🡲`
- **Standard Arrows**: `↑`, `↓`, `←`, `→`, `▲`, `▼`
- **Arrowheads**: `➤`, `⮜`, `⮞`, `⮝`, `⮟`
- **Escapes**: `\u2191` (`↑`), `\u2193` (`↓`)
