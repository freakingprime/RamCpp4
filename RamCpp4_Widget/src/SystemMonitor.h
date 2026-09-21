#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <cstdint>
#include <string>
#include <vector>

enum MetricFlags : uint32_t {
    METRIC_FLAG_NONE = 0,
    METRIC_FLAG_CPU  = 1 << 0,
    METRIC_FLAG_RAM  = 1 << 1,
    METRIC_FLAG_NET  = 1 << 2
};

struct SystemMetrics {
    int cpuPercent = 0;
    int ramPercent = 0;
    int ramFreePercent = 0;
    double ramUsedGb = 0.0;
    double ramFreeGb = 0.0;
    double ramTotalGb = 0.0;
    double netDownMbps = 0.0;
    double netUpMbps = 0.0;
    double netDownMBps = 0.0;
    double netUpMBps = 0.0;
};

struct CachedAdapterStats {
    NET_LUID luid;
    uint64_t prevInBytes = 0;
    uint64_t prevOutBytes = 0;
    bool wasUp = false;
};

class SystemMonitor {
public:
    SystemMonitor();

    // Sets the configured adapter (Auto / Name / IP) and invalidates cache if changed or forced
    void SetConfiguredAdapter(const std::wstring& adapterName, bool force = false);

    // Queries only metrics requested by neededFlags.
    void Update(uint32_t neededFlags, SystemMetrics& outMetrics);

private:
    // CPU state
    uint64_t m_prevIdleTime = 0;
    uint64_t m_prevKernelTime = 0;
    uint64_t m_prevUserTime = 0;
    bool m_isFirstCpuSample = true;

    // Network state
    std::wstring m_targetAdapter = L"Auto";
    std::vector<CachedAdapterStats> m_cachedAdapters;
    ULONGLONG m_prevNetTick = 0;

    void RefreshNetworkAdapters();
    static uint64_t FileTimeToUint64(const FILETIME& ft);
};
