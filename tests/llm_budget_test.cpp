/**
 * @file llm_budget_test.cpp
 * @brief LLM-слой сам укладывает промпт в бюджет роли.
 */

#include "llm/CLlmLayer.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/ILogger.h"

namespace
{

/** @brief Логгер, запоминающий имена событий. */
class CRecordingLogger final : public fcpp::ILogger
{
public:
    void log(fcpp::ELogLevel, const std::string& message) override
    {
        m_messages.push_back(message);
    }

    void log(fcpp::ELogLevel, const fcpp::SLogEvent& event) override
    {
        m_messages.push_back(event.m_name);
    }

    const std::vector<std::string>& messages() const
    {
        return m_messages;
    }

private:
    std::vector<std::string> m_messages;
};

/** @brief Модель с заданным ответом (в том числе пустым). */
class CReplyModel final : public fcpp::IModelApi
{
public:
    explicit CReplyModel(std::string text)
        : m_text(std::move(text))
    {
    }

    bool is_available() const override
    {
        return true;
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>&) override
    {
        return fcpp::SModelReply{true, m_text, {}, "stop", {}};
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>& messages,
                               const std::vector<fcpp::SModelTool>&) override
    {
        return complete(messages);
    }

    fcpp::SModelReply complete_stream(const std::vector<fcpp::SModelMessage>& messages,
                                      const fcpp::SStreamCallback&) override
    {
        return complete(messages);
    }

private:
    std::string m_text;
};

/** @brief Модель, запоминающая отправленный промпт. */
class CCapturingModel final : public fcpp::IModelApi
{
public:
    explicit CCapturingModel(std::vector<std::string>* captured)
        : m_captured(captured)
    {
    }

    bool is_available() const override
    {
        return true;
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>& messages) override
    {
        if (m_captured != nullptr)
        {
            m_captured->clear();
            for (const fcpp::SModelMessage& message : messages)
            {
                m_captured->push_back(message.m_content);
            }
        }
        return fcpp::SModelReply{true, "ok", {}, "stop", {}};
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>& messages,
                               const std::vector<fcpp::SModelTool>&) override
    {
        return complete(messages);
    }

    fcpp::SModelReply complete_stream(const std::vector<fcpp::SModelMessage>& messages,
                                      const fcpp::SStreamCallback&) override
    {
        return complete(messages);
    }

private:
    std::vector<std::string>* m_captured;
};

/** @brief Роли с заданным окном и резервом. */
fcpp::SLlmRoleSet make_roles(std::size_t window, std::size_t reserve)
{
    fcpp::SLlmRoleSet roles;
    fcpp::SLlmModelRole& role = roles.at(fcpp::ELlmRole::CoderLocal);
    role.m_api = "http://127.0.0.1:9/v1";
    role.m_model = "local";
    role.m_type = "openai-compat";
    role.m_configured = true;
    role.m_resolved_api = role.m_api;
    role.m_resolved_model = role.m_model;
    role.m_resolved_type = role.m_type;
    role.m_resolved_role = fcpp::ELlmRole::CoderLocal;
    role.m_resolved_context_window_tokens = window;
    role.m_resolved_output_reserve_tokens = reserve;
    return roles;
}

/** @brief Строка, содержащая метку в начале и в конце. */
std::string long_prompt(std::size_t bytes)
{
    std::string text = "HEAD_MARKER ";
    text.append(bytes, 'x');
    text += " TAIL_MARKER";
    return text;
}

} // namespace

TEST_CASE("llm layer truncates a prompt that does not fit the role window",
          "[llm][budget]")
{
    // Окно 1000, резерв 800 → на промпт остаётся ~200 токенов (~600 байт).
    const fcpp::SLlmRoleSet roles = make_roles(1000, 800);
    CRecordingLogger logger;
    std::vector<std::string> captured;

    fcpp::CLlmLayer layer(roles, logger,
        [&captured](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CCapturingModel>(&captured);
        });

    const fcpp::SLlmExecutionResult result =
        layer.request_text(fcpp::ELlmRole::CoderLocal, "You are a helper.", long_prompt(20000));

    REQUIRE(result.m_ok);
    REQUIRE(captured.size() == 2);

    // Промпт заметно урезан по сравнению с исходным.
    REQUIRE(captured[1].size() < 20000);
    // Начало (улика) сохранено.
    REQUIRE(captured[1].find("HEAD_MARKER") != std::string::npos);
    // Конец (сам вопрос) тоже сохранён, потеряна именно середина.
    REQUIRE(captured[1].find("TAIL_MARKER") != std::string::npos);
    REQUIRE(captured[1].find("prompt truncated") != std::string::npos);

    bool logged_truncation = false;
    for (const std::string& message : logger.messages())
    {
        if (message == "llm.prompt_truncated")
        {
            logged_truncation = true;
        }
    }
    REQUIRE(logged_truncation);
}

