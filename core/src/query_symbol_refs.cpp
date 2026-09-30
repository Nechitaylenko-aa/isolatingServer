#include "queries.h"
#include "clang_index.h"
#include <set>
#include <tuple>

namespace cpptool {

namespace {

struct RefsCtx {
    const std::string* targetUSR;
    json* results;
    std::set<std::tuple<std::string, unsigned, unsigned>>* seenLocations;
};

CXChildVisitResult refsVisitor(CXCursor cursor, CXCursor /*parent*/, CXClientData clientData) {
    auto* ctx = static_cast<RefsCtx*>(clientData);

    CXCursor referenced = clang_getCursorReferenced(cursor);
    if (!clang_Cursor_isNull(referenced) && !clang_isInvalid(clang_getCursorKind(referenced))) {
        std::string usr = cursorUSR(referenced);
        if (!usr.empty() && usr == *ctx->targetUSR) {
            std::string file = cursorFile(cursor);
            unsigned line = cursorLine(cursor);
            unsigned col = cursorColumn(cursor);
            auto key = std::make_tuple(file, line, col);
            // Одно место в исходнике часто даёт несколько вложенных курсоров
            // (CallExpr → UnexposedExpr → DeclRefExpr), которые все резолвятся
            // к той же цели — без дедупликации по позиции один вызов метода
            // считался бы за три ссылки.
            if (ctx->seenLocations->insert(key).second) {
                ctx->results->push_back({
                    {"file", file},
                    {"line", line},
                    {"column", col},
                    {"kind", cursorKindName(cursor)},
                    {"is_definition", static_cast<bool>(clang_isCursorDefinition(cursor))}
                });
            }
        }
    }
    return CXChildVisit_Recurse;
}

} // namespace

json querySymbolRefs(const std::string& targetUSR, const std::string& file,
                      const std::vector<std::string>& flags) {
    auto unit = ParsedUnit::parse(file, flags);
    if (!unit) {
        return json{{"ok", false}, {"error", {{"code", "parse_failed"}, {"message", "clang не смог разобрать TU"}}}};
    }

    json results = json::array();
    std::set<std::tuple<std::string, unsigned, unsigned>> seenLocations;
    RefsCtx ctx{&targetUSR, &results, &seenLocations};
    clang_visitChildren(unit->rootCursor(), refsVisitor, &ctx);

    json result;
    result["ok"] = true;
    result["target_usr"] = targetUSR;
    result["refs"] = results;
    result["searched_file"] = file;
    result["note"] = "поиск ограничен этим одним TU, не всем проектом — см. ТЗ п. symbol_refs";

    auto diags = unit->diagnostics();
    if (!diags.empty()) result["diagnostics"] = diags;

    return result;
}

} // namespace cpptool
