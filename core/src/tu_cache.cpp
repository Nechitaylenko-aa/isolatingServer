#include "tu_cache.h"

#include "clang_index.h"
#include "compile_commands.h"
#include "db_manager.h"
#include "hash.h"

#include <clang-c/Index.h>

#include <filesystem>
#include <unordered_map>

namespace cpptool
{

namespace
{

// Версия схемы БД. Меняется при любом изменении состава таблиц или колонок —
// старые файлы кэша после этого непригодны. Хранится в PRAGMA user_version
// (не в таблице meta), чтобы её можно было прочитать до создания таблиц.
constexpr int kSchemaVersion = 1;

// Ключи таблицы meta.
constexpr const char* kMetaLibclangVersion = "libclang_version";

// Типы refs, актуальные для текущей версии схемы. Сейчас пишется только
// refsCount — если в будущем таблице refs понадобится писать другие виды
// вхождений, это делается через эту колонку, без смены схемы.
constexpr const char* kRefKindCount = "count";

// Приводит путь к каноническому виду, чтобы "core/../core/a.h" и "core/a.h"
// не давали двух разных записей в кэше. Работает и для несуществующих путей.
std::string canonicalize(const std::string& path)
{
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec);
    if (ec)
        abs = std::filesystem::path(path);
    std::filesystem::path weak = std::filesystem::weakly_canonical(abs, ec);
    if (ec)
        weak = abs.lexically_normal();
    return weak.string();
}

// Проектный ли это файл: только такие отслеживаются. Сравнение по границе
// каталога, иначе /proj/app-mock считался бы частью /proj/app.
bool is_within_project(const std::string& path, const std::string& root)
{
    if (root.empty())
        return false;
    if (path == root)
        return true;
    if (path.size() <= root.size())
        return false;
    if (path.compare(0, root.size(), root) != 0)
        return false;
    return path[root.size()] == '/';
}

} // namespace

CTuCache::CTuCache(std::string dbPath)
    : db_(std::make_unique<CDbManager>())
    , dbPath_(std::move(dbPath))
{
    if (!db_->open(dbPath_))
    {
        openError_ = db_->last_error();
        return;
    }
    if (!ensure_schema())
    {
        openError_ = db_->last_error().empty() ? "схема кэша не создалась" : db_->last_error();
        return;
    }
    if (!sync_libclang_version())
    {
        openError_ = db_->last_error();
        return;
    }
    isOpen_ = true;
}

CTuCache::~CTuCache() = default;

bool CTuCache::ensure_schema()
{
    // Версия схемы проверяется ДО создания таблиц: старая/неизвестная версия
    // стирается целиком, потому что читать её мог бы только код, знающий
    // прошлый формат. PRAGMA user_version выставляется сразу после DROP —
    // если сравнивать версию заново после пересоздания таблиц, свежая база
    // (user_version по умолчанию 0) всегда читалась бы как «несовпадение» и
    // база пересоздавалась бы на каждом открытии.
    int64_t storedVersion = 0;
    db_->query_int("PRAGMA user_version", storedVersion);
    if (storedVersion != kSchemaVersion)
    {
        db_->execute("DROP TABLE IF EXISTS refs");
        db_->execute("DROP TABLE IF EXISTS tu_dep");
        db_->execute("DROP TABLE IF EXISTS tu");
        db_->execute("DROP TABLE IF EXISTS meta");
        if (!db_->execute("PRAGMA user_version=" + std::to_string(kSchemaVersion)))
            return false;
    }

    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS meta ("
            "  key TEXT PRIMARY KEY,"
            "  value TEXT NOT NULL"
            ")"))
        return false;

    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS tu ("
            "  id INTEGER PRIMARY KEY,"
            "  path TEXT NOT NULL UNIQUE,"
            "  source_hash TEXT NOT NULL,"
            "  flags_hash TEXT NOT NULL DEFAULT '',"
            "  parsed_at INTEGER NOT NULL DEFAULT 0"
            ")"))
        return false;

    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS tu_dep ("
            "  tu_id INTEGER NOT NULL,"
            "  path TEXT NOT NULL,"
            "  content_hash TEXT NOT NULL,"
            "  PRIMARY KEY (tu_id, path)"
            ")"))
        return false;

    if (!db_->execute(
            "CREATE TABLE IF NOT EXISTS refs ("
            "  tu_id INTEGER NOT NULL,"
            "  usr TEXT NOT NULL,"
            "  kind TEXT NOT NULL DEFAULT 'count',"
            "  ref_count INTEGER NOT NULL,"
            "  PRIMARY KEY (tu_id, usr, kind)"
            ")"))
        return false;

    return db_->execute("CREATE INDEX IF NOT EXISTS idx_refs_usr ON refs(usr)");
}

