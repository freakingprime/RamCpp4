#include "TaskbarManager.h"
#include "WidgetWindow.h"
#include <shellapi.h>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")

constexpr UINT_PTR TRAY_SUBCLASS_ID = 4001;

TaskbarManager::TaskbarManager() = default;

TaskbarManager::~TaskbarManager() {
    Uninitialize();
}

bool TaskbarManager::RefreshHandles() {
    m_hShellTrayWnd = FindWindowW(L"Shell_TrayWnd", NULL);
    if (!m_hShellTrayWnd || !IsWindow(m_hShellTrayWnd)) {
        m_hShellTrayWnd = nullptr;
        m_hTrayNotifyWnd = nullptr;
        return false;
    }

    m_hTrayNotifyWnd = FindWindowExW(m_hShellTrayWnd, NULL, L"TrayNotifyWnd", NULL);
    if (!m_hTrayNotifyWnd || !IsWindow(m_hTrayNotifyWnd)) {
        m_hTrayNotifyWnd = nullptr;
        return false;
    }

    return true;
}

bool TaskbarManager::Initialize(WidgetWindow* pWidget) {
    m_pWidget = pWidget;
    if (!RefreshHandles()) return false;

    if (!m_subclassed && m_hShellTrayWnd) {
        m_subclassed = SetWindowSubclass(
            m_hShellTrayWnd,
            ShellTraySubclassProc,
            TRAY_SUBCLASS_ID,
            reinterpret_cast<DWORD_PTR>(m_pWidget)
        );
    }

    return true;
}

void TaskbarManager::Uninitialize() {
    if (m_subclassed && m_hShellTrayWnd && IsWindow(m_hShellTrayWnd)) {
        RemoveWindowSubclass(m_hShellTrayWnd, ShellTraySubclassProc, TRAY_SUBCLASS_ID);
        m_subclassed = false;
    }
}

LRESULT CALLBACK TaskbarManager::ShellTraySubclassProc(
    HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {

    WidgetWindow* pWidget = reinterpret_cast<WidgetWindow*>(dwRefData);

    switch (uMsg) {
        case WM_WINDOWPOSCHANGING:
        case WM_WINDOWPOSCHANGED:
        case WM_SIZE:
            if (pWidget) {
                pWidget->OnTaskbarReposition();
            }
            break;

        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
            if (pWidget) {
                pWidget->OnThemeChanged();
            }
            break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

bool TaskbarManager::CalculateWidgetRect(const AppConfig& config, RECT& outRect, bool& outIsHorizontal) {
    if (!RefreshHandles()) return false;

    RECT rcTrayScreen = { 0 };
    if (!GetWindowRect(m_hTrayNotifyWnd, &rcTrayScreen)) return false;

    RECT rcShellClient = { 0 };
    if (!GetClientRect(m_hShellTrayWnd, &rcShellClient)) return false;

    int shellWidth = rcShellClient.right - rcShellClient.left;
    int shellHeight = rcShellClient.bottom - rcShellClient.top;
    if (shellWidth <= 0 || shellHeight <= 0) return false;

    // Convert TrayNotifyWnd top-left to Shell_TrayWnd client coordinates
    POINT ptTrayClient = { rcTrayScreen.left, rcTrayScreen.top };
    ScreenToClient(m_hShellTrayWnd, &ptTrayClient);

    // Dimension-based orientation: width > height is horizontal, height > width is vertical
    outIsHorizontal = (shellWidth > shellHeight);

    int w = 0, h = 0, x = 0, y = 0;

    if (outIsHorizontal) {
        w = config.horizontalWidth;
        h = config.verticalHeight;
        if (h > shellHeight || h <= 0) h = shellHeight;

        x = ptTrayClient.x - w + config.offsetX;
        y = (shellHeight - h) / 2 + config.offsetY;
    } else {
        w = shellWidth;
        h = config.verticalHeight;
        if (h <= 0) h = 40;

        x = (shellWidth - w) / 2 + config.offsetX;
        y = ptTrayClient.y - h + config.offsetY;
    }

    // Clamp within Shell_TrayWnd client area
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > shellWidth) x = shellWidth - w;
    if (y + h > shellHeight) y = shellHeight - h;

    outRect.left = x;
    outRect.top = y;
    outRect.right = x + w;
    outRect.bottom = y + h;

    return true;
}

COLORREF TaskbarManager::GetAutoTextColor() const {
    DWORD lightTheme = 0;
    DWORD size = sizeof(lightTheme);
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {

        RegQueryValueExW(hKey, L"SystemUsesLightTheme", NULL, NULL, reinterpret_cast<LPBYTE>(&lightTheme), &size);
        RegCloseKey(hKey);
    }

    // 1 = Light taskbar (dark text #181818), 0 = Dark taskbar (white text #FFFFFF)
    return (lightTheme == 1) ? RGB(24, 24, 24) : RGB(255, 255, 255);
}
