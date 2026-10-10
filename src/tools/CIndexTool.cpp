/**
 * @file CIndexTool.cpp
 * @brief Реализация детерминированного инструмента над индексом.
 */

#include "tools/CIndexTool.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "fcpp-agent/ICompilerIndex.h"
#include "index/CSQLiteIndex.h"
#include "index/CSqlCompilerIndex.h"
#include "tools/CToolContext.h"

namespace fcpp
{

namespace
{

/** @brief Имена инструментов, индексированные по EToolName. */
static const char* const m_tool_names[static_cast<std::size_t>(EToolName::Count)]
{
    "outline",
    "callers",
    "callees",
    "refs",
    "includes",
    "overrides",
    "dispatch",
    "def",
    "ancestors",
    "derived",
    "search",
    "source"
};

/** @brief Имена категорий сущностей, индексированные по ESymbolKind. */
static const char* const m_symbol_kind_names[static_cast<std::size_t>(ESymbolKind::Count)]
{
    "namespace",
    "class",
    "struct",
    "union",
    "enum",
    "enum_constant",
    "typedef",
    "function",
    "method",
    "constructor",
    "destructor",
    "variable",
    "field",
    "parameter"
};

/**
 * @brief Проверить, что строка похожа на USR.
 * @param text Строка.
 * @return true если это USR.
 */
bool looks_like_usr(const std::string& text)
{
    return text.rfind("c:", 0) == 0;
}

/**
 * @brief Сформировать результат-ошибку.
 * @param kind Категория ошибки.
 * @param message Сообщение.
 * @return Результат с ошибкой.
 */
SToolResult make_error(EErrorKind kind, const std::string& message)
{
    SToolResult result;
    result.m_ok = false;
    result.m_error = kind;
    result.m_message = message;
    return result;
}

/**
 * @brief Получить отображаемое имя типа по USR.
 * @param context Контекст.
 * @param qname Квалифицированное имя (если известно).
 * @param usr USR.
 * @return Человекочитаемое имя.
 */
std::string resolve_name(CToolContext& context, const std::string& qname, const std::string& usr)
{
    if (!usr.empty())
    {
        const std::optional<SSymbol> symbol = context.index().find_symbol_by_usr(usr);
        if (symbol.has_value() && !symbol->m_qualified_name.empty())
        {
            return symbol->m_qualified_name;
        }
    }
    if (!qname.empty())
    {
        return qname;
    }
    return usr;
}

} // namespace

const char* tool_name(EToolName name)
{
    return m_tool_names[static_cast<std::size_t>(name)];
}

namespace
{

/** @brief Краткие описания инструментов для модели (ReAct), по EToolName. */
static const char* const m_tool_descriptions[static_cast<std::size_t>(EToolName::Count)]
{
    "List members (methods/fields/nested types) of a class or struct by qualified name.",
    "List functions that call the given symbol.",
    "List symbols called by the given function.",
    "List locations where the given symbol is referenced/used.",
    "List files included by the given file path.",
    "List methods that override the given virtual method.",
    "Resolve a virtual method to all its implementations in derived classes.",
    "Return the canonical definition of a symbol.",
    "List base classes (ancestors) of the given class.",
    "List classes derived from the given class.",
    "Find symbols by (qualified) name.",
    "Return the source code slice (body/signature) of a symbol.",
};

} // namespace

SModelTool tool_schema(EToolName name)
{
    SModelTool schema;
    schema.m_name = tool_name(name);
    schema.m_description = m_tool_descriptions[static_cast<std::size_t>(name)];

    SModelToolParam target;
    target.m_name = (name == EToolName::Includes) ? "path" : "name";
    target.m_type = "string";
    target.m_description = (name == EToolName::Includes)
                               ? "File path (relative to project root)."
                               : "Qualified name (or USR) of the target symbol.";
    target.m_required = true;
    schema.m_params.push_back(std::move(target));
    return schema;
}

std::vector<SModelTool> tool_schemas()
{
    std::vector<SModelTool> schemas;
    schemas.reserve(static_cast<std::size_t>(EToolName::Count));
    for (std::size_t index = 0; index < static_cast<std::size_t>(EToolName::Count); ++index)
    {
        schemas.push_back(tool_schema(static_cast<EToolName>(index)));
    }
    return schemas;
}

std::string format_tool_result(const SToolResult& result)
{
    if (!result.m_ok)
    {
        std::string text = std::string("ERROR: ") + error_kind_name(result.m_error) + ": " + result.m_message;
        if (!result.m_names.empty())
        {
            text += "\nCandidates:";
            for (const std::string& name : result.m_names)
            {
                text += "\n  " + name;
            }
        }
        return text;
    }

    std::string text;
    for (const SSymbol& symbol : result.m_symbols)
    {
        text += symbol_kind_name(symbol.m_kind);
        text += " ";
        text += symbol.m_qualified_name;
        if (!symbol.m_signature.empty())
        {
            text += "  // ";
            text += symbol.m_signature;
        }
        text += "\n";
    }
    for (const std::string& name : result.m_names)
    {
        text += name;
        text += "\n";
    }
    for (const SSymbolLocation& location : result.m_locations)
    {
        text += location.m_file + ":" + std::to_string(location.m_range.m_line);
        text += "\n";
    }
    if (!result.m_text.empty())
    {
        text += result.m_text;
        text += "\n";
    }
    if (text.empty())
    {
        text = "(no matches)\n";
    }
    return text;
}

const char* symbol_kind_name(ESymbolKind kind)
{
    return m_symbol_kind_names[static_cast<std::size_t>(kind)];
}

CIndexTool::CIndexTool(EToolName name)
    : m_name(name)
{
}

EToolName CIndexTool::name() const
{
    return m_name;
}

bool CIndexTool::resolve_usr(const SToolRequest& request,
                             CToolContext& context,
                             std::string& out_usr,
                             std::vector<SSymbol>& out_candidates,
                             SError& out_error) const
{
    out_candidates.clear();
    if (!request.m_file.empty() && request.m_line > 0 && context.clang() != nullptr)
    {
        std::vector<std::string> arguments;
        context.index().load_file_arguments(request.m_file, arguments);

        const std::optional<SClangSymbol> symbol =
            context.clang()->cursor_at(request.m_file, request.m_line, request.m_column, arguments);
        if (symbol.has_value() && !symbol->m_usr.empty())
        {
            out_usr = symbol->m_usr;
            return true;
        }
    }

    if (looks_like_usr(request.m_target))
    {
        out_usr = request.m_target;
        return true;
    }

    // Для квалифицированного имени ищем по последнему сегменту и фильтруем по суффиксу.
    std::string lookup_name = request.m_target;
    const std::size_t separator = request.m_target.rfind("::");
    if (separator != std::string::npos)
    {
        lookup_name = request.m_target.substr(separator + 2);
    }

    std::vector<SSymbol> candidates = context.index().find_symbols_by_name(lookup_name);
    if (separator != std::string::npos)
    {
        std::vector<SSymbol> filtered;
        const std::string suffix = "::" + request.m_target;
        for (const SSymbol& symbol : candidates)
        {
            if (symbol.m_qualified_name == request.m_target
                    || symbol.m_qualified_name.ends_with(suffix))
            {
                filtered.push_back(symbol);
            }
        }
        candidates = std::move(filtered);
    }

    if (candidates.empty())
    {
        out_error.m_kind = EErrorKind::SymbolNotFound;
        out_error.m_message = "symbol not found: " + request.m_target;
        return false;
    }

    if (candidates.size() == 1)
    {
        out_usr = candidates.front().m_usr;
        return true;
    }

    // Предпочтение определению.
    std::vector<SSymbol> definitions;
    for (const SSymbol& symbol : candidates)
    {
        if (symbol.m_is_definition)
        {
            definitions.push_back(symbol);
        }
    }
    if (definitions.size() == 1)
    {
        out_usr = definitions.front().m_usr;
        return true;
    }

    // Предпочтение типу при неоднозначности (например, класс и его конструктор).
    std::vector<SSymbol> types;
    for (const SSymbol& symbol : candidates)
    {
        if (symbol.m_kind == ESymbolKind::Class
                || symbol.m_kind == ESymbolKind::Struct
                || symbol.m_kind == ESymbolKind::Union
                || symbol.m_kind == ESymbolKind::Enum)
        {
            types.push_back(symbol);
        }
    }
    if (types.size() == 1)
    {
        out_usr = types.front().m_usr;
        return true;
    }

    // Неоднозначность: отдаём кандидатов, чтобы модель выбрала следующий вызов.
    out_candidates = candidates;
    out_error.m_kind = EErrorKind::Ambiguous;
    out_error.m_message = "ambiguous target: " + request.m_target
                          + " (candidates listed; call again with a qualified name)";
    return false;
}

void CIndexTool::apply_limit(const SToolRequest& request, SToolResult& result)
{
    if (request.m_limit == 0)
    {
        return;
    }

    if (result.m_symbols.size() > request.m_limit)
    {
        result.m_symbols.resize(request.m_limit);
    }
    if (result.m_locations.size() > request.m_limit)
    {
        result.m_locations.resize(request.m_limit);
    }
    if (result.m_names.size() > request.m_limit)
    {
        result.m_names.resize(request.m_limit);
    }
}

SToolResult CIndexTool::execute(const SToolRequest& request, CToolContext& context) const
{
    if (m_name == EToolName::Search)
    {
        const std::vector<SSymbol> matches = context.index().find_symbols_by_name(request.m_target);
        if (matches.empty())
        {
            return make_error(EErrorKind::SymbolNotFound, "symbol not found: " + request.m_target);
        }
        SToolResult result;
        result.m_ok = true;
        result.m_error = EErrorKind::Ok;
        result.m_symbols = matches;
        apply_limit(request, result);
        return result;
    }

    if (m_name == EToolName::Includes)
    {
        const std::optional<long long> file_id = context.index().find_file_id(request.m_target);
        if (!file_id.has_value())
        {
            return make_error(EErrorKind::SymbolNotFound, "file not indexed: " + request.m_target);
        }
        SToolResult result;
        result.m_ok = true;
        result.m_error = EErrorKind::Ok;
        result.m_names = context.graph().file_includes(file_id.value());
        apply_limit(request, result);
        return result;
    }

    std::string usr;
    std::vector<SSymbol> candidates;
    SError error;
    if (!resolve_usr(request, context, usr, candidates, error))
    {
        SToolResult failed;
        failed.m_ok = false;
        failed.m_error = error.m_kind;
        failed.m_message = error.m_message;
        // Для модели: список кандидатов (qualified_name) при неоднозначности.
        for (const SSymbol& symbol : candidates)
        {
            failed.m_names.push_back(symbol.m_qualified_name);
        }
        return failed;
    }

    SToolResult result;
    result.m_ok = true;
    result.m_error = EErrorKind::Ok;

    switch (m_name)
    {
        case EToolName::Outline:
            result.m_symbols = context.index().outline(usr);
            break;
        case EToolName::Callers:
            result.m_symbols = context.index().callers(usr);
            break;
        case EToolName::Callees:
            result.m_symbols = context.index().callees(usr);
            break;
        case EToolName::References:
            result.m_locations = context.index().references(usr);
            break;
        case EToolName::Overrides:
        {
            const std::vector<std::string> entries = context.graph().overrides(usr);
            for (const std::string& entry : entries)
            {
                result.m_names.push_back(resolve_name(context, {}, entry));
            }
            break;
        }
        case EToolName::Dispatch:
        {
            const std::vector<std::string> targets = context.graph().dispatch_targets(usr);
            for (const std::string& target : targets)
            {
                result.m_names.push_back(resolve_name(context, {}, target));
            }
            break;
        }
        case EToolName::Definition:
        {
            const std::optional<SSymbol> symbol = context.index().find_symbol_by_usr(usr);
            if (symbol.has_value())
            {
                result.m_symbols.push_back(symbol.value());
            }
            break;
        }
        case EToolName::Ancestors:
        {
            const std::vector<SBaseType> ancestors = context.graph().ancestors(usr);
            for (const SBaseType& base : ancestors)
            {
                result.m_names.push_back(resolve_name(context, base.m_qname, base.m_usr));
            }
            break;
        }
        case EToolName::Derived:
        {
            const std::vector<SBaseType> derived = context.graph().derived_types(usr);
            for (const SBaseType& entry : derived)
            {
                result.m_names.push_back(resolve_name(context, entry.m_qname, entry.m_usr));
            }
            break;
        }
        case EToolName::Source:
        {
            const std::optional<SSourceSlice> slice = context.index().get_source_slice(usr);
            if (!slice.has_value())
            {
                return make_error(EErrorKind::IoError, "source unavailable: " + request.m_target);
            }
            result.m_text = slice->m_text;
            break;
        }
        default:
            return make_error(EErrorKind::InvalidRequest, "unsupported tool");
    }

    apply_limit(request, result);
    return result;
}

} // namespace fcpp
