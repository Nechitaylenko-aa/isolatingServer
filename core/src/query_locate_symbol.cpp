#include "queries.h"
#include "clang_index.h"

namespace cpptool {

namespace {

struct FindEnclosingFnCtx {
    CXFile file;
    unsigned targetOffset;
    CXCursor found = clang_getNullCursor();
};

// Ищет самое глубоко вложенное определение функции/метода, чей extent
// (диапазон исходного текста) накрывает нужную позицию. Это надёжнее, чем
// подъём по clang_getCursor*Parent от курсора внутри тела: для
// expression/statement-курсоров оба родителя в libclang часто пустые.
CXChildVisitResult findEnclosingFnVisitor(CXCursor cursor, CXCursor /*parent*/, CXClientData clientData) {
    auto* ctx = static_cast<FindEnclosingFnCtx*>(clientData);
    CXCursorKind k = clang_getCursorKind(cursor);

    bool isFnLike = (k == CXCursor_CXXMethod || k == CXCursor_FunctionDecl ||
                      k == CXCursor_Constructor || k == CXCursor_Destructor);

    if (isFnLike && clang_isCursorDefinition(cursor)) {
        CXSourceRange extent = clang_getCursorExtent(cursor);
        CXSourceLocation start = clang_getRangeStart(extent);
        CXSourceLocation end = clang_getRangeEnd(extent);
        CXFile sf, ef; unsigned sOff, eOff, dummy;
        clang_getSpellingLocation(start, &sf, &dummy, &dummy, &sOff);
        clang_getSpellingLocation(end, &ef, &dummy, &dummy, &eOff);

        if (sf == ctx->file && ef == ctx->file &&
            ctx->targetOffset >= sOff && ctx->targetOffset <= eOff) {
            ctx->found = cursor; // самый глубокий найденный перезапишет внешний
            return CXChildVisit_Recurse;
        }
    }
    return CXChildVisit_Recurse;
}

} // namespace

json queryLocateSymbol(const std::string& file, unsigned line, unsigned col,
                        const std::vector<std::string>& flags) {
    auto unit = ParsedUnit::parse(file, flags);
    if (!unit) {
        return json{{"ok", false}, {"error", {{"code", "parse_failed"}, {"message", "clang не смог разобрать TU"}}}};
    }

    CXFile cxFile = clang_getFile(unit->tu(), file.c_str());
    CXSourceLocation loc = clang_getLocation(unit->tu(), cxFile, line, col);
    CXCursor cursor = clang_getCursor(unit->tu(), loc);

    if (clang_Cursor_isNull(cursor) || clang_isInvalid(clang_getCursorKind(cursor))) {
        return json{{"ok", false}, {"error", {{"code", "no_symbol_at_position"}}}};
    }

    unsigned targetOffset;
    { CXFile f; unsigned l, c; clang_getSpellingLocation(loc, &f, &l, &c, &targetOffset); }

    FindEnclosingFnCtx fnCtx{cxFile, targetOffset};
    clang_visitChildren(unit->rootCursor(), findEnclosingFnVisitor, &fnCtx);
    CXCursor enclosingFunction = fnCtx.found;

    // Класс метода (в т.ч. определённого вне тела класса, Class::method в .cpp)
    // берём через семантического родителя найденного метода — для деклараций
    // семантический родитель в libclang надёжен, в отличие от курсоров внутри тела.
    CXCursor enclosingClass = clang_getNullCursor();
    if (!clang_Cursor_isNull(enclosingFunction)) {
        CXCursor semanticParent = clang_getCursorSemanticParent(enclosingFunction);
        CXCursorKind pk = clang_getCursorKind(semanticParent);
        if (pk == CXCursor_ClassDecl || pk == CXCursor_StructDecl) {
            enclosingClass = semanticParent;
        }
    }

    json result;
    result["ok"] = true;
    result["cursor"] = {
        {"kind", cursorKindName(cursor)},
        {"spelling", cursorSpelling(cursor)},
        {"usr", cursorUSR(cursor)}
    };

    if (!clang_Cursor_isNull(enclosingFunction)) {
        result["enclosing_method"] = {
            {"usr", cursorUSR(enclosingFunction)},
            {"signature", buildMethodSignature(enclosingFunction)},
            {"file", cursorFile(enclosingFunction)},
            {"line", cursorLine(enclosingFunction)}
        };
    } else {
        result["enclosing_method"] = nullptr;
    }

    if (!clang_Cursor_isNull(enclosingClass)) {
        result["enclosing_class"] = {
            {"usr", cursorUSR(enclosingClass)},
            {"name", cursorSpelling(enclosingClass)}
        };
    } else {
        result["enclosing_class"] = nullptr;
    }

    auto diags = unit->diagnostics();
    if (!diags.empty()) result["diagnostics"] = diags;

    return result;
}

} // namespace cpptool
