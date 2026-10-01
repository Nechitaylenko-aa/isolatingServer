#include "queries.h"
#include "clang_index.h"
#include "compile_commands.h"
#include <fstream>
#include <set>
#include <unordered_map>
#include <unordered_set>

// queryFindImplementors: по имени базового класса и имени метода находит все
// конкретные реализации этого метода в наследниках, итерируясь по всем TU из
// compile_commands.json. Работает даже если проект не собирается полностью —
// clang парсит каждый TU отдельно и терпимо к ошибкам в других файлах.
// Не зависит от clangd и фонового индекса.

namespace cpptool {

namespace {

// Для одного TU собираем: классы с их базовыми классами и методами.
struct ClassInfo {
    std::string name;
    std::string usr;
    std::string file;
    unsigned line{0};
    std::vector<std::string> baseClassNames; // имена базовых (как в spelling)
    std::vector<json> methods;               // {usr, signature, file, line, is_pure_virtual}
};

struct ScanCtx {
    std::vector<ClassInfo>* classes;
    ClassInfo* currentClass{nullptr};
};

CXChildVisitResult scanTUVisitor(CXCursor cursor, CXCursor /*parent*/, CXClientData data) {
    auto* ctx = static_cast<ScanCtx*>(data);
    CXCursorKind k = clang_getCursorKind(cursor);

    if ((k == CXCursor_ClassDecl || k == CXCursor_StructDecl) && clang_isCursorDefinition(cursor)) {
        ClassInfo ci;
        ci.name = cursorSpelling(cursor);
        ci.usr  = cursorUSR(cursor);
        ci.file = cursorFile(cursor);
        ci.line = cursorLine(cursor);

        // Собираем базовые классы и методы через вложенный обход.
        struct InnerCtx { ClassInfo* ci; };
        InnerCtx inner{&ci};
        clang_visitChildren(cursor, [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
            auto* ic = static_cast<InnerCtx*>(d);
            CXCursorKind ck = clang_getCursorKind(c);
            if (ck == CXCursor_CXXBaseSpecifier) {
                CXType baseType = clang_getCursorType(c);
                std::string spelling = cxStringToStd(clang_getTypeSpelling(baseType));
                // Убираем "class " / "struct " prefix если есть.
                for (const char* pfx : {"class ", "struct "}) {
                    if (spelling.rfind(pfx, 0) == 0) { spelling = spelling.substr(strlen(pfx)); break; }
                }
                ic->ci->baseClassNames.push_back(spelling);
                return CXChildVisit_Continue;
            }
            if ((ck == CXCursor_CXXMethod) && clang_isCursorDefinition(c)) {
                ic->ci->methods.push_back({
                    {"usr",            cursorUSR(c)},
                    {"signature",      buildMethodSignature(c)},
                    {"file",           cursorFile(c)},
                    {"line",           (int)cursorLine(c)},
                    {"is_pure_virtual", static_cast<bool>(clang_CXXMethod_isPureVirtual(c))}
                });
            }
            return CXChildVisit_Continue; // не рекурсируем внутрь тел методов
        }, &inner);

        ctx->classes->push_back(std::move(ci));
        return CXChildVisit_Continue; // не вложенный класс — продолжаем на том же уровне
    }
    return CXChildVisit_Recurse;
}

// Строим множество имён всех наследников baseClassName (транзитивно).
std::unordered_set<std::string> collectDescendants(
    const std::string& baseClassName,
    const std::unordered_map<std::string, std::vector<std::string>>& baseMap)
{
    std::unordered_set<std::string> result;
    std::vector<std::string> queue = {baseClassName};
    while (!queue.empty()) {
        std::string cur = queue.back(); queue.pop_back();
        auto it = baseMap.find(cur);
        if (it == baseMap.end()) continue;
        for (auto& child : it->second) {
            if (result.insert(child).second) queue.push_back(child);
        }
    }
    return result;
}

} // namespace

// Возвращает {ok, implementors:[{class, method_usr, signature, file, line}], files_scanned, files_total}
json queryFindImplementors(const std::string& compileCommandsPath,
                           const std::string& baseClassName,
                           const std::string& methodName) {
    if (baseClassName.empty() || methodName.empty())
        return json{{"ok", false}, {"error", {{"code", "empty_args"}}}};

    auto idxOpt = CompileCommandsIndex::load(compileCommandsPath);
    if (!idxOpt)
        return json{{"ok", false}, {"error", {{"code", "compile_commands_failed"},
            {"message", "не удалось загрузить: " + compileCommandsPath}}}};

    // Читаем список (file, flags) напрямую из json.
    std::vector<std::pair<std::string, std::vector<std::string>>> files;
    {
        std::ifstream in(compileCommandsPath);
        if (!in) return json{{"ok", false}, {"error", {{"code", "read_failed"}}}};
        json j; try { in >> j; } catch (...) { return json{{"ok", false}, {"error", {{"code", "parse_failed"}}}}; }
        for (auto& e : j) {
            std::string f = e.value("file", "");
            if (f.empty()) continue;
            auto fl = idxOpt->flagsFor(f);
            if (!fl) continue;
            files.emplace_back(f, *fl);
        }
    }

    // Первый проход: парсим все TU, собираем ClassInfo.
    // Строим обратный индекс: baseClassName -> [childClassName].
    std::vector<ClassInfo> allClasses;
    std::unordered_map<std::string, std::vector<std::string>> childrenOf; // base -> [children]
    int scanned = 0;

    for (auto& [absPath, flags] : files) {
        auto unit = ParsedUnit::parse(absPath, flags);
        if (!unit) continue;
        scanned++;
        std::vector<ClassInfo> tuClasses;
        ScanCtx ctx{&tuClasses};
        clang_visitChildren(unit->rootCursor(), scanTUVisitor, &ctx);
        for (auto& ci : tuClasses) {
            for (auto& base : ci.baseClassNames) {
                childrenOf[base].push_back(ci.name);
            }
        }
        for (auto& ci : tuClasses) allClasses.push_back(std::move(ci));
    }

    // Транзитивное замыкание: все наследники baseClassName.
    auto descendants = collectDescendants(baseClassName, childrenOf);

    // Второй проход по уже собранным классам: ищем реализации methodName.
    json implementors = json::array();
    std::set<std::string> seen; // дедуп по USR метода
    for (auto& ci : allClasses) {
        if (descendants.find(ci.name) == descendants.end()) continue;
        for (auto& m : ci.methods) {
            std::string sig = m.value("signature", "");
            // Матчим по имени метода внутри сигнатуры (грубо, но надёжно без полного парса).
            if (sig.find(methodName) == std::string::npos) continue;
            std::string usr = m.value("usr", "");
            if (!usr.empty() && !seen.insert(usr).second) continue;
            implementors.push_back({
                {"class",     ci.name},
                {"signature", sig},
                {"file",      m.value("file", "")},
                {"line",      m.value("line", 0)},
                {"usr",       usr}
            });
        }
    }

    return json{
        {"ok",           true},
        {"base_class",   baseClassName},
        {"method_name",  methodName},
        {"implementors", implementors},
        {"descendants",  (int)descendants.size()},
        {"files_scanned", scanned},
        {"files_total",  (int)files.size()}
    };
}

} // namespace cpptool