#pragma once
#include "json.hpp"
#include "api_ai.h"
#include <memory>
#include <string>
#include <vector>

namespace cppagent {

using json = nlohmann::json;

// Роль main-модели (12 GiB) — генерация Plan/Execute, в отличие от light (8 GiB) которая только классифицирует.
// Интерфейс отделен чтобы Stub и Http реализации менялись без правок handlers/orchestrator.
struct GenerateRequest {
    std::string primitiveStr; // "SANDBOX_FIX" | "QUARRY_DESIGN" | ...
    std::string goalText;
    std::string file;
    std::string methodSignature;
    std::string methodUSR;
    json scaleFacts; // из blast_radius cross-TU
    std::string explicitMode;
    // Фрагмент файла вокруг курсора (нумерация строк + текст). Без него модель не может
    // выдать oldText, точно совпадающий с содержимым, — и выдумывает несуществующий текст.
    std::string fileContext;
};

struct GenerateResult {
    bool ok{false};
    std::string planText; // markdown/текст плана от модели
    json actions{json::array()}; // будущие file edits (пока пусто)
    std::string rationale;
    std::string error; // если !ok
    bool modelUnavailable{false};
};

class MainModelBroker {
public:
    virtual ~MainModelBroker() = default;
    virtual GenerateResult generate(const GenerateRequest& req) = 0;
    virtual std::string roleName() const = 0;
};

class StubMainModelBroker : public MainModelBroker {
public:
    GenerateResult generate(const GenerateRequest& req) override;
    std::string roleName() const override { return "stub-main"; }
};

// Реальная реализация: генерация через IApiAI (OpenAI-совместимый), как
// LightModelBroker, но с большим max_tokens.
class HttpMainModelBroker : public MainModelBroker {
public:
    explicit HttpMainModelBroker(std::string baseUrl, std::string modelName = "");
    // Инъекция произвольного провайдера (тесты — FakeApi; потом Claude/remote).
    explicit HttpMainModelBroker(std::shared_ptr<IApiAI> api, std::string modelName = "")
        : api_(std::move(api)), modelName_(std::move(modelName)) {}
    GenerateResult generate(const GenerateRequest& req) override;
    std::string roleName() const override { return "main"; }
private:
    std::shared_ptr<IApiAI> api_;
    std::string modelName_;
};

} // namespace cppagent
