#include "WidgetWindow.h"
#include <shellapi.h>
#include <algorithm>
#include <chrono>

#define IDM_RELOAD        1001
#define IDM_EDIT_SETTINGS 1002
#define IDM_EXIT          1003

extern HINSTANCE g_hDllInstance;

namespace {
    WidgetWindow* g_pWidgetInstance = nullptr;
}

WidgetWindow::WidgetWindow() {
    g_pWidgetInstance = this;
}

WidgetWindow::~WidgetWindow() {
    Destroy();
    if (g_pWidgetInstance == this) {
        g_pWidgetInstance = nullptr;
    }
}

void WidgetWindow::Destroy() {
    m_workerRunning = false;
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
    }

    m_taskbarManager.Uninitialize();

    if (m_hWnd && IsWindow(m_hWnd)) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }

    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
}

bool WidgetWindow::Create(HINSTANCE hInstance, const std::wstring& configPath) {
    m_hInstance = hInstance;
    m_configPath = configPath;

    LoadAppConfig(m_configPath, m_config);
    m_horizontalTemplate.Compile(m_config.horizontalTemplate);
    m_verticalTemplate.Compile(m_config.verticalTemplate);
    m_monitor.SetConfiguredAdapter(m_config.networkAdapter);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WidgetWindow::WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"RamCpp4_TaskbarWidget";
    RegisterClassExW(&wc);

    if (!m_taskbarManager.RefreshHandles()) {
        return false;
    }

    RECT rcWidget = { 0 };
    m_taskbarManager.CalculateWidgetRect(m_config, rcWidget, m_isHorizontal);

    int w = rcWidget.right - rcWidget.left;
    int h = rcWidget.bottom - rcWidget.top;
    HWND hShellTray = m_taskbarManager.GetShellTrayWnd();

    // In-process child window of Shell_TrayWnd
    m_hWnd = CreateWindowExW(
        0,
        L"RamCpp4_TaskbarWidget",
        L"RamCpp4_Widget",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        rcWidget.left, rcWidget.top, w, h,
        hShellTray,
        NULL,
        hInstance,
        this
    );

    if (!m_hWnd) return false;

    m_taskbarManager.Initialize(this);
    RecreateFont();

    // Start background hardware polling thread
    m_workerRunning = true;
    m_workerThread = std::jthread([this](std::stop_token st) {
        WorkerLoop(st);
    });

    return true;
}

void WidgetWindow::RecreateFont() {
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }

    HDC hdc = GetDC(m_hWnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if (hdc) ReleaseDC(m_hWnd, hdc);

    int fontHeight = -MulDiv(m_config.fontSize, dpi, 72);

    m_hFont = CreateFontW(
        fontHeight,
        0, 0, 0,
        m_config.fontBold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        m_config.fontFamily.c_str()
    );
}

void WidgetWindow::ReloadConfig() {
    LoadAppConfig(m_configPath, m_config);
    m_horizontalTemplate.Compile(m_config.horizontalTemplate);
    m_verticalTemplate.Compile(m_config.verticalTemplate);
    m_monitor.SetConfiguredAdapter(m_config.networkAdapter, true);

    RecreateFont();
    OnTaskbarReposition();

    // Trigger immediate update
    OnMetricsUpdated();
}

void WidgetWindow::OnTaskbarReposition() {
    if (!m_hWnd || !IsWindow(m_hWnd)) return;

    RECT rcWidget = { 0 };
    if (m_taskbarManager.CalculateWidgetRect(m_config, rcWidget, m_isHorizontal)) {
        int w = rcWidget.right - rcWidget.left;
        int h = rcWidget.bottom - rcWidget.top;
        SetWindowPos(
            m_hWnd, NULL,
            rcWidget.left, rcWidget.top, w, h,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW
        );
        InvalidateRect(m_hWnd, NULL, FALSE);
    }
}

void WidgetWindow::OnThemeChanged() {
    if (!m_hWnd || !IsWindow(m_hWnd)) return;
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void WidgetWindow::OnMetricsUpdated() {
    if (!m_hWnd || !IsWindow(m_hWnd)) return;
    InvalidateRect(m_hWnd, NULL, FALSE);
}

void WidgetWindow::WorkerLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested() && m_workerRunning) {
        const auto& activeTpl = m_isHorizontal ? m_horizontalTemplate : m_verticalTemplate;
        uint32_t neededFlags = activeTpl.GetNeededMetricFlags();

        SystemMetrics metrics;
        m_monitor.Update(neededFlags, metrics);

        wchar_t formatted[512] = { 0 };
        activeTpl.Format(metrics, formatted, 512, m_config.enablePadding);

        if (wcscmp(m_currentText, formatted) != 0) {
            wcscpy_s(m_currentText, formatted);
            if (m_hWnd && IsWindow(m_hWnd)) {
                PostMessageW(m_hWnd, WM_USER_METRICS_UPDATED, 0, 0);
            }
        }

        // Sleep interruptibly up to updateInterval ms in 100ms slices
        int remainingMs = (std::max)(m_config.updateInterval, 900);
        while (remainingMs > 0 && !stopToken.stop_requested() && m_workerRunning) {
            int sleepTime = (std::min)(remainingMs, 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
            remainingMs -= sleepTime;
        }
    }
}

