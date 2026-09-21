#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

namespace Injector {
    // Injects RamCpp4_Widget.dll into explorer.exe using SetWindowsHookExW on Shell_TrayWnd's thread,
    // and unhooks immediately after the widget window is created.
    bool Inject(const std::wstring& dllPath);

    // Checks if the widget window currently exists inside Explorer
    bool IsWidgetRunning();

    // Sends exit signal to the widget inside Explorer
    void SignalExit();

    // Sends reload signal to the widget inside Explorer
    void SignalReload();
}
