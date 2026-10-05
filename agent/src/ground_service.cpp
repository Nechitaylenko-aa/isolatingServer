#include "ground_service.h"
#include "blast_radius.h"
#include "compile_commands.h"
#include "queries.h"
#include "clang_index.h"

namespace cppagent {

namespace {

// Слой 2 (arch.md): где мы физически находимся в коде. Чисто детерминированная
// функция от фактов, которые Ground уже собрал — ничего не парсит заново.
//
// Эвристика, а не факт (помечено явно, не выдаётся за достоверное): pimpl
// определяется по суффиксу "Impl" в имени класса. Ложноположительные случаи
// возможны (класс просто называется *Impl не из-за идиомы pimpl) — это не
// хуже старого DetailKind, который pimpl вообще не различал, но и не лучше,
// чем оно могло бы быть при реальном структурном анализе (отдельный класс
// с теми же именами методов без наследования). Уточнить при необходимости.
CursorContext computeCursorContext(const GroundResult& g, const std::string& file) {
    auto endsWith = [](const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    bool isHeaderFile = endsWith(file, ".h") || endsWith(file, ".hpp") || endsWith(file, ".hh");

    if (g.enclosingMethod.is_null()) {
        if (!g.ok) return CursorContext::NO_CONTEXT;
        if (!isHeaderFile) return CursorContext::NO_CONTEXT; // .cpp вне метода — некуда отнести точнее
        std::string kind = g.locate.value("cursor_kind", "");
        if (kind.find("ClassDecl") != std::string::npos || kind.find("StructDecl") != std::string::npos)
            return CursorContext::INTERFACE_CLASS_DECL;
        if (kind.find("EnumDecl") != std::string::npos || kind.find("TypedefDecl") != std::string::npos ||
            kind.find("VarDecl") != std::string::npos  || kind.find("FieldDecl") != std::string::npos)
            return CursorContext::PLAIN_HEADER;
        return CursorContext::CC_FOREST; // пробел/комментарий/между классами в .h — наименее специфичный остаток
    }

    bool isPureVirtual = g.enclosingMethod.value("is_pure_virtual", false);
    bool isVirtual      = g.enclosingMethod.value("is_virtual", false);
    if (isPureVirtual) return CursorContext::PURE_VIRTUAL_METHOD;
    if (isVirtual)     return CursorContext::VIRTUAL_METHOD_WITH_BODY;

    if (g.enclosingClass.is_null()) return CursorContext::FREE_FUNCTION;

    std::string className = g.enclosingClass.value("name", "");
    if (endsWith(className, "Impl")) return CursorContext::PIMPL_IMPL; // approx, см. комментарий выше

    std::string methodFile = g.enclosingMethod.value("file", "");
    bool bodyInCpp = endsWith(methodFile, ".cpp") || endsWith(methodFile, ".cc") || endsWith(methodFile, ".cxx");
    return bodyInCpp ? CursorContext::CONCRETE_METHOD_IMPL : CursorContext::NON_VIRTUAL_METHOD;
}

// Если enclosingMethod — виртуальный, ищем ВСЕ переопределения базового метода,
// а не только потомков текущего. Курсор может стоять на CMySQLModel::execSQL_read,
// но нам нужен и CPostgresModel::execSQL_read — они siblings (оба переопределяют
// CDataBaseModel::execSQL_read), а не parent-child.
//
// Раньше подъём к базовому USR делался РУЧНЫМ повторным парсингом TU
// (ParsedUnit::parse + visitor, искали курсор с нужным usr, затем
// clang_getOverriddenCursors) — дорого и дублировало то, что build_and_store
// и так уже один раз посчитал в overrides-таблице. Теперь CTuCache сам
// поднимается к базовому внутри queryVirtualOverrides (один SELECT) — здесь
// просто передаём usr текущего метода как есть, без предварительного подъёма.
void fillVirtualOverridesIfNeeded(GroundResult& g, const std::string& compileCommandsPath) {
    if (g.enclosingMethod.is_null()) return;
    bool isVirtual = g.enclosingMethod.value("is_virtual", false);
    if (!isVirtual) return;
    g.virtualOverridesChecked = true;
    std::string usr = g.enclosingMethod.value("usr", "");
    g.virtualOverrides = cpptool::queryVirtualOverrides(compileCommandsPath, usr);
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
// decl в CTuCache глобален по usr — file больше не нужен для поиска вовсе
// (раньше был нужен, чтобы знать, какой TU парсить; теперь это SELECT по
// всему индексу проекта).
json GroundService::resolveCursorByUsr(const std::string& file, const std::string& usr) {
    (void)file; // оставлен в сигнатуре ради обратной совместимости вызывающего кода
    if (usr.empty())
        return json{{"ok", false}, {"error", {{"code","cursor_anchor_lost"},{"message","usr пуст — курсор не заякорен"}}}};
    return cpptool::queryLocateByUSR(usr, compileCommandsPath_);
}

GroundResult GroundService::buildGround(const std::string& file, int line, int col) {
    GroundResult g;
    g.locate = cpptool::queryLocateSymbol(file, line, col, compileCommandsPath_);
    if (!g.locate.value("ok", false)) { g.ok=false; g.error=g.locate; return g; }
    g.ok=true;
    g.enclosingMethod = g.locate.value("enclosing_method", json());
    g.enclosingClass  = g.locate.value("enclosing_class", json());
    std::string usr = g.enclosingMethod.is_null()?"":g.enclosingMethod.value("usr","");
    std::string className;
    if(!g.enclosingClass.is_null()) className=g.enclosingClass.value("name","");

    std::vector<std::string> flags; json flagsErr;
    resolveFlags(file, flags, flagsErr); // нужны только для querySymbolRefs внутри blast_radius (single-TU путь)
    json br = cpptool::queryBlastRadiusCross(compileCommandsPath_, file, className, usr, flags);
    if(br.value("ok",false)){
        g.suggestedMode = br.value("suggested_mode","sandbox");
        g.scaleFacts = br.value("facts", json::object());
        if(br.contains("outline")) g.classOutline = br["outline"];
    } else {
        g.suggestedMode="sandbox";
        g.scaleFacts=json{{"error",br.value("error",json::object())},{"note","blast_radius failed, fallback sandbox"}};
    }
    fillVirtualOverridesIfNeeded(g, compileCommandsPath_);
    g.cursorContext = computeCursorContext(g, file);
    return g;
}

// UNDERSTAND — лёгкий путь: только locate, без symbolRefs/outline/scale.
//
// Раньше здесь был отдельный regex-фоллбэк на queryFindImplementors для
// заголовков, отсутствующих в compile_commands.json (не было другого способа
// узнать флаги компиляции для файла, который сам не компилируется). Теперь
// он не нужен: queryLocateSymbol сам индексирует весь проект через
// ensure_project_indexed и ищет decl по file/line НАПРЯМУЮ — а заголовок,
// если он хоть раз был замечен как #include кем-то из проиндексированных
// .cpp, уже имеет свои decl-записи в кэше (reverse-индекс по tu_dep). Файл
// просто обязан быть внутри project root — другого условия больше нет.
GroundResult GroundService::buildGroundLight(const std::string& file, int line, int col) {
    GroundResult g;
    g.locate = cpptool::queryLocateSymbol(file, line, col, compileCommandsPath_);
    if (!g.locate.value("ok", false)) { g.ok=false; g.error=g.locate; return g; }
    g.ok=true;
    g.enclosingMethod = g.locate.value("enclosing_method", json());
    g.enclosingClass  = g.locate.value("enclosing_class", json());
    g.suggestedMode="sandbox";
    g.scaleFacts=json{{"note","light ground: blast_radius not computed"}};
    fillVirtualOverridesIfNeeded(g, compileCommandsPath_);
    g.cursorContext = computeCursorContext(g, file);
    return g;
}

} // namespace cppagent
