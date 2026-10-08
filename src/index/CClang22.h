/**
 * @file CClang22.h
 * @brief Реализация IClang поверх libclang (целевая версия 22, ADR-11).
 *
 * Все заголовки clang-c инкапсулированы в .cpp; наружу виден только IClang.
 * Класс не хранит состояния разбора: каждый вызов create/dispose независим.
 */

#ifndef FCPP_AGENT_CCLANG22_H
#define FCPP_AGENT_CCLANG22_H

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fcpp-agent/IClang.h"

namespace fcpp
{

/**
 * @brief Backend доступа к libclang (версия 22).
 */
class CClang22 final : public IClang
{
public:
    /**
     * @brief Конструктор по умолчанию.
     */
    CClang22() = default;

    /**
     * @brief Виртуальный деструктор.
     */
    ~CClang22() override = default;

    /**
     * @brief Проверить доступность backend.
     * @return true если libclang доступен.
     */
    bool is_available() const override;

    /**
     * @brief Версия libclang.
     * @return Строка версии.
     */
    std::string version() const override;

    /**
     * @brief Разобрать translation unit и извлечь символы и связи.
     * @param source_file Разбираемый файл.
     * @param arguments Аргументы компиляции (argv из compile_commands.json).
     * @param out Результат извлечения.
     * @return true при успехе; false при ошибке разбора.
     */
    bool extract(const std::filesystem::path& source_file,
                 const std::vector<std::string>& arguments,
                 SClangExtract& out) const override;

    /**
     * @brief Найти ближайшую семантическую сущность по позиции.
     * @param source_file Файл.
     * @param line Строка (1-базовая).
     * @param column Колонка (1-базовая).
     * @param arguments Аргументы компиляции.
     * @return Сущность либо пустое значение.
     */
    std::optional<SClangSymbol> cursor_at(const std::filesystem::path& source_file,
                                          std::uint32_t line,
                                          std::uint32_t column,
                                          const std::vector<std::string>& arguments) const override;
};

} // namespace fcpp

#endif // FCPP_AGENT_CCLANG22_H