TEST_CASE("llm layer leaves a prompt that fits the window untouched",
          "[llm][budget]")
{
    const fcpp::SLlmRoleSet roles = make_roles(30000, 4096);
    CRecordingLogger logger;
    std::vector<std::string> captured;

    fcpp::CLlmLayer layer(roles, logger,
        [&captured](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CCapturingModel>(&captured);
        });

    const std::string prompt = long_prompt(1000);
    const fcpp::SLlmExecutionResult result =
        layer.request_text(fcpp::ELlmRole::CoderLocal, "You are a helper.", prompt);

    REQUIRE(result.m_ok);
    REQUIRE(captured.size() == 2);
    REQUIRE(captured[1] == prompt);

    for (const std::string& message : logger.messages())
    {
        REQUIRE(message != "llm.prompt_truncated");
    }
}

TEST_CASE("llm layer reports the configured context window",
          "[llm][budget]")
{
    const fcpp::SLlmRoleSet roles = make_roles(8192, 4096);
    CRecordingLogger logger;
    fcpp::CLlmLayer layer(roles, logger);

    REQUIRE(layer.context_window_tokens(fcpp::ELlmRole::CoderLocal) == 8192);
    REQUIRE(layer.context_window_tokens(fcpp::ELlmRole::Count) == 0);
}

TEST_CASE("llm layer does not escalate an empty reply caused by prompt truncation",
          "[llm][budget][escalation]")
{
    // Резерв 800 при окне 1000 оставляет промпту ~200 токенов: промпт на 20 КБ
    // гарантированно урезается, и модель отвечает пустой строкой.
    const fcpp::SLlmRoleSet roles = make_roles(1000, 800);
    CRecordingLogger logger;
    std::vector<fcpp::ELlmRole> called_roles;

    fcpp::CLlmLayer layer(roles, logger,
        [&called_roles](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            called_roles.push_back(role);
            return std::make_unique<CReplyModel>(std::string{});
        });

    const fcpp::SLlmExecutionResult result =
        layer.request_text(fcpp::ELlmRole::CoderLocal, "You are a helper.", long_prompt(20000));

    REQUIRE_FALSE(result.m_ok);
    REQUIRE_FALSE(result.m_escalated);
    REQUIRE(result.m_error.find("did not fit the role context window") != std::string::npos);
    // Только первичная роль: повторять тот же сломанный промпт смысла нет.
    REQUIRE(called_roles.size() == 1);
    REQUIRE(called_roles.front() == fcpp::ELlmRole::CoderLocal);
}

TEST_CASE("llm layer still escalates when the prompt was not truncated",
          "[llm][budget][escalation]")
{
    // Окно большое, промпт влезает целиком: пустой ответ — это уже свойство
    // модели, и переход на резервную роль оправдан.
    const fcpp::SLlmRoleSet roles = make_roles(30000, 4096);
    CRecordingLogger logger;
    std::vector<fcpp::ELlmRole> called_roles;

    fcpp::CLlmLayer layer(roles, logger,
        [&called_roles](fcpp::ELlmRole role) -> std::unique_ptr<fcpp::IModelApi>
        {
            called_roles.push_back(role);
            return std::make_unique<CReplyModel>(std::string{});
        });

    const fcpp::SLlmExecutionResult result =
        layer.request_text(fcpp::ELlmRole::CoderLocal, "You are a helper.", long_prompt(1000));

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_escalated);
    REQUIRE(called_roles.size() == 2);
    REQUIRE(called_roles.front() == fcpp::ELlmRole::CoderLocal);
    REQUIRE(called_roles.back() == fcpp::ELlmRole::CoderSmart);

    bool logged_truncation = false;
    for (const std::string& message : logger.messages())
    {
        if (message == "llm.prompt_truncated")
        {
            logged_truncation = true;
        }
    }
    REQUIRE_FALSE(logged_truncation);
}
