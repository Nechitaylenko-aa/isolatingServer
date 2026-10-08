/**
 * @file CExecutor.h
 * @brief Безопасный исполнитель внешних утилит (ADR-08).
 *
 * Реализует IExecutor: жёсткий whitelist утилит, защита от shell-инъекций
 * и запуск процессов напрямую через fork/execvp без участия оболочки.
 */

#ifndef FCPP_AGENT_CEXECUTOR_H
#define FCPP_AGENT_CEXECUTOR_H

#include <functional>
#include <string>

#include "fcpp-agent/IExecutor.h"

namespace fcpp
{

/**
 * @brief Исполнитель внешних утилит под закрытой моделью безопасности.
 *
 * Разрешены только утилиты из внутреннего whitelist (cmake, ninja, ctest,
 * clang-tidy). Аргументы проверяются на запрещённые shell-метасимволы.
 * Запуск выполняется через fork + execvp с раздельным захватом stdout/stderr.
 */
class CExecutor final : public IExecutor
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CExecutor() = default;

    /**
     * @brief Виртуальный деструктор.
     */
    ~CExecutor() override = default;

    /**
     * @brief Проверить запрос по whitelist и защите от инъекций.
     * @param args Аргументы запуска утилиты.
     * @return ALLOWED, DENIED или CONFIRMATION_REQUIRED.
     */
    EDecision check(const SCommandArgs& args) const override;

    /**
     * @brief Выполнить команду с учётом политики и подтверждения.
     * @param args Аргументы запуска утилиты.
     * @param confirm Callback подтверждения (Y/N).
     * @return Результат выполнения.
     */
    SExecutionResult execute(const SCommandArgs& args,
                             const std::function<bool(const SCommandArgs&)>& confirm) override;

private:
    /**
     * @brief Проверить базовое имя утилиты по whitelist.
     * @param tool Путь или имя утилиты.
     * @return true если утилита разрешена.
     */
    static bool is_tool_allowed(const std::string& tool);

    /**
     * @brief Проверить строку на наличие запрещённых shell-метасимволов.
     * @param text Проверяемая строка.
     * @return true если найден хотя бы один запрещённый символ.
     */
    static bool has_forbidden_metacharacters(const std::string& text);

    /**
     * @brief Запустить процесс через fork/execvp с захватом stdout/stderr.
     * @param args Аргументы запуска утилиты.
     * @return Результат выполнения.
     */
    static SExecutionResult run_process(const SCommandArgs& args);
};

} // namespace fcpp

#endif // FCPP_AGENT_CEXECUTOR_H
