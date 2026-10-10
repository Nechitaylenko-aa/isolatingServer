/**
 * @file ITool.h
 * @brief Интерфейс детерминированного инструмента (ADR-11, Секция 3).
 *
 * Инструменты не зависят от LLM: принимают типизированный request и возвращают
 * типизированный result или типизированную ошибку. Выбор инструмента выполняет
 * deterministic planner, а не модель.
 */

#ifndef FCPP_AGENT_ITOOL_H
#define FCPP_AGENT_ITOOL_H

#include <cstdint>
#include <string>
#include <vector>

#include "fcpp-agent/IIndex.h"
#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/SError.h"

namespace fcpp
{

class CToolContext;

/**
 * @brief Имя инструмента.
 */
enum class EToolName
{
    Outline,    ///< Структура сущности.
    Callers,    ///< Кто вызывает.
    Callees,    ///< Кого вызывает.
    References, ///< Где используется.
    Includes,   ///< Include-граф.
    Overrides,  ///< Переопределения метода.
    Dispatch,   ///< Виртуальный метод -> все реализации.
    Definition, ///< Каноническое определение.
    Ancestors,  ///< Базовые классы.
    Derived,    ///< Наследники.
    Search,     ///< Поиск по имени.
    Source,     ///< Минимальный срез кода (Zero-Window).
    Count       ///< Маркер количества для инициализации массива.
};

/**
 * @brief Унифицированный запрос к инструменту.
 */
struct SToolRequest
{
    EToolName m_tool = EToolName::Outline; ///< Инструмент.
    std::string m_target;    ///< Целевая сущность (имя, квалифицированное имя или USR).
    std::string m_file;      ///< Файл (для позиционного резолва).
    std::uint32_t m_line = 0;   ///< Строка (для позиционного резолва).
    std::uint32_t m_column = 0; ///< Колонка (для позиционного резолва).
    std::uint32_t m_limit = 0;  ///< Лимит элементов (0 — без лимита, для QueryContext §64).
};

/**
 * @brief Унифицированный результат инструмента.
 */
struct SToolResult
{
    bool m_ok = false;                      ///< Успех выполнения.
    EErrorKind m_error = EErrorKind::Ok;    ///< Категория ошибки.
    std::string m_message;                  ///< Сообщение.
    std::vector<SSymbol> m_symbols;         ///< Найденные сущности.
    std::vector<SSymbolLocation> m_locations;///< Позиции.
    std::vector<std::string> m_names;       ///< Имена (для dispatch/derived/ancestors).
    std::string m_text;                     ///< Текстовый срез (source).
};

/**
 * @brief Интерфейс детерминированного инструмента.
 */
class ITool
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~ITool() = default;

    /**
     * @brief Имя инструмента.
     * @return Имя.
     */
    virtual EToolName name() const = 0;

    /**
     * @brief Выполнить инструмент.
     * @param request Запрос.
     * @param context Контекст доступа к индексу.
     * @return Результат либо типизированная ошибка.
     */
    virtual SToolResult execute(const SToolRequest& request, CToolContext& context) const = 0;
};

/**
 * @brief Получить машинное имя инструмента.
 * @param name Имя инструмента.
 * @return Стабильное текстовое имя.
 */
const char* tool_name(EToolName name);

/**
 * @brief Получить машинное имя категории сущности.
 * @param kind Категория сущности.
 * @return Стабильное текстовое имя.
 */
const char* symbol_kind_name(ESymbolKind kind);

/**
 * @brief Схемы всех детерминированных инструментов (для ReAct-режима модели).
 *
 * Единый источник правды — EToolName: имя (tool_name), описание, параметры.
 * @return Список схем инструментов.
 */
std::vector<SModelTool> tool_schemas();

/**
 * @brief Описание одного инструмента для модели.
 * @param name Имя инструмента.
 * @return Схема.
 */
SModelTool tool_schema(EToolName name);

/**
 * @brief Сериализовать результат инструмента в компактный текст для модели.
 *
 * Zero-Window: только срезы/relationships, без файлов целиком (§56/§63).
 * @param result Результат инструмента.
 * @return Текст для передачи модели.
 */
std::string format_tool_result(const SToolResult& result);

} // namespace fcpp

#endif // FCPP_AGENT_ITOOL_H
