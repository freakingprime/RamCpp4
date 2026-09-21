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

    // UTF-8 BOM for maximum compatibility across Windows editors
    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    file.write(reinterpret_cast<const char*>(bom), sizeof(bom));

    file << "# ==========================================================\n"
         << "# RamCpp4 Configuration File (Windows 10 & 11 64-bit)\n"
         << "# In-Process Taskbar System Monitor\n"
         << "# ==========================================================\n\n"
         << "# Metrics refresh and UI update interval in milliseconds (default: 2000, minimum: 900)\n"
         << "UpdateInterval=2000\n\n"
         << "# Text Color:\n"
         << "# Set to 'Auto' to automatically match the Windows taskbar clock color,\n"
         << "# or specify a custom Hex RRGGBB (e.g. FFFFFF, 00E5FF, FFB300)\n"
         << "TextColor=Auto\n\n"
         << "# Font Settings (Uses native ClearType for crisp text matching the clock)\n"
         << "FontFamily=Segoe UI\n"
         << "FontSize=9\n"
         << "FontBold=0\n\n"
         << "# Text Alignment: Center, Left, Right\n"
         << "Alignment=Center\n\n"
         << "# Network Adapter Selection:\n"
         << "# Auto = Sums traffic across all active physical network adapters (default)\n"
         << "# Or specify Adapter Name / Description (e.g. \"Ethernet\", \"Wi-Fi\", \"Realtek\")\n"
         << "# Or specify Local IP Address (e.g. \"192.168.1.100\")\n"
         << "NetworkAdapter=Auto\n\n"
         << "# Formatting Templates\n"
         << "# Available tokens:\n"
         << "#   {cpu}              - CPU usage % (0-99, 2-digit fixed width via space padding)\n"
         << "#   {ram_percent}      - Used RAM % (2-digit fixed width via space padding)\n"
         << "#   {ram_free_percent} - Free RAM % (2-digit fixed width via space padding)\n"
         << "#   {ram_used_gb}      - Used RAM in GB (4-char fixed width, e.g. 7.8)\n"
         << "#   {ram_free_gb}      - Free RAM in GB (4-char fixed width)\n"
         << "#   {ram_total_gb}     - Total RAM in GB (e.g. 16.0)\n"
         << "#   {net_down}         - Download speed in Mbps with 1 decimal (e.g. 4.8, 150.2)\n"
         << "#   {net_up}           - Upload speed in Mbps with 1 decimal (e.g. 1.2, 42.5)\n"
         << "#   {net_down_mb}      - Download speed in MB/s with 1 decimal (e.g. 15.6)\n"
         << "#   {net_up_mb}        - Upload speed in MB/s with 1 decimal (e.g. 2.1)\n"
         << "#   {net_down_int}     - Download speed in Mbps whole integer without decimal (e.g. 5, 150)\n"
         << "#   {net_up_int}       - Upload speed in Mbps whole integer without decimal (e.g. 1, 42)\n"
         << "#   {net_down_mb_int}  - Download speed in MB/s whole integer without decimal (e.g. 16)\n"
         << "#   {net_up_mb_int}    - Upload speed in MB/s whole integer without decimal (e.g. 2)\n"
         << "# Use \\n for newline.\n"
         << "# Literal Unicode symbols (↑, ↓, ▲, ▼) and escapes (\\u2191, \\u2193) are fully supported.\n"
         << "# Example with Network: HorizontalTemplate=CPU: {cpu}%  ↓{net_down_mb_int}M\\nRAM: {ram_percent}%  ↑{net_up_mb_int}M\n"
         << "HorizontalTemplate=CPU: {cpu}%\\nRAM: {ram_percent}%\n"
         << "VerticalTemplate=C:{cpu}%\\nR:{ram_percent}%\n\n"
         << "# Dimensions & Spacing (in pixels)\n"
         << "HorizontalWidth=90\n"
         << "VerticalHeight=40\n"
         << "PaddingX=4\n"
         << "PaddingY=2\n\n"
         << "# Position Manual Offset (in pixels):\n"
         << "# OffsetX: negative moves left, positive moves right\n"
         << "# OffsetY: negative moves up, positive moves down\n"
         << "OffsetX=0\n"
         << "OffsetY=0\n\n"
         << "# Number Padding (Alignment):\n"
         << "# 1 = Pad numbers with leading spaces for fixed column alignment (e.g. \" 7%\", \" 4.2G\")\n"
         << "# 0 = No space padding (e.g. \"7%\", \"4.2G\")\n"
         << "EnablePadding=1\n\n"
         << "# Debug Sizing Border:\n"
         << "# 1 = Draw 1-pixel high-contrast border to visually assist size/padding adjustment\n"
         << "# 0 = Completely invisible background (normal mode)\n"
         << "ShowDebugBorder=0\n";
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
