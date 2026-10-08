/**
 * @file SError.h
 * @brief Типизированные ошибки приложения (Секция 3 rules.md).
 */

#ifndef FCPP_AGENT_SERROR_H
#define FCPP_AGENT_SERROR_H

#include <string>

namespace fcpp
{

/**
 * @brief Категория ошибки приложения.
 *
 * Преобразование в текст — статическим массивом в .cpp, без switch-case.
 */
enum class EErrorKind
{
    Ok,             ///< Ошибки нет.
    InvalidRequest, ///< Некорректный запрос.
    SymbolNotFound, ///< Сущность не найдена.
    Ambiguous,      ///< Неоднозначное совпадение.
    IndexNotReady,  ///< Индекс не готов.
    IndexStale,     ///< Индекс устарел.
    IoError,        ///< Ошибка ввода-вывода.
    ProjectNotOpen, ///< Проект не открыт.
    PermissionDenied,///< Операция запрещена политикой (§47).
    PatchInvalid,   ///< Патч невалиден (§46).
    PatchApplyFailed,///< Ошибка применения патча (§77).
    Count           ///< Маркер количества для инициализации массива.
};

/**
 * @brief Типизированная ошибка с человекочитаемым сообщением.
 */
struct SError
{
    EErrorKind m_kind;   ///< Категория ошибки.
    std::string m_message; ///< Человекочитаемое сообщение.
};

/**
 * @brief Получить машинное имя категории ошибки.
 * @param kind Категория ошибки.
 * @return Стабильное текстовое имя.
 */
const char* error_kind_name(EErrorKind kind);

} // namespace fcpp

#endif // FCPP_AGENT_SERROR_H
