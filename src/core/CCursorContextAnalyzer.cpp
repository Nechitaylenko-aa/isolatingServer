#include "core/CCursorContextAnalyzer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/** @brief Сколько объявлений показывать модели при выборе сущности. */
constexpr std::size_t kOutlineDeclarations = 120;

namespace fcpp
{
namespace
{

/**
 * @brief Оставить в строке не больше max_bytes байт по границе UTF-8.
 * @param text Исходная строка.
 * @param max_bytes Лимит байт.
 * @return Префикс строки без разрезанного символа UTF-8.
 */
std::string utf8_prefix(const std::string& text, std::size_t max_bytes)
{
    if (text.size() <= max_bytes)
    {
        return text;
    }
    std::size_t end = max_bytes;
    while (end > 0 && end < text.size()
           && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U)
    {
        --end;
    }
    return text.substr(0, end);
}

/**
 * @brief Извлечь строки объявлений из содержимого файла.
 *
 * Модели для решения «относится ли запрос к файлу» нужны имена и сигнатуры, а
 * не тела функций. Отбираются строки, похожие на объявления верхнего уровня:
 * `class`, `struct`, `enum`, `union`, `namespace`, прототипы функций и
 * определения верхнего уровня. Комментарии и пустые строки отбрасываются.
 *
 * @param content Содержимое файла.
 * @param max_lines Лимит числа отобранных строк.
 * @return Компактный список объявлений.
 */
std::string file_outline(const std::string& content, std::size_t max_lines)
{
    static const char* const k_markers[] = {
        "class ", "struct ", "enum ", "union ", "namespace ",
        "typedef ", "using ", "#include", "template "
    };

    std::string outline;
    std::size_t taken = 0;
    std::size_t number = 0;
    std::size_t start = 0;
    while (start <= content.size() && taken < max_lines)
    {
        const std::size_t end = content.find('\n', start);
        const std::size_t length =
            end == std::string::npos ? content.size() - start : end - start;
        std::string line = content.substr(start, length);
        ++number;

        const std::size_t first = line.find_first_not_of(" \t\r");
        if (first != std::string::npos)
        {
            line.erase(0, first);
        }
        const std::size_t last = line.find_last_not_of(" \t\r");
        if (last != std::string::npos)
        {
            line.erase(last + 1);
        }

        const bool is_comment = line.rfind("//", 0) == 0
            || line.rfind("/*", 0) == 0 || line.rfind("*", 0) == 0;
        if (!line.empty() && !is_comment)
        {
            bool keep = false;
            for (const char* marker : k_markers)
            {
                if (line.rfind(marker, 0) == 0)
                {
                    keep = true;
                    break;
                }
            }
            // A line ending with ';' or '{' at zero indent is a declaration or
            // a definition head, even inside a class.
            if (!keep && (line.back() == ';' || line.back() == '{'))
            {
                keep = true;
            }
            if (keep)
            {
                // Numbers keep the real coordinates: the model must not invent
                // its own line numbering.
                outline += std::to_string(number) + ": " + line + "\n";
                ++taken;
            }
        }

        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }

    if (outline.empty())
    {
        return "(no declarations found)";
    }
    return outline;
}

/**
 * @brief Прочитать срез файла окном вокруг строки курсора.
 *
 * Диапазон сущности здесь не используется намеренно: курсор внутри большого
 * класса иначе отправляет в модель весь класс. Окно гарантирует, что строка
 * курсора находится в центре среза и код вокруг неё связный.
 *
 * @param file Путь к файлу.
 * @param cursor_line Строка курсора (1-based).
 * @param radius_lines Сколько строк брать выше и ниже.
 * @param max_bytes Жёсткий лимит размера результата.
 * @param out_source Выходной срез.
 * @param out_truncated Признак усечения по лимиту байт.
 * @param out_error Причина сбоя.
 * @return true при успехе.
 */
bool read_cursor_slice(const std::filesystem::path& file,
                       std::uint32_t cursor_line,
                       std::size_t radius_lines,
                       std::size_t max_bytes,
                       std::string& out_source,
                       bool& out_truncated,
                       std::string& out_error)
{
    std::ifstream input(file);
    if (!input.is_open())
    {
        out_error = "cannot open cursor source file: " + file.filename().string();
        return false;
    }

    const std::uint32_t first_line = cursor_line > radius_lines
        ? cursor_line - static_cast<std::uint32_t>(radius_lines) : 1U;
    const std::uint32_t last_line =
        cursor_line + static_cast<std::uint32_t>(radius_lines);

    std::string line;
    std::uint32_t current_line = 0;
    while (std::getline(input, line))
    {
        ++current_line;
        if (current_line < first_line)
        {
            continue;
        }
        if (current_line > last_line)
        {
            break;
        }

        // Одна строка шире всего лимита (минифицированный код): режем саму
        // строку по границе UTF-8, иначе срез не поместится вообще.
        const std::string prefix = std::to_string(current_line) + ": ";
        if (prefix.size() + line.size() + 1U > max_bytes)
        {
            const std::size_t allowance =
                max_bytes > prefix.size() ? max_bytes - prefix.size() : 0U;
            out_source += prefix + utf8_prefix(line, allowance);
            out_truncated = true;
            break;
        }

        // Номер строки в срезе сохраняется: модель должна видеть реальные
        // координаты, а не пересчитанные с единицы.
        const std::string numbered = prefix + line + "\n";
        if (!out_source.empty()
            && out_source.size() + numbered.size() > max_bytes)
        {
            out_truncated = true;
            break;
        }
        out_source += numbered;
    }

    if (out_source.empty())
    {
        out_error = "cursor line is outside the source file";
        return false;
    }
    return true;
}

/**
 * @brief Убрать пробельные символы по краям.
 * @param text Строка.
 * @return Строка без ведущих и замыкающих пробелов.
 */
std::string trim_ascii(const std::string& text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

/**
 * @brief Привести ответ модели к верхнему регистру без краевых пробелов.
 * @param text Ответ модели.
 * @return Нормализованный ответ.
 */
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

/**
 * @brief Грубо оценить число токенов по байтам.
 *
 * Используется только для диагностики в логе. Точный подсчёт выполняет
 * провайдер (llama.cpp /tokenize); здесь намеренно консервативная оценка.
 *
 * @param text Текст.
 * @return Оценка токенов.
 */
std::size_t estimate_tokens(const std::string& text)
{
    return (text.size() + 2U) / 3U + 4U;
}

} // namespace

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
    const std::vector<std::string>& compile_arguments,
    const SCursorSliceLimits& limits) const
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
        // Семантической сущности в позиции нет: модель не привлекается.
        return result;
    }

    if (!read_cursor_slice(source_file, line, limits.m_radius_lines,
                           limits.m_max_bytes, result.m_source_context,
                           result.m_slice_truncated, result.m_error))
    {
        result.m_ok = false;
        return result;
    }
    result.m_slice_tokens_estimate = estimate_tokens(result.m_source_context);

    const SClangSymbol& symbol = *result.m_symbol;
    std::ostringstream description;
    description << "name: " << symbol.m_name
                << "\nqualified name: " << symbol.m_qualified_name
                << "\nsignature: " << symbol.m_signature
                << "\nfile: " << symbol.m_location.m_file
                << "\nline: " << symbol.m_location.m_line
                << "\ncolumn: " << symbol.m_location.m_column
                << "\nis definition: " << (symbol.m_is_definition ? "yes" : "no")
                << "\naccess: " << symbol.m_access
                << "\ncursor line: " << line;

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

