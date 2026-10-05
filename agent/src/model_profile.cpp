#include "model_profile.h"

#include "hash.h"
#include "json.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace cppagent {

using json = nlohmann::json;

namespace {
std::string homeDir() {
    const char* h = std::getenv("HOME");
    return h ? h : "";
}

// 5 канареек: просим строгий JSON со схемой — считаем долю валидных.
const char* kJsonProbePrompts[] = {
    "Верни строго JSON {\"ok\": true} и ничего больше.",
    "Верни строго JSON {\"n\": 3} и ничего больше.",
    "Верни строго JSON {\"items\": [1,2,3]} и ничего больше.",
    "Верни строго JSON {\"name\": \"x\"} и ничего больше.",
    "Верни строго JSON {\"a\": {\"b\": 1}} и ничего больше.",
};
constexpr int kJsonProbeCount = 5;
} // namespace

std::string ModelProfile::defaultDir() {
    return homeDir() + "/.cf_agent/models";
}

ModelProfile ModelProfile::load(const std::string& dir, const std::string& model) {
    ModelProfile p;
    p.model = model;
    if (dir.empty() || model.empty()) return p;
    fs::path f = fs::path(dir) / (cpptool::hash_string(model) + ".profile");
    std::ifstream in(f);
    if (!in) return p;
    try {
        json j; in >> j;
        p.probed = j.value("probed", false);
        p.contextTokens = j.value("context_tokens", 0);
        p.jsonCompliance = j.value("json_compliance", 0.0);
        p.capableCoding = j.value("capable_coding", false);
        p.capableReasoning = j.value("capable_reasoning", false);
        p.verdict = j.value("verdict", "");
        p.source = j.value("source", "");
    } catch (...) { /* битый профиль — считаем непробованным */ }
    return p;
}

static void save(const std::string& dir, const ModelProfile& p) {
    if (dir.empty() || p.model.empty()) return;
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path f = fs::path(dir) / (cpptool::hash_string(p.model) + ".profile");
    json j = {
        {"model", p.model}, {"probed", p.probed}, {"context_tokens", p.contextTokens},
        {"json_compliance", p.jsonCompliance}, {"capable_coding", p.capableCoding},
        {"capable_reasoning", p.capableReasoning}, {"verdict", p.verdict}, {"source", p.source}
    };
    std::ofstream out(f, std::ios::trunc);
    out << j.dump(2);
}

ModelProfile ModelProfile::probe(IApiAI& model, const std::string& modelName,
                                 const std::string& dir, IApiAI* remote) {
    ModelProfile p;
    p.model = modelName;
    p.probed = true;

    int valid = 0, answered = 0;
    for (int i = 0; i < kJsonProbeCount; ++i) {
        ChatRequest req;
        req.system = "Ты возвращаешь строго валидный JSON без markdown.";
        req.user = kJsonProbePrompts[i];
        req.model = modelName;
        req.temperature = 0.0;
        req.maxTokens = 64;
        req.wantJson = true;
        ChatResult r = model.chat(req);
        if (!r.ok) continue;
        ++answered;
        try { json::parse(r.text); ++valid; } catch (...) {}
    }
    p.jsonCompliance = answered > 0 ? double(valid) / double(answered) : 0.0;

    // Пороги (эвристика; если есть remote — он переопределит вердикт).
    p.capableCoding = p.jsonCompliance >= 0.6 && answered >= 3;
    p.capableReasoning = answered >= 3;

    if (remote) {
        // Сильнейший игрок (remote) выносит вердикт по измерениям.
        json facts = {
            {"model", modelName}, {"json_compliance", p.jsonCompliance},
            {"answered", answered}, {"probe_count", kJsonProbeCount}
        };
        ChatRequest req;
        req.system =
            "Ты классифицируешь возможности ЛОКАЛЬНОЙ модели по результатам пробы. "
            "Верни строго JSON {\"capable_coding\": bool, \"capable_reasoning\": bool, \"verdict\": \"кратко\"}.";
        req.user = "Результаты пробы: " + facts.dump();
        req.temperature = 0.0;
        req.maxTokens = 200;
        req.wantJson = true;
        ChatResult rr = remote->chat(req);
        if (rr.ok) {
            try {
                json v = json::parse(rr.text);
                p.capableCoding = v.value("capable_coding", p.capableCoding);
                p.capableReasoning = v.value("capable_reasoning", p.capableReasoning);
                p.verdict = v.value("verdict", "");
                p.source = "remote";
            } catch (...) { p.source = "heuristic"; }
        } else {
            p.source = "heuristic";
        }
    } else {
        p.source = "heuristic";
    }
    if (p.verdict.empty())
        p.verdict = p.capableCoding ? "тянет кодинг" : "слабый JSON/инструкции — только справка";

    save(dir, p);
    return p;
}

} // namespace cppagent
