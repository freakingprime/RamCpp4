#pragma once
#include <windows.h>
#include <string>

struct AppConfig {
    int updateInterval = 2000;              // In milliseconds (minimum 900)
    std::wstring textColorStr = L"Auto";    // "Auto" or Hex "RRGGBB" / "#RRGGBB"
    COLORREF customTextColor = RGB(255, 255, 255);
    bool isTextColorAuto = true;

    std::wstring fontFamily = L"Segoe UI";
    int fontSize = 9;                       // Points
    bool fontBold = false;
    int fontWeight = FW_NORMAL;             // FW_NORMAL (400), FW_LIGHT (300), FW_SEMILIGHT (350), FW_BOLD (700)
    float textThinning = 1.4f;              // Alpha power curve: 1.0 (standard), 1.4 (slender/clock-matched), 1.8 (ultra-thin)
    std::wstring fontQuality = L"AntiAliased"; // "AntiAliased" (smooth grayscale) or "ClearType" (subpixel)
    UINT alignment = DT_CENTER;             // DT_CENTER, DT_LEFT, DT_RIGHT

    std::wstring horizontalTemplate = L"CPU: {cpu}%\nRAM: {ram_percent}%";
    std::wstring verticalTemplate = L"C:{cpu}%\nR:{ram_percent}%";

    int horizontalWidth = 90;
    int verticalHeight = 40;
    int paddingX = 4;
    int paddingY = 2;
    int offsetX = 0;
    int offsetY = 0;
    bool enablePadding = true;

    std::wstring networkAdapter = L"Auto";

    bool showDebugBorder = false;
    COLORREF debugBorderColor = RGB(255, 0, 128); // High-visibility pink/magenta
};

// Loads configuration from settings.txt. If file doesn't exist, generates default settings.txt.
bool LoadAppConfig(const std::wstring& configFilePath, AppConfig& config);
void CreateDefaultConfigFile(const std::wstring& configFilePath);
COLORREF ParseColorString(const std::wstring& str, COLORREF defaultColor = RGB(255, 255, 255));
