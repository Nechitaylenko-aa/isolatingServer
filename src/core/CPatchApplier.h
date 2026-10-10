/**
 * @file CPatchApplier.h
 * @brief Атомарное применение валидированного патча (§77).
 *
 * Записывает подготовленные CPatchValidator изменения на диск. При любой ошибке
 * выполняется откат уже применённых изменений — источник остаётся неизменным
 * (all-or-nothing).
 */

#ifndef FCPP_AGENT_CPATCHAPPLIER_H
#define FCPP_AGENT_CPATCHAPPLIER_H

#include <string>
#include <vector>

#include "fcpp-agent/SPatch.h"

namespace fcpp
{

/**
 * @brief Аппликатор патча.
 */
class CPatchApplier
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CPatchApplier() = default;

    /**
     * @brief Деструктор.
     */
    ~CPatchApplier() = default;

    /**
     * @brief Применить запланированные изменения атомарно.
     * @param changes Изменения из CPatchValidator.
     * @param out_message Сообщение об ошибке (при неуспехе).
     * @return true при успехе; false с полным откатом.
     */
    bool apply(const std::vector<SFileChange>& changes, std::string& out_message) const;

    /**
     * @brief Откатить ранее применённые изменения (восстановить оригиналы).
     * @param changes Изменения из CPatchValidator.
     * @param out_message Сообщение об ошибке (при неуспехе).
     * @return true при успехе.
     */
    bool revert(const std::vector<SFileChange>& changes, std::string& out_message) const;
};

} // namespace fcpp

#endif // FCPP_AGENT_CPATCHAPPLIER_H