bool CTuCache::sync_libclang_version()
{
    const std::string current = clangVersion();
    std::string stored;
    if (db_->query_string(
            "SELECT value FROM meta WHERE key = '" + std::string(kMetaLibclangVersion) + "'", stored))
    {
        if (stored == current)
            return true;
        // Версия сменилась: USR и AST другой версии несопоставимы с
        // сохранёнными, поэтому кэш стирается целиком, а не по частям.
        if (!wipe_all())
            return false;
    }

    CDbStatement stmt;
    if (!stmt.prepare(db_->handle(), "INSERT OR REPLACE INTO meta(key, value) VALUES (?, ?)"))
        return false;
    if (!stmt.bind_string(1, kMetaLibclangVersion) || !stmt.bind_string(2, current))
        return false;
    stmt.step();
    return true;
}

bool CTuCache::wipe_all()
{
    return db_->execute("DELETE FROM refs") &&
           db_->execute("DELETE FROM tu_dep") &&
           db_->execute("DELETE FROM tu");
}

int64_t CTuCache::tu_id_for(const std::string& file)
{
    {
        CDbStatement stmt;
        if (!stmt.prepare(db_->handle(), "SELECT id FROM tu WHERE path = ?"))
            return -1;
        if (!stmt.bind_string(1, file) || !stmt.step() || !stmt.has_row())
            return -1;
        return stmt.column_int(0);
    }
}

bool CTuCache::clear_tu_contents(int64_t tuId)
{
    CDbStatement refs;
    if (!refs.prepare(db_->handle(), "DELETE FROM refs WHERE tu_id = ?"))
        return false;
    if (!refs.bind_int(1, tuId) || refs.step())
        return false;

    CDbStatement deps;
    if (!deps.prepare(db_->handle(), "DELETE FROM tu_dep WHERE tu_id = ?"))
        return false;
    if (!deps.bind_int(1, tuId) || deps.step())
        return false;

    return true;
}

