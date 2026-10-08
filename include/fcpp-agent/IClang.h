/**
 * @file IClang.h
 * @brief Низкоуровневый интерфейс доступа к libclang (ADR-11).
 *
 * Все обращения к libclang инкапсулированы в этом виртуальном интерфейсе.
 * Application-код и прогрев кэша зависят только от IClang и никогда не
 * включают clang-c напрямую. Заголовок не тянет clang-заголовки.
 */

#ifndef FCPP_AGENT_ICLANG_H
#define FCPP_AGENT_ICLANG_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fcpp-agent/IIndex.h"

namespace fcpp
{

/**
 * @brief Позиция в исходном коде (1-базовые строка и колонка).
 */
struct SClangLocation
{
    std::string m_file;         ///< Путь к файлу.
    std::uint32_t m_line;       ///< Начальная строка.
    std::uint32_t m_column;     ///< Начальная колонка.
    std::uint32_t m_end_line;   ///< Конечная строка.
    std::uint32_t m_end_column; ///< Конечная колонка.
};

/**
 * @brief Извлечённая сущность C++.
 */
struct SClangSymbol
{
    std::string m_usr;             ///< Стабильный идентификатор (USR).
    std::string m_name;            ///< Короткое имя.
    std::string m_qualified_name;  ///< Квалифицированное имя.
    std::string m_signature;       ///< Сигнатура/display.
    ESymbolKind m_kind;            ///< Категория сущности.
    std::string m_access;          ///< public/protected/private.
    SClangLocation m_location;     ///< Позиция.
    bool m_is_definition;          ///< Является определением.
    bool m_is_virtual;             ///< Виртуальный метод.
    bool m_is_pure_virtual;        ///< Чисто виртуальный метод.
    bool m_is_static;              ///< Статический метод/член.
    std::string m_parent_usr;      ///< USR семантического родителя.
    std::string m_overridden_usr;  ///< USR переопределяемого базового метода.
};

/**
 * @brief Базовый тип (наследование).
 */
struct SClangBase
{
    std::string m_derived_usr; ///< USR производного класса.
    std::string m_base_usr;    ///< USR базового класса (если резолвится).
    std::string m_base_qname;  ///< Имя базового класса (spelling).
    std::string m_access;      ///< public/protected/private.
    bool m_is_virtual;         ///< Виртуальное наследование.
};

/**
 * @brief Ребро графа вызовов.
 */
struct SClangCall
{
    std::string m_caller_usr;  ///< USR вызывающей функции.
    std::string m_callee_usr;  ///< USR вызываемой сущности (если резолвится).
    std::string m_callee_qname;///< Имя вызываемой сущности.
    SClangLocation m_location; ///< Позиция вызова.
};

/**
 * @brief Использование символа.
 */
struct SClangReference
{
    std::string m_symbol_usr;  ///< USR используемой сущности.
    std::string m_kind;        ///< Вид использования (call/type/value).
    SClangLocation m_location; ///< Позиция.
};

/**
 * @brief Директива include.
 */
struct SClangInclude
{
    std::string m_from_file; ///< Включающий файл.
    std::string m_to_path;   ///< Текст включения.
    bool m_is_angle;         ///< Угловые скобки.
};

/**
 * @brief Диагностика компиляции.
 */
struct SClangDiagnostic
{
    std::string m_severity; ///< error/warning/note/fatal.
    std::string m_file;     ///< Файл.
    std::uint32_t m_line;   ///< Строка.
    std::uint32_t m_column; ///< Колонка.
    std::string m_message;  ///< Текст диагностики.
};

/**
 * @brief Результат разбора одного translation unit.
 */
struct SClangExtract
{
    std::vector<SClangSymbol> m_symbols;         ///< Сущности.
    std::vector<SClangBase> m_bases;             ///< Наследование.
    std::vector<SClangCall> m_calls;             ///< Вызовы.
    std::vector<SClangReference> m_references;   ///< Ссылки.
    std::vector<SClangInclude> m_includes;       ///< Include-граф.
    std::vector<SClangDiagnostic> m_diagnostics; ///< Диагностики.
};

/**
 * @brief Низкоуровневый интерфейс доступа к libclang.
 */
class IClang
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~IClang() = default;

    /**
     * @brief Проверить доступность backend.
     * @return true если libclang доступен.
     */
    virtual bool is_available() const = 0;

    /**
     * @brief Версия libclang.
     * @return Строка версии.
     */
    virtual std::string version() const = 0;

    /**
     * @brief Разобрать translation unit и извлечь символы и связи.
     * @param source_file Разбираемый файл.
     * @param arguments Аргументы компиляции (argv из compile_commands.json).
     * @param out Результат извлечения.
     * @return true при успехе; false при ошибке разбора.
     */
    virtual bool extract(const std::filesystem::path& source_file,
                         const std::vector<std::string>& arguments,
                         SClangExtract& out) const = 0;

    /**
     * @brief Найти ближайшую семантическую сущность по позиции.
     * @param source_file Файл.
     * @param line Строка (1-базовая).
     * @param column Колонка (1-базовая).
     * @param arguments Аргументы компиляции.
     * @return Сущность либо пустое значение.
     */
    virtual std::optional<SClangSymbol> cursor_at(const std::filesystem::path& source_file,
                                                  std::uint32_t line,
                                                  std::uint32_t column,
                                                  const std::vector<std::string>& arguments) const = 0;
};

} // namespace fcpp

#endif // FCPP_AGENT_ICLANG_H
