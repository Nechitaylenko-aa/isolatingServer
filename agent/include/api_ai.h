#pragma once

#include <memory>
#include <string>

namespace cppagent {

// Провайдер-агностик интерфейс одной «чатовой» операции: дай system+user,
// получи текст. Именно это общее у OpenAI/Anthropic/Gemini; конкретный wire-
// формат (/v1/chat/completions у OpenAI-совместимых) — деталь реализации.
struct ChatRequest {
    std::string system;
    std::string user;
    std::string model;             // пусто = сервер сам выберет
    double temperature{0.1};
    int maxTokens{400};
    bool wantJson{false};          // просим строгий JSON (response_format)
    long connectTimeoutSec{5};
    long timeoutSec{600};
};

struct ChatResult {
    bool ok{false};                // true = получен текст ассистента
    std::string text;              // content ассистента
    std::string finishReason;      // "stop" | "length" | ...
    std::string error;             // человекочитаемая причина при !ok
};

// Абстракция провайдера AI-API. Расширяется новыми форматами (Claude и т.п.)
// без изменения ModelBroker/MainModelBroker.
class IApiAI {
public:
    virtual ~IApiAI() = default;
    virtual ChatResult chat(const ChatRequest& req) = 0;
    virtual std::string name() const = 0;   // "openai" | "anthropic" | ...
};

// OpenAI-совместимый API (llama.cpp, vLLM, Ollama, OpenAI, OpenRouter...).
// POST {baseUrl}/v1/chat/completions, Bearer-ключ (если задан), при wantJson —
// response_format=json_object с откатом на запрос без него.
class CApiOpenAI : public IApiAI {
public:
    explicit CApiOpenAI(std::string baseUrl, std::string modelName = "",
                        std::string apiKey = "",
                        long connectTimeoutSec = 5, long timeoutSec = 600);
    ChatResult chat(const ChatRequest& req) override;
    std::string name() const override { return "openai"; }

private:
    std::string baseUrl_;
    std::string modelName_;
    std::string apiKey_;
    long connectTimeoutSec_;
    long timeoutSec_;
};

// Вырезает первый сбалансированный {...} из текста, ПРОПУСКАЯ строковые
// литералы (rationale по-русски с { } ломал наивный подсчёт скобок). Общий
// util для разбора ответов моделей (используется обоими broker'ами).
std::string extractFirstJsonObject(const std::string& text);

} // namespace cppagent
