#include "tu_cache.h"

#include "clang_index.h"
#include "compile_commands.h"
#include "db_manager.h"
#include "hash.h"
#include "json.hpp"

#include <clang-c/Index.h>

#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <mutex>

namespace cpptool
{

using json = nlohmann::json;

namespace
{

// v2: decl/refs хранятся как факты (позиции, сигнатуры, иерархия), а не
// только счётчики. 2 -> 3: полная замена схемы, см. tu_cache.h.
// 3 -> 4: PK decl стал (usr, file, start_line) вместо голого usr — иначе
// декларация в заголовке и определение в .cpp для того же метода схлопывались
// в одну строку, и find_enclosing по заголовку терял метод (см. схему ниже).
// 4 -> 5: добавлена таблица calls (граф вызовов caller_usr -> callee_usr).
// 5 -> 6: исправлена семантика чистки calls (по caller_file, не caller_usr).
// 6 -> 7: mtime (source_mtime в tu, mtime в tu_dep) для fast-path в is_valid —
// без чтения содержимого файлов, если время модификации не менялось.
constexpr int kSchemaVersion = 7;
constexpr const char* kMetaLibclangVersion = "libclang_version";

// Override файла кэша (из конфига cf-agent). Пусто = рядом с compile_commands.json.
std::string g_cacheFile;

std::string canonicalize(const std::string& path)
{
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec);
    if (ec) abs = std::filesystem::path(path);
    std::filesystem::path weak = std::filesystem::weakly_canonical(abs, ec);
    if (ec) weak = abs.lexically_normal();
    return weak.string();
}

bool is_within_project(const std::string& path, const std::string& root)
{
    if (root.empty()) return false;
    if (path == root) return true;
    if (path.size() <= root.size()) return false;
    if (path.compare(0, root.size(), root) != 0) return false;
    return path[root.size()] == '/';
}

bool is_fn_like_kind(const std::string& k)
{
    return k == "CXXMethod" || k == "Constructor" || k == "Destructor" || k == "FunctionDecl";
}
bool is_class_like_kind(const std::string& k)
{
    return k == "ClassDecl" || k == "StructDecl" || k == "ClassTemplate";
}

void cursorExtentLines(CXCursor c, unsigned& startLine, unsigned& endLine)
{
    CXSourceRange ext = clang_getCursorExtent(c);
    CXSourceLocation s = clang_getRangeStart(ext), e = clang_getRangeEnd(ext);
    CXFile sf, ef; unsigned sc, ec, so, eo;
    clang_getSpellingLocation(s, &sf, &startLine, &sc, &so);
    clang_getSpellingLocation(e, &ef, &endLine, &ec, &eo);
}

std::string accessSpecString(CXCursor c)
{
    switch (clang_getCXXAccessSpecifier(c))
    {
        case CX_CXXPublic: return "public";
        case CX_CXXProtected: return "protected";
        case CX_CXXPrivate: return "private";
        default: return "";
    }
}

// --- строки для сбора в одном проходе build_and_store ---
struct DeclRow
{
    std::string usr, kind, name, file, parentUsr, access, signature;
    unsigned startLine{0}, endLine{0}, declLine{0}, declCol{0};
    bool isDefinition{false}, isVirtual{false}, isPureVirtual{false}, isStatic{false};
};
struct RefRow { std::string file, usr, kind; unsigned line{0}, col{0}; };
struct BaseRow { std::string classUsr, baseUsr, baseName; int ord{0}; };
struct OverrideRow { std::string methodUsr, baseMethodUsr; };
struct CallRow { std::string callerUsr, calleeUsr, callerFile; unsigned callerLine{0}; };

struct BuildCtx
{
    const std::string* projectRoot;
    std::vector<DeclRow>* decls;
    std::vector<RefRow>* refs;
    std::vector<BaseRow>* bases;
    std::vector<OverrideRow>* overrides;
    std::vector<CallRow>* calls;
    std::unordered_set<std::string>* seenRefPositions;
};

void captureBases(CXCursor classCursor, const std::string& classUsr, std::vector<BaseRow>* out)
{
    struct Inner { const std::string* classUsr; std::vector<BaseRow>* out; int ord; };
    Inner inner{&classUsr, out, 0};
    clang_visitChildren(classCursor, [](CXCursor c, CXCursor, CXClientData d) -> CXChildVisitResult {
        auto* in = static_cast<Inner*>(d);
        if (clang_getCursorKind(c) == CXCursor_CXXBaseSpecifier)
        {
            CXType baseType = clang_getCursorType(c);
            std::string spelling = cxStringToStd(clang_getTypeSpelling(baseType));
            for (const char* pfx : {"class ", "struct "})
                if (spelling.rfind(pfx, 0) == 0) { spelling = spelling.substr(strlen(pfx)); break; }
            CXCursor baseDecl = clang_getTypeDeclaration(baseType);
            std::string baseUsr = clang_Cursor_isNull(baseDecl) ? "" : cursorUSR(baseDecl);
            in->out->push_back({*in->classUsr, baseUsr, spelling, in->ord});
            ++in->ord;
        }
        return CXChildVisit_Continue;
    }, &inner);
}

CXChildVisitResult buildVisitor(CXCursor c, CXCursor, CXClientData data)
{
    auto* ctx = static_cast<BuildCtx*>(data);
    CXCursorKind k = clang_getCursorKind(c);

    // --- calls: рёбра графа вызовов (только внутри project root) ---
    // Вызывающий определяется подъёмом по clang_getCursorSemanticParent до
    // ближайшей функции/метода. Стек с push/pop тут НЕ работает: libclang
    // обходит детей уже ПОСЛЕ возврата visitor, поэтому pop срабатывал бы до
    // рекурсии в тело функции (проверено — calls оставались пустыми).
    if (k == CXCursor_CallExpr)
    {
        CXCursor ref = clang_getCursorReferenced(c);
        if (!clang_Cursor_isNull(ref) && !clang_isInvalid(clang_getCursorKind(ref)))
        {
            std::string calleeUsr = cursorUSR(ref);
            if (!calleeUsr.empty() && is_within_project(canonicalize(cursorFile(ref)), *ctx->projectRoot))
            {
                for (CXCursor p = clang_getCursorSemanticParent(c); !clang_Cursor_isNull(p);
                     p = clang_getCursorSemanticParent(p))
                {
                    CXCursorKind pk = clang_getCursorKind(p);
                    if (pk == CXCursor_FunctionDecl || pk == CXCursor_CXXMethod ||
                        pk == CXCursor_Constructor || pk == CXCursor_Destructor)
                    {
                        std::string callerUsr = cursorUSR(p);
                        if (!callerUsr.empty())
                            ctx->calls->push_back({callerUsr, calleeUsr,
                                                   canonicalize(cursorFile(c)), cursorLine(c)});
                        break;
                    }
                }
            }
        }
    }

    // --- refs: использования любого символа, объявленного внутри project root ---
    CXCursor referenced = clang_getCursorReferenced(c);
    // Декларация часто "ссылается" сама на себя (канонический redecl) —
    // это не использование символа, а сама декларация. Без этого фильтра
    // каждый прототип в заголовке считался бы ещё одной ссылкой на себя
    // (измерено: api.h:2:5 и beta.cpp:1:5 давали себя же как refs).
    if (!clang_Cursor_isNull(referenced) && !clang_isInvalid(clang_getCursorKind(referenced)) &&
        !clang_equalCursors(c, referenced))
    {
        std::string usr = cursorUSR(referenced);
        if (!usr.empty() && is_within_project(canonicalize(cursorFile(referenced)), *ctx->projectRoot))
        {
            // Канонизируем здесь же — без этого один и тот же заголовок,
            // попавший в два разных TU через разные относительные #include
            // ("../include/Foo.h" из src/, "include/Foo.h" из другого TU),
            // лёг бы в decl/refs под разными строками-путями и find_enclosing
            // по каноническому пути (как его передаёт любой вызывающий код)
            // не нашёл бы запись вообще (воспроизведено и исправлено по факту).
            std::string file = canonicalize(cursorFile(c));
            unsigned line = cursorLine(c), col = cursorColumn(c);
            std::string posKey = file + ":" + std::to_string(line) + ":" + std::to_string(col);
            // Одно место в исходнике даёт несколько вложенных курсоров
            // (CallExpr -> UnexposedExpr -> DeclRefExpr) — без дедупликации
            // по позиции один вызов дал бы несколько ссылок.
            if (ctx->seenRefPositions->insert(posKey).second)
                ctx->refs->push_back({file, usr, cursorKindName(c), line, col});
        }
    }

    std::string kindName = cursorKindName(c);
    // Канонический путь сразу — см. комментарий выше про decl/refs под разными
    // строками для одного и того же физического файла.
    std::string file = canonicalize(cursorFile(c));
    bool inProject = is_within_project(file, *ctx->projectRoot);

    // Без гейта по clang_isCursorDefinition: голая декларация в заголовке
    // (int do_work(int value); без тела — обычный прототип) тоже должна быть
    // найдена как enclosing_method, иначе курсор на ней не разрешится вовсе.
    // is_definition хранится как данные (ON CONFLICT ниже не даёт более
    // поздней голой декларации того же usr затереть уже найденное определение).
    if (inProject && is_fn_like_kind(kindName))
    {
        DeclRow row;
        row.usr = cursorUSR(c);
        row.kind = kindName;
        row.name = cursorSpelling(c);
        row.file = file;
        cursorExtentLines(c, row.startLine, row.endLine);
        row.declLine = cursorLine(c);
        row.declCol = cursorColumn(c);
        row.isDefinition = static_cast<bool>(clang_isCursorDefinition(c));
        bool isMethod = (k == CXCursor_CXXMethod);
        row.isVirtual = isMethod && static_cast<bool>(clang_CXXMethod_isVirtual(c));
        row.isPureVirtual = isMethod && static_cast<bool>(clang_CXXMethod_isPureVirtual(c));
        row.isStatic = isMethod && static_cast<bool>(clang_CXXMethod_isStatic(c));
        row.access = accessSpecString(c);
        row.signature = buildMethodSignature(c);
        CXCursor semanticParent = clang_getCursorSemanticParent(c);
        CXCursorKind pk = clang_getCursorKind(semanticParent);
        if (pk == CXCursor_ClassDecl || pk == CXCursor_StructDecl)
            row.parentUsr = cursorUSR(semanticParent);
        if (!row.usr.empty())
            ctx->decls->push_back(row);

        if (isMethod)
        {
            CXCursor* overridden = nullptr;
            unsigned count = 0;
            clang_getOverriddenCursors(c, &overridden, &count);
            for (unsigned i = 0; i < count; ++i)
            {
                std::string baseUsr = cursorUSR(overridden[i]);
                if (!baseUsr.empty() && !row.usr.empty())
                    ctx->overrides->push_back({row.usr, baseUsr});
            }
            clang_disposeOverriddenCursors(overridden);
        }
    }
    else if (inProject && is_class_like_kind(kindName) && clang_isCursorDefinition(c))
    {
        DeclRow row;
        row.usr = cursorUSR(c);
        row.kind = kindName;
        row.name = cursorSpelling(c);
        row.file = file;
        cursorExtentLines(c, row.startLine, row.endLine);
        row.declLine = cursorLine(c);
        row.declCol = cursorColumn(c);
        row.isDefinition = true;
        if (!row.usr.empty())
        {
            ctx->decls->push_back(row);
            captureBases(c, row.usr, ctx->bases);
        }
    }
    else if (inProject && (kindName == "EnumDecl" || kindName == "TypedefDecl" ||
                           kindName == "VarDecl" || kindName == "FieldDecl"))
    {
        // Нужны не ради сигнатур, а чтобы find_enclosing честно отличал
        // "курсор на enum/typedef/var в .h" от пустого места (CC_FOREST) —
        // см. computeCursorContext в agent/ground_service.cpp.
        DeclRow row;
        row.usr = cursorUSR(c);
        if (!row.usr.empty())
        {
            row.kind = kindName;
            row.name = cursorSpelling(c);
            row.file = file;
            row.declLine = row.startLine = cursorLine(c);
            row.declCol = cursorColumn(c);
            row.endLine = row.startLine;
            ctx->decls->push_back(row);
        }
    }

    return CXChildVisit_Recurse;
}

} // namespace

