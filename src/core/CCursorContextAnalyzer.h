#ifndef FCPP_AGENT_CCURSORCONTEXTANALYZER_H
#define FCPP_AGENT_CCURSORCONTEXTANALYZER_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fcpp-agent/IClang.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{
struct SCursorContextResult
{
    bool m_ok = true;
    bool m_context_applicable = false;
    bool m_llm_called = false;
    bool m_escalated = false;
    ELlmRole m_used_role = ELlmRole::Count;
    std::optional<SClangSymbol> m_symbol;
    std::string m_source_context;
    std::string m_error;
};

class CCursorContextAnalyzer
{
public:
    CCursorContextAnalyzer(const IClang& clang, CLlmLayer& llm);

    SCursorContextResult assess(const std::string& current_request,
                                const std::filesystem::path& source_file,
                                std::uint32_t line,
                                std::uint32_t column,
                                const std::vector<std::string>& compile_arguments) const;

private:
    const IClang& m_clang;
    CLlmLayer& m_llm;
};
} // namespace fcpp

#endif
