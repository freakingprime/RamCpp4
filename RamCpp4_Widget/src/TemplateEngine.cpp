#include "TemplateEngine.h"
#include <cwchar>
#include <algorithm>
#include <cmath>

namespace {
    struct TwoDigitTables {
        wchar_t padded[100][3];
        wchar_t unpadded[100][3];
        size_t unpaddedLen[100];

        TwoDigitTables() {
            for (int i = 0; i < 100; ++i) {
                if (i < 10) {
                    padded[i][0] = L' ';
                    padded[i][1] = static_cast<wchar_t>(L'0' + i);
                    padded[i][2] = L'\0';

                    unpadded[i][0] = static_cast<wchar_t>(L'0' + i);
                    unpadded[i][1] = L'\0';
                    unpadded[i][2] = L'\0';
                    unpaddedLen[i] = 1;
                } else {
                    padded[i][0] = static_cast<wchar_t>(L'0' + (i / 10));
                    padded[i][1] = static_cast<wchar_t>(L'0' + (i % 10));
                    padded[i][2] = L'\0';

                    unpadded[i][0] = static_cast<wchar_t>(L'0' + (i / 10));
                    unpadded[i][1] = static_cast<wchar_t>(L'0' + (i % 10));
                    unpadded[i][2] = L'\0';
                    unpaddedLen[i] = 2;
                }
            }
        }
    };

    const TwoDigitTables g_tables;
}

void CompiledTemplate::Compile(const std::wstring& templateStr) {
    m_chunks.clear();
    m_neededFlags = METRIC_FLAG_NONE;

    if (templateStr.empty()) return;

    size_t pos = 0;
    const size_t len = templateStr.size();

    while (pos < len) {
        size_t braceStart = templateStr.find(L'{', pos);
        if (braceStart == std::wstring::npos) {
            // Remainder is literal text
            m_chunks.push_back({ OP_LITERAL, templateStr.substr(pos) });
            break;
        }

        // Add literal before '{'
        if (braceStart > pos) {
            m_chunks.push_back({ OP_LITERAL, templateStr.substr(pos, braceStart - pos) });
        }

        size_t braceEnd = templateStr.find(L'}', braceStart + 1);
        if (braceEnd == std::wstring::npos) {
            // Unclosed brace, treat rest as literal
            m_chunks.push_back({ OP_LITERAL, templateStr.substr(braceStart) });
            break;
        }

        std::wstring token = templateStr.substr(braceStart + 1, braceEnd - braceStart - 1);
        if (_wcsicmp(token.c_str(), L"cpu") == 0) {
            m_chunks.push_back({ OP_CPU, L"" });
            m_neededFlags |= METRIC_FLAG_CPU;
        } else if (_wcsicmp(token.c_str(), L"ram_percent") == 0) {
            m_chunks.push_back({ OP_RAM_PERCENT, L"" });
            m_neededFlags |= METRIC_FLAG_RAM;
        } else if (_wcsicmp(token.c_str(), L"ram_free_percent") == 0) {
            m_chunks.push_back({ OP_RAM_FREE_PERCENT, L"" });
            m_neededFlags |= METRIC_FLAG_RAM;
        } else if (_wcsicmp(token.c_str(), L"ram_used_gb") == 0) {
            m_chunks.push_back({ OP_RAM_USED_GB, L"" });
            m_neededFlags |= METRIC_FLAG_RAM;
        } else if (_wcsicmp(token.c_str(), L"ram_free_gb") == 0) {
            m_chunks.push_back({ OP_RAM_FREE_GB, L"" });
            m_neededFlags |= METRIC_FLAG_RAM;
        } else if (_wcsicmp(token.c_str(), L"ram_total_gb") == 0) {
            m_chunks.push_back({ OP_RAM_TOTAL_GB, L"" });
            m_neededFlags |= METRIC_FLAG_RAM;
        } else if (_wcsicmp(token.c_str(), L"net_down") == 0) {
            m_chunks.push_back({ OP_NET_DOWN, L"" });
            m_neededFlags |= METRIC_FLAG_NET;
        } else if (_wcsicmp(token.c_str(), L"net_up") == 0) {
            m_chunks.push_back({ OP_NET_UP, L"" });
            m_neededFlags |= METRIC_FLAG_NET;
        } else if (_wcsicmp(token.c_str(), L"net_down_mb") == 0 || _wcsicmp(token.c_str(), L"net_down_mbs") == 0) {
            m_chunks.push_back({ OP_NET_DOWN_MB, L"" });
            m_neededFlags |= METRIC_FLAG_NET;
        } else if (_wcsicmp(token.c_str(), L"net_up_mb") == 0 || _wcsicmp(token.c_str(), L"net_up_mbs") == 0) {
            m_chunks.push_back({ OP_NET_UP_MB, L"" });
            m_neededFlags |= METRIC_FLAG_NET;
        } else if (_wcsicmp(token.c_str(), L"net_down_int") == 0 || _wcsicmp(token.c_str(), L"net_down_round") == 0 || _wcsicmp(token.c_str(), L"net_down_0") == 0) {
            m_chunks.push_back({ OP_NET_DOWN_INT, L"" });
            m_neededFlags |= METRIC_FLAG_NET;
        } else if (_wcsicmp(token.c_str(), L"net_up_int") == 0 || _wcsicmp(token.c_str(), L"net_up_round") == 0 || _wcsicmp(token.c_str(), L"net_up_0") == 0) {
            m_chunks.push_back({ OP_NET_UP_INT, L"" });
            m_neededFlags |= METRIC_FLAG_NET;
        } else if (_wcsicmp(token.c_str(), L"net_down_mb_int") == 0 || _wcsicmp(token.c_str(), L"net_down_mb_round") == 0 || _wcsicmp(token.c_str(), L"net_down_mb_0") == 0 || _wcsicmp(token.c_str(), L"net_down_mbs_int") == 0) {
            m_chunks.push_back({ OP_NET_DOWN_MB_INT, L"" });
            m_neededFlags |= METRIC_FLAG_NET;
        } else if (_wcsicmp(token.c_str(), L"net_up_mb_int") == 0 || _wcsicmp(token.c_str(), L"net_up_mb_round") == 0 || _wcsicmp(token.c_str(), L"net_up_mb_0") == 0 || _wcsicmp(token.c_str(), L"net_up_mbs_int") == 0) {
            m_chunks.push_back({ OP_NET_UP_MB_INT, L"" });
            m_neededFlags |= METRIC_FLAG_NET;
        } else {
            // Unrecognized token, preserve literal string "{token}"
            m_chunks.push_back({ OP_LITERAL, templateStr.substr(braceStart, braceEnd - braceStart + 1) });
        }

        pos = braceEnd + 1;
    }
}

