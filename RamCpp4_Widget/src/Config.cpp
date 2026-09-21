#include "Config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace {

std::wstring Trim(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return L"";
    size_t last = str.find_last_not_of(L" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::wstring UnescapeString(const std::wstring& str) {
    std::wstring result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == L'\\' && i + 1 < str.size()) {
            wchar_t next = str[i + 1];
            if (next == L'n' || next == L'N') {
                result += L'\n';
                ++i;
            } else if (next == L'r' || next == L'R') {
                result += L'\r';
                ++i;
            } else if (next == L't' || next == L'T') {
                result += L'\t';
                ++i;
            } else if (next == L'\\') {
                result += L'\\';
                ++i;
            } else if ((next == L'u' || next == L'U') && i + 5 < str.size()) {
                // Unicode escape: \uXXXX (4 hex digits, e.g. \u2191 -> ↑)
                std::wstring hexStr = str.substr(i + 2, 4);
                wchar_t* endPtr = nullptr;
                unsigned long codepoint = wcstoul(hexStr.c_str(), &endPtr, 16);
                if (endPtr && *endPtr == L'\0') {
                    result += static_cast<wchar_t>(codepoint);
                    i += 5;
                } else {
                    result += str[i];
                }
            } else {
                result += str[i];
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

} // namespace

COLORREF ParseColorString(const std::wstring& str, COLORREF defaultColor) {
    std::wstring s = Trim(str);
    if (s.empty()) return defaultColor;
    if (s.front() == L'#') s.erase(0, 1);

    if (s.length() == 6) {
        wchar_t* endPtr = nullptr;
        unsigned long val = wcstoul(s.c_str(), &endPtr, 16);
        if (endPtr && *endPtr == L'\0') {
            BYTE r = (BYTE)((val >> 16) & 0xFF);
            BYTE g = (BYTE)((val >> 8) & 0xFF);
            BYTE b = (BYTE)(val & 0xFF);
            return RGB(r, g, b);
        }
    }
    return defaultColor;
}

void CreateDefaultConfigFile(const std::wstring& configFilePath) {
    std::ofstream file(configFilePath, std::ios::binary);
    if (!file.is_open()) return;

    // UTF-8 BOM for maximum compatibility across Windows text editors
    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    file.write(reinterpret_cast<const char*>(bom), sizeof(bom));

    file << R"(# ==============================================================================
# RamCpp4 Configuration File (Windows 10 & 11 64-bit)
# Real-Time In-Process Taskbar System Monitor
# ==============================================================================

# ------------------------------------------------------------------------------
# 1. TIMING & PERFORMANCE
# ------------------------------------------------------------------------------
# UpdateInterval: Polling and UI refresh interval in milliseconds.
# Recommended: 1000 (1 second) or 2000 (2 seconds). Hard minimum is 900.
UpdateInterval=1000


# ------------------------------------------------------------------------------
# 2. COLOR SETTINGS
# ------------------------------------------------------------------------------
# TextColor:
#   - Auto: Automatically matches the taskbar clock (white on dark theme,
#           dark gray #181818 on light theme).
#   - Custom Hex: RRGGBB or #RRGGBB (e.g. FFFFFF, 00E5FF, FFB300, 00FF66).
TextColor=Auto


# ------------------------------------------------------------------------------
# 3. TYPOGRAPHY & FONT SETTINGS
# ------------------------------------------------------------------------------
# FontFamily: Any installed font family.
# Popular choices:
#   - Consolas                 (Monospaced, clean column alignment)
#   - Segoe UI                 (Native Windows 10 taskbar font)
#   - Segoe UI Variable Text   (Native Windows 11 taskbar font)
#   - Cascadia Code            (Modern monospaced programming font)
FontFamily=Consolas

# FontSize: Font size in points (integer from 6 to 36).
FontSize=11

# FontBold:
#   0 = Normal weight
#   1 = Bold weight (overrides FontWeight to 700)
FontBold=0

# FontWeight: Precise font stroke weight (100 to 900, or text keyword):
#   - Thin       (100)
#   - ExtraLight (200)
#   - Light      (300)
#   - SemiLight  (350) - Matches Windows 10 clock numerals
#   - Normal     (400) - Regular weight
#   - Medium     (500)
#   - SemiBold   (600)
#   - Bold       (700)
FontWeight=Normal

# TextThinning: Perceptual alpha thinning exponent (from 0.5 to 3.0).
# Eliminates Desktop Window Manager (DWM) layered window edge swelling:
#   1.0 = Standard GDI anti-aliasing (slightly heavier)
#   1.4 = Slender (recommended for clock matching)
#   1.8 = Ultra-thin, delicate strokes
TextThinning=1.4

# FontQuality: Font smoothing engine:
#   - AntiAliased : Grayscale anti-aliasing (smooth, 0 color fringing, uses TextThinning)
#   - ClearType   : Subpixel LCD rendering (RamCpp3 style, crisp 1-pixel stems)
FontQuality=AntiAliased

# Alignment: Text alignment within the widget box:
#   - Center
#   - Left
#   - Right
Alignment=Center


# ------------------------------------------------------------------------------
# 4. NETWORK ADAPTER SELECTION
# ------------------------------------------------------------------------------
# NetworkAdapter:
#   - Auto        : Automatically sums traffic across all active physical adapters.
#   - Name/Desc   : Case-insensitive adapter name or part of description
#                   (e.g. "Wi-Fi", "Ethernet", "Realtek", "Intel").
#   - IP Address  : Match by local IPv4 address (e.g. "192.168.1.100").
NetworkAdapter=Wi-Fi


# ------------------------------------------------------------------------------
# 5. TEMPLATES & AVAILABLE TOKENS
# ------------------------------------------------------------------------------
# Use \n to insert a newline.
#
# Available Tokens:
#   CPU:
#     {cpu}              - CPU usage % (0-99, fixed 2-digit with space padding)
#
#   RAM:
#     {ram_percent}      - Used RAM % (0-99, fixed 2-digit)
#     {ram_free_percent} - Free RAM % (0-99, fixed 2-digit)
#     {ram_used_gb}      - Used RAM in GB (4 chars, e.g.  7.8)
#     {ram_free_gb}      - Free RAM in GB (4 chars, e.g.  9.5)
#     {ram_total_gb}     - Total physical RAM in GB (e.g. 16.0)
#
#   NETWORK (Mbps - Megabits per second):
#     {net_down}         - Download speed with 1 decimal (e.g.  15.2)
#     {net_up}           - Upload speed with 1 decimal (e.g.   2.1)
#     {net_down_int}     - Download whole integer (e.g. 15) [aliases: {net_down_0}, {net_down_round}]
#     {net_up_int}       - Upload whole integer (e.g. 2)    [aliases: {net_up_0}, {net_up_round}]
#
#   NETWORK (MB/s - Megabytes per second):
#     {net_down_mb}      - Download in MB/s with 1 decimal (e.g. 1.9)
#     {net_up_mb}        - Upload in MB/s with 1 decimal (e.g. 0.3)
#     {net_down_mb_int}  - Download whole integer in MB/s   [aliases: {net_down_mb_0}]
#     {net_up_mb_int}    - Upload whole integer in MB/s     [aliases: {net_up_mb_0}]
#
# Unicode Arrows Reference (copy-paste directly or use escapes):
#   - Standard: ↑, ↓, ▲, ▼, \u2191, \u2193
#   - Barb Arrows: 🡠 🡢 🡡 🡣 • 🡨 🡪 🡩 🡫 • 🡰 🡲 🡱 🡳 • 🡸 🡺 🡹 🡻 • 🢀 🢂 🢁 🢃
#   - Black Arrowheads: ➤ • ⮜ ⮞ ⮝ ⮟ • ➢ ➣ • ⮘ ⮚ ⮙ ⮛
#   - Heavy Triangles: 🠀 🠂 🠁 🠃 • 🠄 🠆 🠅 🠇 • 🠈 🠊 🠉 🠋
#
# Active Templates:
HorizontalTemplate=Free: {ram_free_gb} GB ({ram_free_percent}%) 🠉{net_up_int}\nCPU : {cpu}   %        🠋{net_down_int}
VerticalTemplate=C:{cpu}%\nFree:\n{ram_free_gb} GB\n({ram_free_percent}%)\n🠉{net_up_int}\n🠋{net_down_int}


# ------------------------------------------------------------------------------
# 6. DIMENSIONS, PADDING & POSITIONING
# ------------------------------------------------------------------------------
# HorizontalWidth: Width in pixels when taskbar is horizontal (30 to 400).
HorizontalWidth=220

# VerticalHeight: Height in pixels when taskbar is vertical (20 to 300).
# Recommended: 40 for 2 lines, 120 for 6 lines.
VerticalHeight=120

# Inner text padding in pixels:
PaddingX=1
PaddingY=1

# Manual Position Offsets in pixels (useful for fine-tuning dock placement):
#   OffsetX: negative moves left, positive moves right
#   OffsetY: negative moves up, positive moves down
OffsetX=0
OffsetY=0

# EnablePadding:
#   1 = Pads single-digit numbers with leading spaces for fixed column alignment (e.g. " 7%")
#   0 = No padding (e.g. "7%")
EnablePadding=1


# ------------------------------------------------------------------------------
# 7. DIAGNOSTICS & DEBUG
# ------------------------------------------------------------------------------
# ShowDebugBorder:
#   1 = Draws a 1-pixel high-contrast border around the widget box to assist sizing/placement.
#   0 = Normal mode (100% invisible transparent background).
ShowDebugBorder=0
)";
}

bool LoadAppConfig(const std::wstring& configFilePath, AppConfig& config) {
    DWORD dwAttrib = GetFileAttributesW(configFilePath.c_str());
    if (dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        CreateDefaultConfigFile(configFilePath);
    }

    std::ifstream file(configFilePath, std::ios::binary);
    if (!file.is_open()) return false;

    // Read entire file as binary bytes
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (bytes.empty()) return true;

    std::wstring content;
    const unsigned char* pData = reinterpret_cast<const unsigned char*>(bytes.data());
    size_t len = bytes.size();

    if (len >= 3 && pData[0] == 0xEF && pData[1] == 0xBB && pData[2] == 0xBF) {
        // UTF-8 with BOM
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, bytes.data() + 3, static_cast<int>(len - 3), nullptr, 0);
        if (wideLen > 0) {
            content.resize(wideLen);
            MultiByteToWideChar(CP_UTF8, 0, bytes.data() + 3, static_cast<int>(len - 3), &content[0], wideLen);
        }
    } else if (len >= 2 && pData[0] == 0xFF && pData[1] == 0xFE) {
        // UTF-16 LE with BOM
        size_t wcharCount = (len - 2) / sizeof(wchar_t);
        content.assign(reinterpret_cast<const wchar_t*>(bytes.data() + 2), wcharCount);
    } else {
        // Auto-detect: Attempt conversion as UTF-8 (strict validation)
        int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(len), nullptr, 0);
        if (wideLen > 0) {
            content.resize(wideLen);
            MultiByteToWideChar(CP_UTF8, 0, bytes.data(), static_cast<int>(len), &content[0], wideLen);
        } else {
            // Fallback to ANSI (CP_ACP)
            wideLen = MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(len), nullptr, 0);
            if (wideLen > 0) {
                content.resize(wideLen);
                MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(len), &content[0], wideLen);
            }
        }
    }

    std::wistringstream ss(content);
    std::wstring line;
    while (std::getline(ss, line)) {
        std::wstring trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == L'#' || trimmed.front() == L';') {
            continue;
        }

        size_t eqPos = trimmed.find(L'=');
        if (eqPos == std::wstring::npos) continue;

        std::wstring key = Trim(trimmed.substr(0, eqPos));
        std::wstring val = Trim(trimmed.substr(eqPos + 1));

        if (_wcsicmp(key.c_str(), L"UpdateInterval") == 0) {
            int interval = _wtoi(val.c_str());
            config.updateInterval = (interval >= 900) ? interval : 900;
        } else if (_wcsicmp(key.c_str(), L"TextColor") == 0) {
            config.textColorStr = val;
            if (_wcsicmp(val.c_str(), L"Auto") == 0) {
                config.isTextColorAuto = true;
            } else {
                config.isTextColorAuto = false;
                config.customTextColor = ParseColorString(val, RGB(255, 255, 255));
            }
        } else if (_wcsicmp(key.c_str(), L"FontFamily") == 0) {
            if (!val.empty()) config.fontFamily = val;
        } else if (_wcsicmp(key.c_str(), L"FontSize") == 0) {
            int sz = _wtoi(val.c_str());
            if (sz >= 6 && sz <= 36) config.fontSize = sz;
        } else if (_wcsicmp(key.c_str(), L"FontBold") == 0) {
            config.fontBold = (_wtoi(val.c_str()) != 0);
            if (config.fontBold && config.fontWeight == FW_NORMAL) {
                config.fontWeight = FW_BOLD;
            }
        } else if (_wcsicmp(key.c_str(), L"FontWeight") == 0) {
            int fw = _wtoi(val.c_str());
            if (fw >= 100 && fw <= 900) {
                config.fontWeight = fw;
            } else if (_wcsicmp(val.c_str(), L"Light") == 0) {
                config.fontWeight = 300;
            } else if (_wcsicmp(val.c_str(), L"SemiLight") == 0) {
                config.fontWeight = 350;
            } else if (_wcsicmp(val.c_str(), L"Normal") == 0 || _wcsicmp(val.c_str(), L"Regular") == 0) {
                config.fontWeight = 400;
            } else if (_wcsicmp(val.c_str(), L"Bold") == 0) {
                config.fontWeight = 700;
            }
        } else if (_wcsicmp(key.c_str(), L"TextThinning") == 0) {
            float th = static_cast<float>(_wtof(val.c_str()));
            if (th >= 0.5f && th <= 3.0f) config.textThinning = th;
        } else if (_wcsicmp(key.c_str(), L"FontQuality") == 0) {
            if (_wcsicmp(val.c_str(), L"ClearType") == 0) {
                config.fontQuality = L"ClearType";
            } else {
                config.fontQuality = L"AntiAliased";
            }
        } else if (_wcsicmp(key.c_str(), L"Alignment") == 0) {
            if (_wcsicmp(val.c_str(), L"Left") == 0) config.alignment = DT_LEFT;
            else if (_wcsicmp(val.c_str(), L"Right") == 0) config.alignment = DT_RIGHT;
            else config.alignment = DT_CENTER;
        } else if (_wcsicmp(key.c_str(), L"HorizontalTemplate") == 0) {
            config.horizontalTemplate = UnescapeString(val);
        } else if (_wcsicmp(key.c_str(), L"VerticalTemplate") == 0) {
            config.verticalTemplate = UnescapeString(val);
        } else if (_wcsicmp(key.c_str(), L"HorizontalWidth") == 0) {
            int w = _wtoi(val.c_str());
            if (w >= 30 && w <= 400) config.horizontalWidth = w;
        } else if (_wcsicmp(key.c_str(), L"VerticalHeight") == 0) {
            int h = _wtoi(val.c_str());
            if (h >= 20 && h <= 300) config.verticalHeight = h;
        } else if (_wcsicmp(key.c_str(), L"PaddingX") == 0) {
            config.paddingX = _wtoi(val.c_str());
        } else if (_wcsicmp(key.c_str(), L"PaddingY") == 0) {
            config.paddingY = _wtoi(val.c_str());
        } else if (_wcsicmp(key.c_str(), L"OffsetX") == 0) {
            config.offsetX = _wtoi(val.c_str());
        } else if (_wcsicmp(key.c_str(), L"OffsetY") == 0) {
            config.offsetY = _wtoi(val.c_str());
        } else if (_wcsicmp(key.c_str(), L"EnablePadding") == 0) {
            config.enablePadding = (_wtoi(val.c_str()) != 0);
        } else if (_wcsicmp(key.c_str(), L"NetworkAdapter") == 0) {
            if (!val.empty()) config.networkAdapter = val;
        } else if (_wcsicmp(key.c_str(), L"ShowDebugBorder") == 0) {
            config.showDebugBorder = (_wtoi(val.c_str()) != 0);
        }
    }

    return true;
}
