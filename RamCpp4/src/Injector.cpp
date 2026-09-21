#include "Injector.h"

#define IDM_RELOAD 1001
#define IDM_EXIT   1003

namespace Injector {

HWND FindWidgetWindow() {
    HWND hWnd = FindWindowW(L"RamCpp4_TaskbarWidget", NULL);
    if (!hWnd) {
        HWND hTray = FindWindowW(L"Shell_TrayWnd", NULL);
        if (hTray) {
            hWnd = FindWindowExW(hTray, NULL, L"RamCpp4_TaskbarWidget", NULL);
        }
    }
    return hWnd;
}

bool IsWidgetRunning() {
    return (FindWidgetWindow() != NULL);
}

void SignalExit() {
    HWND hWidget = FindWidgetWindow();
    if (hWidget) {
        PostMessageW(hWidget, WM_COMMAND, MAKEWPARAM(IDM_EXIT, 0), 0);
    }
}

void SignalReload() {
    HWND hWidget = FindWidgetWindow();
    if (hWidget) {
        PostMessageW(hWidget, WM_COMMAND, MAKEWPARAM(IDM_RELOAD, 0), 0);
    }
}

bool Inject(const std::wstring& dllPath) {
    // If widget is already running, signal reload and return
    if (IsWidgetRunning()) {
        SignalReload();
        return true;
    }

    // Locate primary taskbar window Shell_TrayWnd
    HWND hTray = nullptr;
    for (int retry = 0; retry < 20; ++retry) {
        hTray = FindWindowW(L"Shell_TrayWnd", NULL);
        if (hTray && IsWindow(hTray)) break;
        Sleep(250);
    }

    if (!hTray) {
        return false;
    }

    DWORD dwExplorerPid = 0;
    DWORD dwExplorerThreadId = GetWindowThreadProcessId(hTray, &dwExplorerPid);
    if (dwExplorerThreadId == 0) {
        return false;
    }

    // Load library in loader to retrieve hook procedure pointer
    HMODULE hDll = LoadLibraryW(dllPath.c_str());
    if (!hDll) {
        return false;
    }

    HOOKPROC pfnHook = reinterpret_cast<HOOKPROC>(GetProcAddress(hDll, "RamCpp4_HookProc"));
    if (!pfnHook) {
        FreeLibrary(hDll);
        return false;
    }

    // Install hook onto Explorer's tray thread
    HHOOK hHook = SetWindowsHookExW(WH_GETMESSAGE, pfnHook, hDll, dwExplorerThreadId);
    if (!hHook) {
        FreeLibrary(hDll);
        return false;
    }

    // Post WM_NULL to Shell_TrayWnd to trigger message processing in Explorer
    PostMessageW(hTray, WM_NULL, 0, 0);

    // Wait for the widget window to appear in Explorer
    for (int i = 0; i < 50; ++i) {
        Sleep(50);
        if (IsWidgetRunning()) break;
    }

    // Immediately unhook to eliminate message hook overhead
    UnhookWindowsHookEx(hHook);
    FreeLibrary(hDll);

    return IsWidgetRunning();
}

} // namespace Injector
