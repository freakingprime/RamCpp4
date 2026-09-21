#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include "Config.h"
#include "TaskbarManager.h"
#include "SystemMonitor.h"
#include "TemplateEngine.h"

#define WM_USER_METRICS_UPDATED (WM_USER + 201)
#define WM_USER_REPOSITION       (WM_USER + 202)
#define WM_USER_THEME_CHANGED    (WM_USER + 203)

class WidgetWindow {
public:
    WidgetWindow();
    ~WidgetWindow();

    bool Create(HINSTANCE hInstance, const std::wstring& configPath);
    void Destroy();

    void ReloadConfig();
    void OnTaskbarReposition();
    void OnThemeChanged();
    void OnMetricsUpdated();

    HWND GetHwnd() const { return m_hWnd; }

private:
    HWND m_hWnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    std::wstring m_configPath;

    AppConfig m_config;
    CompiledTemplate m_horizontalTemplate;
    CompiledTemplate m_verticalTemplate;
    SystemMonitor m_monitor;
    TaskbarManager m_taskbarManager;

    HFONT m_hFont = nullptr;
    COLORREF m_currentTextColor = RGB(255, 255, 255);
    wchar_t m_currentText[512] = { 0 };
    bool m_isHorizontal = true;

    // Background worker thread for hardware monitoring
    std::jthread m_workerThread;
    std::atomic<bool> m_workerRunning = false;
    void WorkerLoop(std::stop_token stopToken);

    void RecreateFont();
    void Render(HDC hdc);
    void ShowContextMenu();
    void UnloadAndExit();

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
