/**
 * @file CProjectManager.h
 * @brief Управление проектами агента (ADR-03).
 *
 * Открывает проект строго по каноническому пути CMake-корня, регистрирует его
 * в реестре и создаёт/мигрирует per-project базу индекса index.db. Поиск по
 * дереву файловой системы не выполняется (Zero-Search).
 */

#ifndef FCPP_AGENT_CPROJECTMANAGER_H
#define FCPP_AGENT_CPROJECTMANAGER_H

#include <filesystem>
#include <system_error>
#include <string>
#include <vector>

#include "core/CConfigManager.h"
#include "core/CProjectRegistry.h"
#include "fcpp-agent/IIndex.h"
#include "fcpp-agent/SProject.h"

namespace fcpp
{

/**
 * @brief Менеджер жизненного цикла проектов.
 */
class CProjectManager
{
public:
    /**
     * @brief Конструктор.
     * @param config_manager Менеджер конфигурации (должен пережить менеджер).
     */
    explicit CProjectManager(const CConfigManager& config_manager);

    /**
     * @brief Деструктор.
     */
    ~CProjectManager();

    /**
     * @brief Копирование запрещено.
     */
    CProjectManager(const CProjectManager&) = delete;

    /**
     * @brief Присваивание запрещено.
     * @return Ссылка на текущий объект.
     */
    CProjectManager& operator=(const CProjectManager&) = delete;

    /**
     * @brief Открыть проект по каноническому пути.
     *
     * Проверяет наличие CMakeLists.txt в корне (иначе FAIL), вычисляет
     * project_id и db_path, регистрирует проект, применяет миграции схемы и
     * открывает базу индекса. Идемпотентно: повторный open того же пути даёт
     * тот же project_id.
     * @param path Путь к CMake-корню проекта.
     * @return true при успехе; false при ошибке.
     */
    bool open(const std::filesystem::path& path, std::string* out_failure_stage = nullptr);

    /**
     * @brief Задать явный путь к compile_commands.json (флаг CLI).
     *
     * Имеет наивысший приоритет среди источников (ADR-04).
     * @param path Путь к файлу compile_commands.json.
     */
    void set_compile_commands_path(const std::filesystem::path& path);

    /**
     * @brief Закрыть текущий проект (запись реестра сохраняется).
     */
    void close();

    /**
     * @brief Проверить наличие открытого проекта.
     * @return true если проект открыт.
     */
    bool has_current() const;

    /**
     * @brief Получить контекст текущего проекта.
     * @return Константная ссылка на контекст.
     */
    const SProjectContext& current() const;

    /** @brief Получить паспорт текущего проекта для явной установки/чтения. */
    CProjectProfile& profile();
    const CProjectProfile& profile() const;

    /**
     * @brief Сохранить паспорт текущего проекта в его SQLite-БД.
     * @return true при успешной записи; false при отсутствии проекта или ошибке БД.
     */
    bool set_profile(std::string original_text);

    /**
     * @brief Получить список зарегистрированных проектов.
     * @return Список проектов из реестра.
     */
    std::vector<SProject> list() const;

    /**
     * @brief Обновить индекс текущего проекта в активной generation.
     * @return true при успехе; false при ошибке.
     */
    bool refresh_index();

    /**
     * @brief Полностью перестроить индекс текущего проекта в новой generation.
     * @return true при успехе; false при ошибке.
     */
    bool rebuild_index();

    /**
     * @brief Сохранить текущий проект в сессию (~/.fcpp-agent/session).
     * @return true при успехе.
     */
    bool save_session() const;

    /**
     * @brief Восстановить текущий проект из сессии, если он есть.
     * @return true при успехе (или сессии нет).
     */
    bool restore_session();

    /**
     * @brief Лёгкое восстановление сессии: идентификация проекта + статус из БД.
     *
     * Не выполняет open/warm/refresh — только вычисляет пути и читает состояние
     * индекса (read-only). Для команд `status`/`index.status`, где тяжёлая
     * индексация недопустима.
     * @return true если проект идентифицирован.
     */
    bool restore_session_light();

    /**
     * @brief Очистить сессию (текущий проект).
     */
    void clear_session() const;

    /**
     * @brief Найти сущности текущего проекта по имени (через индекс).
     * @param name Имя сущности.
     * @return Совпавшие сущности (пусто, если проект не открыт).
     */
    std::vector<SSymbol> find_symbols(const std::string& name) const;

private:
    /**
     * @brief Найти compile_commands.json по разрешённым источникам (ADR-04).
     * @param project_root Канонический корень проекта.
     * @param out_error Выходной код ошибки (устанавливается при FAIL).
     * @return Путь к файлу compile_commands.json.
     */
    std::filesystem::path locate_compile_commands(const std::filesystem::path& project_root,
                                                  std::error_code& out_error) const;

    const CConfigManager& m_config_manager;      ///< Источник путей и конфигурации.
    CProjectRegistry m_registry;                 ///< Реестр проектов.
    std::filesystem::path m_compile_commands_path;///< Явный путь compile_commands.json (CLI).
    bool m_has_current;                          ///< Признак открытого проекта.
    SProjectContext m_current;                   ///< Контекст текущего проекта.
};

} // namespace fcpp

#endif // FCPP_AGENT_CPROJECTMANAGER_H
