#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <mutex>
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
    wchar_t m_currentText[512] = { 0 };
    std::mutex m_textMutex;
    bool m_isHorizontal = true;

    // Background worker thread for hardware monitoring (isolated from loader lock)
    HANDLE m_hWorkerThread = nullptr;
    HANDLE m_hStopEvent = nullptr;
    static DWORD WINAPI WorkerThreadThunk(LPVOID lpParam);
    void WorkerLoop();

    void RecreateFont();
    void RedrawLayered();
    void ShowContextMenu();
    void UnloadAndExit();

    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
