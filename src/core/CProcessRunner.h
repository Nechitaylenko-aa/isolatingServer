/**
 * @file CProcessRunner.h
 * @brief Низкоуровневый запуск процесса (fork/execvp, захват stdout/stderr).
 *
 * Выделено из CExecutor: там это было private static run_process(), с которым
 * был жёстко связан whitelist утилит (ADR-08). Запуск процесса и политика
 * допуска — разные по природе вещи: первое нужно и внутренним механизмам
 * агента (например, теневому git-репозиторию для отката правок), которые
 * никогда не проверяются по whitelist пользовательского проекта, потому что
 * не являются его мутацией. CExecutor::execute() по-прежнему единственная
 * точка, где решение "разрешено ли это" применяется к git.
 */

#ifndef FCPP_AGENT_CPROCESSRUNNER_H
#define FCPP_AGENT_CPROCESSRUNNER_H

#include "fcpp-agent/IExecutor.h"

namespace fcpp
{

/**
 * @brief Запустить процесс через fork/execvp напрямую (без участия оболочки)
 * и захватить stdout/stderr с таймаутом.
 *
 * Не проверяет args по whitelist и не вызывает confirm-callback — это
 * исключительно механика запуска. Вызывающий отвечает за то, что args.m_tool
 * безопасен к запуску (доверенная внутренняя утилита — cmake/ninja через
 * CExecutor::execute(), git через CGitProcess).
 *
 * @param args Аргументы запуска (инструмент, аргументы, рабочий каталог,
 * таймаут). args.m_effect не используется этой функцией.
 * @return Результат выполнения; m_allowed=false только при сбое запуска
 * (pipe/fork), не при отказе политики — её тут нет.
 */
SExecutionResult spawn_process(const SCommandArgs& args);

} // namespace fcpp

#endif // FCPP_AGENT_CPROCESSRUNNER_H