CTuCache::CTuCache(std::string dbPath)
    : db_(std::make_unique<CDbManager>())
    , dbPath_(std::move(dbPath))
{
    if (!db_->open(dbPath_)) { openError_ = db_->last_error(); return; }
    if (!ensure_schema()) { openError_ = db_->last_error().empty() ? "схема кэша не создалась" : db_->last_error(); return; }
    if (!sync_libclang_version()) { openError_ = db_->last_error(); return; }
    isOpen_ = true;
}

CTuCache::~CTuCache() = default;

std::string defaultCachePathFor(const std::string& compileCommandsPath)
{
    if (!g_cacheFile.empty()) return g_cacheFile;
    std::filesystem::path ccDir = std::filesystem::path(compileCommandsPath).parent_path();
    return (ccDir / ".cpp-tool-cache" / "index.db").string();
}

void set_cache_file(const std::string& dbFilePath)
{
    g_cacheFile = dbFilePath;
}

std::shared_ptr<CTuCache> CTuCache::shared(const std::string& dbPath)
{
    // Реестр на процесс: один живой кэш на путь. Это убирает повторный
    // ensure_project_indexed для каждого под-запроса в рамках одного прогона
    // (locate + overrides + call-chain и т.п.), который заново сканировал ВСЕ TU.
    static std::mutex mtx;
    static std::unordered_map<std::string, std::weak_ptr<CTuCache>> registry;
    std::lock_guard<std::mutex> lock(mtx);
    auto it = registry.find(dbPath);
    if (it != registry.end())
    {
        if (auto alive = it->second.lock())
            return alive;
    }
    auto fresh = std::make_shared<CTuCache>(dbPath);
    registry[dbPath] = fresh;
    return fresh;
}

