/**
 * @file CEvidenceCollector.cpp
 * @brief Реализация сборщика Evidence (Zero-Window, §56/§63).
 */

#include "core/CEvidenceCollector.h"

#include <cstddef>
#include <memory>
#include <string>

#include "fcpp-agent/IClang.h"
#include "fcpp-agent/ICompilerIndex.h"
#include "fcpp-agent/ITool.h"
#include "index/CClangFactory.h"
#include "index/CSQLiteIndex.h"
#include "index/CSqlCompilerIndex.h"
#include "tools/CToolContext.h"
#include "tools/CToolRegistry.h"

namespace fcpp
{

namespace
{

/** @brief Лимит строк источника в секции (§64). */
constexpr std::size_t k_max_source_lines = 80;

/** @brief Лимит элементов relationships. */
constexpr std::uint32_t k_max_items = 40;

/** @brief Лимит методов класса при fan-out (ADR-19). */
constexpr std::size_t k_fanout_max_methods = 10;

/** @brief Лимит реализаций метода при fan-out (ADR-19). */
constexpr std::size_t k_fanout_max_impls = 20;

/**
 * @brief Является ли категория вызываемой сущностью (метод/функция/ctor/dtor).
 * @param kind Категория.
 * @return true если вызываемая сущность.
 */
bool is_callable_kind(ESymbolKind kind)
{
    return kind == ESymbolKind::Function
        || kind == ESymbolKind::Method
        || kind == ESymbolKind::Constructor
        || kind == ESymbolKind::Destructor;
}

/**
 * @brief Извлечь короткое имя.
 * @param qualified Квалифицированное имя.
 * @return Короткое имя.
 */
std::string short_name(const std::string& qualified)
{
    const std::size_t separator = qualified.rfind("::");
    return (separator == std::string::npos) ? qualified : qualified.substr(separator + 2);
}

} // namespace

std::string CEvidenceCollector::limit_lines(const std::string& text, std::size_t max_lines)
{
    std::size_t count = 0;
    std::size_t position = 0;
    while (position < text.size())
    {
        const std::size_t newline = text.find('\n', position);
        if (newline == std::string::npos)
        {
            break;
        }
        ++count;
        if (count >= max_lines)
        {
            return text.substr(0, newline) + "\n/* ... elided ... */\n";
        }
        position = newline + 1;
    }
    return text;
}

CEvidenceCollector::CEvidenceCollector(const CConfigManager& config_manager,
                                       CProjectManager& project_manager)
    : m_config_manager(config_manager)
    , m_project_manager(project_manager)
{
}

bool CEvidenceCollector::collect(const SIntent& intent, const SPlan& plan, SEvidence& out_evidence) const
{
    out_evidence = SEvidence{};

    if (!m_project_manager.has_current())
    {
        return false;
    }

    const SProjectContext& context = m_project_manager.current();
    std::unique_ptr<IClang> clang = create_clang_backend(m_config_manager.config().m_clang_path);

    CToolContext tool_context(context.m_project.m_db_path, context.m_project.m_canonical_path, clang.get());
    if (!tool_context.open())
    {
        return false;
    }

    const CToolRegistry registry;

    // Fan-out (ADR-19, path A): класс × методы × реализации — детерминированно,
    // до обхода детерминированного плана. Для вопросов о переопределениях;
    // имя может быть классом даже если lite присвоил kind=METHOD.
    if (intent.m_relation == ERelation::Overrides && !intent.m_subject.m_name.empty())
    {
        if (collect_overrides_fanout(tool_context, intent.m_subject.m_name, registry, out_evidence))
        {
            return true;
        }
    }

    for (const EToolName tool : plan.m_tools)
    {
        SToolRequest request;
        request.m_tool = tool;
        request.m_target = intent.m_subject.m_name;
        request.m_file = intent.m_subject.m_name; // для includes (путь)
        request.m_limit = k_max_items;

        const SToolResult result = registry.execute(tool, request, tool_context);
        if (!result.m_ok)
        {
            continue; // инструмент не дал данных — пропускаем секцию
        }

        SEvidenceSection section;
        section.m_tool = tool;
        section.m_title = tool_name(tool);

        for (const SSymbol& symbol : result.m_symbols)
        {
            section.m_text += symbol_kind_name(symbol.m_kind);
            section.m_text += " ";
            section.m_text += symbol.m_qualified_name;
            if (!symbol.m_signature.empty() && symbol.m_signature != short_name(symbol.m_qualified_name))
            {
                section.m_text += "  // " + symbol.m_signature;
            }
            section.m_text += "\n";
        }
        for (const std::string& name : result.m_names)
        {
            section.m_text += name + "\n";
        }
        for (const SSymbolLocation& location : result.m_locations)
        {
            section.m_text += location.m_file + ":" + std::to_string(location.m_range.m_line) + "\n";
        }
        if (!result.m_text.empty())
        {
            section.m_text += limit_lines(result.m_text, k_max_source_lines);
        }

        if (!section.m_text.empty())
        {
            out_evidence.m_sections.push_back(std::move(section));
        }
    }

    return !out_evidence.m_sections.empty();
}

bool CEvidenceCollector::collect_overrides_fanout(CToolContext& context,
                                                  const std::string& class_name,
                                                  const CToolRegistry& registry,
                                                  SEvidence& out_evidence) const
{
    if (class_name.empty())
    {
        return false;
    }

    // 1. Методы класса (outline резолвит целевой класс и отдаёт детей с USR).
    SToolRequest outline_request;
    outline_request.m_tool = EToolName::Outline;
    outline_request.m_target = class_name;
    const SToolResult outline = registry.execute(EToolName::Outline, outline_request, context);
    if (!outline.m_ok)
    {
        return false;
    }

    bool produced = false;
    std::size_t method_count = 0;
    for (const SSymbol& method : outline.m_symbols)
    {
        if (!is_callable_kind(method.m_kind) || method.m_usr.empty())
        {
            continue;
        }
        if (method_count >= k_fanout_max_methods)
        {
            break;
        }
        ++method_count;

        // 2. Реализации метода: базовая + все переопределения (по USR).
        std::vector<std::string> impls = context.graph().dispatch_targets(method.m_usr);
        if (impls.empty())
        {
            impls.push_back(method.m_usr);
        }

        SEvidenceSection section;
        section.m_tool = EToolName::Overrides;
        section.m_title = "override " + method.m_qualified_name;

        std::size_t impl_count = 0;
        for (const std::string& impl_usr : impls)
        {
            if (impl_count >= k_fanout_max_impls)
            {
                break;
            }
            ++impl_count;

            const std::optional<SSymbol> impl = context.index().find_symbol_by_usr(impl_usr);
            const std::string impl_name =
                impl.has_value() ? impl->m_qualified_name : impl_usr;

            section.m_text += "  " + impl_name + " -> ";
            const std::optional<SSourceSlice> slice = context.index().get_source_slice(impl_usr);
            if (!slice.has_value() || slice->m_text.empty())
            {
                section.m_text += "(no body)\n";
                continue;
            }

            std::string body = slice->m_text;
            // Схлопнуть переводы строк для компактной индикации заглушки.
            bool has_throw = body.find("throw") != std::string::npos;
            bool has_todo = body.find("TODO") != std::string::npos
                || body.find("FIXME") != std::string::npos;
            if (body.find_first_not_of(" \t\r\n{}") == std::string::npos)
            {
                section.m_text += "(stub: empty body)";
            }
            else if (has_throw)
            {
                section.m_text += "(stub: throws)";
            }
            else if (has_todo)
            {
                section.m_text += "(stub: TODO/FIXME)";
            }
            else
            {
                section.m_text += "(implemented)";
            }
            section.m_text += "\n";
        }

        out_evidence.m_sections.push_back(std::move(section));
        produced = true;
    }

    return produced;
}

} // namespace fcpp
