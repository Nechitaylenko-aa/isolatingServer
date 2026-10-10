/**
 * @file ICompilerIndex.h
 * @brief Высокоуровневый интерфейс графа отношений (ADR-11).
 *
 * Обслуживает детерминированные запросы над уже собранным индексом SQLite:
 * иерархию, диспетчеризацию виртуальных методов, include-граф. Не обращается
 * к libclang: все данные берутся из БД, наполняемой прогревом (CClang22).
 */

#ifndef FCPP_AGENT_ICOMPILERINDEX_H
#define FCPP_AGENT_ICOMPILERINDEX_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "fcpp-agent/IIndex.h"

namespace fcpp
{

/**
 * @brief Базовый тип в иерархии класса.
 */
struct SBaseType
{
    std::string m_usr;           ///< USR базового типа (если резолвится).
    std::string m_qname;         ///< Имя базового типа.
    std::string m_access;        ///< public/protected/private.
    bool m_is_virtual = false;   ///< Виртуальное наследование.
};

/**
 * @brief Высокоуровневый запросный интерфейс графа (из SQLite).
 */
class ICompilerIndex
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~ICompilerIndex() = default;

    /**
     * @brief Прямые базовые типы класса.
     * @param class_usr USR класса.
     * @return Список базовых типов.
     */
    virtual std::vector<SBaseType> base_types(const std::string& class_usr) const = 0;

    /**
     * @brief Все предки класса (подъём до корня).
     * @param class_usr USR класса.
     * @return Список USR/имён предков в порядке подъёма.
     */
    virtual std::vector<SBaseType> ancestors(const std::string& class_usr) const = 0;

    /**
     * @brief Прямые наследники класса (reverse-запрос из БД).
     * @param class_usr USR класса.
     * @return Список USR/имён наследников.
     */
    virtual std::vector<SBaseType> derived_types(const std::string& class_usr) const = 0;

    /**
     * @brief Методы, переопределяющие данный метод (в наследниках).
     * @param method_usr USR метода.
     * @return Список USR переопределяющих методов.
     */
    virtual std::vector<std::string> overrides(const std::string& method_usr) const = 0;

    /**
     * @brief Базовый метод, который переопределяет данный метод.
     * @param method_usr USR метода-реализации.
     * @return USR базового метода либо пустая строка.
     */
    virtual std::string base_method(const std::string& method_usr) const = 0;

    /**
     * @brief Сквозной резолв виртуального метода до всех реализаций в наследниках.
     * @param method_usr USR метода.
     * @return USR всех реализаций (включая исходный метод).
     */
    virtual std::vector<std::string> dispatch_targets(const std::string& method_usr) const = 0;

    /**
     * @brief Файлы, напрямую включаемые данным файлом.
     * @param file_id Идентификатор файла.
     * @return Список имён включаемых путей.
     */
    virtual std::vector<std::string> file_includes(long long file_id) const = 0;

    /**
     * @brief Файлы, включающие данный файл (reverse-запрос).
     * @param file_id Идентификатор файла.
     * @return Список имён включающих файлов.
     */
    virtual std::vector<std::string> included_by(long long file_id) const = 0;

    /**
     * @brief Позиционный резолв сущности над индексом (ADR-22).
     *
     * Возвращает самую внутреннюю сущность, чей диапазон содержит позицию
     * (file, line, column). Используется как кэш-путь для разрешения курсора
     * (fallback — IClang::cursor_at по свежему/непокрытому файлу).
     * @param file Путь файла (как в индексе).
     * @param line Строка (1-базовая).
     * @param column Колонка (1-базовая).
     * @return Сущность либо пустое значение.
     */
    virtual std::optional<SSymbol> symbol_at(const std::string& file,
                                             std::uint32_t line,
                                             std::uint32_t column) const = 0;
};

} // namespace fcpp

#endif // FCPP_AGENT_ICOMPILERINDEX_H