bool CTuCache::ensure_schema()
{
    int64_t storedVersion = 0;
    db_->query_int("PRAGMA user_version", storedVersion);
    if (storedVersion != kSchemaVersion)
    {
        for (const char* t : {"refs", "decl", "bases", "overrides", "calls", "tu_dep", "tu", "meta"})
            db_->execute(std::string("DROP TABLE IF EXISTS ") + t);
        if (!db_->execute("PRAGMA user_version=" + std::to_string(kSchemaVersion)))
            return false;
        db_->execute("VACUUM"); // старая запись могла быть сильно раздута — см. git-историю
    }

    if (!db_->execute("CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)"))
        return false;

    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS tu ("
            "  id INTEGER PRIMARY KEY, path TEXT NOT NULL UNIQUE,"
            "  source_hash TEXT NOT NULL, flags_hash TEXT NOT NULL DEFAULT '',"
            "  source_mtime INTEGER NOT NULL DEFAULT 0,"
            "  parsed_at INTEGER NOT NULL DEFAULT 0)"))
        return false;

    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS tu_dep ("
            "  tu_id INTEGER NOT NULL, path TEXT NOT NULL, content_hash TEXT NOT NULL,"
            "  mtime INTEGER NOT NULL DEFAULT 0,"
            "  PRIMARY KEY (tu_id, path))"))
        return false;
    if (!db_->execute("CREATE INDEX IF NOT EXISTS idx_tu_dep_path ON tu_dep(path)"))
        return false;

    // PK (usr, file, start_line), НЕ usr сам по себе: один и тот же usr
    // физически существует в НЕСКОЛЬКИХ местах одновременно, если метод
    // объявлен в классе (.h) и определён вне тела (.cpp) — это обычный,
    // массовый C++-паттерн, не редкий случай. PK по одному usr схлопывал бы
    // декларацию в заголовке и определение в .cpp в одну строку — тогда
    // find_enclosing по заголовку никогда не находил бы метод (воспроизведено
    // на реальном проекте: calculateNextMinute в DateTimeCalculator.h терялся
    // полностью, выживала только запись из .cpp).
    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS decl ("
            "  usr TEXT NOT NULL, kind TEXT NOT NULL, name TEXT NOT NULL,"
            "  file TEXT NOT NULL, start_line INTEGER NOT NULL, end_line INTEGER NOT NULL,"
            "  decl_line INTEGER NOT NULL DEFAULT 0, decl_col INTEGER NOT NULL DEFAULT 0,"
            "  parent_usr TEXT NOT NULL DEFAULT '', access TEXT NOT NULL DEFAULT '',"
            "  is_definition INTEGER NOT NULL DEFAULT 0, is_virtual INTEGER NOT NULL DEFAULT 0,"
            "  is_pure_virtual INTEGER NOT NULL DEFAULT 0, is_static INTEGER NOT NULL DEFAULT 0,"
            "  signature TEXT NOT NULL DEFAULT '',"
            "  PRIMARY KEY (usr, file, start_line))"))
        return false;
    if (!db_->execute("CREATE INDEX IF NOT EXISTS idx_decl_file_lines ON decl(file, start_line, end_line)"))
        return false;
    if (!db_->execute("CREATE INDEX IF NOT EXISTS idx_decl_parent ON decl(parent_usr)"))
        return false;
    if (!db_->execute("CREATE INDEX IF NOT EXISTS idx_decl_name ON decl(name)"))
        return false;
    if (!db_->execute("CREATE INDEX IF NOT EXISTS idx_decl_usr ON decl(usr)")) // усечённый PK больше не покрывает usr один
        return false;

    // PK по физической позиции, не по tu_id — повторная вставка той же
    // ссылки из заголовка от другого включающего TU становится no-op'ом.
    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS refs ("
            "  file TEXT NOT NULL, line INTEGER NOT NULL, col INTEGER NOT NULL,"
            "  usr TEXT NOT NULL, kind TEXT NOT NULL DEFAULT '',"
            "  PRIMARY KEY (file, line, col, usr))"))
        return false;
    if (!db_->execute("CREATE INDEX IF NOT EXISTS idx_refs_usr ON refs(usr)"))
        return false;

    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS bases ("
            "  class_usr TEXT NOT NULL, base_usr TEXT NOT NULL DEFAULT '',"
            "  base_name TEXT NOT NULL, ord INTEGER NOT NULL,"
            "  PRIMARY KEY (class_usr, ord))"))
        return false;

    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS overrides ("
            "  method_usr TEXT NOT NULL, base_method_usr TEXT NOT NULL,"
            "  PRIMARY KEY (method_usr, base_method_usr))"))
        return false;
    if (!db_->execute("CREATE INDEX IF NOT EXISTS idx_overrides_base ON overrides(base_method_usr)"))
        return false;

    // PK (caller,callee,caller_file,caller_line): сайт вызова уникален по
    // физической позиции (один вызов = одно ребро), повторная вставка из
    // включающего TU — no-op.
    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS calls ("
            "  caller_usr TEXT NOT NULL, callee_usr TEXT NOT NULL,"
            "  caller_file TEXT NOT NULL, caller_line INTEGER NOT NULL,"
            "  PRIMARY KEY (caller_usr, callee_usr, caller_file, caller_line))"))
        return false;
    if (!db_->execute("CREATE INDEX IF NOT EXISTS idx_calls_caller ON calls(caller_usr)"))
        return false;
    return db_->execute("CREATE INDEX IF NOT EXISTS idx_calls_callee ON calls(callee_usr)");
}

bool CTuCache::sync_libclang_version()
{
    const std::string current = clangVersion();
    std::string stored;
    if (db_->query_string("SELECT value FROM meta WHERE key = '" + std::string(kMetaLibclangVersion) + "'", stored))
    {
        if (stored == current) return true;
        if (!wipe_all()) return false;
    }
    CDbStatement stmt;
    if (!stmt.prepare(db_->handle(), "INSERT OR REPLACE INTO meta(key, value) VALUES (?, ?)")) return false;
    if (!stmt.bind_string(1, kMetaLibclangVersion) || !stmt.bind_string(2, current)) return false;
    stmt.step();
    return true;
}

