/**
 * @file ILogger.h
 * @brief Интерфейс structured logging (§60).
 *
 * Уровни: ERROR/WARN/INFO/DEBUG/TRACE. Логи агента пишутся вне проекта.
 */

#ifndef FCPP_AGENT_ILOGGER_H
#define FCPP_AGENT_ILOGGER_H

#include <string>

namespace fcpp
{

/**
 * @brief Уровень логирования.
 *
 * Преобразование в текст — статическим массивом в .cpp (Секция 3).
 */
enum class ELogLevel
{
    Error, ///< Ошибка.
    Warn,  ///< Предупреждение.
    Info,  ///< Информация.
    Debug, ///< Отладка.
    Trace, ///< Трассировка.
    Count  ///< Маркер количества для инициализации массива.
};

/**
 * @brief Интерфейс логгера.
 */
class ILogger
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~ILogger() = default;

    /**
     * @brief Записать сообщение.
     * @param level Уровень.
     * @param message Сообщение.
     */
    virtual void log(ELogLevel level, const std::string& message) = 0;

    /**
     * @brief Получить машинное имя уровня.
     * @param level Уровень.
     * @return Стабильное текстовое имя.
     */
    static const char* level_name(ELogLevel level);
};

} // namespace fcpp

#endif // FCPP_AGENT_ILOGGER_H
