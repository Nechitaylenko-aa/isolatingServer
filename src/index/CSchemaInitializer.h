/**
 * @file CSchemaInitializer.h
 * @brief Создание актуальной схемы SQLite для новых баз и проверка существующих.
 */

#ifndef FCPP_AGENT_CSCHEMAINITIALIZER_H
#define FCPP_AGENT_CSCHEMAINITIALIZER_H

#include <filesystem>

namespace fcpp
{

/**
 * @brief Инициализатор текущей схемы SQLite без поддержки миграций.
 *
 * Для нового файла БД создаёт текущую схему из schema/current.sql.
 * Для существующего файла только проверяет наличие обязательных таблиц:
 * структура существующей базы никогда не изменяется.
 */
class CSchemaInitializer
{
public:
    /**
     * @brief Создать схему новой БД либо проверить уже существующую.
     * @return true, если база готова к использованию; false при ошибке.
     */
    static bool initialize_or_validate(const std::filesystem::path& db_path);
};

} // namespace fcpp

#endif // FCPP_AGENT_CSCHEMAINITIALIZER_H