bool CTuCache::wipe_all()
{
    bool ok = true;
    for (const char* t : {"refs", "decl", "bases", "overrides", "calls", "tu_dep", "tu"})
        ok = ok && db_->execute(std::string("DELETE FROM ") + t);
    if (ok) db_->execute("VACUUM");
    return ok;
}

int64_t CTuCache::tu_id_for(const std::string& file)
{
    CDbStatement stmt;
    if (!stmt.prepare(db_->handle(), "SELECT id FROM tu WHERE path = ?")) return -1;
    if (!stmt.bind_string(1, file) || !stmt.step() || !stmt.has_row()) return -1;
    return stmt.column_int(0);
}

bool CTuCache::clear_tu_contents(int64_t tuId, const std::vector<std::string>& depFiles)
{
    // usr всех деклараций в этих файлах — чтобы почистить bases/overrides,
    // ключи которых (class_usr/method_usr) не привязаны к файлу. Собираем
    // ДО удаления decl.
    std::set<std::string> usrs;
    {
        CDbStatement q;
        if (!q.prepare(db_->handle(), "SELECT DISTINCT usr FROM decl WHERE file = ?")) return false;
        for (const auto& f : depFiles)
        {
            if (!q.reset() || !q.bind_string(1, f)) return false;
            while (q.step() && q.has_row()) usrs.insert(q.column_string(0));
        }
    }

    // decl/refs/calls — по файлам нового набора зависимостей (см. комментарий
    // в tu_cache.h: PK incl. start_line, иначе остаются призраки старых строк).
    // calls чистятся по caller_file (ФИЗИЧЕСКОЙ позиции сайта вызова), НЕ по
    // caller_usr: методы общего заголовка включаются многими TU, и удаление по
    // usr стирало бы рёбра, посеянные другим TU (проверено — DateTimeCalculator
    // терял все вызовы). По файлу же пересборка этого TU их заново и засеет.
    for (const auto& f : depFiles)
    {
        for (const char* sql : {"DELETE FROM decl WHERE file = ?",
                                "DELETE FROM refs WHERE file = ?",
                                "DELETE FROM calls WHERE caller_file = ?"})
        {
            CDbStatement st;
            if (!st.prepare(db_->handle(), sql)) return false;
            // step() у DELETE возвращает false (SQLITE_DONE) — true означало бы ошибку.
            if (!st.bind_string(1, f) || st.step()) return false;
        }
    }

    for (const auto& u : usrs)
    {
        {
            CDbStatement st;
            if (!st.prepare(db_->handle(), "DELETE FROM bases WHERE class_usr = ?")) return false;
            if (!st.bind_string(1, u) || st.step()) return false;
        }
        {
            CDbStatement st;
            if (!st.prepare(db_->handle(), "DELETE FROM overrides WHERE method_usr = ? OR base_method_usr = ?")) return false;
            if (!st.bind_string(1, u) || !st.bind_string(2, u) || st.step()) return false;
        }
    }

    CDbStatement deps;
    if (!deps.prepare(db_->handle(), "DELETE FROM tu_dep WHERE tu_id = ?")) return false;
    if (!deps.bind_int(1, tuId) || deps.step()) return false;
    return true;
}

