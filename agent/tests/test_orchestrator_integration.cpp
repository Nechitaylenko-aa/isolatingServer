#include <catch2/catch_test_macros.hpp>
#include "orchestrator.h"
#include "model_broker.h"
#include "main_model_broker.h"
#include "ground_service.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>

using namespace cppagent;
using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string readFile(const std::string& p){
    std::ifstream in(p); std::ostringstream ss; ss<<in.rdbuf(); return ss.str();
}
static void writeFile(const std::string& p, const std::string& c){
    std::ofstream out(p, std::ios::trunc); out<<c;
}

// Хелпер: создаёт минимальный C++ проект с compile_commands.json
// Fail-closed verify (контракт нарушен, пока сборка не подтвердит обратное) означает,
// что verify теперь реально зовёт cmake --build. Значит TestProject должен быть
// НАСТОЯЩИМ собираемым CMake-проектом, а не просто файлом + вручную написанным
// compile_commands.json без build-каталога — иначе verify честно проваливается
// (buildDir есть, но там нет CMakeCache) и answer превращается в error.
struct TestProject {
    fs::path root;      // buildDir — сюда CMake кладёт compile_commands.json и кэш
    fs::path srcFile;
    fs::path ccPath;
    
    TestProject() {
        root = fs::temp_directory_path() / ("cpp_tool_integ_" + std::to_string(::rand()));
        fs::create_directories(root);
        srcFile = root / "test.cpp";
        ccPath = root / "compile_commands.json";

        writeFile(srcFile.string(),
            "#include <vector>\n"
            "class Calculator {\n"
            "public:\n"
            "    int add(int a, int b) {\n"
            "        return a + b;\n"
            "    }\n"
            "    int multiply(int a, int b) {\n"
            "        return a * b;\n"
            "    }\n"
            "};\n");

        writeFile((root / "CMakeLists.txt").string(),
            "cmake_minimum_required(VERSION 3.10)\n"
            "project(cpp_tool_integ_test)\n"
            "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n"
            "set(CMAKE_CXX_STANDARD 17)\n"
            "add_library(t STATIC test.cpp)\n");

        // Конфигурация генерирует compile_commands.json сама — с корректными
        // директорией/флагами, которые реально соберутся.
        std::string cmd = "cmake -S " + root.string() + " -B " + root.string() + " -G Ninja >/dev/null 2>&1";
        std::system(cmd.c_str());
    }
    
    ~TestProject() {
        fs::remove_all(root);
    }
};

// Брокер, эмулирующий лежащую модель: classifyIntent всегда modelUnavailable.
// Нужен, чтобы покрыть fallback-путь (модель offline -> stub / needs_input).
class DeadModelBroker : public ModelBroker {
public:
    PrimitiveGuess classifyIntent(const std::string&) override {
        return PrimitiveGuess{Primitive::UNDERSTAND, "", "сеть: Connection refused (fake)", true};
    }
    std::string roleName() const override { return "light"; }
};

TEST_CASE("Orchestrator: UNDERSTAND with Stub model", "[orchestrator][stub]"){
    TestProject proj;
    StubModelBroker model;
    Orchestrator orch(model, proj.ccPath.string());
    
    json task = {
        {"goal_text", "Что делает метод add?"},
        {"cursor", {
            {"file", proj.srcFile.string()},
            {"line", 4},
            {"column", 9}
        }}
    };
    
    json outcome = orch.run(task);
    
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "UNDERSTAND");
    CHECK(outcome["answered_by"].value("role", "") == "stub");
    CHECK(outcome["answer"]["text"].get<std::string>().find("add") != std::string::npos);
}

TEST_CASE("Orchestrator: SANDBOX_FIX with Stub models", "[orchestrator][stub]"){
    TestProject proj;
    StubModelBroker model;
    StubMainModelBroker mainModel;
    Orchestrator orch(model, proj.ccPath.string(), &mainModel);
    
    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {
            {"file", proj.srcFile.string()},
            {"line", 4},
            {"column", 9}
        }},
        // Однофайловый тестовый проект триггерит relative blast_radius threshold
        // (1 файл из 1 = 100% > 25%) — это ожидаемо и корректно для маленьких проектов,
        // поэтому явно поднимаем порог для этого юнит-теста.
        {"blast_radius_relative_threshold", 1.1}
    };
    
    json outcome = orch.run(task);
    
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "SANDBOX_FIX");
    CHECK(outcome.contains("execute"));
    CHECK(outcome.contains("verify"));
    CHECK(outcome.contains("checkpoint"));
}

