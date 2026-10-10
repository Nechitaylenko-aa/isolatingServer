/**
 * @file SConfig.h
 * @brief Структура конфигурации fcpp-agent.
 */

#ifndef FCPP_AGENT_SCONFIG_H
#define FCPP_AGENT_SCONFIG_H

#include <filesystem>
#include <string>

namespace fcpp
{

/**
 * @brief Тип удалённого провайдера (ADR-07/§9).
 *
 * Преобразование в текст — статическим массивом в .cpp (Секция 3).
 */
enum class EModelRemoteType
{
    OpenAICompat, ///< OpenAI-совместимый API.
    Claude,       ///< Anthropic Claude.
    Count         ///< Маркер количества для инициализации массива.
};

/**
 * @brief Параметры одной роли модели (§6/§7).
 */
struct SModelConfig
{
    std::string m_url;    ///< Endpoint (пусто — локальный провайдер/дефолт).
    std::string m_name;   ///< Имя модели.
    std::string m_type;   ///< Тип провайдера (для remote).
    std::string m_key;    ///< API-ключ (секрет, не логируется, §61).
};

/**
 * @brief Параметры конфигурации агента (agent.conf).
 *
 * Пустые пути к инструментам означают поиск соответствующего бинаря в PATH.
 * Значения задаются при загрузке конфигурации через CConfigManager.
 */
struct SConfig
{
    std::filesystem::path m_clang_path;      ///< Путь к clang/libclang (пусто — поиск в PATH).
    std::filesystem::path m_cmake_path;      ///< Путь к cmake (пусто — поиск в PATH).
    std::filesystem::path m_ninja_path;      ///< Путь к ninja (пусто — поиск в PATH).
    std::filesystem::path m_clang_tidy_path; ///< Путь к clang-tidy (пусто — поиск в PATH).
    bool m_enable_tidy = true;               ///< Включён ли статический анализ (clang-tidy).
    std::filesystem::path m_registry_path;   ///< Путь к глобальному реестру проектов.
    std::filesystem::path m_compile_commands_path; ///< Путь к compile_commands.json (пусто — стандарт).
    bool m_allow_read = true;                ///< Разрешает чтение проекта и индекса (§47).
    bool m_allow_write = true;               ///< Разрешает изменение исходников (§47).
    bool m_allow_exec = true;                ///< Разрешает запуск build/tests/tidy (§47).
    bool m_allow_subtasks = false;           ///< Разрешает дочерние задачи (§48, v2.0 не обязателен).

    bool m_react_loop = true;                ///< Управление loop'ом моделью (ReAct, ADR-23). Дефолт YES.
    std::string m_loop_model = "main";       ///< Какая роль ведёт loop: main|light|remote (`same_as_main`).

    std::filesystem::path m_db_dir;          ///< Каталог баз данных агента (вне проектов).
    std::string m_default_build = "build";   ///< Имя build-каталога по умолчанию (§2.3).

    SModelConfig m_model_main;               ///< Роль `main` (§6.2).
    SModelConfig m_model_light;              ///< Роль `lite` (§6.1); пусто/same_as_main — как main.
    SModelConfig m_model_remote;             ///< Роль `remote` (§6.3, задел).
    EModelRemoteType m_remote_type = EModelRemoteType::OpenAICompat; ///< Тип remote-провайдера.
};

/**
 * @brief Получить машинное имя типа remote-провайдера.
 * @param type Тип.
 * @return Стабильное текстовое имя.
 */
const char* remote_type_name(EModelRemoteType type);

/**
 * @brief Разобрать тип remote-провайдера из строки конфига.
 * @param name Имя ("openai-compat" / "claude").
 * @return Тип (OpenAICompat по умолчанию/при неизвестном значении).
 */
EModelRemoteType remote_type_from_name(const std::string& name);

} // namespace fcpp

#endif // FCPP_AGENT_SCONFIG_H
