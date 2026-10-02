#include "queries.h"
#include "clang_index.h"
#include "compile_commands.h"
#include <fstream>
#include <set>
#include <tuple>

namespace cpptool {

namespace {

// Контекст обхода одного TU: ищем определения методов, которые переопределяют
// целевой (базовый) метод. Матчим не по имени, а по USR базового метода:
// clang_getOverriddenCursors даёт курсоры ОБЪЯВЛЕНИЙ базовых методов, чей USR
// сравниваем с targetUSR. Имя ненадёжно (одноимённые методы в разных иерархиях),
// а USR однозначен и переживает переименования в других классах.
struct OverridesCtx {
    const std::string* targetUSR;
    json* results;
    std::set<std::tuple<std::string, unsigned>>* seen;
};

CXChildVisitResult overridesVisitor(CXCursor cursor, CXCursor /*parent*/, CXClientData data) {
    auto* ctx = static_cast<OverridesCtx*>(data);
    if (clang_getCursorKind(cursor) == CXCursor_CXXMethod && clang_isCursorDefinition(cursor)) {
        CXCursor* overridden = nullptr;
        unsigned count = 0;
        clang_getOverriddenCursors(cursor, &overridden, &count);
        bool matches = false;
        for (unsigned i = 0; i < count; ++i) {
            if (cursorUSR(overridden[i]) == *ctx->targetUSR) { matches = true; break; }
        }
        clang_disposeOverriddenCursors(overridden);
        if (matches) {
            std::string file = cursorFile(cursor);
            unsigned line = cursorLine(cursor);
            // Дедуп по (file,line): один и тот же метод может прийти от нескольких
            // вложенных курсоров, и без этого один override посчитался бы дважды.
            if (ctx->seen->insert(std::make_tuple(file, line)).second) {
                ctx->results->push_back({
                    {"usr", cursorUSR(cursor)},
                    {"signature", buildMethodSignature(cursor)},
                    {"file", file},
                    {"line", line},
                    {"is_pure_virtual", static_cast<bool>(clang_CXXMethod_isPureVirtual(cursor))}
                });
            }
        }
    }
    return CXChildVisit_Recurse;
}

} // namespace

json queryVirtualOverrides(const std::string& compileCommandsPath,
                           const std::string& targetUSR) {
    if (targetUSR.empty())
        return json{{"ok", false}, {"error", {{"code", "usr_empty"}, {"message", "пустой USR базового метода"}}}};

    auto idxOpt = CompileCommandsIndex::load(compileCommandsPath);
    if (!idxOpt)
        return json{{"ok", false}, {"error", {{"code", "compile_commands_failed"},
            {"message", "не удалось загрузить compile_commands.json: " + compileCommandsPath}}}};

    // Список (file, flags) — читаем json напрямую, т.к. набор файлов
    // compile_commands.json не отдаёт ничем, кроме files() за O(n) обхода.
    std::vector<std::pair<std::string, std::vector<std::string>>> files;
    {
        std::ifstream in(compileCommandsPath);
        if (!in)
            return json{{"ok", false}, {"error", {{"code", "compile_commands_read_failed"},
                {"message", "не удалось открыть compile_commands.json"}}}};
        json j;
        try { in >> j; }
        catch (const std::exception& e) {
            return json{{"ok", false}, {"error", {{"code", "compile_commands_parse_failed"},
                {"message", std::string("compile_commands.json не JSON: ") + e.what()}}}};
        }
        for (auto& e : j) {
            std::string f = e.value("file", "");
            if (f.empty()) continue;
            auto flagsOpt = idxOpt->flagsFor(f);
            if (!flagsOpt) continue;
            files.emplace_back(f, *flagsOpt);
        }
    }

    json results = json::array();
    std::set<std::tuple<std::string, unsigned>> seen;
    int scanned = 0;
    for (auto& [absPath, flags] : files) {
        auto unit = ParsedUnit::parse(absPath, flags);
        if (!unit) continue; // не разобранный TU пропускаем, а не падаем
        scanned++;
        OverridesCtx ctx{&targetUSR, &results, &seen};
        clang_visitChildren(unit->rootCursor(), overridesVisitor, &ctx);
    }

    json result;
    result["ok"] = true;
    result["target_usr"] = targetUSR;
    result["overrides"] = results;
    result["files_scanned"] = scanned;
    result["files_total"] = static_cast<int>(files.size());
    return result;
}

} // namespace cpptool