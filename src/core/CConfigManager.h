/**
 * @file CConfigManager.h
 * @brief Менеджмент конфигурации и каталогов агента.
 *
 * Отвечает за создание структуры каталогов ~/.fcpp-agent (bootstrap)
 * и детерминированное чтение/валидацию файла agent.conf (load).
 */

#ifndef FCPP_AGENT_CCONFIGMANAGER_H
#define FCPP_AGENT_CCONFIGMANAGER_H

#include <filesystem>

#include "fcpp-agent/SConfig.h"

namespace fcpp
{

/**
 * @brief Управляет каталогом ~/.fcpp-agent и файлом конфигурации agent.conf.
 *
 * bootstrap() создаёт каталоги db/ и projects/ и дефолтный agent.conf.
 * load() читает и валидирует конфигурацию, заполняя SConfig.
 */
class CConfigManager
{
public:
    /**
     * @brief Конструктор. Определяет базовые пути агента.
     */
    CConfigManager();

    /**
     * @brief Создать каталоги агента и дефолтный agent.conf.
     *
     * Создаёт ~/.fcpp-agent, ~/.fcpp-agent/db, ~/.fcpp-agent/projects.
     * Если agent.conf отсутствует — генерирует дефолтный в стиле rc.conf.
     * @return true при успехе; false при ошибке файловой системы.
     */
    bool bootstrap();

    /**
     * @brief Прочитать и валидировать agent.conf (формат rc.conf).
     * @return true при успехе; false если файл отсутствует или повреждён.
     */
    bool load() const;

    /**
     * @brief Перечитать конфигурацию на лету (для работающего агента).
     * @return true при успехе; false если файл отсутствует или повреждён.
     */
    bool reload_config() const;

    /**
     * @brief Получить загруженную конфигурацию.
     * @return Константная ссылка на структуру конфигурации.
     */
    const SConfig& config() const;

    /**
     * @brief Получить путь каталога агента (~/.fcpp-agent).
     * @return Путь каталога агента.
     */
    std::filesystem::path agent_dir() const;

    /**
     * @brief Получить путь каталога баз данных (db/).
     * @return Путь каталога баз данных.
     */
    std::filesystem::path db_dir() const;

    /**
     * @brief Получить путь каталога проектов (projects/).
     * @return Путь каталога проектов.
     */
    std::filesystem::path projects_dir() const;

    /**
     * @brief Получить путь к файлу конфигурации agent.conf.
     * @return Путь файла конфигурации.
     */
    std::filesystem::path config_path() const;

private:
    std::filesystem::path m_agent_dir;    ///< Каталог ~/.fcpp-agent.
    std::filesystem::path m_config_path;  ///< Путь к agent.conf.
    std::filesystem::path m_db_dir;       ///< Каталог db/.
    std::filesystem::path m_projects_dir; ///< Каталог projects/.
    mutable SConfig m_config;             ///< Загруженная конфигурация (мутабельна для reload).

    /**
     * @brief Определить домашний каталог пользователя кроссплатформенно.
     * @return Путь к домашнему каталогу.
     */
    static std::filesystem::path resolve_home_dir();
};

} // namespace fcpp

#endif // FCPP_AGENT_CCONFIGMANAGER_H
