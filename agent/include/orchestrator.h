#pragma once
#include "json.hpp"
#include "model_broker.h"
#include "main_model_broker.h"
#include <string>

namespace cppagent {

using json = nlohmann::json;

// Первый честный срез оркестратора (см. ТЗ §8): Intake → Ground →
// (Clarify | Report). Plan/Execute/Verify сюда сознательно НЕ включены —
// без настоящей генерирующей модели это был бы театр, а не оркестрация.
//
// В отличие от предыдущей версии, здесь Ground вызывает функции слоя 0
// (cpptool::query*) НАПРЯМУЮ как C++-функции внутри того же процесса —
// без subprocess/JSON-по-stdout. core/ по-прежнему ничего не знает о
// моделях (никаких #include из agent/ в core/), а agent/ по-прежнему не
// линкует libclang сам — оба ограничения соблюдены, просто не через
// границу процесса, а через границу единиц компиляции в одном бинарнике.
class Orchestrator {
public:
    // mainModel — опционально: если nullptr, используется StubMainModelBroker внутри.
    // remoteMainModel — удалённая (сильнейшая) модель; если задана, тяжёлые классы
    // задач маршрутизируются на неё проактивно, без ожидания сбоя локальной.
    // localCapable — подтверждена ли (пробой) способность локальной main-модели.
    // FAIL-CLOSED: по умолчанию false, пока профиль не подтвердит обратное.
    explicit Orchestrator(ModelBroker& model, std::string compileCommandsPath,
                          MainModelBroker* mainModel = nullptr,
                          MainModelBroker* remoteMainModel = nullptr,
                          bool localCapable = false,
                          int localMaxEditFiles = 3,
                          bool allowRead = true, bool allowWrite = true, bool allowExec = false)
        : model_(model), compileCommandsPath_(std::move(compileCommandsPath)),
          mainModel_(mainModel), remoteMainModel_(remoteMainModel),
          localCapable_(localCapable), localMaxEditFiles_(localMaxEditFiles),
          allowRead_(allowRead), allowWrite_(allowWrite), allowExec_(allowExec) {}

    // task — объект "task" из хода клиента (см. ТЗ §7.1)
    json run(const json& task);

private:
    ModelBroker& model_;
    std::string compileCommandsPath_;
    MainModelBroker* mainModel_{nullptr};
    MainModelBroker* remoteMainModel_{nullptr};
    bool localCapable_{false};        // fail-closed: не способна, пока не подтверждено
    int localMaxEditFiles_{3};
    bool allowRead_{true};            // политики доступа (agent.conf)
    bool allowWrite_{true};
    bool allowExec_{false};
};

} // namespace cppagent