bool CTuCache::build_and_store(const std::string& file, const std::vector<std::string>& flags,
                               const std::string& projectRoot)
{
    // Хэш до парсинга. Если файл изменится, пока мы его парсим, AST будет
    // построен по одному содержимому, а запись — по другому; поэтому после
    // парсинга хэш берётся ещё раз и сравнивается.
    const std::string hashBefore = hash_file(file);
    if (hashBefore.empty())
        return false; // файл не читается — кэшировать нечего

    auto unitOpt = ParsedUnit::parse(file, flags);
    if (!unitOpt)
        return false; // не распарсился: записи не создаём, следующий вызов попробует снова

    const std::string hashAfter = hash_file(file);
    if (hashAfter != hashBefore)
        return false; // файл менялся во время парсинга — доверять результату нельзя

    // Собираем входы AST: сам TU плюс всё, что препроцессор открыл. Пути вне
    // project root не отслеживаются (осознанное ограничение, см. заголовок).
    std::vector<std::pair<std::string, std::string>> deps;
    std::unordered_map<std::string, bool> seen;
    std::vector<std::string> rawInclusions = unitOpt->inclusions();
    rawInclusions.push_back(file);
    for (const auto& raw : rawInclusions)
    {
        const std::string path = canonicalize(raw);
        if (!is_within_project(path, projectRoot))
            continue;
        if (seen.find(path) != seen.end())
            continue;
        seen[path] = true;
        const std::string contentHash = hash_file(path);
        if (contentHash.empty())
            continue; // файл исчез между парсингом и обходом — зависимость неизвестна
        deps.emplace_back(path, contentHash);
    }

    if (!db_->begin_transaction())
        return false;

    const std::string flagsHash = hash_flags(flags);
    {
        CDbStatement upsert;
        if (!upsert.prepare(db_->handle(),
                "INSERT INTO tu(path, source_hash, flags_hash, parsed_at) VALUES (?, ?, ?, ?) "
                "ON CONFLICT(path) DO UPDATE SET "
                "source_hash = excluded.source_hash, "
                "flags_hash = excluded.flags_hash, "
                "parsed_at = excluded.parsed_at"))
            return db_->rollback();

        if (!upsert.bind_string(1, file) ||
            !upsert.bind_string(2, hashBefore) ||
            !upsert.bind_string(3, flagsHash) ||
            !upsert.bind_int(4, 0))
            return db_->rollback();

        upsert.step(); // SQLITE_DONE: строк не возвращает, has_row() остаётся false
    }

    int64_t tuId = -1;
    {
        CDbStatement idStmt;
        if (!idStmt.prepare(db_->handle(), "SELECT id FROM tu WHERE path = ?") ||
            !idStmt.bind_string(1, file) ||
            !idStmt.step() || !idStmt.has_row())
            return db_->rollback();
        tuId = idStmt.column_int(0);
    }

    if (!clear_tu_contents(tuId))
        return db_->rollback();

    {
        CDbStatement depStmt;
        if (!depStmt.prepare(db_->handle(),
                "INSERT OR REPLACE INTO tu_dep(tu_id, path, content_hash) VALUES (?, ?, ?)"))
            return db_->rollback();
        for (const auto& [path, contentHash] : deps)
        {
            if (!depStmt.reset() ||
                !depStmt.bind_int(1, tuId) ||
                !depStmt.bind_string(2, path) ||
                !depStmt.bind_string(3, contentHash))
                return db_->rollback();
            depStmt.step();
        }
    }

    {
        // Обход AST: считаем ИСПОЛЬЗОВАНИЯ символа — так же, как это делает
        // querySymbolRefs внутри одного TU. Считать собственные USR курсоров
        // (как делал прежний индекс проекта) нельзя: у DeclRefExpr в месте
        // вызова cursorUSR пустой, поэтому в «число ссылок» попадали бы только
        // объявления и определения самого символа, а вызовы — нет.
        struct Ctx
        {
            std::unordered_map<std::string, int>* counts;
            std::unordered_map<std::string, bool>* seenLocations;
        };
        std::unordered_map<std::string, int> counts;
        // Одно место в исходнике даёт несколько вложенных курсоров
        // (CallExpr → UnexposedExpr → DeclRefExpr), которые ссылаются на ту же
        // цель — без дедупликации по позиции один вызов дал бы несколько ссылок.
        std::unordered_map<std::string, bool> seenLocations;
        Ctx ctx{&counts, &seenLocations};
        auto visitor = [](CXCursor c, CXCursor, CXClientData data) -> CXChildVisitResult
        {
            auto* ctx = static_cast<Ctx*>(data);
            CXCursor referenced = clang_getCursorReferenced(c);
            if (clang_Cursor_isNull(referenced) || clang_isInvalid(clang_getCursorKind(referenced)))
                return CXChildVisit_Recurse;
            std::string usr = cursorUSR(referenced);
            if (usr.empty())
                return CXChildVisit_Recurse;

            unsigned line = cursorLine(c);
            unsigned col = cursorColumn(c);
            std::string key = cursorFile(c) + ":" + std::to_string(line) + ":" + std::to_string(col);
            if (!ctx->seenLocations->emplace(key, true).second)
                return CXChildVisit_Recurse;

            (*ctx->counts)[usr]++;
            return CXChildVisit_Recurse;
        };
        clang_visitChildren(unitOpt->rootCursor(), visitor, &ctx);

        CDbStatement refStmt;
        if (!refStmt.prepare(db_->handle(),
                "INSERT OR REPLACE INTO refs(tu_id, usr, kind, ref_count) VALUES (?, ?, ?, ?)"))
            return db_->rollback();
        for (const auto& [usr, count] : counts)
        {
            if (!refStmt.reset() ||
                !refStmt.bind_int(1, tuId) ||
                !refStmt.bind_string(2, usr) ||
                !refStmt.bind_string(3, kRefKindCount) ||
                !refStmt.bind_int(4, count))
                return db_->rollback();
            refStmt.step();
        }
    }

    return db_->commit();
}

