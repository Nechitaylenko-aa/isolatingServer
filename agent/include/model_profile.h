#pragma once

#include <memory>
#include <string>
#include "api_ai.h"

namespace cppagent {

// Профиль возможностей модели — ДОБЫВАЕТСЯ измерением (проба), не хардкодится.
// Способности меняются при смене модели (Qwen-14B -> 30B-A3B и т.п.), поэтому
// профиль кэшируется на диске и инвалидируется по имени модели.
struct ModelProfile {
    std::string model;
    bool probed{false};
    int contextTokens{0};          // 0 = неизвестно
    double jsonCompliance{0.0};    // доля валидного строгого JSON на канарейках, 0..1
    bool capableCoding{false};     // тянет ли генерацию правок
    bool capableReasoning{false};  // тянет ли обзор/архитектурное суждение
    std::string verdict;           // краткий вывод (при недоступности remote — из чисел)
    std::string source;            // "remote" | "heuristic"

    // Каталог кэша: по умолчанию ~/.cf_agent/models.
    static std::string defaultDir();

    // Загрузить ранее сохранённый профиль (если есть).
    static ModelProfile load(const std::string& dir, const std::string& model);

    // Проба модели через провайдера; кэширует результат в <dir>/<hash(model)>.profile.
    // remoteAvailable/remote — если задан, вердикт выносит remote (сильнейший игрок),
    // иначе — детерминированные пороги по числам пробы.
    static ModelProfile probe(IApiAI& model, const std::string& modelName,
                              const std::string& dir,
                              IApiAI* remote = nullptr);

    bool operator==(const ModelProfile& o) const {
        return model == o.model && probed == o.probed &&
               contextTokens == o.contextTokens &&
               jsonCompliance == o.jsonCompliance &&
               capableCoding == o.capableCoding &&
               capableReasoning == o.capableReasoning;
    }
};

} // namespace cppagent