TEST_CASE("Orchestrator: SANDBOX_FIX on tiny project passes without confirmation (adaptive thresholds)", "[orchestrator][stub]"){
    // Адаптивные пороги: проект с 1 файлом и 1 ref НЕ должен триггерить защиту —
    // раньше фиксированный THR_REL=0.25 давал 100% >= 25% и требовал подтверждения
    // на каждой правке в маленьком проекте. Теперь tiny-проекты используют только
    // абсолютный порог (8 refs для <=5 файлов), который тут не достигнут.
    TestProject proj;
    StubModelBroker model;
    StubMainModelBroker mainModel;
    Orchestrator orch(model, proj.ccPath.string(), &mainModel);
    
    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {
            {"file", proj.srcFile.string()},
            {"line", 4},
            {"column", 9}
        }}
    };
    
    json outcome = orch.run(task);
    
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "SANDBOX_FIX");
    CHECK(outcome.contains("execute"));
}

TEST_CASE("Orchestrator: SANDBOX_FIX triggers blast_radius needs_input with explicit low thresholds", "[orchestrator][stub]"){
    // Механизм защиты с explicit-порогами: даже на 1-файловом проекте
    // можно принудительно заставить сработать needs_input для проверки цикла.
    TestProject proj;
    StubModelBroker model;
    Orchestrator orch(model, proj.ccPath.string());
    
    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {
            {"file", proj.srcFile.string()},
            {"line", 4},
            {"column", 9}
        }},
        {"blast_radius_threshold", 1},
        {"blast_radius_relative_threshold", 0.01}
    };
    
    json outcome = orch.run(task);
    
    REQUIRE(outcome.value("type", "") == "needs_input");
    CHECK(outcome["needs_input"].value("question_id", "") == "blast-radius-wide");
    
    // Подтверждаем через clarification -> proceed
    json task2 = task;
    task2["clarification"] = {
        {"question_id", "blast-radius-wide"},
        {"chosen_option_id", "proceed"}
    };
    json outcome2 = orch.run(task2);
    REQUIRE(outcome2.value("type", "") == "answer");
    CHECK(outcome2.value("primitive", "") == "SANDBOX_FIX");
}

TEST_CASE("Orchestrator: INFRA without cursor", "[orchestrator][stub]"){
    TestProject proj;
    StubModelBroker model;
    Orchestrator orch(model, proj.ccPath.string());
    
    json task = {
        {"goal_text", "Пересобери cmake"}
    };
    
    json outcome = orch.run(task);
    
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "INFRA");
    // INFRA не требует курсора
    CHECK(!outcome.contains("execute"));
}

TEST_CASE("Orchestrator: needs_input on low confidence (manual simulation)", "[orchestrator][stub]"){
    TestProject proj;
    StubModelBroker model;
    Orchestrator orch(model, proj.ccPath.string());
    
    // Эмулируем ситуацию низкой уверенности через self-consistency
    json task = {
        {"goal_text", "Попробуй исправить и покрой тестами"},
        {"cursor", {
            {"file", proj.srcFile.string()},
            {"line", 4},
            {"column", 9}
        }},
        {"classify_self_consistency", {
            {"repeat", 5},
            {"temperature", 0.7},
            {"minority_threshold", 0.3}
        }}
    };
    
    json outcome = orch.run(task);
    
    // С высоким minority_threshold и неоднозначной фразой должен спросить
    // (но Stub детерминирован, так что получим прямой ответ)
    // Этот тест проверяет что оркестратор не падает на self-consistency параметрах
    REQUIRE(outcome.contains("type"));
}

TEST_CASE("Orchestrator: blast_radius wide warning", "[orchestrator][stub]"){
    TestProject proj;
    StubModelBroker model;
    Orchestrator orch(model, proj.ccPath.string());
    
    json task = {
        {"goal_text", "Исправь метод add"},
        {"cursor", {
            {"file", proj.srcFile.string()},
            {"line", 4},
            {"column", 9}
        }},
        // Искусственно снижаем порог чтобы триггернуть предупреждение
        {"blast_radius_threshold", 1},
        {"blast_radius_relative_threshold", 0.01}
    };
    
    json outcome = orch.run(task);
    
    // Либо needs_input (если blast radius превысил порог), либо answer
    REQUIRE(outcome.contains("type"));
    std::string t = outcome.value("type", "");
    CHECK((t == "needs_input" || t == "answer"));
}

