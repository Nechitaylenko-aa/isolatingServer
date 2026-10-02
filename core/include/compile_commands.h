#pragma once
#include "json.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace cpptool {

using json = nlohmann::json;

// Разбирает compile_commands.json один раз и даёт быстрый поиск по абсолютному
// пути файла. compile_commands.json хранит "command" одной строкой — здесь она
// токенизируется в argv-подобный список и из него вычищаются
// входной/выходной файлы (-o ..., -c <file>), чтобы остались только флаги,
// пригодные для передачи в clang_parseTranslationUnit.
class CompileCommandsIndex {
public:
    static std::optional<CompileCommandsIndex> load(const std::string& compileCommandsPath);

    // Возвращает флаги для файла (без самого файла и без -o/-c), либо nullopt,
    // если файла нет в базе — тогда query.file_flags вернёт needs_input,
    // а не тихо подставит дефолтные флаги.
    std::optional<std::vector<std::string>> flagsFor(const std::string& absoluteFilePath) const;

    // Эвристика для заголовков (см. ТЗ §12): заголовок сам по себе не имеет
    // записи в compile_commands.json, только .cpp, который его подключает.
    // Возвращает флаги первого найденного файла из базы, чей текст содержит
    // "#include ... <basename заголовка>", и путь этого файла — чтобы вызывающий
    // код мог честно указать в ответе, что флаги позаимствованы у соседа,
    // а не найдены напрямую.
    struct HeaderFlagsResult {
        std::vector<std::string> flags;
        std::string resolved_via_file;
    };
    std::optional<HeaderFlagsResult> flagsForHeaderViaCompanion(const std::string& headerPath) const;

    size_t size() const { return byFile_.size(); }

    // Список всех записей базы: путь файла -> флаги компиляции.
    // Нужен кэшу TU: чтобы собрать агрегат по проекту, ему нужен полный
    // перечень .cpp, а не поиск по одному известному имени.
    const std::unordered_map<std::string, std::vector<std::string>>& files() const { return byFile_; }

    // Общий каталог-предок всех исходников базы — то, что для кэша является
    // «project root». Брать в этом качестве каталог самого compile_commands.json
    // нельзя: CMake кладёт его в build/, а исходники лежат рядом с build/, и
    // тогда ни один заголовок проекта не попал бы в отслеживаемые.
    // Возвращает пустую строку, если база пуста или общего предка нет.
    std::string common_source_root() const;

private:
    std::unordered_map<std::string, std::vector<std::string>> byFile_;
};

json queryFileFlags(const std::string& compileCommandsPath, const std::string& file);

} // namespace cpptool
