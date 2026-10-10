/**
 * @file CSqlCompilerIndex.h
 * @brief Реализация ICompilerIndex поверх SQLite (ADR-11).
 *
 * Отвечает на запросы графа отношений из уже собранного индекса. libclang не
 * используется: все данные читаются из таблиц symbol/inheritance/override/
 * reference/call/include. Соединение с БД заимствуется (не владеет им).
 */

#ifndef FCPP_AGENT_CSQLCOMPILERINDEX_H
#define FCPP_AGENT_CSQLCOMPILERINDEX_H

#include <string>
#include <vector>

#include "fcpp-agent/ICompilerIndex.h"

struct sqlite3;

namespace fcpp
{

/**
 * @brief Запросный слой графа отношений над SQLite.
 */
class CSqlCompilerIndex final : public ICompilerIndex
{
public:
    /**
     * @brief Конструктор с заимствованным соединением.
     * @param db Открытое соединение SQLite (владелец — вызывающая сторона).
     */
    explicit CSqlCompilerIndex(sqlite3* db);

    /**
     * @brief Деструктор (соединение не закрывает).
     */
    ~CSqlCompilerIndex() override = default;

    /**
     * @brief Прямые базовые типы класса.
     * @param class_usr USR класса.
     * @return Список базовых типов.
     */
    std::vector<SBaseType> base_types(const std::string& class_usr) const override;

    /**
     * @brief Все предки класса.
     * @param class_usr USR класса.
     * @return Список предков в порядке подъёма.
     */
    std::vector<SBaseType> ancestors(const std::string& class_usr) const override;

    /**
     * @brief Прямые наследники класса.
     * @param class_usr USR класса.
     * @return Список наследников.
     */
    std::vector<SBaseType> derived_types(const std::string& class_usr) const override;

    /**
     * @brief Методы, переопределяющие данный метод.
     * @param method_usr USR метода.
     * @return USR переопределяющих методов.
     */
    std::vector<std::string> overrides(const std::string& method_usr) const override;

    /**
     * @brief Базовый метод данного переопределения.
     * @param method_usr USR метода.
     * @return USR базового метода либо пустая строка.
     */
    std::string base_method(const std::string& method_usr) const override;

    /**
     * @brief Сквозной резолв виртуального метода до всех реализаций.
     * @param method_usr USR метода.
     * @return USR всех реализаций.
     */
    std::vector<std::string> dispatch_targets(const std::string& method_usr) const override;

    /**
     * @brief Позиционный резолв сущности над индексом (ADR-22).
     * @param file Путь файла.
     * @param line Строка (1-базовая).
     * @param column Колонка (1-базовая).
     * @return Самая внутренняя сущность, содержащая позицию.
     */
    std::optional<SSymbol> symbol_at(const std::string& file,
                                     std::uint32_t line,
                                     std::uint32_t column) const override;

    /**
     * @brief Файлы, напрямую включаемые данным файлом.
     * @param file_id Идентификатор файла.
     * @return Список включаемых путей.
     */
    std::vector<std::string> file_includes(long long file_id) const override;

    /**
     * @brief Файлы, включающие данный файл.
     * @param file_id Идентификатор файла.
     * @return Список включающих файлов.
     */
    std::vector<std::string> included_by(long long file_id) const override;

private:
    /**
     * @brief Найти id символа по USR в активной generation.
     * @param usr USR символа.
     * @return id символа (0 если не найден).
     */
    long long symbol_id_by_usr(const std::string& usr) const;

    /**
     * @brief Найти USR символа по id.
     * @param id Идентификатор символа.
     * @return USR символа (пустая строка если не найден).
     */
    std::string symbol_usr_by_id(long long id) const;

    sqlite3* m_db; ///< Заимствованное соединение SQLite.
};

} // namespace fcpp

#endif // FCPP_AGENT_CSQLCOMPILERINDEX_H
