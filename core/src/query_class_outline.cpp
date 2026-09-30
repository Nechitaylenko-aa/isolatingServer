#include "queries.h"
#include "clang_index.h"

namespace cpptool {

namespace {

struct FindClassCtx {
    const std::string* name;
    CXCursor found = clang_getNullCursor();
    bool foundAny = false;
    int matchesSeen = 0; // если > 1 — имя неоднозначно (несколько классов с таким именем)
};

CXChildVisitResult findClassVisitor(CXCursor cursor, CXCursor /*parent*/, CXClientData clientData) {
    auto* ctx = static_cast<FindClassCtx*>(clientData);
    CXCursorKind k = clang_getCursorKind(cursor);
    if ((k == CXCursor_ClassDecl || k == CXCursor_StructDecl ||
         k == CXCursor_ClassTemplate) &&
        clang_isCursorDefinition(cursor)) {
        std::string name = cursorSpelling(cursor);
        if (name == *ctx->name) {
            ctx->matchesSeen++;
            if (!ctx->foundAny) {
                ctx->found = cursor;
                ctx->foundAny = true;
            }
        }
    }
    return CXChildVisit_Recurse; // не Break — специально досчитываем все совпадения ради matchesSeen
}

struct OutlineCtx {
    json* baseClasses;
    json* publicMethods;
};

CXChildVisitResult classMemberVisitor(CXCursor cursor, CXCursor /*parent*/, CXClientData clientData) {
    auto* ctx = static_cast<OutlineCtx*>(clientData);
    CXCursorKind k = clang_getCursorKind(cursor);

    if (k == CXCursor_CXXBaseSpecifier) {
        CXType baseType = clang_getCursorType(cursor);
        ctx->baseClasses->push_back(cxStringToStd(clang_getTypeSpelling(baseType)));
        return CXChildVisit_Continue;
    }

    bool isMethodLike = (k == CXCursor_CXXMethod || k == CXCursor_Constructor || k == CXCursor_Destructor);
    if (isMethodLike && clang_getCXXAccessSpecifier(cursor) == CX_CXXPublic) {
        ctx->publicMethods->push_back({
            {"usr", cursorUSR(cursor)},
            {"signature", buildMethodSignature(cursor)},
            {"is_virtual", static_cast<bool>(clang_CXXMethod_isVirtual(cursor))},
            {"is_static", static_cast<bool>(clang_CXXMethod_isStatic(cursor))},
            {"is_pure_virtual", static_cast<bool>(clang_CXXMethod_isPureVirtual(cursor))}
        });
    }
    // Не рекурсируем внутрь тел методов — нам нужны только объявления members.
    return CXChildVisit_Continue;
}

} // namespace

json queryClassOutline(const std::string& file, const std::string& className,
                       const std::vector<std::string>& flags) {
    auto unit = ParsedUnit::parse(file, flags);
    if (!unit) {
        return json{{"ok", false}, {"error", {{"code", "parse_failed"}, {"message", "clang не смог разобрать TU"}}}};
    }

    FindClassCtx findCtx{&className};
    clang_visitChildren(unit->rootCursor(), findClassVisitor, &findCtx);

    if (!findCtx.foundAny) {
        return json{{"ok", false}, {"error", {{"code", "class_not_found"}, {"message", "класс \"" + className + "\" не найден в этом TU"}}}};
    }

    json baseClasses = json::array();
    json publicMethods = json::array();
    OutlineCtx outlineCtx{&baseClasses, &publicMethods};
    clang_visitChildren(findCtx.found, classMemberVisitor, &outlineCtx);

    json result;
    result["ok"] = true;
    result["usr"] = cursorUSR(findCtx.found);
    result["name"] = cursorSpelling(findCtx.found);
    result["file"] = cursorFile(findCtx.found);
    result["base_classes"] = baseClasses;
    result["public_methods"] = publicMethods;

    if (findCtx.matchesSeen > 1) {
        // Ровно та ситуация из ТЗ: искать по имени без USR — риск взять не тот класс.
        result["warning"] = "ambiguous_name: найдено " + std::to_string(findCtx.matchesSeen) +
                             " классов с именем \"" + className + "\" в этом TU, взят первый";
    }

    auto diags = unit->diagnostics();
    if (!diags.empty()) result["diagnostics"] = diags;

    return result;
}

} // namespace cpptool
