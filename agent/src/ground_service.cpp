#include "ground_service.h"
#include "blast_radius.h"
#include "compile_commands.h"
#include "queries.h"
#include "clang_index.h"
#include <fstream>
#include <regex>

namespace cppagent {

namespace {
// Если enclosingMethod — виртуальный, ищем ВСЕ переопределения базового метода,
// а не только потомков текущего. Курсор может стоять на CMySQLModel::execSQL_read,
// но нам нужен и CPostgresModel::execSQL_read — они siblings (оба переопределяют
// CDataBaseModel::execSQL_read), а не parent-child. Поэтому поднимаемся к базовому
// методу и ищем переопределения его USR, а не USR текущего метода.
void fillVirtualOverridesIfNeeded(GroundResult& g, const std::string& compileCommandsPath, GroundService* svc) {
    if (g.enclosingMethod.is_null()) return;
    bool isVirtual = g.enclosingMethod.value("is_virtual", false);
    if (!isVirtual) return;
    g.virtualOverridesChecked = true;
    std::string usr = g.enclosingMethod.value("usr", "");
    std::string file = g.enclosingMethod.value("file", "");

    // Поднимаемся к базовому методу: если текущий метод переопределяет что-то,
    // берём USR базового, иначе используем USR текущего (сам базовый или не override).
    std::string baseUSR = usr;
    if (!file.empty()) {
        std::vector<std::string> flags;
        json flagsErr;
        if (svc->resolveFlags(file, flags, flagsErr)) {
            json locByCur = cpptool::queryLocateByUSR(file, usr, flags);
            if (locByCur.value("ok", false)) {
                // Перепарсим TU и найдём overridden курсоры у найденного метода.
                // Это дорого (один extra parse), но без этого siblings не находятся.
                // Оптимизация: можно кэшировать base USR в GroundResult, но сейчас проще так.
                auto unit = cpptool::ParsedUnit::parse(file, flags);
                if (unit) {
                    // Ищем курсор с нашим USR в TU.
                    struct FindCtx { std::string usr; CXCursor found = clang_getNullCursor(); };
                    FindCtx ctx; ctx.usr = usr;
                    auto visitor = [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
                        auto* ctx = static_cast<FindCtx*>(d);
                        if (!clang_Cursor_isNull(ctx->found)) return CXChildVisit_Continue;
                        if (cpptool::cursorUSR(c) == ctx->usr) { ctx->found = c; return CXChildVisit_Break; }
                        return CXChildVisit_Recurse;
                    };
                    clang_visitChildren(unit->rootCursor(), visitor, &ctx);
                    if (!clang_Cursor_isNull(ctx.found)) {
                        CXCursor* overridden = nullptr;
                        unsigned count = 0;
                        clang_getOverriddenCursors(ctx.found, &overridden, &count);
                        if (count > 0) baseUSR = cpptool::cursorUSR(overridden[0]);
                        clang_disposeOverriddenCursors(overridden);
                    }
                }
            }
        }
    }

    g.virtualOverrides = cpptool::queryVirtualOverrides(compileCommandsPath, baseUSR);
}
} // namespace

bool GroundService::resolveFlags(const std::string& file, std::vector<std::string>& outFlags, json& outError) {
    json ff = cpptool::queryFileFlags(compileCommandsPath_, file);
    if (!ff.value("ok", false)) { outError = ff; return false; }
    if (ff.contains("flags") && ff["flags"].is_array())
        for (auto& f : ff["flags"]) outFlags.push_back(f.get<std::string>());
    return true;
}

// USR -> актуальные line/col. Якорь на USR переживает сдвиги строк.
// Если символа больше нет (usr_not_found) — возвращаем ошибку как есть, НЕ откатываясь
// на устаревший line/col: это тот же протухший снимок, только молчаливый.
json GroundService::resolveCursorByUsr(const std::string& file, const std::string& usr) {
    if (file.empty() || usr.empty())
        return json{{"ok", false}, {"error", {{"code","cursor_anchor_lost"},{"message","file или usr пусты — курсор не заякорен"}}}};
    std::vector<std::string> flags;
    json flagsErr;
    if (!resolveFlags(file, flags, flagsErr)) return flagsErr;
    return cpptool::queryLocateByUSR(file, usr, flags);
}

GroundResult GroundService::buildGround(const std::string& file, int line, int col) {
    GroundResult g;
    std::vector<std::string> flags;
    json flagsErr;
    if (!resolveFlags(file, flags, flagsErr)) { g.ok=false; g.error=flagsErr; return g; }
    g.locate = cpptool::queryLocateSymbol(file, line, col, flags);
    if (!g.locate.value("ok", false)) { g.ok=false; g.error=g.locate; return g; }
    g.ok=true;
    g.enclosingMethod = g.locate.value("enclosing_method", json());
    g.enclosingClass  = g.locate.value("enclosing_class", json());
    std::string usr = g.enclosingMethod.is_null()?"":g.enclosingMethod.value("usr","");
    std::string className;
    if(!g.enclosingClass.is_null()) className=g.enclosingClass.value("name","");
    // S3 cross-TU с кэшем: ProjectIndex (L1 mem + L2 JsonFileStore)
    json br = cpptool::queryBlastRadiusCross(compileCommandsPath_, file, className, usr, flags);
    if(br.value("ok",false)){
        g.suggestedMode = br.value("suggested_mode","sandbox");
        g.scaleFacts = br.value("facts", json::object());
        if(br.contains("outline")) g.classOutline = br["outline"];
        // refs не храним целиком — в кэше они по всем файлам, в facts уже агрегат
    } else {
        g.suggestedMode="sandbox";
        g.scaleFacts=json{{"error",br.value("error",json::object())},{"note","blast_radius failed, fallback sandbox"}};
    }
    fillVirtualOverridesIfNeeded(g, compileCommandsPath_, this);
    // Compute DetailKind deterministically.
    if (!g.enclosingMethod.is_null()) {
        bool isPureVirtual = g.enclosingMethod.value("is_pure_virtual", false);
        bool isVirtual     = g.enclosingMethod.value("is_virtual", false);
        if (isPureVirtual)   g.detailKind = DetailKind::PURE_VIRTUAL;
        else if (isVirtual)  g.detailKind = DetailKind::VIRTUAL_WITH_BASE;
    }
    return g;
}

GroundResult GroundService::buildGroundLight(const std::string& file, int line, int col) {
    GroundResult g;
    std::vector<std::string> flags;
    json flagsErr;
    if (!resolveFlags(file, flags, flagsErr)) {
        // Файла нет в compile_commands.json (типичный случай: заголовочный файл).
        // Читаем файл напрямую: извлекаем имя класса (ищем 'class X' выше строки курсора)
        // и имя метода (из строки курсора). Затем queryFindImplementors итерируется по
        // всем TU из compile_commands.json и находит реализации через наследование.
        // Это детерминированно и не зависит от фонового индекса clangd.
        std::string className, methodName;
        {
            std::ifstream hf(file);
            if (hf) {
                std::vector<std::string> lines;
                std::string ln;
                while (std::getline(hf, ln)) lines.push_back(ln);
                // Имя метода: из строки курсора.
                if ((int)line - 1 < (int)lines.size()) {
                    const std::string& cursorLine = lines[(int)line - 1];
                    // Ищем идентификатор непосредственно перед '(' — это имя метода.
                    std::smatch m;
                    if (std::regex_search(cursorLine, m, std::regex(R"([\w:]+(?=\s*\())")))
                        methodName = m[0].str();
                    // Убираем возможный квалификатор.
                    auto pos = methodName.rfind("::");
                    if (pos != std::string::npos) methodName = methodName.substr(pos + 2);
                }
                // Имя класса: ищем 'class X' выше строки курсора.
                for (int i = (int)line - 2; i >= 0; --i) {
                    std::smatch m2;
                    if (std::regex_search(lines[i], m2, std::regex(R"(\bclass\s+(\w+))")))
                    { className = m2[1].str(); break; }
                }
            }
        }
        if (!className.empty() && !methodName.empty()) {
            json implRes = cpptool::queryFindImplementors(compileCommandsPath_, className, methodName);
            if (implRes.value("ok", false) && implRes.contains("implementors")) {
                g.ok = true;
                g.suggestedMode = "sandbox";
                g.scaleFacts = json{{"note", "header fallback: queryFindImplementors"}};
                g.enclosingMethod = json{
                    {"usr",""},
                    {"signature", "virtual " + methodName + " (из " + className + ", резолв через наследование)"},
                    {"file", file}, {"line", (int)line},
                    {"is_virtual", true}, {"is_pure_virtual", true}
                };
                json overrides = json::array();
                for (auto& impl : implRes["implementors"]) {
                    overrides.push_back(json{
                        {"usr",       impl.value("usr", "")},
                        {"signature", impl.value("signature", "")},
                        {"file",      impl.value("file", "")},
                        {"line",      impl.value("line", 0)}
                    });
                }
                g.virtualOverrides = json{{"ok",true},{"overrides",overrides},
                    {"source","queryFindImplementors"},
                    {"descendants", implRes.value("descendants",0)},
                    {"files_scanned", implRes.value("files_scanned",0)}};
                g.virtualOverridesChecked = true;
                g.detailKind = DetailKind::HEADER_NOT_IN_CC;
                return g;
            }
        }
        g.ok=false; g.error=flagsErr; return g;
    }
    g.locate = cpptool::queryLocateSymbol(file, line, col, flags);
    if (!g.locate.value("ok", false)) { g.ok=false; g.error=g.locate; return g; }
    g.ok=true;
    g.enclosingMethod = g.locate.value("enclosing_method", json());
    g.enclosingClass  = g.locate.value("enclosing_class", json());
    g.suggestedMode="sandbox";
    g.scaleFacts=json{{"note","light ground: blast_radius not computed"}};
    fillVirtualOverridesIfNeeded(g, compileCommandsPath_, this);
    // Compute DetailKind deterministically.
    if (!g.enclosingMethod.is_null()) {
        bool isPureVirtual = g.enclosingMethod.value("is_pure_virtual", false);
        bool isVirtual     = g.enclosingMethod.value("is_virtual", false);
        if (isPureVirtual)   g.detailKind = DetailKind::PURE_VIRTUAL;
        else if (isVirtual)  g.detailKind = DetailKind::VIRTUAL_WITH_BASE;
    }
    return g;
}

} // namespace cppagent
