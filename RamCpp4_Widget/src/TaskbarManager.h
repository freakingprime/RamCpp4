#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include "Config.h"

class WidgetWindow;

class TaskbarManager {
public:
    TaskbarManager();
    ~TaskbarManager();

    bool Initialize(WidgetWindow* pWidget);
    void Uninitialize();

    bool RefreshHandles();
    bool CalculateWidgetRect(const AppConfig& config, RECT& outRect, bool& outIsHorizontal);
    COLORREF GetAutoTextColor() const;

    HWND GetShellTrayWnd() const { return m_hShellTrayWnd; }
    HWND GetTrayNotifyWnd() const { return m_hTrayNotifyWnd; }

private:
    HWND m_hShellTrayWnd = nullptr;
    HWND m_hTrayNotifyWnd = nullptr;
    WidgetWindow* m_pWidget = nullptr;
    bool m_subclassed = false;

    static LRESULT CALLBACK ShellTraySubclassProc(
        HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
        UINT_PTR uIdSubclass, DWORD_PTR dwRefData
    );
};
