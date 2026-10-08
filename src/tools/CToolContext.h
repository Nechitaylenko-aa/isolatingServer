/**
 * @file CToolContext.h
 * @brief Контекст доступа инструментов к индексу проекта.
 *
 * Владеет соединением с базой индекса проекта и предоставляет инструментам
 * высокоуровневый граф (CSqlCompilerIndex) и низкоуровневый индекс (CSQLiteIndex).
 */

#ifndef FCPP_AGENT_CTOOLCONTEXT_H
#define FCPP_AGENT_CTOOLCONTEXT_H

#include <filesystem>
#include <memory>

#include "fcpp-agent/IClang.h"

struct sqlite3;

namespace fcpp
{

class CSqlCompilerIndex;
class CSQLiteIndex;

/**
 * @brief Контекст выполнения инструментов над одним проектом.
 */
class CToolContext
{
public:
    /**
     * @brief Конструктор.
     * @param db_path Путь к базе индекса проекта.
     * @param project_root Канонический корень проекта.
     * @param clang Backend libclang для позиционного резолва.
     */
    CToolContext(std::filesystem::path db_path,
                 std::filesystem::path project_root,
                 IClang* clang);

    /**
     * @brief Деструктор: закрывает соединения.
     */
    ~CToolContext();

    /**
     * @brief Копирование запрещено (владение соединением).
     */
    CToolContext(const CToolContext&) = delete;

    /**
     * @brief Присваивание запрещено.
     * @return Ссылка на текущий объект.
     */
    CToolContext& operator=(const CToolContext&) = delete;

    /**
     * @brief Открыть соединение и подготовить слои доступа.
     * @return true при успехе.
     */
    bool open();

    /**
     * @brief Высокоуровневый граф отношений.
     * @return Ссылка на граф.
     */
    CSqlCompilerIndex& graph();

    /**
     * @brief Низкоуровневый индекс.
     * @return Ссылка на индекс.
     */
    CSQLiteIndex& index();

    /**
     * @brief Проект, к которому относится контекст.
     * @return Контекст проекта.
     */
    const std::filesystem::path& project_root() const;

    /**
     * @brief Backend libclang (может быть не задан).
     * @return Указатель либо nullptr.
     */
    IClang* clang();

private:
    std::filesystem::path m_db_path;              ///< Путь к базе индекса.
    std::filesystem::path m_project_root;         ///< Корень проекта.
    IClang* m_clang;                              ///< Backend libclang (не владеет).
    sqlite3* m_db;                                ///< Соединение для графа.
    std::unique_ptr<CSqlCompilerIndex> m_graph;   ///< Граф отношений.
    std::unique_ptr<CSQLiteIndex> m_index;        ///< Низкоуровневый индекс.
};

} // namespace fcpp

#endif // FCPP_AGENT_CTOOLCONTEXT_H
