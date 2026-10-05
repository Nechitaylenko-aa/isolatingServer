#include "queries.h"
#include "tu_cache.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cpptool {

namespace {

std::vector<std::string> readLines(const std::string& file) {
    std::vector<std::string> lines;
    std::ifstream in(file);
    if (!in) return lines;
    std::string l;
    while (std::getline(in, l)) lines.push_back(l);
    return lines;
}

// Файлы для анализа: конкретный файл, иначе — все файлы класса (если класс задан).
std::vector<std::string> filesFor(CTuCache& cache, const std::string& file, const std::string& classUsr) {
    std::vector<std::string> files;
    if (!file.empty()) { files.push_back(file); return files; }
    if (!classUsr.empty()) {
        json j = json::parse(cache.get_class_files_json(classUsr));
        if (j.value("ok", false))
            for (auto& f : j["files"]) files.push_back(f.get<std::string>());
    }
    return files;
}

bool isTodoMarker(const std::string& s, const std::string& m) {
    auto p = s.find(m);
    if (p == std::string::npos) return false;
    // Маркер должен быть словом: TODOX не считаем (но TODO(/TODO:/TODO  считаем).
    if (p > 0) {
        char prev = s[p - 1];
        if (std::isalnum(static_cast<unsigned char>(prev)) || prev == '_') return false;
    }
    size_t after = p + m.size();
    if (after < s.size()) {
        char nxt = s[after];
        if (std::isalnum(static_cast<unsigned char>(nxt)) || nxt == '_') return false;
    }
    return true;
}

// Первая значимая строка файла — doc-комментарий, если начинается с /// , //! , /** или /*! .
bool hasHeaderDoc(const std::vector<std::string>& lines) {
    for (const auto& raw : lines) {
        std::string s = raw;
        size_t b = s.find_first_not_of(" \t\r");
        if (b == std::string::npos) continue; // пустая
        s = s.substr(b);
        // пропускаем устаревшие #pragma / #ifndef / #define — они часто идут первыми
        if (s.rfind("#", 0) == 0) continue;
        if (s.rfind("///", 0) == 0 || s.rfind("//!", 0) == 0 ||
            s.rfind("/**", 0) == 0 || s.rfind("/*!", 0) == 0)
            return true;
        return false; // первая значимая строка — не doc-комментарий
    }
    return false;
}

} // namespace

json queryDependencyMap(const std::string& file, const std::string& compileCommandsPath) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened())
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok)
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};
    return json::parse(cache->get_file_deps_json(file));
}

json queryTodoScan(const std::string& compileCommandsPath, const std::string& file,
                   const std::string& classUsr) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened())
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok)
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};

    std::vector<std::string> files = filesFor(*cache, file, classUsr);
    if (files.empty())
        return json{{"ok", false}, {"error", {{"code", "no_scope"},
            {"message", "ни file ни classUsr не заданы"}}}};

    json markers = json::array();
    int scanned = 0;
    for (const auto& f : files) {
        auto lines = readLines(f);
        if (lines.empty()) continue;
        ++scanned;
        for (size_t i = 0; i < lines.size(); ++i) {
            for (const char* m : {"TODO", "FIXME", "HACK", "XXX"}) {
                if (isTodoMarker(lines[i], m)) {
                    markers.push_back({{"file", f}, {"line", static_cast<int>(i + 1)},
                                       {"marker", m}, {"text", lines[i]}});
                    break; // одна строка — один маркер (первый по приоритету)
                }
            }
        }
    }
    return json{{"ok", true}, {"markers", markers}, {"files_scanned", scanned}};
}

// Максимальная вложенность фигурных скобок в диапазоне строк (детерминированно,
// без парсинга — грубая, но стабильная метрика).
int maxBraceNesting(const std::vector<std::string>& lines, int fromLine, int toLine) {
    int cur = 0, maxN = 0;
    size_t from = fromLine > 0 ? static_cast<size_t>(fromLine) : 1;
    size_t to = std::min(lines.size(), static_cast<size_t>(toLine));
    for (size_t i = from; i <= to; ++i) {
        for (char ch : lines[i-1]) {
            if (ch == '{') { ++cur; if (cur > maxN) maxN = cur; }
            else if (ch == '}') { if (cur > 0) --cur; }
        }
    }
    return maxN;
}

