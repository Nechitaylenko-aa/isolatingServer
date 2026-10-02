#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cpptool
{

class CDbManager;

/**
 * @brief Результат агрегата ссылок на символ по всему проекту.
 *
 * Состав полей намеренно совпадает с прежним BlastRefsResult, чтобы логика
 * адаптивных порогов в blast_radius.cpp не менялась вместе с источником данных.
 */
struct SBlastRefsResult
{
    int totalRefs{0};              ///< сколько раз USR встретился во всех .cpp
    int distinctFilesWithRefs{0};  ///< в скольких .cpp он встретился хотя бы раз
    int filesScanned{0};           ///< сколько TU перепарсено в этом вызове
    int filesTotal{0};             ///< знаменатель для относительного порога
    int fromCache{0};              ///< сколько TU взято из кэша без парсинга
    bool ok{true};                 ///< false — ошибка на уровне проекта, см. error
    std::string error;             ///< причина при ok == false
};

/**
 * @brief Постоянный кэш разбора translation unit'ов на SQLite.
 *
 * Одна запись на .cpp из compile_commands.json. Валидность записи определяется
 * набором входов, от которых зависит AST: хэш содержимого главного файла, хэш
 * флагов и хэши содержимого всех файлов, которые препроцессор реально открыл в
 * этом TU. Версия схемы и версия libclang проверяются при открытии: при
 * расхождении кэш очищается целиком, потому что строковый формат USR задаёт
 * конкретная версия clang, и старые ключи после смены версии перестают
 * находиться — отказ был бы тихим (refs = 0 вместо ошибки).
 *
 * Ограничение, принятое осознанно: отслеживаются только файлы внутри project
 * root. Правка заголовка вне root (системного или вендорного) кэш не
 * инвалидирует. Не отслеживаются и пробы вида __has_include: если новый файл
 * появляется по пробуемому пути, но ранее не открывался, запись остаётся
 * валидной.
 *
 * Project root не задаётся вызывающим кодом, а выводится из самой базы: это
 * общий каталог-предок всех .cpp. Брать в этом качестве каталог
 * compile_commands.json нельзя — CMake кладёт его в build/, а исходники лежат
 * рядом с build/, и тогда ни один заголовок проекта не попал бы в
 * отслеживаемые.
 */
class CTuCache
{
public:
    /**
     * @brief Открывает кэш в указанном файле БД.
     *
     * @param dbPath путь к файлу SQLite
     */
    explicit CTuCache(std::string dbPath);

    ~CTuCache();

    CTuCache(const CTuCache&) = delete;
    CTuCache& operator=(const CTuCache&) = delete;

    /**
     * @brief true, если БД открыта и схема готова к работе.
     */
    bool opened() const { return isOpen_; }

    /**
     * @brief Причина, по которой кэш не открылся.
     */
    const std::string& open_error() const { return openError_; }

    /**
     * @brief Суммирует вхождения USR по всем TU проекта.
     *
     * Перепарсивает только грязные TU, остальные читает из БД. Заголовки
     * проекта, попавшие в разбор, отслеживаются: их правка делает TU грязным.
     *
     * @param compileCommandsPath путь к compile_commands.json
     * @param targetUSR искомый USR
     * @return агрегат; ok == false только при ошибке уровня проекта
     */
    SBlastRefsResult get_refs_for_usr(const std::string& compileCommandsPath,
                                      const std::string& targetUSR);

    /**
     * @brief true, если запись для файла валидна и её можно использовать как есть.
     *
     * @param file путь к .cpp
     * @param flags флаги компиляции для этого файла
     * @param projectRoot корень проекта; пути вне него не отслеживаются
     */
    bool is_valid(const std::string& file, const std::vector<std::string>& flags,
                  const std::string& projectRoot);

private:
    friend class CTuCacheTestAccess;

    bool ensure_schema();
    bool sync_libclang_version();
    bool wipe_all();

    /** @brief id записи для файла, либо -1 если записи нет. */
    int64_t tu_id_for(const std::string& file);

    /**
     * @brief Распарсивает файл и записывает результат в БД.
     *
     * Возвращает false, если файл изменился между парсингом и хэшированием:
     * в этом случае запись остаётся невалидной, чтобы не оставить валидную
     * с виду запись с AST от другого содержимого.
     *
     * @param file путь к .cpp
     * @param flags флаги компиляции
     * @param projectRoot корень проекта; зависит от набора файлов в базе
     */
    bool build_and_store(const std::string& file, const std::vector<std::string>& flags,
                         const std::string& projectRoot);

    /** @brief Удаляет refs и tu_dep записи — перед перезаписью. */
    bool clear_tu_contents(int64_t tuId);

    bool count_ref_for_usr(int64_t tuId, const std::string& usr, int& out, bool& found);

    std::unique_ptr<CDbManager> db_;
    std::string dbPath_;
    bool isOpen_{false};
    std::string openError_;
};

} // namespace cpptool
