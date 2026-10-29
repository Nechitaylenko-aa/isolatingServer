/**
 * @file CClang22.cpp
 * @brief Реализация IClang поверх libclang (версия 22).
 */

#include "index/CClang22.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <clang-c/Index.h>

namespace fcpp
{

namespace
{

/** @brief RAII-владелец строки libclang (CXString). */
class CClangString
{
public:
    /**
     * @brief Захватить строку libclang.
     * @param value Строка libclang.
     */
    explicit CClangString(CXString value)
        : m_value(value)
    {
    }

    /**
     * @brief Освободить строку libclang.
     */
    ~CClangString()
    {
        clang_disposeString(m_value);
    }

    CClangString(const CClangString&) = delete;
    CClangString& operator=(const CClangString&) = delete;

    /**
     * @brief Получить C-строку.
     * @return Указатель на строку (может быть nullptr).
     */
    const char* c_str() const
    {
        return clang_getCString(m_value);
    }

    /**
     * @brief Получить std::string.
     * @return Строка.
     */
    std::string str() const
    {
        const char* text = clang_getCString(m_value);
        return (text != nullptr) ? std::string(text) : std::string();
    }

private:
    CXString m_value;
};

/**
 * @brief Преобразовать CXString в std::string.
 * @param value Строка libclang.
 * @return Строка.
 */
std::string to_string(CXString value)
{
    const CClangString holder(value);
    return holder.str();
}

/**
 * @brief Сопоставить курсор clang категории ESymbolKind.
 * @param cursor Курсор.
 * @param out_kind Категория сущности.
 * @return true если курсор является индексируемой сущностью.
 */
bool map_symbol_kind(CXCursor cursor, ESymbolKind& out_kind)
{
    switch (clang_getCursorKind(cursor))
    {
        case CXCursor_Namespace:
            out_kind = ESymbolKind::Namespace;
            return true;
        case CXCursor_ClassDecl:
            out_kind = ESymbolKind::Class;
            return true;
        case CXCursor_StructDecl:
            out_kind = ESymbolKind::Struct;
            return true;
        case CXCursor_UnionDecl:
            out_kind = ESymbolKind::Union;
            return true;
        case CXCursor_EnumDecl:
            out_kind = ESymbolKind::Enum;
            return true;
        case CXCursor_EnumConstantDecl:
            out_kind = ESymbolKind::EnumConstant;
            return true;
        case CXCursor_TypedefDecl:
        case CXCursor_TypeAliasDecl:
            out_kind = ESymbolKind::Typedef;
            return true;
        case CXCursor_FunctionDecl:
            out_kind = ESymbolKind::Function;
            return true;
        case CXCursor_CXXMethod:
            out_kind = ESymbolKind::Method;
            return true;
        case CXCursor_Constructor:
            out_kind = ESymbolKind::Constructor;
            return true;
        case CXCursor_Destructor:
            out_kind = ESymbolKind::Destructor;
            return true;
        case CXCursor_VarDecl:
            out_kind = ESymbolKind::Variable;
            return true;
        case CXCursor_FieldDecl:
            out_kind = ESymbolKind::Field;
            return true;
        case CXCursor_ParmDecl:
            out_kind = ESymbolKind::Parameter;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Проверить, является ли сущность функцией/методом/конструктором.
 * @param kind Курсор.
 * @return true если это вызываемая сущность.
 */
bool is_function_like_kind(CXCursorKind kind)
{
    return kind == CXCursor_FunctionDecl
           || kind == CXCursor_CXXMethod
           || kind == CXCursor_Constructor
           || kind == CXCursor_Destructor;
}

/**
 * @brief Строка доступа из CX_CXXAccessSpecifier.
 * @param cursor Курсор.
 * @return "public"/"protected"/"private" или пустая строка.
 */
std::string access_string(CXCursor cursor)
{
    switch (clang_getCXXAccessSpecifier(cursor))
    {
        case CX_CXXPublic:
            return "public";
        case CX_CXXProtected:
            return "protected";
        case CX_CXXPrivate:
            return "private";
        default:
            return {};
    }
}

/**
 * @brief Извлечь локацию курсора.
 * @param cursor Курсор.
 * @return Локация с путём файла и диапазоном.
 */
SClangLocation location_of(CXCursor cursor)
{
    SClangLocation location;
    location.m_line = 0;
    location.m_column = 0;
    location.m_end_line = 0;
    location.m_end_column = 0;

    const CXSourceRange range = clang_getCursorExtent(cursor);
    CXFile file = nullptr;
    unsigned line = 0;
    unsigned column = 0;
    clang_getFileLocation(clang_getRangeStart(range), &file, &line, &column, nullptr);

    if (file != nullptr)
    {
        location.m_file = to_string(clang_getFileName(file));
    }
    location.m_line = line;
    location.m_column = column;

    CXFile end_file = nullptr;
    unsigned end_line = 0;
    unsigned end_column = 0;
    clang_getFileLocation(clang_getRangeEnd(range), &end_file, &end_line, &end_column, nullptr);
    location.m_end_line = end_line;
    location.m_end_column = end_column;

    return location;
}

/**
 * @brief Построить квалифицированное имя по семантическим родителям.
 * @param cursor Курсор.
 * @return Квалифицированное имя.
 */
std::string qualified_name_of(CXCursor cursor)
{
    std::vector<std::string> parts;
    CXCursor current = cursor;

    while (true)
    {
        const CXCursorKind kind = clang_getCursorKind(current);
        if (kind == CXCursor_TranslationUnit)
        {
            break;
        }

        if (kind != CXCursor_Namespace
                || !parts.empty())
        {
            const std::string spelling = to_string(clang_getCursorSpelling(current));
            if (!spelling.empty())
            {
                parts.push_back(spelling);
            }
        }

        CXCursor parent = clang_getCursorSemanticParent(current);
        if (clang_Cursor_isNull(parent)
                || clang_equalCursors(parent, current)
                || clang_getCursorKind(parent) == CXCursor_TranslationUnit)
        {
            break;
        }
        current = parent;
    }

    std::string result;
    for (std::size_t index = parts.size(); index > 0; --index)
    {
        if (!result.empty())
        {
            result += "::";
        }
        result += parts[index - 1];
    }
    return result;
}

/**
 * @brief Контекст обхода AST.
 */
struct SVisitContext
{
    SClangExtract* m_extract;               ///< Накопитель результатов.
    std::vector<std::string> m_function_stack; ///< Стек USR текущих функций.
};

/**
 * @brief Записать сущность, если курсор ею является.
 * @param cursor Курсор.
 * @param context Контекст обхода.
 */
bool fill_symbol(CXCursor cursor, SClangSymbol& symbol)
{
    ESymbolKind kind;
    if (!map_symbol_kind(cursor, kind))
    {
        return false;
    }

    symbol.m_usr = to_string(clang_getCursorUSR(cursor));
    symbol.m_name = to_string(clang_getCursorSpelling(cursor));
    symbol.m_qualified_name = qualified_name_of(cursor);
    symbol.m_signature = to_string(clang_getCursorDisplayName(cursor));
    symbol.m_kind = kind;
    symbol.m_access = access_string(cursor);
    symbol.m_location = location_of(cursor);
    symbol.m_is_definition = (clang_isCursorDefinition(cursor) != 0);
    symbol.m_is_virtual = false;
    symbol.m_is_pure_virtual = false;
    symbol.m_is_static = false;

    const CXCursorKind cursor_kind = clang_getCursorKind(cursor);
    if (cursor_kind == CXCursor_CXXMethod
            || cursor_kind == CXCursor_Constructor
            || cursor_kind == CXCursor_Destructor)
    {
        symbol.m_is_virtual = (clang_CXXMethod_isVirtual(cursor) != 0);
        symbol.m_is_pure_virtual = (clang_CXXMethod_isPureVirtual(cursor) != 0);
        symbol.m_is_static = (clang_CXXMethod_isStatic(cursor) != 0);
    }
    else if (cursor_kind == CXCursor_FunctionDecl)
    {
        symbol.m_is_static = (clang_Cursor_getStorageClass(cursor) == CX_SC_Static);
    }

    const CXCursor semantic_parent = clang_getCursorSemanticParent(cursor);
    if (!clang_Cursor_isNull(semantic_parent))
    {
        const CXCursorKind parent_kind = clang_getCursorKind(semantic_parent);
        if (parent_kind != CXCursor_TranslationUnit)
        {
            symbol.m_parent_usr = to_string(clang_getCursorUSR(semantic_parent));
        }
    }

    if (cursor_kind == CXCursor_CXXMethod)
    {
        CXCursor* overridden = nullptr;
        unsigned count = 0;
        clang_getOverriddenCursors(cursor, &overridden, &count);
        if (overridden != nullptr && count > 0)
        {
            symbol.m_overridden_usr = to_string(clang_getCursorUSR(overridden[0]));
        }
        if (overridden != nullptr)
        {
            clang_disposeOverriddenCursors(overridden);
        }
    }

    return true;
}

void record_symbol(CXCursor cursor, SVisitContext& context)
{
    if (clang_Location_isInSystemHeader(clang_getCursorLocation(cursor)) != 0)
    {
        return;
    }

    SClangSymbol symbol;
    if (!fill_symbol(cursor, symbol))
    {
        return;
    }

    context.m_extract->m_symbols.push_back(std::move(symbol));
}

/**
 * @brief Записать базовые типы класса.
 * @param cursor Курсор базового спецификатора.
 * @param class_usr USR производного класса.
 * @param context Контекст обхода.
 */
void record_base(CXCursor cursor, const std::string& class_usr, SVisitContext& context)
{
    SClangBase base;
    base.m_derived_usr = class_usr;
    base.m_base_qname = to_string(clang_getCursorSpelling(cursor));
    base.m_access = access_string(cursor);
    base.m_is_virtual = (clang_isVirtualBase(cursor) != 0);

    const CXType base_type = clang_getCursorType(cursor);
    const CXCursor base_decl = clang_getTypeDeclaration(base_type);
    if (!clang_Cursor_isNull(base_decl))
    {
        base.m_base_usr = to_string(clang_getCursorUSR(base_decl));
        if (base.m_base_qname.empty())
        {
            base.m_base_qname = to_string(clang_getCursorSpelling(base_decl));
        }
    }

    context.m_extract->m_bases.push_back(std::move(base));
}

/**
 * @brief Записать вызов/ссылку по decl-ref курсору.
 * @param cursor Курсор использования.
 * @param is_call Признак вызова.
 * @param context Контекст обхода.
 */
void record_reference(CXCursor cursor, bool is_call, SVisitContext& context)
{
    if (clang_Location_isInSystemHeader(clang_getCursorLocation(cursor)) != 0)
    {
        return;
    }

    const CXCursor referenced = clang_getCursorReferenced(cursor);
    if (clang_Cursor_isNull(referenced))
    {
        return;
    }

    if (clang_Location_isInSystemHeader(clang_getCursorLocation(referenced)) != 0)
    {
        return;
    }

    const std::string symbol_usr = to_string(clang_getCursorUSR(referenced));
    if (symbol_usr.empty())
    {
        return;
    }

    const SClangLocation location = location_of(cursor);
    const CXCursorKind referenced_kind = clang_getCursorKind(referenced);

    SClangReference reference;
    reference.m_symbol_usr = symbol_usr;
    reference.m_kind = is_call ? "call" : "value";
    reference.m_location = location;
    context.m_extract->m_references.push_back(std::move(reference));

    if (is_call && is_function_like_kind(referenced_kind))
    {
        std::string caller_usr;
        if (!context.m_function_stack.empty())
        {
            caller_usr = context.m_function_stack.back();
        }
        else
        {
            CXCursor enclosing = clang_getCursorSemanticParent(cursor);
            while (!clang_Cursor_isNull(enclosing)
                   && !is_function_like_kind(clang_getCursorKind(enclosing)))
            {
                enclosing = clang_getCursorSemanticParent(enclosing);
            }
            if (!clang_Cursor_isNull(enclosing))
            {
                caller_usr = to_string(clang_getCursorUSR(enclosing));
            }
        }

        if (!caller_usr.empty())
        {
            SClangCall call;
            call.m_caller_usr = caller_usr;
            call.m_callee_usr = symbol_usr;
            call.m_callee_qname = to_string(clang_getCursorSpelling(referenced));
            call.m_location = location;
            context.m_extract->m_calls.push_back(std::move(call));
        }
    }
}

/**
 * @brief Записать директиву include.
 * @param cursor Курсор директивы.
 * @param context Контекст обхода.
 */
void record_include(CXCursor cursor, SVisitContext& context)
{
    SClangInclude include;
    include.m_from_file = location_of(cursor).m_file;
    include.m_is_angle = false;

    CXFile included_file = clang_getIncludedFile(cursor);
    if (included_file != nullptr)
    {
        include.m_to_path = to_string(clang_getFileName(included_file));
    }
    else
    {
        include.m_to_path = to_string(clang_getCursorSpelling(cursor));
    }

    context.m_extract->m_includes.push_back(std::move(include));
}

/**
 * @brief Рекурсивный посетитель AST.
 * @param cursor Текущий курсор.
 * @param parent Родительский курсор.
 * @param client_data Контекст обхода.
 * @return Решение о продолжении обхода.
 */
CXChildVisitResult visit_cursor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    (void)parent;
    auto* context = static_cast<SVisitContext*>(client_data);
    const CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_CXXBaseSpecifier)
    {
        record_base(cursor, to_string(clang_getCursorUSR(parent)), *context);
    }
    else if (kind == CXCursor_InclusionDirective)
    {
        record_include(cursor, *context);
    }
    else
    {
        record_symbol(cursor, *context);

        if (kind == CXCursor_CallExpr)
        {
            record_reference(cursor, true, *context);
        }
        else if (kind == CXCursor_DeclRefExpr || kind == CXCursor_MemberRefExpr)
        {
            record_reference(cursor, false, *context);
        }
    }

    if (is_function_like_kind(kind))
    {
        const std::string usr = to_string(clang_getCursorUSR(cursor));
        context->m_function_stack.push_back(usr);
        clang_visitChildren(cursor, visit_cursor, client_data);
        context->m_function_stack.pop_back();
    }
    else
    {
        clang_visitChildren(cursor, visit_cursor, client_data);
    }

    return CXChildVisit_Continue;
}

/**
 * @brief Отфильтровать аргументы компиляции для libclang.
 *
 * Удаляются output/link-специфичные флаги (-o, -c, -MF/-MT/-MQ) и сам файл
 * исходника, поскольку libclang принимает исходный файл отдельным параметром.
 * @param arguments Аргументы из compile_commands.json.
 * @param source_file Разбираемый файл.
 * @return Очищенный список аргументов.
 */
std::vector<std::string> sanitize_arguments(const std::vector<std::string>& arguments,
                                            const std::filesystem::path& source_file)
{
    const std::string source_text = source_file.string();
    std::vector<std::string> filtered;
    filtered.reserve(arguments.size());

    for (std::size_t index = 0; index < arguments.size(); ++index)
    {
        const std::string& argument = arguments[index];

        if (argument == "-o" || argument == "-MF" || argument == "-MT" || argument == "-MQ")
        {
            ++index;
            continue;
        }
        if (argument == "-c" || argument == source_text)
        {
            continue;
        }
        if (argument.rfind("-o", 0) == 0 && argument.size() > 2)
        {
            continue;
        }

        filtered.push_back(argument);
    }

    return filtered;
}

/**
 * @brief Проверить, задаёт ли язык хотя бы один из аргументов.
 *
 * libclang уважает явный `-x`, поэтому автоматическое определение языка не
 * должно его перекрывать.
 * @param arguments Аргументы компиляции.
 * @return true если `-x <lang>` уже присутствует.
 */
bool has_explicit_language(const std::vector<std::string>& arguments)
{
    for (const std::string& argument : arguments)
    {
        if (argument == "-x")
        {
            return true;
        }
        if (argument.rfind("-x", 0) == 0 && argument.size() > 2)
        {
            return true;
        }
        if (argument.rfind("--language", 0) == 0)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Определить язык разбора по расширению файла.
 *
 * libclang выбирает язык по расширению, и для `.h` это C — в нём нет классов
 * и пространств имён, поэтому AST заголовка C++ оказывается пустым и курсор
 * не разрешается. Заголовки разбираются как C++-заголовки: поддерживаемый
 * стек проекта — C++20 (ADR-06).
 *
 * @param source_file Разбираемый файл.
 * @return Значение для `-x` либо пустая строка, если вмешиваться не нужно.
 */
std::string language_for_file(const std::filesystem::path& source_file)
{
    std::string extension = source_file.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character)
                   {
                       return static_cast<char>(std::tolower(character));
                   });

    if (extension == ".h" || extension == ".hpp" || extension == ".hh"
        || extension == ".hxx" || extension == ".h++" || extension == ".ipp")
    {
        return "c++-header";
    }
    if (extension == ".c")
    {
        return "c";
    }
    if (extension == ".cpp" || extension == ".cc" || extension == ".cxx"
        || extension == ".c++")
    {
        return "c++";
    }
    if (extension == ".m")
    {
        return "objective-c";
    }
    if (extension == ".mm")
    {
        return "objective-c++";
    }
    // Unknown extension: libclang decides on its own.
    return {};
}

/**
 * @brief Разобрать TU с заданными аргументами.
 * @param source_file Исходный файл.
 * @param arguments Аргументы компиляции.
 * @param out_index Выходной индекс.
 * @param out_tu Выходная единица трансляции.
 * @return true при успехе.
 */
bool parse_translation_unit(const std::filesystem::path& source_file,
                            const std::vector<std::string>& arguments,
                            CXIndex& out_index,
                            CXTranslationUnit& out_tu)
{
    out_index = clang_createIndex(0, 0);
    if (out_index == nullptr)
    {
        return false;
    }

    std::vector<std::string> storage = sanitize_arguments(arguments, source_file);

    // Never override an explicit language provided by the compile database.
    if (!has_explicit_language(storage))
    {
        const std::string language = language_for_file(source_file);
        if (!language.empty())
        {
            storage.push_back("-x");
            storage.push_back(language);
        }
    }

    std::vector<const char*> argv;
    argv.reserve(storage.size());
    for (const std::string& argument : storage)
    {
        argv.push_back(argument.c_str());
    }

    const unsigned options = CXTranslationUnit_KeepGoing
                             | CXTranslationUnit_DetailedPreprocessingRecord;

    const CXErrorCode result = clang_parseTranslationUnit2(
        out_index,
        source_file.string().c_str(),
        argv.data(),
        static_cast<int>(argv.size()),
        nullptr,
        0,
        options,
        &out_tu);

    return result == CXError_Success && out_tu != nullptr;
}

} // namespace

bool CClang22::is_available() const
{
    return !version().empty();
}

std::string CClang22::version() const
{
    return to_string(clang_getClangVersion());
}

bool CClang22::extract(const std::filesystem::path& source_file,
                       const std::vector<std::string>& arguments,
                       SClangExtract& out) const
{
    out = SClangExtract{};

    CXIndex index = nullptr;
    CXTranslationUnit translation_unit = nullptr;
    if (!parse_translation_unit(source_file, arguments, index, translation_unit))
    {
        if (translation_unit != nullptr)
        {
            clang_disposeTranslationUnit(translation_unit);
        }
        if (index != nullptr)
        {
            clang_disposeIndex(index);
        }
        return false;
    }

    SVisitContext context;
    context.m_extract = &out;
    clang_visitChildren(clang_getTranslationUnitCursor(translation_unit), visit_cursor, &context);

    const unsigned diagnostic_count = clang_getNumDiagnostics(translation_unit);
    for (unsigned index_value = 0; index_value < diagnostic_count; ++index_value)
    {
        CXDiagnostic diagnostic = clang_getDiagnostic(translation_unit, index_value);
        SClangDiagnostic entry;
        entry.m_message = to_string(clang_getDiagnosticSpelling(diagnostic));

        const enum CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(diagnostic);
        if (severity == CXDiagnostic_Error || severity == CXDiagnostic_Fatal)
        {
            entry.m_severity = "error";
        }
        else if (severity == CXDiagnostic_Warning)
        {
            entry.m_severity = "warning";
        }
        else
        {
            entry.m_severity = "note";
        }

        CXFile file = nullptr;
        unsigned line = 0;
        unsigned column = 0;
        clang_getFileLocation(clang_getDiagnosticLocation(diagnostic), &file, &line, &column, nullptr);
        if (file != nullptr)
        {
            entry.m_file = to_string(clang_getFileName(file));
        }
        entry.m_line = line;
        entry.m_column = column;

        out.m_diagnostics.push_back(std::move(entry));
        clang_disposeDiagnostic(diagnostic);
    }

    clang_disposeTranslationUnit(translation_unit);
    clang_disposeIndex(index);
    return true;
}

/**
 * @brief Проверить, соответствует ли сущность запрошенному уровню охвата (§21).
 * @param kind Категория сущности.
 * @param focus Запрошенный уровень.
 * @return true если категория попадает под уровень.
 */
static bool matches_focus(ESymbolKind kind, EFocusKind focus)
{
    if (focus == EFocusKind::Innermost)
    {
        return true;
    }
    if (focus == EFocusKind::Type)
    {
        return kind == ESymbolKind::Class
            || kind == ESymbolKind::Struct
            || kind == ESymbolKind::Union
            || kind == ESymbolKind::Enum;
    }
    if (focus == EFocusKind::Callable)
    {
        return kind == ESymbolKind::Function
            || kind == ESymbolKind::Method
            || kind == ESymbolKind::Constructor
            || kind == ESymbolKind::Destructor;
    }
    if (focus == EFocusKind::Namespace)
    {
        return kind == ESymbolKind::Namespace;
    }
    return false;
}

std::optional<SClangSymbol> CClang22::cursor_at(const std::filesystem::path& source_file,
                                                std::uint32_t line,
                                                std::uint32_t column,
                                                const std::vector<std::string>& arguments,
                                                EFocusKind focus) const
{
    CXIndex index = nullptr;
    CXTranslationUnit translation_unit = nullptr;
    if (!parse_translation_unit(source_file, arguments, index, translation_unit))
    {
        if (translation_unit != nullptr)
        {
            clang_disposeTranslationUnit(translation_unit);
        }
        if (index != nullptr)
        {
            clang_disposeIndex(index);
        }
        return std::nullopt;
    }

    const std::string target_file = source_file.string();
    CXFile file = clang_getFile(translation_unit, target_file.c_str());

    std::optional<SClangSymbol> result;
    if (file != nullptr)
    {
        const CXSourceLocation location =
            clang_getLocation(translation_unit, file, line, column);
        CXCursor cursor = clang_getCursor(translation_unit, location);

        std::optional<SClangSymbol> innermost;
        bool matched = false;
        while (!clang_Cursor_isNull(cursor)
               && clang_getCursorKind(cursor) != CXCursor_TranslationUnit)
        {
            SClangSymbol symbol;
            if (fill_symbol(cursor, symbol) && !symbol.m_usr.empty())
            {
                if (!innermost.has_value())
                {
                    innermost = symbol;
                }

                // Для контекстного фокуса параметр не должен заслонять метод:
                // сохраняем его как fallback, но продолжаем подъём по AST.
                if (focus == EFocusKind::Innermost && symbol.m_kind == ESymbolKind::Parameter)
                {
                    cursor = clang_getCursorSemanticParent(cursor);
                    continue;
                }

                if (matches_focus(symbol.m_kind, focus))
                {
                    result = std::move(symbol);
                    matched = true;
                    break;
                }
            }
            cursor = clang_getCursorSemanticParent(cursor);
        }

        // Вид не найден — сохраняем прежнее поведение (Innermost-совместимость).
        if (!matched)
        {
            result = std::move(innermost);
        }
    }

    clang_disposeTranslationUnit(translation_unit);
    clang_disposeIndex(index);
    return result;
}

} // namespace fcpp
