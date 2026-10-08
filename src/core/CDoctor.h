/**
 * @file CDoctor.h
 * @brief Preflight-проверка инструментов окружения (ADR-06).
 *
 * Проверяет наличие критичных (cmake, libclang) и важных (ninja, clang-tidy,
 * ctest) инструментов, печатает таблицу и сообщает, можно ли работать.
 */

#ifndef FCPP_AGENT_CDOCTOR_H
#define FCPP_AGENT_CDOCTOR_H

#include "fcpp-agent/SConfig.h"

namespace fcpp
{

/**
 * @brief Диагностика окружения.
 */
class CDoctor
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CDoctor() = default;

    /**
     * @brief Деструктор.
     */
    ~CDoctor() = default;

    /**
     * @brief Выполнить preflight-проверку и напечатать отчёт.
     * @param config Конфигурация агента (пути к инструментам и enable_tidy).
     * @return true если все критичные инструменты доступны; false иначе.
     */
    bool run(const SConfig& config) const;

private:
    /**
     * @brief Проверить инструмент через CExecutor (argv, без shell).
     * @param tool Путь/имя инструмента.
     * @param out_version Выходная строка версии (первая строка вывода).
     * @return true если инструмент запустился успешно.
     */
    static bool probe(const std::string& tool, std::string& out_version);
};

} // namespace fcpp

#endif // FCPP_AGENT_CDOCTOR_H