bool CTuCache::build_and_store(const std::string& file, const std::vector<std::string>& flags,
                               const std::string& projectRoot)
{
    const std::string hashBefore = hash_file(file);
    if (hashBefore.empty()) return false;
    const int64_t mtimeBefore = file_mtime_ns(file);

    auto unitOpt = ParsedUnit::parse(file, flags);
    if (!unitOpt) return false;

    const std::string hashAfter = hash_file(file);
    if (hashAfter != hashBefore) return false;

    std::vector<std::tuple<std::string, std::string, int64_t>> deps; // path, hash, mtime
    std::unordered_map<std::string, bool> seen;
    std::vector<std::string> rawInclusions = unitOpt->inclusions();
    rawInclusions.push_back(file);
    for (const auto& raw : rawInclusions)
    {
        const std::string path = canonicalize(raw);
        if (!is_within_project(path, projectRoot)) continue;
        if (seen.find(path) != seen.end()) continue;
        seen[path] = true;
        const std::string contentHash = hash_file(path);
        if (contentHash.empty()) continue;
        deps.emplace_back(path, contentHash, file_mtime_ns(path));
    }

    if (!db_->begin_transaction()) return false;

    const std::string flagsHash = hash_flags(flags);
    {
        CDbStatement upsert;
        if (!upsert.prepare(db_->handle(),
                "INSERT INTO tu(path, source_hash, flags_hash, source_mtime, parsed_at) VALUES (?, ?, ?, ?, ?) "
                "ON CONFLICT(path) DO UPDATE SET source_hash=excluded.source_hash, "
                "flags_hash=excluded.flags_hash, source_mtime=excluded.source_mtime, parsed_at=excluded.parsed_at"))
            return db_->rollback();
        if (!upsert.bind_string(1, file) || !upsert.bind_string(2, hashBefore) ||
            !upsert.bind_string(3, flagsHash) || !upsert.bind_int(4, mtimeBefore) ||
            !upsert.bind_int(5, 0))
            return db_->rollback();
        upsert.step();
    }

    int64_t tuId = -1;
    {
        CDbStatement idStmt;
        if (!idStmt.prepare(db_->handle(), "SELECT id FROM tu WHERE path = ?") ||
            !idStmt.bind_string(1, file) || !idStmt.step() || !idStmt.has_row())
            return db_->rollback();
        tuId = idStmt.column_int(0);
    }

    std::vector<std::string> depFiles;
    depFiles.reserve(deps.size());
    for (const auto& d : deps) depFiles.push_back(std::get<0>(d));
    if (!clear_tu_contents(tuId, depFiles)) return db_->rollback();

    {
        CDbStatement depStmt;
        if (!depStmt.prepare(db_->handle(), "INSERT OR REPLACE INTO tu_dep(tu_id, path, content_hash, mtime) VALUES (?, ?, ?, ?)"))
            return db_->rollback();
        for (const auto& [path, contentHash, mtime] : deps)
        {
            if (!depStmt.reset() || !depStmt.bind_int(1, tuId) || !depStmt.bind_string(2, path) ||
                !depStmt.bind_string(3, contentHash) || !depStmt.bind_int(4, mtime))
                return db_->rollback();
            depStmt.step();
        }
    }

    std::vector<DeclRow> decls;
    std::vector<RefRow> refRows;
    std::vector<BaseRow> baseRows;
    std::vector<OverrideRow> overrideRows;
    std::vector<CallRow> callRows;
    std::unordered_set<std::string> seenRefPositions;
    BuildCtx ctx{&projectRoot, &decls, &refRows, &baseRows, &overrideRows, &callRows, &seenRefPositions};
    clang_visitChildren(unitOpt->rootCursor(), buildVisitor, &ctx);

    {
        CDbStatement st;
        // PK теперь (usr,file,start_line) — конфликт означает "та же самая
        // физическая точка увидена повторно" (переобход того же TU), а не
        // "решаем, какая из двух РАЗНЫХ точек для этого usr лучше". Там, где
        // нужна одна "лучшая" строка на usr (class_outline, get_overrides_json,
        // get_location_by_usr_json) — выбор definition-приоритета делается на
        // чтении (ORDER BY is_definition DESC), не на записи.
        if (!st.prepare(db_->handle(),
                "INSERT OR REPLACE INTO decl(usr,kind,name,file,start_line,end_line,decl_line,decl_col,"
                "parent_usr,access,is_definition,is_virtual,is_pure_virtual,is_static,signature) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"))
            return db_->rollback();
        for (auto& r : decls)
        {
            if (!st.reset() || !st.bind_string(1,r.usr) || !st.bind_string(2,r.kind) ||
                !st.bind_string(3,r.name) || !st.bind_string(4,r.file) ||
                !st.bind_int(5,r.startLine) || !st.bind_int(6,r.endLine) ||
                !st.bind_int(7,r.declLine) || !st.bind_int(8,r.declCol) ||
                !st.bind_string(9,r.parentUsr) || !st.bind_string(10,r.access) ||
                !st.bind_int(11,r.isDefinition?1:0) || !st.bind_int(12,r.isVirtual?1:0) ||
                !st.bind_int(13,r.isPureVirtual?1:0) || !st.bind_int(14,r.isStatic?1:0) ||
                !st.bind_string(15,r.signature))
                return db_->rollback();
            st.step();
        }
    }
    {
        CDbStatement st;
        if (!st.prepare(db_->handle(), "INSERT OR IGNORE INTO refs(file,line,col,usr,kind) VALUES (?,?,?,?,?)"))
            return db_->rollback();
        for (auto& r : refRows)
        {
            if (!st.reset() || !st.bind_string(1,r.file) || !st.bind_int(2,r.line) ||
                !st.bind_int(3,r.col) || !st.bind_string(4,r.usr) || !st.bind_string(5,r.kind))
                return db_->rollback();
            st.step();
        }
    }
    {
        CDbStatement st;
        if (!st.prepare(db_->handle(), "INSERT OR REPLACE INTO bases(class_usr,base_usr,base_name,ord) VALUES (?,?,?,?)"))
            return db_->rollback();
        for (auto& r : baseRows)
        {
            if (!st.reset() || !st.bind_string(1,r.classUsr) || !st.bind_string(2,r.baseUsr) ||
                !st.bind_string(3,r.baseName) || !st.bind_int(4,r.ord))
                return db_->rollback();
            st.step();
        }
    }
    {
        CDbStatement st;
        if (!st.prepare(db_->handle(), "INSERT OR IGNORE INTO overrides(method_usr,base_method_usr) VALUES (?,?)"))
            return db_->rollback();
        for (auto& r : overrideRows)
        {
            if (!st.reset() || !st.bind_string(1,r.methodUsr) || !st.bind_string(2,r.baseMethodUsr))
                return db_->rollback();
            st.step();
        }
    }
    {
        CDbStatement st;
        if (!st.prepare(db_->handle(),
                "INSERT OR IGNORE INTO calls(caller_usr,callee_usr,caller_file,caller_line) VALUES (?,?,?,?)"))
            return db_->rollback();
        for (auto& r : callRows)
        {
            if (!st.reset() || !st.bind_string(1,r.callerUsr) || !st.bind_string(2,r.calleeUsr) ||
                !st.bind_string(3,r.callerFile) || !st.bind_int(4,r.callerLine))
                return db_->rollback();
            st.step();
        }
    }

    return db_->commit();
}

bool CTuCache::is_valid(const std::string& file, const std::vector<std::string>& flags,
                        const std::string& /*projectRoot*/)
{
    if (!isOpen_) return false;
    const std::string path = canonicalize(file);
    const std::string flagsHash = hash_flags(flags);
    std::string storedSourceHash, storedFlagsHash;
    int64_t storedMtime = 0, tuId = -1;
    {
        CDbStatement stmt;
        if (!stmt.prepare(db_->handle(), "SELECT id, source_hash, flags_hash, source_mtime FROM tu WHERE path = ?")) return false;
        if (!stmt.bind_string(1, path) || !stmt.step() || !stmt.has_row()) return false;
        tuId = stmt.column_int(0);
        storedSourceHash = stmt.column_string(1);
        storedFlagsHash = stmt.column_string(2);
        storedMtime = stmt.column_int(3);
    }
    if (storedFlagsHash != flagsHash) return false;

    // Fast-path (пункт: не читать содержимое, если не менялось): mtime главного
    // файла и всех зависимостей сверяем с сохранённым. Только при расхождении
    // (или если mtime недоступен) читаем файл и сравниваем хэш. Это убирает
    // полное чтение всех исходников на каждый запрос в steady-state.
    const int64_t curMtime = file_mtime_ns(path);
    if (curMtime == 0 || curMtime != storedMtime)
    {
        const std::string sourceHash = hash_file(path);
        if (sourceHash.empty() || sourceHash != storedSourceHash) return false;
    }

    CDbStatement depStmt;
    if (!depStmt.prepare(db_->handle(), "SELECT path, content_hash, mtime FROM tu_dep WHERE tu_id = ?")) return false;
    if (!depStmt.bind_int(1, tuId)) return false;
    while (depStmt.step() && depStmt.has_row())
    {
        const std::string depPath = depStmt.column_string(0);
        const std::string depHash = depStmt.column_string(1);
        const int64_t depMtime = depStmt.column_int(2);
        const int64_t curDepMtime = file_mtime_ns(depPath);
        if (curDepMtime != 0 && depMtime != 0 && curDepMtime == depMtime) continue; // не менялся
        const std::string currentHash = hash_file(depPath);
        if (currentHash.empty() || currentHash != depHash) return false;
    }
    return true;
}