void CompiledTemplate::Format(const SystemMetrics& metrics, wchar_t* outBuf, size_t maxLen, bool enablePadding) const {
    if (!outBuf || maxLen == 0) return;
    outBuf[0] = L'\0';

    size_t curLen = 0;
    wchar_t numBuf[32];

    for (const auto& chunk : m_chunks) {
        if (curLen >= maxLen - 1) break;

        switch (chunk.opcode) {
            case OP_LITERAL: {
                size_t toCopy = (std::min)(chunk.literalText.size(), maxLen - 1 - curLen);
                wmemcpy(outBuf + curLen, chunk.literalText.c_str(), toCopy);
                curLen += toCopy;
                break;
            }
            case OP_CPU: {
                int val = std::clamp(metrics.cpuPercent, 0, 99);
                if (enablePadding) {
                    if (curLen + 2 < maxLen) {
                        outBuf[curLen] = g_tables.padded[val][0];
                        outBuf[curLen + 1] = g_tables.padded[val][1];
                        curLen += 2;
                    }
                } else {
                    size_t toCopy = g_tables.unpaddedLen[val];
                    if (curLen + toCopy < maxLen) {
                        wmemcpy(outBuf + curLen, g_tables.unpadded[val], toCopy);
                        curLen += toCopy;
                    }
                }
                break;
            }
            case OP_RAM_PERCENT: {
                int val = std::clamp(metrics.ramPercent, 0, 99);
                if (enablePadding) {
                    if (curLen + 2 < maxLen) {
                        outBuf[curLen] = g_tables.padded[val][0];
                        outBuf[curLen + 1] = g_tables.padded[val][1];
                        curLen += 2;
                    }
                } else {
                    size_t toCopy = g_tables.unpaddedLen[val];
                    if (curLen + toCopy < maxLen) {
                        wmemcpy(outBuf + curLen, g_tables.unpadded[val], toCopy);
                        curLen += toCopy;
                    }
                }
                break;
            }
            case OP_RAM_FREE_PERCENT: {
                int val = std::clamp(metrics.ramFreePercent, 0, 99);
                if (enablePadding) {
                    if (curLen + 2 < maxLen) {
                        outBuf[curLen] = g_tables.padded[val][0];
                        outBuf[curLen + 1] = g_tables.padded[val][1];
                        curLen += 2;
                    }
                } else {
                    size_t toCopy = g_tables.unpaddedLen[val];
                    if (curLen + toCopy < maxLen) {
                        wmemcpy(outBuf + curLen, g_tables.unpadded[val], toCopy);
                        curLen += toCopy;
                    }
                }
                break;
            }
            case OP_RAM_USED_GB: {
                double val = metrics.ramUsedGb < 0.0 ? 0.0 : metrics.ramUsedGb;
                int written = enablePadding ? swprintf_s(numBuf, L"%4.1f", val)
                                            : swprintf_s(numBuf, L"%.1f", val);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_RAM_FREE_GB: {
                double val = metrics.ramFreeGb < 0.0 ? 0.0 : metrics.ramFreeGb;
                int written = enablePadding ? swprintf_s(numBuf, L"%4.1f", val)
                                            : swprintf_s(numBuf, L"%.1f", val);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_RAM_TOTAL_GB: {
                double val = metrics.ramTotalGb < 0.0 ? 0.0 : metrics.ramTotalGb;
                int written = enablePadding ? swprintf_s(numBuf, L"%4.1f", val)
                                            : swprintf_s(numBuf, L"%.1f", val);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_NET_DOWN: {
                double val = metrics.netDownMbps < 0.0 ? 0.0 : metrics.netDownMbps;
                int written = swprintf_s(numBuf, L"%.1f", val);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_NET_UP: {
                double val = metrics.netUpMbps < 0.0 ? 0.0 : metrics.netUpMbps;
                int written = swprintf_s(numBuf, L"%.1f", val);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_NET_DOWN_MB: {
                double val = metrics.netDownMBps < 0.0 ? 0.0 : metrics.netDownMBps;
                int written = swprintf_s(numBuf, L"%.1f", val);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_NET_UP_MB: {
                double val = metrics.netUpMBps < 0.0 ? 0.0 : metrics.netUpMBps;
                int written = swprintf_s(numBuf, L"%.1f", val);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_NET_DOWN_INT: {
                double val = metrics.netDownMbps < 0.0 ? 0.0 : metrics.netDownMbps;
                long rounded = std::lround(val);
                int written = swprintf_s(numBuf, L"%ld", rounded);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_NET_UP_INT: {
                double val = metrics.netUpMbps < 0.0 ? 0.0 : metrics.netUpMbps;
                long rounded = std::lround(val);
                int written = swprintf_s(numBuf, L"%ld", rounded);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_NET_DOWN_MB_INT: {
                double val = metrics.netDownMBps < 0.0 ? 0.0 : metrics.netDownMBps;
                long rounded = std::lround(val);
                int written = swprintf_s(numBuf, L"%ld", rounded);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
            case OP_NET_UP_MB_INT: {
                double val = metrics.netUpMBps < 0.0 ? 0.0 : metrics.netUpMBps;
                long rounded = std::lround(val);
                int written = swprintf_s(numBuf, L"%ld", rounded);
                if (written > 0) {
                    size_t toCopy = (std::min)((size_t)written, maxLen - 1 - curLen);
                    wmemcpy(outBuf + curLen, numBuf, toCopy);
                    curLen += toCopy;
                }
                break;
            }
        }
    }

    outBuf[curLen] = L'\0';
}