// === Тесты full clarification cycle ===

TEST_CASE("Orchestrator: clarification cycle blast_radius -> proceed -> answer", "[orchestrator][stub][clarify]"){
    TestProject proj;
    StubModelBroker model;
    StubMainModelBroker mainModel;
    Orchestrator orch(model, proj.ccPath.string(), &mainModel);

    // Шаг 1: низкий порог -> должен вернуть needs_input blast-radius-wide
    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"blast_radius_threshold", 1},
        {"blast_radius_relative_threshold", 0.01}
    };
    json step1 = orch.run(task);
    REQUIRE(step1.value("type", "") == "needs_input");
    REQUIRE(step1["needs_input"].value("question_id", "") == "blast-radius-wide");
    CHECK(step1["needs_input"].contains("options"));
    CHECK(step1["needs_input"].contains("history_entry"));
    CHECK(step1["needs_input"]["depth"].get<int>() == 1);

    // Шаг 2: отвечаем proceed -> должен дать answer SANDBOX_FIX
    json task2 = task;
    task2["clarification"] = {
        {"question_id", "blast-radius-wide"},
        {"chosen_option_id", "proceed"}
    };
    json step2 = orch.run(task2);
    REQUIRE(step2.value("type", "") == "answer");
    CHECK(step2.value("primitive", "") == "SANDBOX_FIX");
    CHECK(step2.contains("execute"));
    CHECK(step2.contains("checkpoint"));
    // clarification должен попасть в debug_facts
    if(step2.contains("debug_facts") && step2["debug_facts"].contains("clarification"))
        CHECK(step2["debug_facts"]["clarification"].value("question_id", "") == "blast-radius-wide");
}

TEST_CASE("Orchestrator: clarification cycle blast_radius -> cancel", "[orchestrator][stub][clarify]"){
    TestProject proj;
    StubModelBroker model;
    Orchestrator orch(model, proj.ccPath.string());

    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"blast_radius_threshold", 1},
        {"blast_radius_relative_threshold", 0.01}
    };
    // Отвечаем cancel
    task["clarification"] = {
        {"question_id", "blast-radius-wide"},
        {"chosen_option_id", "cancel"}
    };
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "canceled");
    CHECK(outcome["canceled"].value("question_id", "") == "blast-radius-wide");
}

TEST_CASE("Orchestrator: clarification cycle depth protection", "[orchestrator][stub][clarify]"){
    TestProject proj;
    StubModelBroker model;
    Orchestrator orch(model, proj.ccPath.string());

    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"blast_radius_threshold", 1},
        {"blast_radius_relative_threshold", 0.01}
    };
    // Набиваем history до лимита kMaxClarifyDepth=3
    task["clarification"] = {
        {"question_id", "blast-radius-wide"},
        {"chosen_option_id", "proceed"}
    };
    task["clarification_history"] = json::array({
        {{"question_id","blast-radius-wide"},{"chosen_option_id","proceed"},{"free_text",nullptr}},
        {{"question_id","classify-confidence-low"},{"chosen_option_id","SANDBOX_FIX"},{"free_text",nullptr}},
        {{"question_id","blast-radius-wide"},{"chosen_option_id","proceed"},{"free_text",nullptr}}
    });
    json outcome = orch.run(task);
    // При depth >= kMaxClarifyDepth должна быть ошибка
    REQUIRE(outcome.value("type", "") == "error");
    CHECK(outcome["error"].value("code", "") == "clarification_depth_exceeded");
}

TEST_CASE("Orchestrator: clarification replay protection", "[orchestrator][stub][clarify]"){
    TestProject proj;
    StubModelBroker model;
    Orchestrator orch(model, proj.ccPath.string());

    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"blast_radius_threshold", 1},
        {"blast_radius_relative_threshold", 0.01}
    };
    // question_id уже есть в history -> replay
    task["clarification"] = {
        {"question_id", "blast-radius-wide"},
        {"chosen_option_id", "proceed"}
    };
    task["clarification_history"] = json::array({
        {{"question_id","blast-radius-wide"},{"chosen_option_id","proceed"},{"free_text",nullptr}}
    });
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "error");
    CHECK(outcome["error"].value("code", "") == "clarification_replay");
}