bool read_file_slice(const std::filesystem::path& file,
                     const SFileSliceLimits& limits,
                     std::string& out_slice,
                     bool& out_truncated,
                     std::string& out_error)
{
    out_truncated = false;
    out_slice.clear();

    std::ifstream input(file, std::ios::binary);
    if (!input.is_open())
    {
        out_error = "cannot open file: " + file.filename().string();
        return false;
    }

    std::string raw((std::istreambuf_iterator<char>(input)),
                    std::istreambuf_iterator<char>());
    if (raw.empty())
    {
        out_error = "file is empty: " + file.filename().string();
        return false;
    }

    if (raw.size() > limits.m_max_bytes)
    {
        out_slice = utf8_prefix(raw, limits.m_max_bytes)
            + "\n[CONTENT TRUNCATED BY SIZE LIMIT]\n";
        out_truncated = true;
        return true;
    }

    out_slice = std::move(raw);
    return true;
}

SFileContextResult CCursorContextAnalyzer::assess_file(
    const std::string& current_request,
    const std::filesystem::path& source_file,
    const SFileSliceLimits& limits) const
{
    SFileContextResult result;
    if (source_file.empty())
    {
        return result;
    }
    if (current_request.empty())
    {
        result.m_ok = false;
        result.m_error = "current request is empty";
        return result;
    }

    if (!read_file_slice(source_file, limits, result.m_content,
                         result.m_truncated, result.m_error))
    {
        result.m_ok = false;
        return result;
    }

    // Relevance is decided on structure, not on content: declarations and
    // signatures are enough to tell whether the request concerns this file.
    // Sending a 10-24 KB slice would exceed the thinking role's budget and get
    // truncated, so the answer would degrade into an unparseable reply.
    const std::string outline = file_outline(result.m_content, kOutlineDeclarations);
    const std::string prompt =
        "Decide whether the current user request concerns the supplied file. "
        "Answer YES only if answering the request requires reading or discussing "
        "this file (its content, structure, purpose, or something located in it). "
        "Answer NO if the request is unrelated to this file. "
        "Return exactly YES or NO.";
    const std::string input =
        "CURRENT REQUEST:\n" + current_request
        + "\n\nFILE: " + source_file.filename().string()
        + "\n\nFILE DECLARATIONS:\n" + outline;

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
    result.m_used_role = decision.m_used_role;
    result.m_error = decision.m_error;
    if (!result.m_context_applicable)
    {
        // Content is only handed over when the file is actually relevant.
        result.m_content.clear();
    }
    return result;
}