void WidgetWindow::Render(HDC hdc) {
    if (!m_hWnd) return;

    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;
    if (w <= 0 || h <= 0) return;

    // Double-buffered GDI memory DC
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // 1. Snapshot taskbar background beneath widget coordinates
    POINT pt = { 0, 0 };
    MapWindowPoints(m_hWnd, GetParent(m_hWnd), &pt, 1);

    HDC parentDC = GetDC(GetParent(m_hWnd));
    if (parentDC) {
        BitBlt(memDC, 0, 0, w, h, parentDC, pt.x, pt.y, SRCCOPY);
        ReleaseDC(GetParent(m_hWnd), parentDC);
    } else {
        COLORREF bgCol = (m_taskbarManager.GetAutoTextColor() == RGB(255, 255, 255))
            ? RGB(31, 31, 31) : RGB(243, 243, 243);
        HBRUSH hBr = CreateSolidBrush(bgCol);
        FillRect(memDC, &rcClient, hBr);
        DeleteObject(hBr);
    }

    // 2. Set transparent background mode for ClearType text
    SetBkMode(memDC, TRANSPARENT);
    COLORREF textColor = m_config.isTextColorAuto
        ? m_taskbarManager.GetAutoTextColor()
        : m_config.customTextColor;
    SetTextColor(memDC, textColor);
    HGDIOBJ oldFont = SelectObject(memDC, m_hFont);

    // 3. Vertical centering inside padding
    RECT rcText = rcClient;
    rcText.left += m_config.paddingX;
    rcText.right -= m_config.paddingX;
    rcText.top += m_config.paddingY;
    rcText.bottom -= m_config.paddingY;

    RECT rcCalc = rcText;
    DrawTextW(memDC, m_currentText, -1, &rcCalc, m_config.alignment | DT_WORDBREAK | DT_CALCRECT);
    int textH = rcCalc.bottom - rcCalc.top;
    int availH = rcText.bottom - rcText.top;
    if (availH > textH) {
        rcText.top += (availH - textH) / 2;
        rcText.bottom = rcText.top + textH;
    }

    // 4. Draw smooth ClearType text
    DrawTextW(memDC, m_currentText, -1, &rcText, m_config.alignment | DT_WORDBREAK);

    // 5. Optional debug border
    if (m_config.showDebugBorder) {
        HBRUSH hBorder = CreateSolidBrush(m_config.debugBorderColor);
        FrameRect(memDC, &rcClient, hBorder);
        DeleteObject(hBorder);
    }

    // 6. Blit completed buffer onto screen
    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void WidgetWindow::ShowContextMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (hMenu) {
        AppendMenuW(hMenu, MF_STRING, IDM_RELOAD, L"Reload settings");
        AppendMenuW(hMenu, MF_STRING, IDM_EDIT_SETTINGS, L"Edit settings.txt");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

        SetForegroundWindow(m_hWnd);
        TrackPopupMenuEx(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, m_hWnd, NULL);
        PostMessageW(m_hWnd, WM_NULL, 0, 0);
        DestroyMenu(hMenu);
    }
}

void WidgetWindow::UnloadAndExit() {
    // Notify host loader watcher to close
    HWND hHost = FindWindowW(L"RamCpp4_Host", NULL);
    if (hHost) {
        PostMessageW(hHost, WM_CLOSE, 0, 0);
    }

    Destroy();

    // Eject DLL cleanly from explorer.exe on a helper thread
    if (g_hDllInstance) {
        CloseHandle(CreateThread(NULL, 0, [](LPVOID lpParam) -> DWORD {
            Sleep(150);
            FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(lpParam), 0);
        }, g_hDllInstance, 0, NULL));
    }
}

LRESULT CALLBACK WidgetWindow::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    WidgetWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCS = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<WidgetWindow*>(pCS->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<WidgetWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT WidgetWindow::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            Render(hdc);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            // Prevent flicker by returning 1
            return 1;

        case WM_USER_METRICS_UPDATED:
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;

        case WM_USER_REPOSITION:
            OnTaskbarReposition();
            return 0;

        case WM_USER_THEME_CHANGED:
            OnThemeChanged();
            return 0;

        // User preference: disable all left click
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            return 0;

        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowContextMenu();
            return 0;

        case WM_COMMAND: {
            WORD cmdId = LOWORD(wParam);
            if (cmdId == IDM_RELOAD) {
                ReloadConfig();
            } else if (cmdId == IDM_EDIT_SETTINGS) {
                ShellExecuteW(NULL, L"open", L"notepad.exe", m_configPath.c_str(), NULL, SW_SHOW);
            } else if (cmdId == IDM_EXIT) {
                UnloadAndExit();
            }
            return 0;
        }

        case WM_DESTROY:
            m_hWnd = nullptr;
            return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}
