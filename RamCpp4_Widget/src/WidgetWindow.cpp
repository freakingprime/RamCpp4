#include "WidgetWindow.h"
#include <shellapi.h>
#include <algorithm>
#include <cmath>

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
    if (m_hStopEvent) {
        SetEvent(m_hStopEvent);
    }
    if (m_hWorkerThread) {
        WaitForSingleObject(m_hWorkerThread, 500);
        CloseHandle(m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }
    if (m_hStopEvent) {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = nullptr;
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

    // Initial metric sample for instant frame-1 rendering
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        SystemMetrics initialMetrics;
        const auto& activeTpl = m_isHorizontal ? m_horizontalTemplate : m_verticalTemplate;
        m_monitor.Update(activeTpl.GetNeededMetricFlags(), initialMetrics);
        activeTpl.Format(initialMetrics, m_currentText, 512, m_config.enablePadding);
    }

    // In-process popup window owned by Shell_TrayWnd to guarantee topmost position over taskbar buttons
    m_hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        L"RamCpp4_TaskbarWidget",
        L"RamCpp4",
        WS_POPUP | WS_VISIBLE,
        rcWidget.left, rcWidget.top, w, h,
        hShellTray ? hShellTray : NULL,
        NULL,
        hInstance,
        this
    );

    if (!m_hWnd) return false;

    m_taskbarManager.Initialize(this);
    RecreateFont();
    RedrawLayered();

    // Start background hardware polling thread
    m_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    m_hWorkerThread = CreateThread(NULL, 0, WorkerThreadThunk, this, 0, NULL);

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

    DWORD quality = (_wcsicmp(m_config.fontQuality.c_str(), L"ClearType") == 0)
        ? CLEARTYPE_QUALITY
        : ANTIALIASED_QUALITY;

    int weight = m_config.fontWeight;
    if (weight <= 0) weight = m_config.fontBold ? FW_BOLD : FW_NORMAL;

    m_hFont = CreateFontW(
        fontHeight,
        0, 0, 0,
        weight,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        quality,
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
    OnMetricsUpdated();
}

void WidgetWindow::OnTaskbarReposition() {
    if (!m_hWnd || !IsWindow(m_hWnd)) return;

    RECT rcWidget = { 0 };
    if (m_taskbarManager.CalculateWidgetRect(m_config, rcWidget, m_isHorizontal)) {
        int w = rcWidget.right - rcWidget.left;
        int h = rcWidget.bottom - rcWidget.top;

        HWND hShellTray = m_taskbarManager.GetShellTrayWnd();
        if (hShellTray && !IsWindowVisible(hShellTray)) {
            ShowWindow(m_hWnd, SW_HIDE);
            return;
        }

        SetWindowPos(
            m_hWnd, HWND_TOPMOST,
            rcWidget.left, rcWidget.top, w, h,
            SWP_NOACTIVATE | SWP_SHOWWINDOW
        );
        RedrawLayered();
    }
}

void WidgetWindow::OnThemeChanged() {
    if (!m_hWnd || !IsWindow(m_hWnd)) return;
    RedrawLayered();
}

void WidgetWindow::OnMetricsUpdated() {
    if (!m_hWnd || !IsWindow(m_hWnd)) return;
    RedrawLayered();
}

DWORD WINAPI WidgetWindow::WorkerThreadThunk(LPVOID lpParam) {
    WidgetWindow* pThis = reinterpret_cast<WidgetWindow*>(lpParam);
    if (pThis) {
        __try {
            pThis->WorkerLoop();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    return 0;
}

void WidgetWindow::WorkerLoop() {
    while (WaitForSingleObject(m_hStopEvent, 0) == WAIT_TIMEOUT) {
        const auto& activeTpl = m_isHorizontal ? m_horizontalTemplate : m_verticalTemplate;
        uint32_t neededFlags = activeTpl.GetNeededMetricFlags();

        SystemMetrics metrics;
        m_monitor.Update(neededFlags, metrics);

        wchar_t formatted[512] = { 0 };
        activeTpl.Format(metrics, formatted, 512, m_config.enablePadding);

        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_textMutex);
            if (wcscmp(m_currentText, formatted) != 0) {
                wcscpy_s(m_currentText, formatted);
                changed = true;
            }
        }

        if (changed && m_hWnd && IsWindow(m_hWnd)) {
            PostMessageW(m_hWnd, WM_USER_METRICS_UPDATED, 0, 0);
        }

        int interval = (std::max)(m_config.updateInterval, 900);
        if (WaitForSingleObject(m_hStopEvent, interval) != WAIT_TIMEOUT) {
            break;
        }
    }
}