SFileEntityChoice CCursorContextAnalyzer::choose_file_entity(
    const std::string& current_request,
    const std::filesystem::path& source_file,
    const std::string& content) const
{
    SFileEntityChoice result;
    if (current_request.empty() || content.empty())
    {
        return result;
    }

    const std::string outline = file_outline(content, kOutlineDeclarations);
    const std::string prompt =
        "The user asks about a file. Identify whether answering requires one "
        "specific entity (class, struct, enum, or free function) declared AT TOP "
        "LEVEL in this file. "
        "Reply with EXACTLY ONE of these forms and nothing else:\n"
        "ONE <qualified-name>   — exactly one such entity is needed\n"
        "SEVERAL                — more than one entity may be needed\n"
        "NONE                   — no single entity is enough\n"
        "A name that is not declared in this file is a wrong answer.";
    const std::string input =
        "USER REQUEST:\n" + current_request
        + "\n\nFILE: " + source_file.filename().string()
        + "\n\nFILE DECLARATIONS:\n" + outline;

    const auto decision = m_llm.request<std::string>(
        ELlmRole::ThinkerLocal, prompt, input,
        [](const std::string& text, std::string& value, std::string& error)
        {
            const std::string trimmed = trim_ascii(text);
            if (trimmed == "SEVERAL" || trimmed == "NONE")
            {
                value.clear();
                return true;
            }
            if (trimmed.rfind("ONE ", 0) == 0)
            {
                const std::string name = trim_ascii(trimmed.substr(4));
                if (name.empty())
                {
                    error = "empty entity name after ONE";
                    return false;
                }
                value = name;
                return true;
            }
            error = "expected ONE <name>, SEVERAL, or NONE";
            return false;
        });

    result.m_ok = decision.m_ok;
    result.m_used_role = decision.m_used_role;
    result.m_error = decision.m_error;
    if (decision.m_ok && !decision.m_value.empty())
    {
        result.m_single = true;
        result.m_qualified_name = decision.m_value;
    }
    return result;
}
} // namespace fcpp
