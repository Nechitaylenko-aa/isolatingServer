#ifndef FCPP_AGENT_CCURSORCONTEXTANALYZER_H
#define FCPP_AGENT_CCURSORCONTEXTANALYZER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fcpp-agent/IClang.h"
#include "llm/CLlmLayer.h"

namespace fcpp
{

/**
 * @brief Ограничения среза исходника вокруг курсора.
 *
 * Срез строится окном вокруг строки курсора, а не диапазоном всей сущности:
 * иначе курсор внутри большого класса отправляет в модель весь класс и
 * обрубается посреди кода.
 */
struct SCursorSliceLimits
{
    std::size_t m_radius_lines = 60; ///< Сколько строк брать выше и ниже курсора.
    std::size_t m_max_bytes = 10000; ///< Жёсткий лимит размера среза.
};

/**
 * @brief Ограничения ограниченного среза файла целиком.
 *
 * Срез читается с начала файла и всегда укладывается в лимит: в модель не
 * уходит файл произвольного размера (Zero-Window).
 */
struct SFileSliceLimits
{
    std::size_t m_max_bytes = 8000; ///< Жёсткий лимит размера среза.
};

/**
 * @brief Прочитать ограниченный срез файла любого типа.
 *
 * @param file Путь к файлу.
 * @param limits Ограничения среза.
 * @param out_slice Выходной текст среза.
 * @param out_truncated Признак усечения по лимиту.
 * @param out_error Причина сбоя.
 * @return true при успехе.
 */
bool read_file_slice(const std::filesystem::path& file,
                     const SFileSliceLimits& limits,
                     std::string& out_slice,
                     bool& out_truncated,
                     std::string& out_error);

/**
 * @brief Результат оценки применимости курсорного контекста.
 */
struct SCursorContextResult
{
    bool m_ok = true;                        ///< Оценка завершилась без сбоя.
    bool m_context_applicable = false;       ///< Курсор относится к запросу.
    bool m_llm_called = false;               ///< Модель привлекалась к оценке.
    bool m_escalated = false;                ///< Была эскалация роли.
    bool m_slice_truncated = false;          ///< Срез усечён по лимиту.
    std::size_t m_slice_tokens_estimate = 0; ///< Грубая оценка токенов среза.
    ELlmRole m_used_role = ELlmRole::Count;  ///< Фактически использованная роль.
    std::optional<SClangSymbol> m_symbol;    ///< Сущность под курсором.
    std::string m_source_context;            ///< Срез исходника для промпта.
    std::string m_error;                     ///< Причина сбоя.
};

/**
 * @brief Результат оценки применимости контекста файла без курсора.
 */
struct SFileContextResult
{
    bool m_ok = true;                  ///< Оценка завершилась без сбоя.
    bool m_context_applicable = false; ///< Вопрос относится к этому файлу.
    bool m_llm_called = false;         ///< Модель привлекалась к оценке.
    bool m_truncated = false;          ///< Содержимое усечено по лимиту.
    ELlmRole m_used_role = ELlmRole::Count; ///< Фактически использованная роль.
    std::string m_content;             ///< Содержимое файла для промпта.
    std::string m_error;               ///< Причина сбоя.
};

/**
 * @brief Какая сущность из файла нужна для ответа.
 *
 * Модель может назвать одну сущность — тогда факты берутся из индекса по ней.
 * Если названы несколько или ни одной, ответ строится по содержимому файла
 * целиком: угадывать нельзя.
 */
struct SFileEntityChoice
{
    bool m_ok = true;                  ///< Оценка завершилась без сбоя.
    bool m_single = false;             ///< Модель назвала ровно одну сущность.
    std::string m_qualified_name;      ///< Имя сущности при m_single.
    ELlmRole m_used_role = ELlmRole::Count; ///< Фактически использованная роль.
    std::string m_error;               ///< Причина сбоя.
};

/**
 * @brief Определение применимости курсора и подготовка среза исходника.
 */
class CCursorContextAnalyzer
{
public:
    /**
     * @brief Конструктор.
     * @param clang Бэкенд libclang.
     * @param llm Слой моделей для оценки релевантности.
     */
    CCursorContextAnalyzer(const IClang& clang, CLlmLayer& llm);

    /**
     * @brief Оценить, нужен ли курсор для ответа на запрос.
     * @param current_request Текст запроса.
     * @param source_file Файл под курсором.
     * @param line Строка курсора (1-based).
     * @param column Колонка курсора (1-based).
     * @param compile_arguments Аргументы компиляции из compile_commands.json.
     * @param limits Ограничения среза исходника.
     * @return Результат оценки и срез.
     */
    SCursorContextResult assess(const std::string& current_request,
                                const std::filesystem::path& source_file,
                                std::uint32_t line,
                                std::uint32_t column,
                                const std::vector<std::string>& compile_arguments,
                                const SCursorSliceLimits& limits = {}) const;

    /**
     * @brief Оценить, относится ли запрос к файлу без позиции курсора.
     *
     * Файл любого типа (исходник, заголовок, markdown, текст): модель-мыслитель
     * отвечает строго YES/NO, коррелирует ли вопрос с этим файлом. При YES
     * возвращается содержимое файла; при NO контекст неприменим, и оркестратор
     * завершает запрос видимым исходом, а не молчаливым общим ответом.
     *
     * @param current_request Текст запроса.
     * @param source_file Файл из редактора.
     * @param limits Ограничения чтения содержимого.
     * @return Результат оценки и содержимое.
     */
    SFileContextResult assess_file(const std::string& current_request,
                                   const std::filesystem::path& source_file,
                                   const SFileSliceLimits& limits = {}) const;

    /**
     * @brief Спросить, какая сущность из файла нужна для ответа.
     *
     * Ответ модели — текст, а не факт: перед использованием оркестратор
     * подтверждает принадлежность сущности файлу по индексу. Если названо
     * несколько сущностей или ни одной, `m_single` = false, и ответ строится
     * по всему файлу.
     *
     * @param current_request Текст запроса.
     * @param source_file Файл из редактора.
     * @param content Содержимое файла (может быть усечено).
     * @return Выбор сущности либо отказ от выбора.
     */
    SFileEntityChoice choose_file_entity(const std::string& current_request,
                                         const std::filesystem::path& source_file,
                                         const std::string& content) const;

private:
    const IClang& m_clang;
    CLlmLayer& m_llm;
};
} // namespace fcpp

#endif
