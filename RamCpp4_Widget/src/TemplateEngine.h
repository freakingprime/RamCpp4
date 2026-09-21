#pragma once
#include "SystemMonitor.h"
#include <string>
#include <vector>

enum TokenOpcode {
    OP_LITERAL,
    OP_CPU,
    OP_RAM_PERCENT,
    OP_RAM_FREE_PERCENT,
    OP_RAM_USED_GB,
    OP_RAM_FREE_GB,
    OP_RAM_TOTAL_GB,
    OP_NET_DOWN,
    OP_NET_UP,
    OP_NET_DOWN_MB,
    OP_NET_UP_MB,
    OP_NET_DOWN_INT,
    OP_NET_UP_INT,
    OP_NET_DOWN_MB_INT,
    OP_NET_UP_MB_INT
};

struct TemplateChunk {
    TokenOpcode opcode;
    std::wstring literalText;
};

class CompiledTemplate {
public:
    CompiledTemplate() = default;

    void Compile(const std::wstring& templateStr);
    void Format(const SystemMetrics& metrics, wchar_t* outBuf, size_t maxLen, bool enablePadding = true) const;

    uint32_t GetNeededMetricFlags() const { return m_neededFlags; }
    bool IsEmpty() const { return m_chunks.empty(); }

private:
    std::vector<TemplateChunk> m_chunks;
    uint32_t m_neededFlags = METRIC_FLAG_NONE;
};