TEST_CASE("Orchestrator: InfraHandler build with real buildDir", "[orchestrator][stub]"){
    // Проверяем что INFRA реально запускает cmake --build и возвращает лог
    StubModelBroker model;
    // compile_commands.json лежит рядом с cmake-build-debug
    std::string ccPath = "/home/artem/projects/outer/AI-agent/cpp-tool/cmake-build-debug/../cmake-build-debug/compile_commands.json";
    // Если файл не существует, пропускаем тест
    if(!std::filesystem::exists(ccPath)) {
        WARN("compile_commands.json not found at " << ccPath << ", skipping InfraHandler build test");
        return;
    }
    Orchestrator orch(model, ccPath);
    json task = {{"goal_text", "Пересобери cmake"}};
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "INFRA");
    REQUIRE(outcome["answer"].contains("infra"));
    auto& infra = outcome["answer"]["infra"];
    CHECK(infra.value("kind", "") == "build");
    CHECK(infra.contains("log"));
    CHECK(infra.contains("exit_code"));
    // Уже собран — cmake --build должен вернуть 0
    CHECK(infra.value("ok", false) == true);
}

TEST_CASE("Orchestrator: InfraHandler graph targets", "[orchestrator][stub]"){
    StubModelBroker model;
    std::string ccPath = "/home/artem/projects/outer/AI-agent/cpp-tool/cmake-build-debug/compile_commands.json";
    if(!std::filesystem::exists(ccPath)) {
        WARN("compile_commands.json not found, skipping InfraHandler graph test");
        return;
    }
    Orchestrator orch(model, ccPath);
    json task = {{"goal_text", "Покажи граф таргетов"}};
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "INFRA");
    auto& infra = outcome["answer"]["infra"];
    CHECK(infra.value("kind", "") == "graph");
    CHECK(!infra.value("log", "").empty());
}

// === Fallback при недоступной модели классификации ===

TEST_CASE("Orchestrator: dead model + INFRA phrase -> answer via stub (no missing_cursor)", "[orchestrator][stub][fallback]"){
    // Раньше: модель легла -> принудительный UNDERSTAND -> error missing_cursor
    // на фразе про сборку. Теперь INFRA берётся из stub молча — он read-only.
    TestProject proj;
    DeadModelBroker dead;
    Orchestrator orch(dead, proj.ccPath.string());
    json task = {{"goal_text", "Пересобери cmake"}};
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "INFRA");
    CHECK(outcome["answered_by"].value("role", "") == "deterministic_fallback");
}

TEST_CASE("Orchestrator: dead model + question -> answer via stub UNDERSTAND", "[orchestrator][stub][fallback]"){
    TestProject proj;
    DeadModelBroker dead;
    Orchestrator orch(dead, proj.ccPath.string());
    json task = {
        {"goal_text", "Что делает метод add?"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}}
    };
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "UNDERSTAND");
    CHECK(outcome["answered_by"].value("role", "") == "deterministic_fallback");
}

TEST_CASE("Orchestrator: dead model + write phrase -> needs_input model-unavailable", "[orchestrator][stub][fallback]"){
    // Запись при мёртвой модели молча не делаем: спрашиваем явно.
    TestProject proj;
    DeadModelBroker dead;
    StubMainModelBroker mainModel;
    Orchestrator orch(dead, proj.ccPath.string(), &mainModel);
    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}}
    };
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "needs_input");
    CHECK(outcome["needs_input"].value("question_id", "") == "model-unavailable");
    CHECK(outcome["needs_input"].contains("history_entry"));
    CHECK(outcome["needs_input"]["options"].size() >= 2);
    // Файл не тронут
    CHECK(readFile(proj.srcFile.string()).find("int add") != std::string::npos);
}