json queryDocCheck(const std::string& compileCommandsPath, const std::string& file,
                   const std::string& classUsr) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened())
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok)
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};

    std::vector<std::string> files = filesFor(*cache, file, classUsr);
    if (files.empty())
        return json{{"ok", false}, {"error", {{"code", "no_scope"},
            {"message", "ни file ни classUsr не заданы"}}}};

    json undocumented = json::array();
    json checked = json::array();
    for (const auto& f : files) {
        auto lines = readLines(f);
        if (lines.empty()) continue;
        checked.push_back(f);
        if (!hasHeaderDoc(lines))
            undocumented.push_back({{"file", f}, {"line", 1}});
    }
    return json{{"ok", true}, {"undocumented", undocumented}, {"checked", checked}};
}

json queryTypeUsage(const std::string& typeUsr, const std::string& compileCommandsPath) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened())
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok)
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};
    return json::parse(cache->get_refs_locations_json(typeUsr));
}

json queryComplexityHotspot(const std::string& compileCommandsPath, const std::string& file,
                            const std::string& classUsr) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened())
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok)
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};

    json mj = json::parse(cache->get_methods_json(file, classUsr));
    if (!mj.value("ok", false)) return mj;
    json hotspots = json::array();
    for (auto& m : mj["methods"]) {
        std::string mfile = m.value("file", "");
        int s = m.value("start_line", 0), e = m.value("end_line", 0);
        auto lines = readLines(mfile);
        if (lines.empty() || s <= 0 || e < s) continue;
        size_t from = static_cast<size_t>(s);
        size_t to = std::min(lines.size(), static_cast<size_t>(e));
        int bodyLines = 0, longLines = 0;
        for (size_t i = from; i <= to; ++i) {
            const std::string& t = lines[i-1];
            if (t.find_first_not_of(" \t") == std::string::npos) continue;
            ++bodyLines;
            if (t.size() > 120) ++longLines;
        }
        int nesting = maxBraceNesting(lines, s, e);
        int score = bodyLines + nesting * 3 + longLines * 2;
        hotspots.push_back({{"usr", m.value("usr","")}, {"signature", m.value("signature","?")},
                            {"file", mfile}, {"line", s}, {"body_lines", bodyLines},
                            {"max_nesting", nesting}, {"long_lines", longLines}, {"score", score}});
    }
    std::sort(hotspots.begin(), hotspots.end(), [](const json& a, const json& b){
        return a.value("score", 0) > b.value("score", 0);
    });
    return json{{"ok", true}, {"hotspots", hotspots}};
}

json queryDuplicateFind(const std::string& compileCommandsPath, const std::string& file,
                        const std::string& classUsr) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened())
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok)
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};

    json mj = json::parse(cache->get_methods_json(file, classUsr));
    if (!mj.value("ok", false)) return mj;

    // Группируем по нормализованной сигнатуре (типы аргументов без имён): методы
    // с одинаковой сигнатурой в разных классах/файлах — кандидаты на дублирование.
    std::map<std::string, json> groups;
    for (auto& m : mj["methods"]) {
        std::string sig = m.value("signature", "");
        std::string key = sig;
        std::replace(key.begin(), key.end(), ' ', '_');
        groups[key].push_back({{"file", m.value("file","")}, {"line", m.value("start_line",0)},
                               {"signature", sig}});
    }
    json out = json::array();
    for (auto& [key, members] : groups) {
        if (members.size() > 1)
            out.push_back({{"signature", members[0].value("signature","")}, {"members", members}});
    }
    return json{{"ok", true}, {"groups", out}};
}

} // namespace cpptool
