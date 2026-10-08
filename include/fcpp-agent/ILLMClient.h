/**
 * @file ILLMClient.h
 * @brief Интерфейс клиента LLM.
 *
 * Все backend-провайдеры (llama-cpp, OpenAI-compatible, Claude) обязаны
 * реализовывать этот интерфейс. Application-код не зависит от конкретного
 * API провайдера. Формат обмена — лаконичный XML (intent/answer/patch).
 */

#ifndef FCPP_AGENT_ILLMCLIENT_H
#define FCPP_AGENT_ILLMCLIENT_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace fcpp
{

/**
 * @brief Логическая роль модели.
 */
enum ERole
{
    ROLE_LITE,   ///< Классификация запроса, извлечение intent.
    ROLE_MAIN,   ///< Генерация ответа и патчей.
    ROLE_REMOTE  ///< Сложные задачи (облачная модель).
};

/**
 * @brief Запрос к LLM.
 */
struct SLlmRequest
{
    ERole m_role;               ///< Роль модели.
    std::string m_system_prompt;///< Системный промт (стабильный префикс).
    std::string m_user_prompt;  ///< Пользовательский промт (переменная часть).
    double m_temperature;       ///< Температура генерации.
    std::uint32_t m_max_tokens; ///< Лимит токенов ответа.
    std::uint64_t m_seed;       ///< Seed для воспроизводимости.
};

/**
 * @brief Порция потокового ответа LLM.
 */
struct SLlmChunk
{
    std::string m_text; ///< Текст порции.
    bool m_final;       ///< Признак последней порции.
};

/**
 * @brief Разобранный ответ LLM.
 */
struct SLlmResponse
{
    std::string m_intent_xml; ///< Секция <intent>.
    std::string m_answer;     ///< Секция <answer>.
    std::string m_patch;      ///< Секция <patch>.
    bool m_valid;             ///< Признак успешной валидации формата.
};

/**
 * @brief Интерфейс клиента LLM с потоковой генерацией.
 */
class ILLMClient
{
public:
    /**
     * @brief Виртуальный деструктор.
     */
    virtual ~ILLMClient() = default;

    /**
     * @brief Инициализировать клиента по конфигурации агента.
     * @param config_path Путь к agent.conf.
     * @return true при успешной инициализации; false при ошибке.
     */
    virtual bool initialize(const std::filesystem::path& config_path) = 0;

    /**
     * @brief Корректно завершить работу клиента.
     */
    virtual void shutdown() = 0;

    /**
     * @brief Закэшировать стабильный префикс промта (prompt_cache).
     * @param stable_prefix Неизменяемый префикс (system prompt + схема БД).
     * @return true при успешном кэшировании; false при ошибке.
     */
    virtual bool prime_prompt_cache(const std::string& stable_prefix) = 0;

    /**
     * @brief Сбросить кэш промта.
     */
    virtual void reset_prompt_cache() = 0;

    /**
     * @brief Выполнить запрос с потоковой выдачей токенов.
     * @param request Параметры запроса.
     * @param on_chunk Callback, вызываемый для каждой порции ответа.
     * @return Разобранный ответ LLM.
     */
    virtual SLlmResponse complete(const SLlmRequest& request,
                                  const std::function<void(const SLlmChunk&)>& on_chunk) = 0;
};

} // namespace fcpp

#endif // FCPP_AGENT_ILLMCLIENT_H