TEST_CASE("Orchestrator: dead model + clarification proceed -> writes", "[orchestrator][stub][fallback]"){
    TestProject proj;
    DeadModelBroker dead;
    StubMainModelBroker mainModel;
    Orchestrator orch(dead, proj.ccPath.string(), &mainModel);
    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"clarification", {{"question_id", "model-unavailable"}, {"chosen_option_id", "SANDBOX_FIX"}}}
    };
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "SANDBOX_FIX");
    CHECK(outcome.contains("execute"));
}

TEST_CASE("Orchestrator: dead model + clarification wait -> error model_unavailable_wait", "[orchestrator][stub][fallback]"){
    TestProject proj;
    DeadModelBroker dead;
    Orchestrator orch(dead, proj.ccPath.string());
    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"clarification", {{"question_id", "model-unavailable"}, {"chosen_option_id", "wait"}}}
    };
    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "error");
    CHECK(outcome["error"].value("code", "") == "model_unavailable_wait");
}

// === Тесты с реальными моделями (требуют запущенных llama.cpp серверов) ===
// Помечены тегом [live] чтобы можно было запускать отдельно

TEST_CASE("LightModelBroker: classify with real model", "[orchestrator][live][!mayfail]"){
    // Requires: llama.cpp server on http://127.0.0.1:8081
    LightModelBroker model("http://127.0.0.1:8081", "light", 0.1);
    
    auto guess = model.classifyIntent("Что делает этот метод?");
    
    // Если модель недоступна, тест не падает
    if (guess.modelUnavailable) {
        WARN("Light model unavailable: " << guess.rationale);
        return;
    }
    
    CHECK(guess.primitive == Primitive::UNDERSTAND);
    CHECK(guess.confidence > 0.5);
}

TEST_CASE("HttpMainModelBroker: generate plan with real model", "[orchestrator][live][!mayfail]"){
    // Requires: llama.cpp server on http://127.0.0.1:8082
    HttpMainModelBroker mainModel("http://127.0.0.1:8082", "main");
    
    GenerateRequest req;
    req.primitiveStr = "SANDBOX_FIX";
    req.goalText = "Замени int на long";
    req.file = "/tmp/test.cpp";
    req.methodSignature = "int add(int, int)";
    req.methodUSR = "c:@S@Calculator@F@add#I#I#";
    req.scaleFacts = {{"refs_found_total", 2}, {"scope", "file"}};
    
    auto result = mainModel.generate(req);
    
    if (result.modelUnavailable) {
        WARN("Main model unavailable: " << result.error);
        return;
    }
    
    REQUIRE(result.ok);
    CHECK(!result.planText.empty());
    CHECK(result.actions.is_array());
}

TEST_CASE("Orchestrator: full cycle UNDERSTAND with real light model", "[orchestrator][live][!mayfail]"){
    TestProject proj;
    LightModelBroker model("http://127.0.0.1:8081", "light", 0.1);
    Orchestrator orch(model, proj.ccPath.string());
    
    json task = {
        {"goal_text", "Что делает метод multiply?"},
        {"cursor", {
            {"file", proj.srcFile.string()},
            {"line", 7},
            {"column", 9}
        }}
    };
    
    json outcome = orch.run(task);
    
    if (outcome.value("type", "") == "error") {
        std::string err = outcome["error"].value("message", "unknown");
        if (err.find("модель недоступна") != std::string::npos || 
            err.find("unavailable") != std::string::npos) {
            WARN("Light model unavailable, skipping test");
            return;
        }
    }
    
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "UNDERSTAND");
    CHECK(outcome["answered_by"].value("role", "") == "light");
}

TEST_CASE("Orchestrator: full cycle SANDBOX_FIX with real models", "[orchestrator][live][!mayfail]"){
    TestProject proj;
    LightModelBroker model("http://127.0.0.1:8081", "light", 0.1);
    HttpMainModelBroker mainModel("http://127.0.0.1:8082", "main");
    Orchestrator orch(model, proj.ccPath.string(), &mainModel);
    
    json task = {
        {"goal_text", "Добавь const к методу add"},
        {"cursor", {
            {"file", proj.srcFile.string()},
            {"line", 4},
            {"column", 9}
        }},
        // Однофайловый тестовый проект: 1/1 = 100% > 25%, blast_radius всегда сработает.
        // Поднимаем порог чтобы тест проверял реальную генерацию, а не защиту.
        {"blast_radius_relative_threshold", 1.1}
    };
    
    json outcome = orch.run(task);
    
    if (outcome.value("type", "") == "error") {
        std::string err = outcome["error"].value("message", "unknown");
        if (err.find("модель недоступна") != std::string::npos || 
            err.find("unavailable") != std::string::npos) {
            WARN("Models unavailable, skipping test");
            return;
        }
    }
    
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "SANDBOX_FIX");
    CHECK(outcome.contains("execute"));
    CHECK(outcome.contains("verify"));
    CHECK(outcome["answered_by"].value("role", "") == "light");
}
// === TDD-красные кейсы: фиксируют дефекты ДО правки кода ===

