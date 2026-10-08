/**
 * @file CProjectRegistry.h
 * @brief Реестр проектов поверх таблицы registry_project (ADR-03).
 *
 * Работает с единой базой данных агента через нативный C-API sqlite3.
 * Путь к реестру задаётся детерминированно (из конфигурации) — поиск по
 * дереву не выполняется.
 */

#ifndef FCPP_AGENT_CPROJECTREGISTRY_H
#define FCPP_AGENT_CPROJECTREGISTRY_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fcpp-agent/SProject.h"

struct sqlite3;

namespace fcpp
{

/**
 * @brief CRUD-доступ к таблице registry_project.
 */
class CProjectRegistry
{
public:
    /**
     * @brief Конструктор с детерминированным путём к базе реестра.
     * @param db_path Путь к базе данных реестра.
     */
    explicit CProjectRegistry(std::filesystem::path db_path);

    /**
     * @brief Деструктор: закрывает соединение.
     */
    ~CProjectRegistry();

    /**
     * @brief Копирование запрещено (владение соединением sqlite3).
     */
    CProjectRegistry(const CProjectRegistry&) = delete;

    /**
     * @brief Присваивание запрещено.
     * @return Ссылка на текущий объект.
     */
    CProjectRegistry& operator=(const CProjectRegistry&) = delete;

    /**
     * @brief Открыть соединение с базой реестра.
     * @return true при успехе; false при ошибке открытия.
     */
    bool open_db();

    /**
     * @brief Закрыть соединение с базой реестра.
     * @return true при успехе; false при ошибке закрытия.
     */
    bool close_db();

    /**
     * @brief Вставить или обновить запись проекта.
     * @param project Данные проекта.
     * @return true при успехе; false при ошибке.
     */
    bool upsert(const SProject& project);

    /**
     * @brief Найти проект по каноническому пути.
     * @param canonical_path Канонический путь CMake-корня.
     * @return Проект либо пустое значение.
     */
    std::optional<SProject> find_by_path(const std::filesystem::path& canonical_path) const;

    /**
     * @brief Удалить проект по идентификатору.
     * @param project_id Идентификатор проекта.
     * @return true при успехе; false при ошибке.
     */
    bool erase(const std::string& project_id);

    /**
     * @brief Получить все зарегистрированные проекты.
     * @return Список проектов, упорядоченный по времени последнего открытия.
     */
    std::vector<SProject> list() const;

private:
    sqlite3* m_db;                    ///< Открытое соединение SQLite.
    std::filesystem::path m_db_path;  ///< Путь к базе реестра.

    /**
     * @brief Выполнить SQL-выражение без результата.
     * @param sql Текст SQL.
     * @return true при успехе.
     */
    bool exec_sql(const char* sql) const;
};

} // namespace fcpp

#endif // FCPP_AGENT_CPROJECTREGISTRY_H
