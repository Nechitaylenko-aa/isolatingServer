#include "core/CCursorContextAnalyzer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace fcpp
{
namespace
{
constexpr std::size_t kMaxCursorContextBytes = 24000;

bool read_cursor_source(const SClangSymbol& symbol, std::string& source, std::string& error)
{
    if (symbol.m_location.m_file.empty() || symbol.m_location.m_line == 0)
    {
        error = "clang symbol has no source range";
        return false;
    }

    std::ifstream input(symbol.m_location.m_file);
    if (!input.is_open())
    {
        error = "cannot open cursor source file: " + symbol.m_location.m_file;
        return false;
    }

    const std::uint32_t first_line = symbol.m_location.m_line;
    const std::uint32_t last_line = std::max(first_line,
        symbol.m_location.m_end_line == 0 ? first_line : symbol.m_location.m_end_line);
    std::string line;
    std::uint32_t current_line = 0;
    std::size_t bytes = 0;
    while (std::getline(input, line))
    {
        ++current_line;
        if (current_line < first_line)
            continue;
        if (current_line > last_line)
            break;

        const std::size_t remaining = kMaxCursorContextBytes - bytes;
        if (line.size() + 1 > remaining)
        {
            source.append(line, 0, remaining);
            source += "\n[CURSOR CONTEXT TRUNCATED]";
            break;
        }
        source += line;
        source += '\n';
        bytes += line.size() + 1;
        if (bytes >= kMaxCursorContextBytes)
        {
            source += "[CURSOR CONTEXT TRUNCATED]";
            break;
        }
    }

    if (source.empty())
    {
        error = "cursor source range is empty or outside the source file";
        return false;
    }
    return true;
}

std::string normalize_answer(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    std::string answer = text.substr(first, last - first + 1);
    for (char& ch : answer)
    {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return answer;
}
}

CCursorContextAnalyzer::CCursorContextAnalyzer(const IClang& clang, CLlmLayer& llm)
    : m_clang(clang)
    , m_llm(llm)
{
}

SCursorContextResult CCursorContextAnalyzer::assess(
    const std::string& current_request,
    const std::filesystem::path& source_file,
    std::uint32_t line,
    std::uint32_t column,
    const std::vector<std::string>& compile_arguments) const
{
    SCursorContextResult result;
    if (source_file.empty() || line == 0 || column == 0)
    {
        return result;
    }
    if (current_request.empty())
    {
        result.m_ok = false;
        result.m_error = "current request is empty";
        return result;
    }

    result.m_symbol = m_clang.cursor_at(source_file, line, column, compile_arguments);
    if (!result.m_symbol.has_value())
    {
        // No semantic entity at this position: do not ask the LLM.
        return result;
    }

    const SClangSymbol& symbol = *result.m_symbol;
    if (!read_cursor_source(symbol, result.m_source_context, result.m_error))
    {
        result.m_ok = false;
        return result;
    }

    std::ostringstream description;
    description << "name: " << symbol.m_name
                << "\nqualified name: " << symbol.m_qualified_name
                << "\nsignature: " << symbol.m_signature
                << "\nfile: " << symbol.m_location.m_file
                << "\nline: " << symbol.m_location.m_line
                << "\ncolumn: " << symbol.m_location.m_column
                << "\nis definition: " << (symbol.m_is_definition ? "yes" : "no")
                << "\naccess: " << symbol.m_access;

    const std::string prompt =
        "Decide whether the C++ entity under the editor cursor is needed to answer "
        "the current user request. Answer YES only if the request refers to this entity "
        "or understanding/answering the request materially benefits from using it. "
        "Otherwise answer NO. Return exactly YES or NO.";
    const std::string input =
        "CURRENT REQUEST:\n" + current_request
        + "\n\nCURSOR ENTITY:\n" + description.str();

    result.m_llm_called = true;
    const auto decision = m_llm.request<bool>(
        ELlmRole::ThinkerLocal, prompt, input,
        [](const std::string& text, bool& value, std::string& error)
        {
            const std::string answer = normalize_answer(text);
            if (answer == "YES")
            {
                value = true;
                return true;
            }
            if (answer == "NO")
            {
                value = false;
                return true;
            }
            error = "expected exactly YES or NO";
            return false;
        });

    result.m_ok = decision.m_ok;
    result.m_context_applicable = decision.m_ok && decision.m_value;
    result.m_escalated = decision.m_escalated;
    result.m_used_role = decision.m_used_role;
    result.m_error = decision.m_error;
    return result;
}
} // namespace fcpp