// Тестовый брокер: возвращает PrimitiveGuess с управляемым goalIsSpecific.
// Stub всегда даёт true (дефолт), этот брокер нужен чтобы проверить false-путь.
class TestClassifyBroker : public ModelBroker {
    Primitive prim_;
    bool goalSpecific_;
public:
    TestClassifyBroker(Primitive p, bool gs) : prim_(p), goalSpecific_(gs) {}
    PrimitiveGuess classifyIntent(const std::string&) override {
        PrimitiveGuess g{prim_, primitiveToString(prim_), "тестовый брокер: управляемое goalIsSpecific", false};
        g.goalIsSpecific = goalSpecific_;
        return g;
    }
    std::string roleName() const override { return "test_classify"; }
};

// Стаб main-модели, у которой "упал" вызов: ok=false, валидных actions нет.
class FailingMainModelBroker : public MainModelBroker {
public:
    GenerateResult generate(const GenerateRequest&) override {
        GenerateResult r;
        r.ok = false;
        r.error = "main model: connection refused (fake)";
        r.modelUnavailable = true;
        r.actions = json::array();
        return r;
    }
    std::string roleName() const override { return "main"; }
};

TEST_CASE("Orchestrator: failed main plan must not produce green verify", "[orchestrator][stub][planfail]"){
    TestProject proj;
    StubModelBroker model;
    FailingMainModelBroker mainModel;
    Orchestrator orch(model, proj.ccPath.string(), &mainModel);

    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"blast_radius_relative_threshold", 1.1}
    };

    json outcome = orch.run(task);

    // План не сгенерирован -> правок нет -> зелёного verify быть не должно.
    REQUIRE(outcome.value("type", "") == "error");
    CHECK(outcome["error"].value("code", "") == "main_plan_failed");
    CHECK(readFile(proj.srcFile.string()).find("int add") != std::string::npos);
}

TEST_CASE("Orchestrator: INFRA must report failure on real non-zero build", "[orchestrator][stub][infrafail]"){
    fs::path root = fs::temp_directory_path() / ("cpp_tool_infra_fail_" + std::to_string(::rand()));
    fs::create_directories(root);
    writeFile((root / "compile_commands.json").string(), "[]");
    writeFile((root / "build.ninja").string(),
        "rule boom\n"
        "  command = echo boom && exit 1\n"
        "  description = failing step\n"
        "\n"
        "build dummy: boom\n");

    StubModelBroker model;
    Orchestrator orch(model, (root / "compile_commands.json").string());
    json outcome = orch.run(json{{"goal_text", "Пересобери cmake"}});

    REQUIRE(outcome.value("type", "") == "answer");
    REQUIRE(outcome["answer"].contains("infra"));
    auto& infra = outcome["answer"]["infra"];
    CHECK(infra.value("kind", "") == "build");
    // Сейчас exit_code берётся от tail (всегда 0), ошибка вне окна не видна -> красный.
    CHECK(infra.value("exit_code", 0) != 0);
    CHECK(infra.value("ok", true) == false);

    fs::remove_all(root);
}

TEST_CASE("primitiveFromString: model casing/whitespace are tolerated", "[model][unit]"){
    bool ok = false;
    CHECK(primitiveFromString("sandbox_fix", ok) == Primitive::SANDBOX_FIX);
    CHECK(ok);
    CHECK(primitiveFromString(" INFRA ", ok) == Primitive::INFRA);
    CHECK(ok);
    CHECK(primitiveFromString("Quarry_Design", ok) == Primitive::QUARRY_DESIGN);
    CHECK(ok);
    primitiveFromString("NONSENSE", ok);
    CHECK(!ok);
}

// === Якорь курсора на USR: переживает сдвиг строк ===

