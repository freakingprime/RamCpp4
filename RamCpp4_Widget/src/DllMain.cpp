#include <windows.h>
#include "WidgetWindow.h"

HINSTANCE g_hDllInstance = nullptr;

namespace {
    WidgetWindow* g_pWidget = nullptr;
    bool g_bInitialized = false;

    bool IsExplorerProcess() {
        wchar_t exePath[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        const wchar_t* pFilename = wcsrchr(exePath, L'\\');
        if (pFilename) {
            ++pFilename;
        } else {
            pFilename = exePath;
        }
        return (_wcsicmp(pFilename, L"explorer.exe") == 0);
    }

    std::wstring GetConfigPath() {
        wchar_t path[MAX_PATH] = { 0 };
        GetModuleFileNameW(g_hDllInstance, path, MAX_PATH);
        wchar_t* lastSlash = wcsrchr(path, L'\\');
        if (lastSlash) {
            *(lastSlash + 1) = L'\0';
        }
        return std::wstring(path) + L"settings.txt";
    }
}

extern "C" __declspec(dllexport) LRESULT CALLBACK RamCpp4_HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (!g_bInitialized && IsExplorerProcess()) {
        g_bInitialized = true;

        // Increment module reference count so UnhookWindowsHookEx in the loader does not
        // cause the DLL to unload, while still allowing clean ejection via FreeLibraryAndExitThread.
        HMODULE hSelf = NULL;
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(RamCpp4_HookProc),
            &hSelf
        );

        std::wstring configPath = GetConfigPath();

        if (!g_pWidget) {
            g_pWidget = new WidgetWindow();
            if (!g_pWidget->Create(g_hDllInstance, configPath)) {
                delete g_pWidget;
                g_pWidget = nullptr;
                g_bInitialized = false;
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID /*lpvReserved*/) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            g_hDllInstance = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
            break;

        case DLL_PROCESS_DETACH:
            // Safe teardown is performed via UnloadAndExit on the UI thread.
            // Avoid joining threads or executing blocking code inside DllMain.
            break;
    }
    return TRUE;
}
