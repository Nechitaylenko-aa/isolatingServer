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
                             SError& out_error) const
{
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

    out_error.m_kind = EErrorKind::Ambiguous;
    out_error.m_message = "ambiguous target: " + request.m_target;
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
    SError error;
    if (!resolve_usr(request, context, usr, error))
    {
        return make_error(error.m_kind, error.m_message);
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
