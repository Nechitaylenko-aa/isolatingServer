/**
 * @file CEvidenceCollector.h
 * @brief Сбор Evidence по детерминированному плану (Zero-Window, §56/§63).
 *
 * Выполняет инструменты из SPlan через CToolRegistry и формирует минимальный
 * по объёму Evidence. Никаких файлов целиком — только срезы/relationships.
 */

#ifndef FCPP_AGENT_CEVIDENCECOLLECTOR_H
#define FCPP_AGENT_CEVIDENCECOLLECTOR_H

#include "core/CConfigManager.h"
#include "core/CProjectManager.h"
#include "fcpp-agent/SEvidence.h"
#include "fcpp-agent/SIntent.h"

namespace fcpp
{

class CToolContext;
class CToolRegistry;

/**
 * @brief Сборщик Evidence.
 */
class CEvidenceCollector
{
public:
    /**
     * @brief Конструктор.
     * @param config_manager Конфигурация.
     * @param project_manager Менеджер проектов.
     */
    CEvidenceCollector(const CConfigManager& config_manager, CProjectManager& project_manager);

    /**
     * @brief Деструктор.
     */
    ~CEvidenceCollector() = default;

    /**
     * @brief Собрать Evidence по плану для заданного Intent.
     * @param intent Намерение.
     * @param plan План инструментов.
     * @param out_evidence Выходной Evidence.
     * @return true если удалось собрать хотя бы одну секцию.
     */
    bool collect(const SIntent& intent, const SPlan& plan, SEvidence& out_evidence) const;

private:
    /**
     * @brief Ограничить строки секции (top-N, §64).
     * @param text Текст.
     * @param max_lines Лимит строк.
     * @return Усечённый текст.
     */
    static std::string limit_lines(const std::string& text, std::size_t max_lines);

    /**
     * @brief Fan-out «класс → методы → реализации → тела» (ADR-19, path A).
     *
     * Детерминированно разворачивает субъект-класс на все методы и все их
     * реализации через `outline` + `dispatch_targets` + `get_source_slice`,
     * формируя секции Evidence вида «метод → реализации с телами».
     * @param context Контекст доступа к индексу.
     * @param class_name Имя класса-субъекта.
     * @param registry Реестр инструментов (для резолва класса и outline).
     * @param out_evidence Выходной Evidence.
     * @return true если собрана хотя бы одна секция.
     */
    bool collect_overrides_fanout(CToolContext& context,
                                  const std::string& class_name,
                                  const CToolRegistry& registry,
                                  SEvidence& out_evidence) const;

    const CConfigManager& m_config_manager; ///< Конфигурация.
    CProjectManager& m_project_manager;     ///< Менеджер проектов.
};

} // namespace fcpp

#endif // FCPP_AGENT_CEVIDENCECOLLECTOR_H
