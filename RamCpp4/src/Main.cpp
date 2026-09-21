#include <windows.h>
#include <string>
#include "Injector.h"

namespace {
    UINT g_taskbarCreatedMsg = 0;
    std::wstring g_dllPath;

    LRESULT CALLBACK HostWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == g_taskbarCreatedMsg && g_taskbarCreatedMsg != 0) {
            // Explorer restarted - wait briefly for taskbar creation to stabilize, then re-inject
            Sleep(800);
            Injector::Inject(g_dllPath);
            return 0;
        }

        switch (uMsg) {
            case WM_COMMAND: {
                WORD cmd = LOWORD(wParam);
                if (cmd == 1001) {
                    Injector::SignalReload();
                } else if (cmd == 1003) {
                    Injector::SignalExit();
                    PostQuitMessage(0);
                }
                return 0;
            }

            case WM_CLOSE:
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR pCmdLine, int /*nCmdShow*/) {
    // Parse CLI options
    bool isQuit = false;
    bool isReload = false;

    if (pCmdLine && *pCmdLine) {
        if (wcsstr(pCmdLine, L"--quit") || wcsstr(pCmdLine, L"-q")) {
            isQuit = true;
        } else if (wcsstr(pCmdLine, L"--reload") || wcsstr(pCmdLine, L"-r")) {
            isReload = true;
        }
    }

    // Single instance mutex
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"RamCpp4_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (isQuit) {
            Injector::SignalExit();
            HWND hHost = FindWindowW(L"RamCpp4_Host", NULL);
            if (hHost) PostMessageW(hHost, WM_CLOSE, 0, 0);
        } else {
            Injector::SignalReload();
        }

        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    if (isQuit) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // Determine path to RamCpp4_Widget.dll alongside RamCpp4.exe
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0';
    }
    g_dllPath = std::wstring(exePath) + L"RamCpp4_Widget.dll";

    // Register hidden host window class
    WNDCLASSEXW hostWc = { sizeof(WNDCLASSEXW) };
    hostWc.cbSize = sizeof(WNDCLASSEXW);
    hostWc.lpfnWndProc = HostWndProc;
    hostWc.hInstance = hInstance;
    hostWc.lpszClassName = L"RamCpp4_Host";
    RegisterClassExW(&hostWc);

    HWND hHost = CreateWindowExW(
        0,
        L"RamCpp4_Host",
        L"RamCpp4_Host",
        WS_POPUP,
        0, 0, 0, 0,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    // Perform initial injection
    Injector::Inject(g_dllPath);

    // Run message pump for TaskbarCreated watcher and IPC messages
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (hHost && IsWindow(hHost)) {
        DestroyWindow(hHost);
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return 0;
}
