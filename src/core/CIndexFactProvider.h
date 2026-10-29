/**
 * @file CIndexFactProvider.h
 * @brief Поставщик фактов о коде поверх индекса проекта.
 *
 * Оркестратор задаёт сущность, а провайдер детерминированно собирает набор
 * секций Zero-Window: определение, члены, связи и ограниченный срез кода.
 * Ни модель, ни анализатор не выбирают, какой инструмент вызвать.
 */

#ifndef FCPP_AGENT_CINDEXFACTPROVIDER_H
#define FCPP_AGENT_CINDEXFACTPROVIDER_H

#include <cstddef>
#include <filesystem>
#include <string>

#include "fcpp-agent/IFactProvider.h"

namespace fcpp
{

class IClang;

/**
 * @brief Поставщик фактов из индекса проекта.
 */
class CIndexFactProvider final : public IFactProvider
{
public:
    /**
     * @brief Конструктор.
     * @param db_path Путь к базе индекса проекта.
     * @param project_root Канонический корень проекта.
     * @param clang Backend libclang для позиционного резолва (может быть nullptr).
     */
    CIndexFactProvider(std::filesystem::path db_path,
                       std::filesystem::path project_root,
                       IClang* clang);

    /**
     * @brief Собрать факты по сущности.
     * @param request Запрос с именем и позицией сущности.
     * @return Секции фактов либо технический сбой.
     */
    SFactResult collect(const SFactRequest& request) const override;

private:
    std::filesystem::path m_db_path;      ///< База индекса проекта.
    std::filesystem::path m_project_root; ///< Корень проекта.
    IClang* m_clang;                      ///< Backend libclang (не владеет).
};

} // namespace fcpp

#endif // FCPP_AGENT_CINDEXFACTPROVIDER_H