SIndexStats CTuCache::ensure_project_indexed(const std::string& compileCommandsPath)
{
    SIndexStats result;
    if (!isOpen_) { result.ok=false; result.error = openError_.empty()?"кэш не открыт":openError_; return result; }

    auto idxOpt = CompileCommandsIndex::load(compileCommandsPath);
    if (!idxOpt) { result.ok=false; result.error="не удалось загрузить compile_commands.json: "+compileCommandsPath; return result; }

    result.filesTotal = static_cast<int>(idxOpt->files().size());
    const std::string projectRoot = idxOpt->common_source_root();

    // projectRoot в meta — для будущего (другие методы, читающие его без
    // compile_commands.json под рукой). Пишем идемпотентно, ошибку не считаем фатальной.
    {
        CDbStatement stmt;
        if (stmt.prepare(db_->handle(), "INSERT OR REPLACE INTO meta(key,value) VALUES ('project_root', ?)"))
        { stmt.bind_string(1, projectRoot); stmt.step(); }
    }

    for (const auto& [rawFile, flags] : idxOpt->files())
    {
        const std::string file = canonicalize(rawFile);
        const int64_t tuId = tu_id_for(file);
        const bool valid = (tuId >= 0) && is_valid(file, flags, projectRoot);
        if (valid) { result.fromCache++; continue; }
        if (build_and_store(file, flags, projectRoot)) result.filesScanned++;
    }
    return result;
}

std::string CTuCache::find_enclosing_json(const std::string& file, unsigned line, unsigned col)
{
    (void)col; // диапазоны decl построчные — колонка не нужна для containment
    const std::string path = canonicalize(file);
    json result{{"ok", true}, {"enclosing_method", nullptr}, {"enclosing_class", nullptr}, {"cursor_kind", ""}};

    CDbStatement stmt;
    if (!stmt.prepare(db_->handle(),
            "SELECT usr,kind,name,file,start_line,end_line,decl_line,parent_usr,is_virtual,is_pure_virtual,signature "
            "FROM decl WHERE file=? AND start_line<=? AND end_line>=? ORDER BY (end_line-start_line) ASC"))
    {
        return json{{"ok", false}, {"error", {{"code","db_error"},{"message", db_->last_error()}}}}.dump();
    }
    if (!stmt.bind_string(1, path) || !stmt.bind_int(2, line) || !stmt.bind_int(3, line))
        return json{{"ok", false}, {"error", {{"code","db_error"}}}}.dump();

    bool haveMethod = false, haveClass = false;
    std::string methodUsr, methodParentUsr;
    while (stmt.step() && stmt.has_row())
    {
        std::string usr = stmt.column_string(0), kind = stmt.column_string(1), name = stmt.column_string(2);
        std::string f = stmt.column_string(3);
        int64_t startLine = stmt.column_int(4), endLine = stmt.column_int(5);
        int64_t declLine = stmt.column_int(6);
        std::string parentUsr = stmt.column_string(7);
        bool isVirtual = stmt.column_int(8) != 0, isPureVirtual = stmt.column_int(9) != 0;
        std::string sig = stmt.column_string(10);

        if (!haveMethod && is_fn_like_kind(kind))
        {
            haveMethod = true;
            methodUsr = usr; methodParentUsr = parentUsr;
            // start_line/end_line — точный extent метода в ТУ, где он определён.
            // Агент шлёт ровно тело метода в модель вместо фиксированного окна.
            result["enclosing_method"] = {
                {"usr", usr}, {"signature", sig}, {"file", f}, {"line", declLine},
                {"start_line", startLine}, {"end_line", endLine},
                {"is_virtual", isVirtual}, {"is_pure_virtual", isPureVirtual}
            };
        }
        else if (!haveClass && is_class_like_kind(kind))
        {
            haveClass = true;
            result["enclosing_class"] = {{"usr", usr}, {"name", name}};
            // Раньше cursor_kind для класса не заполнялся -> ground не мог
            // отличить INTERFACE_CLASS_DECL от CC_FOREST (курсор на классе
            // считался "пустым местом"). Пишем kind класса как cursor_kind.
            if (result["cursor_kind"].get<std::string>().empty())
                result["cursor_kind"] = kind; // ClassDecl/StructDecl/ClassTemplate
        }
        else if (!haveMethod && !haveClass && result["cursor_kind"].get<std::string>().empty())
        {
            result["cursor_kind"] = kind; // enum/typedef/var/field на этой же строке
        }
    }

    // Класс метода — через parent_usr декларации метода, если своя строка
    // класса в выборку не попала (метод определён вне тела класса, Class::method в .cpp).
    if (haveMethod && !haveClass && !methodParentUsr.empty())
    {
        CDbStatement cs;
        if (cs.prepare(db_->handle(), "SELECT name FROM decl WHERE usr=?") &&
            cs.bind_string(1, methodParentUsr) && cs.step() && cs.has_row())
        {
            result["enclosing_class"] = {{"usr", methodParentUsr}, {"name", cs.column_string(0)}};
        }
    }

    return result.dump();
}

std::string CTuCache::class_outline_json(const std::string& nameOrUsr, bool byUsr, const std::string& fileHint)
{
    std::string usr, name, file;
    int matchesSeen = 0;

    if (byUsr)
    {
        CDbStatement st;
        if (!st.prepare(db_->handle(), "SELECT usr,name,file FROM decl WHERE usr=? AND kind IN ('ClassDecl','StructDecl','ClassTemplate')"))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        if (st.bind_string(1, nameOrUsr) && st.step() && st.has_row())
        {
            usr = st.column_string(0); name = st.column_string(1); file = st.column_string(2);
            matchesSeen = 1;
        }
    }
    else
    {
        std::string hint = fileHint.empty() ? "" : canonicalize(fileHint);
        CDbStatement st;
        if (!st.prepare(db_->handle(), "SELECT usr,name,file FROM decl WHERE name=? AND kind IN ('ClassDecl','StructDecl','ClassTemplate')"))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        if (!st.bind_string(1, nameOrUsr))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        bool any = false;
        while (st.step() && st.has_row())
        {
            matchesSeen++;
            std::string f = st.column_string(2);
            // Предпочитаем совпадение по файлу-подсказке; иначе берём первое.
            if (!any || (!hint.empty() && f == hint))
            {
                usr = st.column_string(0); name = st.column_string(1); file = f;
                any = true;
                if (!hint.empty() && f == hint) break;
            }
        }
    }

    if (usr.empty())
        return json{{"ok",false},{"error",{{"code","class_not_found"},
            {"message","класс \""+nameOrUsr+"\" не найден в индексе"}}}}.dump();

    json baseClasses = json::array();
    {
        CDbStatement st;
        if (st.prepare(db_->handle(), "SELECT base_name FROM bases WHERE class_usr=? ORDER BY ord") && st.bind_string(1, usr))
            while (st.step() && st.has_row()) baseClasses.push_back(st.column_string(0));
    }

    json publicMethods = json::array();
    {
        // Тот же usr может встретиться дважды (декларация в классе +
        // определение вне тела) — берём определение, когда оно есть.
        CDbStatement st;
        if (st.prepare(db_->handle(),
                "SELECT usr,signature,is_virtual,is_static,is_pure_virtual FROM decl d "
                "WHERE parent_usr=? AND kind IN ('CXXMethod','Constructor','Destructor') AND access='public' "
                "AND is_definition = (SELECT MAX(d2.is_definition) FROM decl d2 WHERE d2.usr = d.usr)")
            && st.bind_string(1, usr))
        {
            while (st.step() && st.has_row())
            {
                publicMethods.push_back({
                    {"usr", st.column_string(0)}, {"signature", st.column_string(1)},
                    {"is_virtual", st.column_int(2)!=0}, {"is_static", st.column_int(3)!=0},
                    {"is_pure_virtual", st.column_int(4)!=0}
                });
            }
        }
    }

    json result{{"ok",true},{"usr",usr},{"name",name},{"file",file},
                {"base_classes",baseClasses},{"public_methods",publicMethods}};
    if (matchesSeen > 1)
        result["warning"] = "ambiguous_name: найдено " + std::to_string(matchesSeen) +
                             " классов с именем \"" + nameOrUsr + "\" в индексе, взят первый подходящий";
    return result.dump();
}

