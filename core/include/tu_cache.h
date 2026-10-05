#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cpptool
{

class CDbManager;

/**
 * @brief Каталог/файл кэша по умолчанию для compile_commands.json.
 *
 * Если задан явный файл через set_cache_file() — возвращается он (путь к БД
 * приходит из конфига агента: <db_dir>/<hash(project_root)>.db). Иначе —
 * историческое поведение: рядом с compile_commands.json в .cpp-tool-cache.
 */
std::string defaultCachePathFor(const std::string& compileCommandsPath);

/**
 * @brief Задать единый для процесса файл кэша (override из конфига cf-agent).
 * Пустая строка сбрасывает override к поведению по умолчанию.
 */
void set_cache_file(const std::string& dbFilePath);

/**
 * @brief Результат агрегата ссылок на символ по всему проекту.
 * Состав полей совпадает с прежней версией — adaptive-threshold логика в
 * blast_radius.cpp не меняется вместе с источником данных.
 */
struct SBlastRefsResult
{
    int totalRefs{0};
    int distinctFilesWithRefs{0};
    int filesScanned{0};
    int filesTotal{0};
    int fromCache{0};
    bool ok{true};
    std::string error;
};

/** @brief Результат ensure_project_indexed — сколько TU пересобрано/взято из кэша. */
struct SIndexStats
{
    int filesTotal{0};
    int filesScanned{0};  ///< пересобрано (невалидны были)
    int fromCache{0};     ///< уже валидны, не трогались
    bool ok{true};
    std::string error;
};

/**
 * @brief Постоянный индекс проекта на SQLite — единственный источник ответа
 * на запросы (locate_symbol, class_outline, symbol_refs, overrides, bases).
 *
 * Принцип v2 (в отличие от v1, где кэш был только ускорением поверх
 * повторного парсинга): clang вызывается РОВНО в одном месте —
 * build_and_store, когда запись для TU невалидна. Все query-методы — чистые
 * SELECT, ParsedUnit::parse внутри них не вызывается никогда.
 *
 * decl хранится ГЛОБАЛЬНО по usr (не по tu_id): декларация метода в
 * заголовке, видимом из N TU, существует в природе одна, и физически
 * привязана к файлу, где она написана, а не к тому, кто её #include-ит.
 * Так же refs — первичный ключ (file,line,col,usr), а не tu_id: ссылка,
 * физически написанная внутри заголовка (inline-метод), не размножается на
 * каждый включающий TU — INSERT OR IGNORE делает повторную вставку с тем же
 * физическим местом no-op'ом.
 *
 * Отслеживаются только файлы внутри project root (как и раньше) — декларации
 * и ссылки на символы вне project root (std::/системные/вендорные) не
 * попадают в decl/refs вовсе: без этого фильтра один обычный .cpp даёт
 * десятки тысяч нерелевантных строк (измерено на реальном проекте).
 */
class CTuCache
{
public:
    explicit CTuCache(std::string dbPath);
    ~CTuCache();

    CTuCache(const CTuCache&) = delete;
    CTuCache& operator=(const CTuCache&) = delete;

    bool opened() const { return isOpen_; }
    const std::string& open_error() const { return openError_; }

    /**
     * @brief Проходит все TU из compile_commands.json, пересобирает невалидные.
     *
     * Единая точка входа перед любым query-методом ниже (они её тоже
     * вызывают сами, так что вызывать отдельно нужно, только если важна
     * статистика пересборки сама по себе).
     */
    SIndexStats ensure_project_indexed(const std::string& compileCommandsPath);

    /**
     * @brief Метод/класс/прочая декларация, чей extent накрывает (file,line,col).
     *
     * Чистый SELECT по decl(file,start_line,end_line) — самый узкий
     * охватывающий диапазон побеждает (метод внутри класса, а не класс).
     * file может быть заголовком без собственной записи в
     * compile_commands.json: decl заполняется из ЛЮБОГО TU, который этот
     * заголовок #include-ит, так что работает, если индекс уже строился
     * хоть раз (см. ensure_project_indexed).
     *
     * Возвращает JSON строкой (чтобы не тянуть json.hpp в заголовок) —
     * {ok, cursor_kind, enclosing_method:{...}|null, enclosing_class:{...}|null}
     */
    std::string find_enclosing_json(const std::string& file, unsigned line, unsigned col);

    /** @brief По имени класса (среди всех decl) или по usr. JSON строкой. */
    std::string class_outline_json(const std::string& nameOrUsr, bool byUsr,
                                    const std::string& fileHint);

    /** @brief Суммирует вхождения usr по всем TU проекта (из таблицы refs). */
    SBlastRefsResult get_refs_for_usr(const std::string& compileCommandsPath,
                                      const std::string& targetUSR);

    /**
     * @brief Общий экземпляр кэша на процесс (реестр по пути БД).
     *
     * Без этого каждый query создавал свой CTuCache и заново прогонял
     * ensure_project_indexed — полный скан всех TU на КАЖДЫЙ под-запрос одного
     * прогона агента. Реестр держит по одному живому кэшу на путь; повторный
     * вызов с тем же путём возвращает тот же объект (без повторного открытия).
     * Потокобезопасно: реестр под std::mutex.
     */
    static std::shared_ptr<CTuCache> shared(const std::string& dbPath);

    /**
     * @brief Методы, переопределяющие БАЗОВЫЙ метод для targetUsr.
     *
     * targetUsr не обязан САМ быть базовым: если это override (siblings-кейс —
     * курсор стоит на CMySQLModel::execSQL_read, а нужны все реализации, т.е.
     * и CPostgresModel::execSQL_read тоже), метод сам поднимается к базовому
     * через overrides(method_usr=targetUsr) перед поиском остальных override'ов.
     * Раньше это поднятие делал agent/ground_service.cpp повторным парсингом
     * TU — теперь это два SELECT по уже заполненной таблице.
     */
    std::string get_overrides_json(const std::string& targetUsr);

    /** @brief file/decl_line/decl_col декларации по usr. Якорь курсора переживает сдвиги строк. */
    std::string get_location_by_usr_json(const std::string& usr);

    /**
     * @brief Рёбра графа вызовов вокруг usr.
     *
     * incoming=true  — кто ВЫЗЫВАЕТ usr (CALL_CHAIN: "кто вызывает этот метод");
     * incoming=false — кого ВЫЗЫВАЕТ usr (CALL_GRAPH_SUBTREE: "что вызывает этот метод").
     * Рёбра (caller_usr -> callee_usr) собраны из CallExpr/DeclRefExpr туда, где
     * clang_getCursorReferenced даёт метод/функцию. Для каждой стороны через
     * decl достаётся сигнатура/файл/строка определения (ORDER BY is_definition).
     * Возвращает JSON строкой: {ok, target_usr, direction, calls:[{usr,signature,file,line,call_file,call_line}]}.
     */
    std::string get_calls_json(const std::string& usr, bool incoming);

    /**
     * @brief Зависимости файла-уровня для DEPENDENCY_MAP.
     *
     * Ищет файл в списке TU (по basename, если полного пути нет — результат
     * в файле может быть заголовком/передан относительным путём) и возвращает:
     *   - dependents — файлы TU, которые этот файл ПОДКЛЮЧАЮТ (их tu_dep
     *     содержит наш путь);
     *   - dependencies — файлы, которые ПОДКЛЮЧАЕТ этот файл (его собственные
     *     tu_dep-записи).
     * Если индекс ещё пуст — вызывающий обязан сперва сделать
     * ensure_project_indexed (как и для остальных query).
     */
    std::string get_file_deps_json(const std::string& file);

    /**
     * @brief Файлы/сигнатуры методов класса (parent_usr) — для TODO_SCAN/DOC_CHECK
     * в пределах класса, а не всего проекта. JSON строкой: {ok, files:[...]}.
     */
    std::string get_class_files_json(const std::string& classUsr);

    /** @brief Все места использования usr (file,line) из refs — для TYPE_USAGE. */
    std::string get_refs_locations_json(const std::string& usr, int limit = 500);

    /**
     * @brief Методы области (файл ИЛИ класс) с extent — для COMPLEXITY_HOTSPOT
     * и DUPLICATE_FIND. JSON: {ok, methods:[{usr,signature,file,start_line,end_line}]}.
     */
    std::string get_methods_json(const std::string& scopeFile, const std::string& classUsr);

    /** @brief true, если запись для файла валидна и её можно использовать как есть. */
    bool is_valid(const std::string& file, const std::vector<std::string>& flags,
                  const std::string& projectRoot);

private:
    friend class CTuCacheTestAccess;

    bool ensure_schema();
    bool sync_libclang_version();
    bool wipe_all();

    int64_t tu_id_for(const std::string& file);
    bool build_and_store(const std::string& file, const std::vector<std::string>& flags,
                         const std::string& projectRoot);

    /**
     * @brief Стирает факты, засеянные конкретным TU, перед повторной записью.
     *
     * decl/refs чистятся по ФАЙЛАМ нового набора зависимостей (сам TU + все
     * project-root заголовки, которые он #include-ит). Причина: PK decl =
     * (usr,file,start_line), поэтому сдвиг строк меняет start_line -> INSERT
     * OR REPLACE не затирает старую строку, а добавляет новую, и в таблице
     * остаётся призрак старых координат (find_enclosing/locate по нему
     * возвращают устаревший метод). bases/overrides чистятся по usr
     * деклараций из этих файлов — их ключ (class_usr/method_usr), а не файл.
     *
     * Чистим только файлы НОВОГО набора: если TU перестал #include-ить
     * заголовок, общий с другим TU, удалять его строки нельзя — они всё ещё
     * валидны для второго TU, который не пересобирается.
     */
    bool clear_tu_contents(int64_t tuId, const std::vector<std::string>& depFiles);

    std::unique_ptr<CDbManager> db_;
    std::string dbPath_;
    bool isOpen_{false};
    std::string openError_;
};

} // namespace cpptool
