/**
 * @file CIndexFactProvider.cpp
 * @brief Сбор фактов о коде через Registry инструментов над индексом.
 */

#include "core/CIndexFactProvider.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fcpp-agent/ITool.h"
#include "index/CSQLiteIndex.h"
#include "tools/CToolContext.h"
#include "tools/CToolRegistry.h"

namespace fcpp
{
namespace
{

/** @brief Лимит элементов на один инструмент (Zero-Window, §64). */
constexpr std::uint32_t k_section_limit = 20;

/**
 * @brief Выполнить инструмент и превратить результат в секцию.
 *
 * Технический сбой инструмента не маскируется под «данных нет»: он
 * записывается в текст секции явно, чтобы модель не выдумывала факты.
 *
 * @param registry Реестр инструментов.
 * @param context Контекст доступа к индексу.
 * @param tool Инструмент.
 * @param target Цель.
 * @param title Заголовок секции.
 * @param out_sections Накопитель секций.
 */
void add_section(const CToolRegistry& registry,
                 CToolContext& context,
                 EToolName tool,
                 const std::string& target,
                 const std::string& title,
                 std::vector<SFactSection>& out_sections)
{
    if (target.empty())
    {
        return;
    }

    SToolRequest request;
    request.m_tool = tool;
    request.m_target = target;
    request.m_limit = k_section_limit;

    const SToolResult result = registry.execute(tool, request, context);
    if (!result.m_ok)
    {
        // NotFound — штатное отсутствие данных, остальное — видимый сбой.
        std::string text = format_tool_result(result);
        if (result.m_error != EErrorKind::Ok)
        {
            text = "[tool '" + std::string(tool_name(tool)) + "' failed: "
                + result.m_message + "]\n" + text;
        }
        if (text.empty())
        {
            return;
        }
        out_sections.push_back(SFactSection{title, text});
        return;
    }

    const std::string text = format_tool_result(result);
    if (text.empty())
    {
        return;
    }
    out_sections.push_back(SFactSection{title, text});
}

} // namespace

CIndexFactProvider::CIndexFactProvider(std::filesystem::path db_path,
                                       std::filesystem::path project_root,
                                       IClang* clang)
    : m_db_path(std::move(db_path))
    , m_project_root(std::move(project_root))
    , m_clang(clang)
{
}

SFactResult CIndexFactProvider::collect(const SFactRequest& request) const
{
    SFactResult result;

    // Failure text must be actionable: without the path and the failing stage
    // the log only says "something is wrong" and cannot be debugged.
    if (m_db_path.empty())
    {
        result.m_ok = false;
        // No absolute paths: the text reaches the journal and the client.
        result.m_error = "project index path is not configured (empty db_path); "
                         "entity=" + request.m_qualified_name;
        return result;
    }

    std::error_code exists_error;
    const bool exists = std::filesystem::exists(m_db_path, exists_error);

    CToolContext context(m_db_path, m_project_root, m_clang);
    if (!context.open())
    {
        result.m_ok = false;
        result.m_error = "cannot open project index: index_db_exists="
            + std::string(exists ? "yes" : "no")
            + (exists_error ? std::string(", stat_error=") + exists_error.message() : std::string())
            + ", stage=CSQLiteIndex::open_db (open or PRAGMA failed)"
            + ", entity=" + request.m_qualified_name;
        return result;
    }

    // An entity name may come from the model, not from libclang. Then it is text,
    // not a fact: verify that the entity is actually declared in the expected
    // file before handing its facts to the answer model.
    if (!request.m_expected_file.empty() && !request.m_qualified_name.empty())
    {
        const std::vector<SSymbol> declared =
            context.index().find_symbols_by_name(request.m_qualified_name);
        bool found_in_file = false;
        for (const SSymbol& symbol : declared)
        {
            if (symbol.m_location.m_file == request.m_expected_file
                && symbol.m_is_definition)
            {
                found_in_file = true;
                break;
            }
        }
        if (!found_in_file)
        {
            result.m_rejected = true;
            result.m_error = "entity is not declared in the expected file: "
                + request.m_qualified_name + " @ " + request.m_expected_file;
            return result;
        }
    }

    const CToolRegistry registry;
    const std::string target = !request.m_qualified_name.empty()
        ? request.m_qualified_name : request.m_name;

    // Порядок секций фиксирован: определение, состав, связи.
    add_section(registry, context, EToolName::Definition, target,
                "Definition", result.m_sections);
    add_section(registry, context, EToolName::Outline, target,
                "Members", result.m_sections);
    add_section(registry, context, EToolName::Ancestors, target,
                "Base types", result.m_sections);
    add_section(registry, context, EToolName::Derived, target,
                "Derived types", result.m_sections);
    add_section(registry, context, EToolName::Callers, target,
                "Callers", result.m_sections);

    return result;
}
} // namespace fcpp
