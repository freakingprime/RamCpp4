#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "SystemMonitor.h"
#include <algorithm>
#include <cwctype>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace {
    bool ContainsIgnoreCase(const std::wstring& str, const std::wstring& sub) {
        if (sub.empty()) return true;
        auto it = std::search(
            str.begin(), str.end(),
            sub.begin(), sub.end(),
            [](wchar_t ch1, wchar_t ch2) { return towlower(ch1) == towlower(ch2); }
        );
        return (it != str.end());
    }

    bool IsVirtualOrFilterAdapter(const MIB_IF_ROW2& row) {
        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL) return true;
        // Ignore NDIS LightWeight Filter (LWF) sub-interfaces (e.g. WFP, QoS Packet Scheduler)
        if (row.InterfaceAndOperStatusFlags.FilterInterface) return true;

        std::wstring desc = row.Description ? row.Description : L"";
        std::wstring alias = row.Alias ? row.Alias : L"";
        if (ContainsIgnoreCase(desc, L"Virtual") || ContainsIgnoreCase(alias, L"Virtual") ||
            ContainsIgnoreCase(desc, L"vEthernet") || ContainsIgnoreCase(alias, L"vEthernet") ||
            ContainsIgnoreCase(desc, L"VMware") || ContainsIgnoreCase(desc, L"VirtualBox") ||
            ContainsIgnoreCase(desc, L"Host-Only") || ContainsIgnoreCase(desc, L"Hyper-V") ||
            ContainsIgnoreCase(desc, L"Tailscale") || ContainsIgnoreCase(desc, L"WireGuard") ||
            ContainsIgnoreCase(desc, L"TAP-") || ContainsIgnoreCase(desc, L"Bluetooth") ||
            ContainsIgnoreCase(desc, L"ZeroTier") || ContainsIgnoreCase(desc, L"Wsl")) {
            return true;
        }
        return false;
    }
}

SystemMonitor::SystemMonitor() {
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        m_prevIdleTime = FileTimeToUint64(idleTime);
        m_prevKernelTime = FileTimeToUint64(kernelTime);
        m_prevUserTime = FileTimeToUint64(userTime);
        m_isFirstCpuSample = false;
    }
}