bool CTuCache::is_valid(const std::string& file, const std::vector<std::string>& flags,
                        const std::string& /*projectRoot*/)
{
    if (!isOpen_)
        return false;

    const std::string path = canonicalize(file);
    const std::string sourceHash = hash_file(path);
    if (sourceHash.empty())
        return false;

    const std::string flagsHash = hash_flags(flags);
    std::string storedSourceHash;
    std::string storedFlagsHash;
    int64_t tuId = -1;
    {
        CDbStatement stmt;
        if (!stmt.prepare(db_->handle(),
                "SELECT id, source_hash, flags_hash FROM tu WHERE path = ?"))
            return false;
        if (!stmt.bind_string(1, path) || !stmt.step() || !stmt.has_row())
            return false;
        tuId = stmt.column_int(0);
        storedSourceHash = stmt.column_string(1);
        storedFlagsHash = stmt.column_string(2);
    }

    if (storedSourceHash != sourceHash || storedFlagsHash != flagsHash)
        return false;

    // Проверяем каждый вход AST. Достаточно проверить именно записанный набор:
    // любое изменение, способное повлиять на разбор, обязано изменить
    // содержимое либо главного файла, либо уже отслеживаемого заголовка.
    CDbStatement depStmt;
    if (!depStmt.prepare(db_->handle(), "SELECT path, content_hash FROM tu_dep WHERE tu_id = ?"))
        return false;
    if (!depStmt.bind_int(1, tuId))
        return false;

    while (depStmt.step() && depStmt.has_row())
    {
        const std::string depPath = depStmt.column_string(0);
        const std::string depHash = depStmt.column_string(1);
        const std::string currentHash = hash_file(depPath);
        // Пустой хэш — файл исчез: запись невалидна, а не «совпала случайно».
        if (currentHash.empty() || currentHash != depHash)
            return false;
    }
    return true;
}

bool CTuCache::count_ref_for_usr(int64_t tuId, const std::string& usr, int& out, bool& found)
{
    found = false;
    out = 0;
    CDbStatement stmt;
    if (!stmt.prepare(db_->handle(),
            "SELECT ref_count FROM refs WHERE tu_id = ? AND usr = ? AND kind = ?"))
        return false;
    if (!stmt.bind_int(1, tuId) ||
        !stmt.bind_string(2, usr) ||
        !stmt.bind_string(3, kRefKindCount))
        return false;
    if (!stmt.step())
        return true; // строки нет — это не ошибка, символ просто не встречается
    if (!stmt.has_row())
        return true;
    found = true;
    out = static_cast<int>(stmt.column_int(0));
    return true;
}

SBlastRefsResult CTuCache::get_refs_for_usr(const std::string& compileCommandsPath,
                                            const std::string& targetUSR)
{
    SBlastRefsResult result;
    if (!isOpen_)
    {
        result.ok = false;
        result.error = openError_.empty() ? "кэш не открыт" : openError_;
        return result;
    }

    auto idxOpt = CompileCommandsIndex::load(compileCommandsPath);
    if (!idxOpt)
    {
        result.ok = false;
        result.error = "не удалось загрузить compile_commands.json: " + compileCommandsPath;
        return result;
    }

    result.filesTotal = static_cast<int>(idxOpt->files().size());

    // Корень проекта выводим из самой базы, а не берём из каталога
    // compile_commands.json: CMake кладёт его в build/, и тогда заголовки,
    // лежащие рядом с build/, не попали бы в отслеживаемые.
    const std::string projectRoot = idxOpt->common_source_root();

    for (const auto& [rawFile, flags] : idxOpt->files())
    {
        const std::string file = canonicalize(rawFile);
        const int64_t tuId = tu_id_for(file);

        const bool valid = (tuId >= 0) && is_valid(file, flags, projectRoot);
        if (valid)
        {
            result.fromCache++;
        }
        else
        {
            if (!build_and_store(file, flags, projectRoot))
                continue; // TU не записался (не парсится или менялся) — пропускаем
            result.filesScanned++;
        }

        const int64_t storedTuId = valid ? tuId : tu_id_for(file);
        if (storedTuId < 0)
            continue;

        int count = 0;
        bool found = false;
        if (!count_ref_for_usr(storedTuId, targetUSR, count, found))
            continue;
        if (found && count > 0)
        {
            result.totalRefs += count;
            result.distinctFilesWithRefs++;
        }
    }

    return result;
}

} // namespace cpptool
