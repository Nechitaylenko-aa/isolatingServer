/**
 * @file CGitProcess.h
 * @brief Тонкая обёртка над запуском git для теневого снапшот-репозитория.
 */

#ifndef FCPP_AGENT_CGITPROCESS_H
#define FCPP_AGENT_CGITPROCESS_H

#include <filesystem>
#include <string>
#include <vector>

#include "fcpp-agent/IExecutor.h"

namespace fcpp
{

/**
 * @brief Запускает git с явным --git-dir/--work-tree; не проходит через
 * whitelist CExecutor (ADR-08) — это внутренняя бухгалтерия агента, не
 * мутация пользовательского проекта по его команде. См. CProcessRunner.h.
 */
class CGitProcess
{
public:
    CGitProcess() = default;

    /**
     * @brief Выполнить git-подкоманду над теневым репозиторием.
     * @param git_dir Каталог метаданных теневого репозитория (не work-tree).
     * @param work_tree Рабочее дерево — реальный каталог проекта.
     * @param args Аргументы подкоманды (без "git", без --git-dir/--work-tree —
     * они подставляются этим методом).
     * @param timeout_seconds Таймаут выполнения.
     * @return Результат выполнения (см. SExecutionResult).
     */
    SExecutionResult run(const std::filesystem::path& git_dir,
                        const std::filesystem::path& work_tree,
                        const std::vector<std::string>& args,
                        std::uint32_t timeout_seconds = 30) const;
};

} // namespace fcpp

#endif // FCPP_AGENT_CGITPROCESS_H