// === UTF-8: toLower для кириллицы (регрессия: std::tolower не понижал А-Я) ===

TEST_CASE("StubModelBroker: capitalized Cyrillic reading verb -> UNDERSTAND", "[orchestrator][stub][utf8]"){
    // Раньше toLower использовал std::tolower по байтам: "Проверь" (0xD0 0x9F...)
    // не понижалось, ключ "проверь" не находился, и Stub не распознавал чтение.
    StubModelBroker model;

    auto g1 = model.classifyIntent("Проверь корректность метода add");
    CHECK(g1.primitive == Primitive::UNDERSTAND);

    auto g2 = model.classifyIntent("Убедись, что реализация верна");
    CHECK(g2.primitive == Primitive::UNDERSTAND);

    auto g3 = model.classifyIntent("Дойди до реализации в обоих классах");
    CHECK(g3.primitive == Primitive::UNDERSTAND);
}

TEST_CASE("StubModelBroker: capitalized Cyrillic write verb still writes", "[orchestrator][stub][utf8]"){
    // Обратная сторона: "Исправь" (заглавная) должна остаться write-примитивом.
    StubModelBroker model;
    auto g = model.classifyIntent("Исправь выход за границы в методе add");
    CHECK(g.primitive == Primitive::SANDBOX_FIX);
}

// === Вето-слой: явный запрет правок перебивает write-примитив ===

TEST_CASE("Orchestrator: explicit 'без правок' vetoes write primitive -> UNDERSTAND", "[orchestrator][stub][veto]"){
    TestProject proj;
    // Брокер уверенно говорит SANDBOX_FIX (как Light на "проверь методы X и Y"),
    // но в цели явно сказано "без правок".
    TestClassifyBroker model(Primitive::SANDBOX_FIX, true);
    Orchestrator orch(model, proj.ccPath.string());

    json task = {
        {"goal_text", "Проверь метод add: корректно ли реализован. Без правок"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}}
    };

    json outcome = orch.run(task);

    // Вето понижает до UNDERSTAND: запись не производится.
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "UNDERSTAND");
    CHECK(!outcome.contains("execute"));
    CHECK(!outcome.contains("checkpoint"));
    // Файл не тронут.
    CHECK(readFile(proj.srcFile.string()).find("int add") != std::string::npos);
}

TEST_CASE("Orchestrator: 'только проверь' vetoes write primitive", "[orchestrator][stub][veto]"){
    TestProject proj;
    TestClassifyBroker model(Primitive::SANDBOX_FIX, true);
    Orchestrator orch(model, proj.ccPath.string());

    json task = {
        {"goal_text", "Только проверь корректность метода add, ничего не меняй"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}}
    };

    json outcome = orch.run(task);
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "UNDERSTAND");
    CHECK(!outcome.contains("execute"));
}

TEST_CASE("Orchestrator: write goal WITHOUT no-edit phrase is not vetoed", "[orchestrator][stub][veto]"){
    TestProject proj;
    TestClassifyBroker model(Primitive::SANDBOX_FIX, true);
    StubMainModelBroker mainModel;
    Orchestrator orch(model, proj.ccPath.string(), &mainModel);

    json task = {
        {"goal_text", "Замени int на long в методе add"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"blast_radius_relative_threshold", 1.1}
    };

    json outcome = orch.run(task);
    // Обычная правка не задета вето-слоем.
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "SANDBOX_FIX");
    CHECK(outcome.contains("execute"));
}

// === Проверка goalIsSpecific: needs_input на расплывчатую цель для write-примитива ===

TEST_CASE("Orchestrator: vague goal for write primitive -> needs_input goal-vague", "[orchestrator][stub][goalspec]"){
    TestProject proj;
    // Брокер говорит: это SANDBOX_FIX (правка), но goalIsSpecific=false.
    TestClassifyBroker model(Primitive::SANDBOX_FIX, false);
    Orchestrator orch(model, proj.ccPath.string());

    json task = {
        {"goal_text", "сделай лучше"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}}
    };

    json outcome = orch.run(task);

    // Должен вернуть needs_input с вопросом goal-vague.
    REQUIRE(outcome.value("type", "") == "needs_input");
    CHECK(outcome["needs_input"].value("question_id", "") == "goal-vague");
    CHECK(outcome["needs_input"].contains("text"));
    CHECK(outcome["needs_input"].contains("options"));
    CHECK(outcome["needs_input"]["depth"].get<int>() == 1);
    // Файл не тронут.
    CHECK(readFile(proj.srcFile.string()).find("int add") != std::string::npos);
}