uint64_t SystemMonitor::FileTimeToUint64(const FILETIME& ft) {
    return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

void SystemMonitor::SetConfiguredAdapter(const std::wstring& adapterName, bool force) {
    if (force || m_targetAdapter != adapterName) {
        m_targetAdapter = adapterName;
        m_cachedAdapters.clear();
        m_prevNetTick = 0;
    }
}

void SystemMonitor::RefreshNetworkAdapters() {
    m_lastAdapterRefreshTick = GetTickCount64();
    PMIB_IF_TABLE2 pTable = nullptr;
    if (GetIfTable2(&pTable) != NO_ERROR || !pTable) return;

    m_cachedAdapters.clear();
    bool isAuto = (_wcsicmp(m_targetAdapter.c_str(), L"Auto") == 0 || m_targetAdapter.empty());

    if (!isAuto) {
        // 1. Check if configured target is an IPv4 address (e.g. "192.168.1.100")
        IN_ADDR ipAddr;
        if (InetPtonW(AF_INET, m_targetAdapter.c_str(), &ipAddr) == 1) {
            ULONG ipSize = 0;
            if (GetIpAddrTable(nullptr, &ipSize, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
                PMIB_IPADDRTABLE pIpTable = (PMIB_IPADDRTABLE)malloc(ipSize);
                if (pIpTable && GetIpAddrTable(pIpTable, &ipSize, FALSE) == NO_ERROR) {
                    for (DWORD k = 0; k < pIpTable->dwNumEntries; ++k) {
                        if (pIpTable->table[k].dwAddr == ipAddr.s_addr) {
                            NET_LUID targetLuid;
                            if (ConvertInterfaceIndexToLuid(pIpTable->table[k].dwIndex, &targetLuid) == NO_ERROR) {
                                MIB_IF_ROW2 row = { 0 };
                                row.InterfaceLuid = targetLuid;
                                if (GetIfEntry2(&row) == NO_ERROR) {
                                    CachedAdapterStats stats;
                                    stats.luid = targetLuid;
                                    stats.prevInBytes = row.InOctets;
                                    stats.prevOutBytes = row.OutOctets;
                                    stats.wasUp = (row.OperStatus == IfOperStatusUp);
                                    m_cachedAdapters.push_back(stats);
                                    free(pIpTable);
                                    FreeMibTable(pTable);
                                    return;
                                }
                            }
                        }
                    }
                }
                if (pIpTable) free(pIpTable);
            }
        }

        // 2. Match by Alias or Description (prioritizing exact match and physical adapters over filter drivers)
        bool found = false;
        // Pass A: Exact match on Alias, non-filter
        for (ULONG i = 0; i < pTable->NumEntries; ++i) {
            const MIB_IF_ROW2& row = pTable->Table[i];
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL) continue;
            if (row.InterfaceAndOperStatusFlags.FilterInterface) continue;

            std::wstring alias = row.Alias ? row.Alias : L"";
            if (_wcsicmp(alias.c_str(), m_targetAdapter.c_str()) == 0) {
                CachedAdapterStats stats;
                stats.luid = row.InterfaceLuid;
                stats.prevInBytes = row.InOctets;
                stats.prevOutBytes = row.OutOctets;
                stats.wasUp = (row.OperStatus == IfOperStatusUp);
                m_cachedAdapters.push_back(stats);
                found = true;
                break;
            }
        }

        // Pass B: Substring match on Alias or Description, non-filter
        if (!found) {
            for (ULONG i = 0; i < pTable->NumEntries; ++i) {
                const MIB_IF_ROW2& row = pTable->Table[i];
                if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL) continue;
                if (row.InterfaceAndOperStatusFlags.FilterInterface) continue;

                std::wstring alias = row.Alias ? row.Alias : L"";
                std::wstring desc = row.Description ? row.Description : L"";

                if (ContainsIgnoreCase(alias, m_targetAdapter) || ContainsIgnoreCase(desc, m_targetAdapter)) {
                    CachedAdapterStats stats;
                    stats.luid = row.InterfaceLuid;
                    stats.prevInBytes = row.InOctets;
                    stats.prevOutBytes = row.OutOctets;
                    stats.wasUp = (row.OperStatus == IfOperStatusUp);
                    m_cachedAdapters.push_back(stats);
                    found = true;
                    break;
                }
            }
        }
    } else {
        // Auto mode: Sum operational physical network adapters (filtering out virtual adapters & filter drivers)
        // Pass 1: Active physical adapters with ConnectorPresent == 1
        for (ULONG i = 0; i < pTable->NumEntries; ++i) {
            const MIB_IF_ROW2& row = pTable->Table[i];
            if (IsVirtualOrFilterAdapter(row)) continue;
            if (!row.InterfaceAndOperStatusFlags.ConnectorPresent) continue;

            if (row.OperStatus == IfOperStatusUp) {
                bool duplicate = false;
                for (const auto& existing : m_cachedAdapters) {
                    if (existing.luid.Value == row.InterfaceLuid.Value) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    CachedAdapterStats stats;
                    stats.luid = row.InterfaceLuid;
                    stats.prevInBytes = row.InOctets;
                    stats.prevOutBytes = row.OutOctets;
                    stats.wasUp = true;
                    m_cachedAdapters.push_back(stats);
                }
            }
        }

        // Pass 2: Fallback to any active non-virtual adapter if no ConnectorPresent adapters were found
        if (m_cachedAdapters.empty()) {
            for (ULONG i = 0; i < pTable->NumEntries; ++i) {
                const MIB_IF_ROW2& row = pTable->Table[i];
                if (IsVirtualOrFilterAdapter(row)) continue;

                if (row.OperStatus == IfOperStatusUp) {
                    bool duplicate = false;
                    for (const auto& existing : m_cachedAdapters) {
                        if (existing.luid.Value == row.InterfaceLuid.Value) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        CachedAdapterStats stats;
                        stats.luid = row.InterfaceLuid;
                        stats.prevInBytes = row.InOctets;
                        stats.prevOutBytes = row.OutOctets;
                        stats.wasUp = true;
                        m_cachedAdapters.push_back(stats);
                    }
                }
            }
        }
    }

    // Pass 3: Ultimate Fallback: If still empty, include any active non-loopback/non-tunnel adapter
    if (m_cachedAdapters.empty()) {
        for (ULONG i = 0; i < pTable->NumEntries; ++i) {
            const MIB_IF_ROW2& row = pTable->Table[i];
            if (row.Type != IF_TYPE_SOFTWARE_LOOPBACK && row.Type != IF_TYPE_TUNNEL &&
                !row.InterfaceAndOperStatusFlags.FilterInterface && row.OperStatus == IfOperStatusUp) {
                CachedAdapterStats stats;
                stats.luid = row.InterfaceLuid;
                stats.prevInBytes = row.InOctets;
                stats.prevOutBytes = row.OutOctets;
                stats.wasUp = true;
                m_cachedAdapters.push_back(stats);
            }
        }
    }

    FreeMibTable(pTable);
}