void WidgetWindow::RedrawLayered() {
    if (!m_hWnd || !IsWindow(m_hWnd)) return;

    RECT rcClient;
    GetClientRect(m_hWnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;
    if (w <= 0 || h <= 0) return;

    HDC hdcScreen = GetDC(NULL);
    HDC memDC = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    uint32_t* pPixels = reinterpret_cast<uint32_t*>(pBits);
    const size_t totalPixels = (size_t)w * h;

    bool isClearType = (_wcsicmp(m_config.fontQuality.c_str(), L"ClearType") == 0);

    // Resolve target text color
    COLORREF textColor = m_config.isTextColorAuto
        ? m_taskbarManager.GetAutoTextColor()
        : m_config.customTextColor;
    BYTE targetR = GetRValue(textColor);
    BYTE targetG = GetGValue(textColor);
    BYTE targetB = GetBValue(textColor);

    if (isClearType) {
        // ClearType mode (as in RamCpp3): draw directly with target color on transparent baseline
        for (size_t i = 0; i < totalPixels; ++i) {
            pPixels[i] = 0x01000000;
        }
        SetTextColor(memDC, textColor);
    } else {
        // AntiAliased mode: initialize to zero and draw in pure white for exact coverage extraction
        ZeroMemory(pPixels, totalPixels * sizeof(uint32_t));
        SetTextColor(memDC, RGB(255, 255, 255));
    }

    HGDIOBJ oldFont = SelectObject(memDC, m_hFont);
    SetBkMode(memDC, TRANSPARENT);

    RECT rcText = rcClient;
    rcText.left += m_config.paddingX;
    rcText.right -= m_config.paddingX;
    rcText.top += m_config.paddingY;
    rcText.bottom -= m_config.paddingY;

    wchar_t textToDraw[512] = { 0 };
    {
        std::lock_guard<std::mutex> lock(m_textMutex);
        wcscpy_s(textToDraw, m_currentText);
    }

    // Vertical centering
    RECT rcCalc = rcText;
    DrawTextW(memDC, textToDraw, -1, &rcCalc, m_config.alignment | DT_CALCRECT | DT_NOPREFIX);
    int textH = rcCalc.bottom - rcCalc.top;
    int boxH = rcText.bottom - rcText.top;
    if (boxH > textH) {
        rcText.top += (boxH - textH) / 2;
        rcText.bottom = rcText.top + textH;
    }

    DrawTextW(memDC, textToDraw, -1, &rcText, m_config.alignment | DT_NOPREFIX);

    if (isClearType) {
        // Tag modified ClearType pixels with full alpha 255 (RamCpp3 style)
        for (size_t i = 0; i < totalPixels; ++i) {
            uint32_t rgb = pPixels[i] & 0x00FFFFFF;
            if (rgb != 0) {
                pPixels[i] = 0xFF000000 | rgb;
            }
        }
    } else {
        // Alpha thinning curve: eliminates DWM layered window edge swelling to match the clock
        float gamma = m_config.textThinning;
        if (gamma < 0.5f) gamma = 0.5f;
        if (gamma > 3.0f) gamma = 3.0f;

        BYTE lut[256];
        for (int v = 0; v < 256; ++v) {
            float norm = static_cast<float>(v) / 255.0f;
            float thinned = powf(norm, gamma);
            lut[v] = static_cast<BYTE>(roundf(thinned * 255.0f));
        }

        for (size_t i = 0; i < totalPixels; ++i) {
            BYTE rawA = static_cast<BYTE>(pPixels[i] & 0xFF);
            if (rawA > 0) {
                BYTE a = lut[rawA];
                if (a > 0) {
                    BYTE pr = static_cast<BYTE>((targetR * a + 127) / 255);
                    BYTE pg = static_cast<BYTE>((targetG * a + 127) / 255);
                    BYTE pb = static_cast<BYTE>((targetB * a + 127) / 255);
                    pPixels[i] = (static_cast<uint32_t>(a) << 24) |
                                 (static_cast<uint32_t>(pr) << 16) |
                                 (static_cast<uint32_t>(pg) << 8) |
                                 pb;
                } else {
                    pPixels[i] = 0x01000000;
                }
            } else {
                pPixels[i] = 0x01000000;
            }
        }
    }

    // High-contrast debug border if requested
    if (m_config.showDebugBorder) {
        BYTE bR = GetRValue(m_config.debugBorderColor);
        BYTE bG = GetGValue(m_config.debugBorderColor);
        BYTE bB = GetBValue(m_config.debugBorderColor);
        uint32_t borderPixel = 0xFF000000 | (static_cast<uint32_t>(bR) << 16) | (static_cast<uint32_t>(bG) << 8) | bB;

        for (int x = 0; x < w; ++x) {
            pPixels[x] = borderPixel;
            pPixels[(h - 1) * w + x] = borderPixel;
        }
        for (int y = 0; y < h; ++y) {
            pPixels[y * w] = borderPixel;
            pPixels[y * w + (w - 1)] = borderPixel;
        }
    }

    RECT rcWindow;
    GetWindowRect(m_hWnd, &rcWindow);
    POINT ptDst = { rcWindow.left, rcWindow.top };
    POINT ptSrc = { 0, 0 };
    SIZE size = { w, h };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    UpdateLayeredWindow(m_hWnd, hdcScreen, &ptDst, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, hdcScreen);
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
        case WM_USER_METRICS_UPDATED:
            RedrawLayered();
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
            }
            else if (cmdId == IDM_EDIT_SETTINGS) {
                // Determine directory containing settings.txt
                std::wstring configDir = m_configPath;
                size_t lastSlash = configDir.find_last_of(L"\\/");
                LPCWSTR lpDir = (lastSlash != std::wstring::npos) ? (configDir.substr(0, lastSlash).c_str()) : NULL;
                // Launch with system default editor (e.g. Notepad++)
                HINSTANCE hInst = ShellExecuteW(NULL, L"open", m_configPath.c_str(), NULL, lpDir, SW_SHOWNORMAL);
                if ((INT_PTR)hInst <= 32) {
                    // Fallback to notepad.exe if default association fails
                    ShellExecuteW(NULL, L"open", L"notepad.exe", m_configPath.c_str(), lpDir, SW_SHOWNORMAL);
                }
            }
            else if (cmdId == IDM_EXIT) {
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
