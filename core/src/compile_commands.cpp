#include "compile_commands.h"
#include <fstream>
#include <sstream>

namespace cpptool {

namespace {

// Простая токенизация строки "command" из compile_commands.json.
// Ограничение: не обрабатывает кавычки/экранирование внутри аргументов
// (в наблюдаемых на практике командах CMake+GCC/Clang их не было).
// Если это когда-нибудь станет проблемой — сигнал перейти на
// "arguments": [...] вместо "command", если сборка его отдаёт.
std::vector<std::string> tokenize(const std::string& command) {
    std::vector<std::string> tokens;
    std::istringstream iss(command);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

// Убирает из токенов компилятор (первый токен), "-o <output>" и "-c <input>",
// оставляя только флаги, годные для повторной передачи в clang.
std::vector<std::string> extractFlags(const std::vector<std::string>& tokens) {
    std::vector<std::string> flags;
    for (size_t i = 1; i < tokens.size(); ++i) { // с 1 — пропускаем сам компилятор
        const std::string& t = tokens[i];
        if (t == "-o" || t == "-c") {
            ++i; // пропустить и сам флаг, и его аргумент (output-файл / input-файл)
            continue;
        }
        flags.push_back(t);
    }
    return flags;
}

} // namespace

std::optional<CompileCommandsIndex> CompileCommandsIndex::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) return std::nullopt;

    json data;
    try {
        in >> data;
    } catch (...) {
        return std::nullopt;
    }
    if (!data.is_array()) return std::nullopt;

    CompileCommandsIndex idx;
    for (auto& entry : data) {
        if (!entry.contains("file") || !entry.contains("command")) continue;
        std::string file = entry["file"].get<std::string>();
        std::string command = entry["command"].get<std::string>();
        idx.byFile_[file] = extractFlags(tokenize(command));
    }
    return idx;
}

std::optional<std::vector<std::string>> CompileCommandsIndex::flagsFor(const std::string& absoluteFilePath) const {
    auto it = byFile_.find(absoluteFilePath);
    if (it == byFile_.end()) return std::nullopt;
    return it->second;
}

std::optional<CompileCommandsIndex::HeaderFlagsResult>
CompileCommandsIndex::flagsForHeaderViaCompanion(const std::string& headerPath) const {
    // basename заголовка — то, что реально пишут в #include, путь к каталогу
    // не обязан совпадать (относительные "../include/Foo.h" и т.п.).
    size_t slash = headerPath.find_last_of('/');
    std::string basename = (slash == std::string::npos) ? headerPath : headerPath.substr(slash + 1);

    for (auto& [file, flags] : byFile_) {
        std::ifstream src(file);
        if (!src) continue;
        std::string line;
        while (std::getline(src, line)) {
            if (line.find("#include") != std::string::npos &&
                line.find(basename) != std::string::npos) {
                return HeaderFlagsResult{flags, file};
            }
        }
    }
    return std::nullopt;
}

json queryFileFlags(const std::string& compileCommandsPath, const std::string& file) {
    auto idx = CompileCommandsIndex::load(compileCommandsPath);
    if (!idx) {
        return json{{"ok", false}, {"error", {{"code", "compile_commands_not_found"},
            {"message", "не удалось прочитать/распарсить " + compileCommandsPath}}}};
    }

    auto flags = idx->flagsFor(file);
    if (flags) {
        return json{{"ok", true}, {"file", file}, {"flags", *flags}};
    }

    // Файл не найден напрямую — возможно, это заголовок, который сам не
    // компилируется. Пробуем эвристику: флаги берём у первого .cpp,
    // который его #include-ит (см. ТЗ §12, "открытые вопросы").
    auto viaHeader = idx->flagsForHeaderViaCompanion(file);
    if (viaHeader) {
        return json{
            {"ok", true},
            {"file", file},
            {"flags", viaHeader->flags},
            {"warning", "flags_via_companion_file: точной записи для этого файла в "
                        "compile_commands.json нет (вероятно, заголовок); флаги "
                        "позаимствованы у " + viaHeader->resolved_via_file}
        };
    }

    return json{{"ok", false}, {"error", {{"code", "file_not_in_compile_commands"},
        {"message", "файл \"" + file + "\" не найден среди " +
                    std::to_string(idx->size()) + " записей compile_commands.json, "
                    "и ни один известный файл его не #include-ит"}}}};
}

} // namespace cpptool