void SystemMonitor::Update(uint32_t neededFlags, SystemMetrics& outMetrics) {
    if (neededFlags & METRIC_FLAG_CPU) {
        FILETIME idleTime, kernelTime, userTime;
        if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            uint64_t curIdle = FileTimeToUint64(idleTime);
            uint64_t curKernel = FileTimeToUint64(kernelTime);
            uint64_t curUser = FileTimeToUint64(userTime);

            if (!m_isFirstCpuSample) {
                uint64_t deltaKernel = curKernel - m_prevKernelTime;
                uint64_t deltaUser = curUser - m_prevUserTime;
                uint64_t deltaTotal = deltaKernel + deltaUser;
                uint64_t deltaIdle = curIdle - m_prevIdleTime;

                if (deltaTotal > 0) {
                    if (deltaIdle > deltaTotal) deltaIdle = deltaTotal;
                    int cpu = (int)(((deltaTotal - deltaIdle) * 100) / deltaTotal);
                    outMetrics.cpuPercent = std::clamp(cpu, 0, 99);
                }
            } else {
                m_isFirstCpuSample = false;
            }

            m_prevIdleTime = curIdle;
            m_prevKernelTime = curKernel;
            m_prevUserTime = curUser;
        }
    }

    if (neededFlags & METRIC_FLAG_RAM) {
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            outMetrics.ramPercent = std::clamp((int)memStatus.dwMemoryLoad, 0, 99);
            outMetrics.ramFreePercent = std::clamp(100 - (int)memStatus.dwMemoryLoad, 0, 99);

            const double bytesToGb = 1024.0 * 1024.0 * 1024.0;
            outMetrics.ramTotalGb = (double)memStatus.ullTotalPhys / bytesToGb;
            outMetrics.ramFreeGb = (double)memStatus.ullAvailPhys / bytesToGb;
            outMetrics.ramUsedGb = outMetrics.ramTotalGb - outMetrics.ramFreeGb;
            if (outMetrics.ramUsedGb < 0.0) outMetrics.ramUsedGb = 0.0;
        }
    }

    if (neededFlags & METRIC_FLAG_NET) {
        ULONGLONG curTick = GetTickCount64();
        if (m_cachedAdapters.empty()) {
            if (m_lastAdapterRefreshTick == 0 || curTick >= m_lastAdapterRefreshTick + 20000) {
                RefreshNetworkAdapters();
                m_prevNetTick = curTick;
            }
        }

        uint64_t totalIn = 0;
        uint64_t totalOut = 0;
        uint64_t prevIn = 0;
        uint64_t prevOut = 0;
        bool anyValid = false;

        for (auto& ad : m_cachedAdapters) {
            MIB_IF_ROW2 row = { 0 };
            row.InterfaceLuid = ad.luid;
            if (GetIfEntry2(&row) == NO_ERROR) {
                if (row.OperStatus == IfOperStatusUp) {
                    if (ad.wasUp) {
                        totalIn += row.InOctets;
                        totalOut += row.OutOctets;
                        prevIn += ad.prevInBytes;
                        prevOut += ad.prevOutBytes;
                    }
                    ad.prevInBytes = row.InOctets;
                    ad.prevOutBytes = row.OutOctets;
                    ad.wasUp = true;
                    anyValid = true;
                } else {
                    ad.wasUp = false;
                }
            }
        }

        if (!anyValid) {
            if (curTick >= m_lastAdapterRefreshTick + 20000) {
                RefreshNetworkAdapters();
            }
        } else if (m_prevNetTick > 0 && curTick > m_prevNetTick) {
            double elapsedSec = (curTick - m_prevNetTick) / 1000.0;
            if (elapsedSec > 0.05) {
                uint64_t deltaIn = (totalIn >= prevIn) ? (totalIn - prevIn) : 0;
                uint64_t deltaOut = (totalOut >= prevOut) ? (totalOut - prevOut) : 0;

                outMetrics.netDownMbps = (deltaIn * 8.0) / (elapsedSec * 1000000.0);
                outMetrics.netUpMbps = (deltaOut * 8.0) / (elapsedSec * 1000000.0);

                outMetrics.netDownMBps = (double)deltaIn / (elapsedSec * 1024.0 * 1024.0);
                outMetrics.netUpMBps = (double)deltaOut / (elapsedSec * 1024.0 * 1024.0);
            }
        }

        m_prevNetTick = curTick;
    }
}
