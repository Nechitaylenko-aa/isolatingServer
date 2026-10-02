#pragma once

#include <clang-c/Index.h>
#include <string>
#include <vector>
#include <optional>

namespace cpptool {

// Тонкая RAII-обёртка над CXIndex/CXTranslationUnit.
// Один экземпляр — один разбор одного файла с заданными флагами.
class ParsedUnit {
public:
    // args — флаги компиляции (без имени файла), например {"-std=c++17", "-Ipath"}
    static std::optional<ParsedUnit> parse(const std::string& file,
                                            const std::vector<std::string>& args);

    ParsedUnit(const ParsedUnit&) = delete;
    ParsedUnit& operator=(const ParsedUnit&) = delete;
    ParsedUnit(ParsedUnit&& other) noexcept;
    ~ParsedUnit();

    CXTranslationUnit tu() const { return tu_; }
    CXCursor rootCursor() const;

    // Диагностика парсинга (ошибки/варнинги clang) — полезно вернуть клиенту,
    // если что-то пошло не так из-за неверных флагов.
    std::vector<std::string> diagnostics() const;

    // Все файлы, реально открытые препроцессором в этом TU (транзитивно, включая
    // сам TU). Нужен кэшу: без этого списка правка любого заголовка не
    // инвалидирует запись, и кэш молча отдаёт устаревшие refs.
    // Требует CXTranslationUnit_DetailedPreprocessingRecord (см. parse()).
    std::vector<std::string> inclusions() const;

private:
    ParsedUnit(CXIndex idx, CXTranslationUnit tu) : idx_(idx), tu_(tu) {}
    CXIndex idx_ = nullptr;
    CXTranslationUnit tu_ = nullptr;
};

// --- Общие хелперы, используются в разных query.* ---

std::string cxStringToStd(CXString s); // забирает владение и освобождает CXString

std::string cursorUSR(CXCursor c);
std::string cursorSpelling(CXCursor c);
std::string cursorKindName(CXCursor c);
std::string cursorFile(CXCursor c);
unsigned cursorLine(CXCursor c);
unsigned cursorColumn(CXCursor c);

// Строит человекочитаемую сигнатуру метода/функции:
// "static time_t calculateNextMinute(const EventEntry &, time_t) const"
std::string buildMethodSignature(CXCursor methodCursor);

// Версия libclang, которой распарсен TU (например "clang version 22.1.2").
// Идёт в кэш: формат USR генерируется конкретной версией clang, и после смены
// версии старые ключи перестают находиться — это тихий отказ (refs = 0 вместо
// ошибки), поэтому версия обязана ломать валидность кэша целиком.
std::string clangVersion();

} // namespace cpptool