std::string CTuCache::get_overrides_json(const std::string& targetUsr)
{
    // Поднимаемся к базовому, если targetUsr сам override (siblings-кейс —
    // см. комментарий в tu_cache.h). Если targetUsr ничего не переопределяет,
    // считаем его уже базовым как есть.
    std::string baseUsr = targetUsr;
    {
        CDbStatement st;
        if (st.prepare(db_->handle(), "SELECT base_method_usr FROM overrides WHERE method_usr=? LIMIT 1") &&
            st.bind_string(1, targetUsr) && st.step() && st.has_row())
            baseUsr = st.column_string(0);
    }

    // d.usr может иметь несколько строк (декларация+определение) — берём
    // определение, когда оно есть (условие в WHERE через коррелированный
    // подзапрос: "это максимальный is_definition среди всех строк того же usr").
    json overrides = json::array();
    CDbStatement st;
    if (!st.prepare(db_->handle(),
            "SELECT d.usr, d.signature, d.file, d.decl_line, d.is_pure_virtual "
            "FROM overrides o JOIN decl d ON d.usr = o.method_usr "
            "WHERE o.base_method_usr = ? "
            "AND d.is_definition = (SELECT MAX(d2.is_definition) FROM decl d2 WHERE d2.usr = d.usr)"))
        return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
    if (!st.bind_string(1, baseUsr))
        return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
    while (st.step() && st.has_row())
    {
        overrides.push_back({
            {"usr", st.column_string(0)}, {"signature", st.column_string(1)},
            {"file", st.column_string(2)}, {"line", st.column_int(3)},
            {"is_pure_virtual", st.column_int(4)!=0}
        });
    }
    return json{{"ok",true},{"target_usr",targetUsr},{"base_usr",baseUsr},{"overrides",overrides}}.dump();
}

std::string CTuCache::get_location_by_usr_json(const std::string& usr)
{
    if (usr.empty())
        return json{{"ok",false},{"error",{{"code","usr_empty"},{"message","пустой USR"}}}}.dump();
    // usr теперь может встречаться в decl несколькими строками (декларация в
    // .h + определение в .cpp) — для "где этот символ" предпочитаем
    // определение (информативнее: сигнатура там полная, не форвард).
    CDbStatement st;
    if (!st.prepare(db_->handle(),
            "SELECT file, decl_line, decl_col, is_definition FROM decl WHERE usr=? "
            "ORDER BY is_definition DESC LIMIT 1"))
        return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
    if (!st.bind_string(1, usr) || !st.step() || !st.has_row())
        return json{{"ok",false},{"error",{{"code","usr_not_found"},
            {"message","символ с таким USR не найден в индексе (переименован/удалён?)"}}}}.dump();
    json result{{"ok",true},{"file",st.column_string(0)},{"line",st.column_int(1)},
                {"column",st.column_int(2)},{"is_definition",st.column_int(3)!=0}};
    return result.dump();
}

SBlastRefsResult CTuCache::get_refs_for_usr(const std::string& compileCommandsPath, const std::string& targetUSR)
{
    SBlastRefsResult result;
    if (!isOpen_) { result.ok=false; result.error = openError_.empty()?"кэш не открыт":openError_; return result; }

    SIndexStats idx = ensure_project_indexed(compileCommandsPath);
    if (!idx.ok) { result.ok=false; result.error=idx.error; return result; }
    result.filesTotal = idx.filesTotal;
    result.filesScanned = idx.filesScanned;
    result.fromCache = idx.fromCache;

    CDbStatement st;
    if (!st.prepare(db_->handle(), "SELECT file FROM refs WHERE usr = ?"))
        { result.ok=false; result.error=db_->last_error(); return result; }
    if (!st.bind_string(1, targetUSR))
        { result.ok=false; result.error=db_->last_error(); return result; }

    std::set<std::string> distinctFiles;
    int total = 0;
    while (st.step() && st.has_row()) { total++; distinctFiles.insert(st.column_string(0)); }
    result.totalRefs = total;
    result.distinctFilesWithRefs = static_cast<int>(distinctFiles.size());
    return result;
}

std::string CTuCache::get_calls_json(const std::string& usr, bool incoming)
{
    json calls = json::array();
    if (usr.empty())
        return json{{"ok", false}, {"error", {{"code", "usr_empty"}, {"message", "пустой USR"}}}}.dump();

    // incoming: кто вызывает usr -> рёбра по callee_usr, отчёт по caller_usr.
    // outgoing: кого вызывает usr -> рёбра по caller_usr, отчёт по callee_usr.
    // Для каждой стороны через decl берём сигнатуру/файл/строку определения.
    const char* sql = incoming
        ? "SELECT c.caller_usr, c.caller_file, c.caller_line, d.signature, d.file, d.decl_line "
          "FROM calls c LEFT JOIN decl d ON d.usr = c.caller_usr "
          "WHERE c.callee_usr = ? "
          "AND (d.usr IS NULL OR d.is_definition = "
          "     (SELECT MAX(d2.is_definition) FROM decl d2 WHERE d2.usr = d.usr))"
        : "SELECT c.callee_usr, c.caller_file, c.caller_line, d.signature, d.file, d.decl_line "
          "FROM calls c LEFT JOIN decl d ON d.usr = c.callee_usr "
          "WHERE c.caller_usr = ? "
          "AND (d.usr IS NULL OR d.is_definition = "
          "     (SELECT MAX(d2.is_definition) FROM decl d2 WHERE d2.usr = d.usr))";

    CDbStatement st;
    if (!st.prepare(db_->handle(), sql))
        return json{{"ok", false}, {"error", {{"code", "db_error"}, {"message", db_->last_error()}}}}.dump();
    if (!st.bind_string(1, usr))
        return json{{"ok", false}, {"error", {{"code", "db_error"}}}}.dump();

    std::set<std::string> seen; // дедуп по (usr,call_file,call_line)
    while (st.step() && st.has_row())
    {
        std::string otherUsr = st.column_string(0);
        std::string callFile = st.column_string(1);
        int64_t callLine = st.column_int(2);
        std::string key = otherUsr + "|" + callFile + "|" + std::to_string(callLine);
        if (!seen.insert(key).second) continue;
        calls.push_back({
            {"usr", otherUsr},
            {"signature", st.column_string(3)},
            {"file", st.column_string(4)},
            {"line", st.column_int(5)},
            {"call_file", callFile},
            {"call_line", callLine}
        });
    }
    return json{{"ok", true}, {"target_usr", usr},
                {"direction", incoming ? "incoming" : "outgoing"}, {"calls", calls}}.dump();
}

