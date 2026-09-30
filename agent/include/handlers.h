#pragma once
#include "ground_service.h"
#include "model_broker.h"
#include "main_model_broker.h"
#include "json.hpp"
#include <string>
#include <vector>
#include <memory>

namespace cppagent {

using json = nlohmann::json;

struct HandlerContext {
    json task;
    PrimitiveGuess guess;
    std::string primitiveStr;
    std::string compileCommandsPath;
    std::vector<std::string> flags;
    GroundResult ground;
    std::string explicitMode;
    MainModelBroker* mainModel{nullptr}; // S4: 12 GiB генератор, может быть nullptr -> stub
};

class PrimitiveHandler {
public:
    virtual ~PrimitiveHandler() = default;
    virtual json handle(const HandlerContext& ctx) = 0;
    virtual std::string name() const = 0;
};

// Фабрика — по примитиву отдаёт нужный handler (все state-машины).
std::unique_ptr<PrimitiveHandler> makeHandler(Primitive p);

// 6 реализаций — пока заглушки для Plan/Execute/Verify, честно помечены TODO.
class UnderstandHandler   : public PrimitiveHandler { public: json handle(const HandlerContext&) override; std::string name() const override { return "UNDERSTAND"; } };
class SandboxFixHandler   : public PrimitiveHandler { public: json handle(const HandlerContext&) override; std::string name() const override { return "SANDBOX_FIX"; } };
class QuarryDesignHandler : public PrimitiveHandler { public: json handle(const HandlerContext&) override; std::string name() const override { return "QUARRY_DESIGN"; } };
class TestGenHandler      : public PrimitiveHandler { public: json handle(const HandlerContext&) override; std::string name() const override { return "TEST_GEN"; } };
class ExperimentHandler   : public PrimitiveHandler { public: json handle(const HandlerContext&) override; std::string name() const override { return "EXPERIMENT"; } };
class InfraHandler        : public PrimitiveHandler { public: json handle(const HandlerContext&) override; std::string name() const override { return "INFRA"; } };

} // namespace cppagent
