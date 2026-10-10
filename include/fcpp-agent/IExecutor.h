/**
 * @file IExecutor.h
 * @brief Интерфейс безопасного выполнения команд (ADR-08).
 *
 * Реализует закрытую модель безопасности: всё, что явно не разрешено,
 * запрещено. Команды исполняются только как argv (без shell), мутации
 * требуют подтверждения пользователя.
 */

#ifndef FCPP_AGENT_IEXECUTOR_H
#define FCPP_AGENT_IEXECUTOR_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fcpp
{

/**
 * @brief Уровень воздействия выполняемой команды (не путать с осью Effect Intent).
 *
 * Порядок возрастания опасности: READ_ONLY < WRITE_SOURCE < EXECUTE < WRITE_BUILD.
 */
enum EExecEffect
{
    EFFECT_READ_ONLY,    ///< Только чтение.
    EFFECT_WRITE_SOURCE, ///< Изменение исходников.
    EFFECT_EXECUTE,      ///< Выполнение команд.
    EFFECT_WRITE_BUILD   ///< Запись в build-директорию.
};

/**
 * @brief Решение политики по запросу выполнения.
 */
enum EDecision
{
    DECISION_ALLOWED,               ///< Разрешено whitelist-политикой.
    DECISION_DENIED,                ///< Запрещено политикой.
    DECISION_CONFIRMATION_REQUIRED  ///< Требуется подтверждение пользователя (Y/N).
};

/**
 * @brief Аргументы запуска внешней утилиты.
 *
 * Утилита и аргументы передаются раздельно: shell не используется.
 * Базовое имя m_tool проверяется по whitelist, аргументы — на инъекции.
 */
struct SCommandArgs
{
    std::string m_tool;                  ///< Путь к утилите (или имя в PATH).
    std::vector<std::string> m_arguments;///< Аргументы командной строки.
    std::filesystem::path m_working_dir; ///< Рабочая директория.
    std::uint32_t m_timeout_seconds;     ///< Таймаут выполнения (0 — без таймаута).
    EExecEffect m_effect;                ///< Уровень воздействия операции.
};

/**
 * @brief Результат выполнения команды.
 */
struct SExecutionResult
{
    bool m_allowed;       ///< Было ли выполнение разрешено политикой.
    int m_exit_code;      ///< Код возврата процесса (-1 при ошибке запуска).
    std::string m_stdout; ///< Захваченный stdout.
    std::string m_stderr; ///< Захваченный stderr.
};

/**
 * @brief Интерфейс безопасного выполнения команд.
 *
 * Проверяет запрос по whitelist и защите от инъекций (ADR-08) до запуска
 * процесса. Любая мутация (WRITE_SOURCE) требует явного подтверждения.
 */
class IExecutor
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~IExecutor() = default;

    /**
     * @brief Проверить запрос по политике безопасности без выполнения.
     * @param args Аргументы запуска утилиты.
     * @return Решение политики.
     */
    virtual EDecision check(const SCommandArgs& args) const = 0;

    /**
     * @brief Выполнить запрос с учётом политики и подтверждения.
     * @param args Аргументы запуска утилиты.
     * @param confirm Callback подтверждения (Y/N); вызывается при необходимости.
     * @return Результат выполнения.
     */
    virtual SExecutionResult execute(const SCommandArgs& args,
                                     const std::function<bool(const SCommandArgs&)>& confirm) = 0;
};

} // namespace fcpp

#endif // FCPP_AGENT_IEXECUTOR_H