std::string CTuCache::get_file_deps_json(const std::string& file)
{
    const std::string path = canonicalize(file);
    // Разрешаем файл до реального пути в индексе. Если пользователь передал
    // относительный путь/заголовок, ищем по basename среди путей tu и tu_dep.
    std::string resolved = path;
    bool found = false;
    {
        CDbStatement q;
        if (!q.prepare(db_->handle(), "SELECT 1 FROM tu WHERE path = ?") ||
            !q.bind_string(1, path)) return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        if (q.step() && q.has_row()) { found = true; resolved = path; }
    }
    if (!found)
    {
        std::string base = std::filesystem::path(path).filename().string();
        CDbStatement q;
        if (!q.prepare(db_->handle(),
                "SELECT path FROM tu WHERE path LIKE ? "
                "UNION SELECT path FROM tu_dep WHERE path LIKE ? LIMIT 1"))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        std::string like = "%/" + base;
        if (!q.bind_string(1, like) || !q.bind_string(2, like)) return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        if (q.step() && q.has_row()) { resolved = q.column_string(0); found = true; }
    }
    if (!found)
        return json{{"ok", false}, {"error", {{"code", "file_not_indexed"},
            {"message", "файл не найден в индексе: " + file}}}, {"dependents", json::array()},
            {"dependencies", json::array()}}.dump();

    json dependents = json::array();
    {
        // Кто подключает наш файл: tu_dep содержит запись c path == resolved,
        // а tu_id принадлежит TU-владельцу.
        CDbStatement q;
        if (!q.prepare(db_->handle(),
                "SELECT DISTINCT tu.path FROM tu_dep d JOIN tu ON tu.id = d.tu_id WHERE d.path = ?"))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        if (!q.bind_string(1, resolved)) return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        while (q.step() && q.has_row())
        {
            std::string owner = q.column_string(0);
            if (owner != resolved) dependents.push_back(owner);
        }
    }

    json dependencies = json::array();
    {
        // Что подключает наш файл: tu_dep записи его TU.
        CDbStatement q;
        if (!q.prepare(db_->handle(),
                "SELECT d.path FROM tu t JOIN tu_dep d ON d.tu_id = t.id WHERE t.path = ?"))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        if (!q.bind_string(1, resolved)) return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        while (q.step() && q.has_row())
        {
            std::string dep = q.column_string(0);
            if (dep != resolved) dependencies.push_back(dep);
        }
    }
    return json{{"ok", true}, {"file", resolved},
                {"dependents", dependents}, {"dependencies", dependencies}}.dump();
}

std::string CTuCache::get_class_files_json(const std::string& classUsr)
{
    std::set<std::string> files;
    if (!classUsr.empty())
    {
        CDbStatement q;
        if (!q.prepare(db_->handle(),
                "SELECT DISTINCT file FROM decl WHERE parent_usr = ? AND kind IN "
                "('CXXMethod','Constructor','Destructor')"))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        if (!q.bind_string(1, classUsr)) return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
        while (q.step() && q.has_row()) files.insert(q.column_string(0));
    }
    json out = json::array();
    for (const auto& f : files) out.push_back(f);
    return json{{"ok", true}, {"files", out}}.dump();
}

std::string CTuCache::get_refs_locations_json(const std::string& usr, int limit)
{
    json locations = json::array();
    if (usr.empty())
        return json{{"ok", false}, {"error", {{"code", "usr_empty"}}}, {"locations", locations}}.dump();
    CDbStatement q;
    if (!q.prepare(db_->handle(),
            "SELECT file, line, col FROM refs WHERE usr = ? ORDER BY file, line LIMIT ?"))
        return json{{"ok",false},{"error",{{"code","db_error"},{"message",db_->last_error()}}}}.dump();
    if (!q.bind_string(1, usr) || !q.bind_int(2, limit))
        return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
    while (q.step() && q.has_row())
        locations.push_back({{"file", q.column_string(0)},
                             {"line", q.column_int(1)},
                             {"column", q.column_int(2)}});
    return json{{"ok", true}, {"usr", usr}, {"locations", locations}}.dump();
}

std::string CTuCache::get_methods_json(const std::string& scopeFile, const std::string& classUsr)
{
    json methods = json::array();
    const char* sqlFile =
        "SELECT usr, signature, file, start_line, end_line FROM decl "
        "WHERE file = ? AND kind IN ('CXXMethod','Constructor','Destructor','FunctionDecl') "
        "AND is_definition = (SELECT MAX(d2.is_definition) FROM decl d2 WHERE d2.usr = decl.usr)";
    const char* sqlClass =
        "SELECT usr, signature, file, start_line, end_line FROM decl "
        "WHERE parent_usr = ? AND kind IN ('CXXMethod','Constructor','Destructor') "
        "AND is_definition = (SELECT MAX(d2.is_definition) FROM decl d2 WHERE d2.usr = decl.usr)";

    CDbStatement q;
    if (!scopeFile.empty())
    {
        if (!q.prepare(db_->handle(), sqlFile) || !q.bind_string(1, scopeFile))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
    }
    else if (!classUsr.empty())
    {
        if (!q.prepare(db_->handle(), sqlClass) || !q.bind_string(1, classUsr))
            return json{{"ok",false},{"error",{{"code","db_error"}}}}.dump();
    }
    else
    {
        return json{{"ok", false}, {"error", {{"code", "no_scope"},
            {"message", "ни file ни classUsr не заданы"}}}, {"methods", methods}}.dump();
    }
    while (q.step() && q.has_row())
        methods.push_back({{"usr", q.column_string(0)}, {"signature", q.column_string(1)},
                           {"file", q.column_string(2)}, {"start_line", q.column_int(3)},
                           {"end_line", q.column_int(4)}});
    return json{{"ok", true}, {"methods", methods}}.dump();
}

} // namespace cpptool