TEST_CASE("Orchestrator: vague goal answered -> goal_text replaced and proceeds", "[orchestrator][stub][goalspec]"){
    TestProject proj;
    TestClassifyBroker model(Primitive::SANDBOX_FIX, false);
    StubMainModelBroker mainModel;
    Orchestrator orch(model, proj.ccPath.string(), &mainModel);

    json task = {
        {"goal_text", "сделай лучше"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}},
        {"blast_radius_relative_threshold", 1.1},
        {"clarification", {{"question_id", "goal-vague"}, {"free_text", "Замени int на long в методе add"}}}
    };

    json outcome = orch.run(task);

    // Должен дать answer с SANDBOX_FIX: цель заменена, правка прошла.
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "SANDBOX_FIX");
    CHECK(outcome.contains("execute"));
    // Уточнённая цель попала в debug_facts.
    if(outcome.contains("debug_facts") && outcome["debug_facts"].contains("clarification"))
        CHECK(outcome["debug_facts"]["clarification"].value("question_id", "") == "goal-vague");
}

TEST_CASE("Orchestrator: vague goal for UNDERSTAND -> no blocking", "[orchestrator][stub][goalspec]"){
    TestProject proj;
    // UNDERSTAND + goalIsSpecific=false — не блокируем (чтение не опасно).
    TestClassifyBroker model(Primitive::UNDERSTAND, false);
    Orchestrator orch(model, proj.ccPath.string());

    json task = {
        {"goal_text", "объясни это"},
        {"cursor", {{"file", proj.srcFile.string()}, {"line", 4}, {"column", 9}}}
    };

    json outcome = orch.run(task);

    // Должен вернуть answer, не needs_input.
    REQUIRE(outcome.value("type", "") == "answer");
    CHECK(outcome.value("primitive", "") == "UNDERSTAND");
}

TEST_CASE("GroundService: USR anchor survives line shifts", "[orchestrator][stub][anchor]"){
    TestProject proj;
    GroundService gs(proj.ccPath.string());

    // 1) Берём USR метода add через обычный locate (курсор на теле метода).
    GroundResult g0 = gs.buildGroundLight(proj.srcFile.string(), 5, 9);
    REQUIRE(g0.ok);
    REQUIRE(!g0.enclosingMethod.is_null());
    std::string usr = g0.enclosingMethod.value("usr", "");
    REQUIRE(!usr.empty());
    int line0 = g0.enclosingMethod.value("line", 0);
    REQUIRE(line0 > 0);

    // Якорь разрешается в те же координаты, что и locate.
    json c0 = gs.resolveCursorByUsr(proj.srcFile.string(), usr);
    REQUIRE(c0.value("ok", false));
    CHECK(c0.value("line", 0) == line0);

    // 2) Сдвигаем файл на 3 строки вниз — сохранённый line теперь указывает не туда.
    std::string body = readFile(proj.srcFile.string());
    writeFile(proj.srcFile.string(), "// pad1\n// pad2\n// pad3\n" + body);

    // Старая координата протухла: locate туда больше не попадает в метод.
    GroundResult gone = gs.buildGroundLight(proj.srcFile.string(), line0, 9);
    bool staleLost = gone.enclosingMethod.is_null() ||
                     gone.enclosingMethod.value("usr", "") != usr;
    CHECK(staleLost);

    // А якорь даёт АКТУАЛЬНУЮ строку: сдвиг на +3.
    json c1 = gs.resolveCursorByUsr(proj.srcFile.string(), usr);
    REQUIRE(c1.value("ok", false));
    CHECK(c1.value("line", 0) == line0 + 3);

    // 3) Символ исчез из файла -> явная ошибка, а не тихий промах в "?".
    writeFile(proj.srcFile.string(), "// nothing here\n");
    json c2 = gs.resolveCursorByUsr(proj.srcFile.string(), usr);
    CHECK(!c2.value("ok", true));
    CHECK(c2["error"].value("code", "") == "usr_not_found");
}
