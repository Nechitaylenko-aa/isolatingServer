#include <catch2/catch_test_macros.hpp>
#include "api_ai.h"
#include "model_broker.h"
#include "main_model_broker.h"

#include <memory>

using namespace cppagent;

namespace {
// Провайдер для тестов: отдаёт заранее заданный текст, запоминает последний запрос.
class FakeApi : public IApiAI {
public:
    std::string cannedText;
    std::string finish = "stop";
    bool ok = true;
    std::string error;
    ChatRequest last;

    ChatResult chat(const ChatRequest& req) override {
        last = req;
        ChatResult r;
        r.ok = ok;
        r.text = cannedText;
        r.finishReason = finish;
        r.error = error;
        return r;
    }
    std::string name() const override { return "fake"; }
};
}

TEST_CASE("extractFirstJsonObject skips braces inside strings", "[api]") {
    // rationale по-русски с { } внутри строки не должен ломать баланс.
    std::string s = "prefix {\"primitive\":\"UNDERSTAND\",\"rationale\":\"текст { с } скобками\"} tail";
    std::string ext = extractFirstJsonObject(s);
    REQUIRE(!ext.empty());
    CHECK(ext.front() == '{');
    CHECK(ext.back() == '}');
    CHECK(ext.find("скобками") != std::string::npos);
    CHECK(extractFirstJsonObject("без скобок").empty());
}

TEST_CASE("LightModelBroker parses classifier JSON via injected IApiAI", "[api]") {
    auto fake = std::make_shared<FakeApi>();
    fake->cannedText = R"({"primitive":"SANDBOX_FIX","detail":"LOCAL_FIX","cursor_binding":"NEEDED_PRESENT",
                          "confidence":"0.9","goal_specific":true,"rationale":"правка метода"})";
    LightModelBroker broker(fake, 0.1, "");

    PrimitiveGuess g = broker.classifyIntent("Замени int на long в методе add");
    CHECK(!g.modelUnavailable);
    CHECK(g.primitive == Primitive::SANDBOX_FIX);
    CHECK(g.detail == Detail::LOCAL_FIX);
    CHECK(g.cursorBinding == CursorBinding::NEEDED_PRESENT);
    CHECK(g.confidence == 0.9); // строка "0.9" -> число
    // broker должен был попросить JSON у провайдера
    CHECK(fake->last.wantJson == true);
}

TEST_CASE("LightModelBroker tolerates non-JSON and reports unavailable", "[api]") {
    auto fake = std::make_shared<FakeApi>();
    fake->cannedText = "извините, не могу";
    LightModelBroker broker(fake, 0.1, "");
    PrimitiveGuess g = broker.classifyIntent("что-то");
    CHECK(g.modelUnavailable);
}

TEST_CASE("LightModelBroker surfaces provider error as unavailable", "[api]") {
    auto fake = std::make_shared<FakeApi>();
    fake->ok = false;
    fake->error = "сеть: connection refused";
    LightModelBroker broker(fake, 0.1, "");
    PrimitiveGuess g = broker.classifyIntent("что-то");
    CHECK(g.modelUnavailable);
    CHECK(g.rationale.find("connection refused") != std::string::npos);
}

TEST_CASE("HttpMainModelBroker parses plan/actions via injected IApiAI", "[api]") {
    auto fake = std::make_shared<FakeApi>();
    fake->cannedText = R"({"plan":"шаги плана","rationale":"ок",
                          "actions":[{"oldText":"int add","newText":"long add"},{"noise":true}]})";
    HttpMainModelBroker broker(fake, "");

    GenerateRequest req;
    req.primitiveStr = "SANDBOX_FIX";
    req.goalText = "Замени int на long";
    req.file = "/tmp/x.cpp";
    GenerateResult r = broker.generate(req);
    REQUIRE(r.ok);
    CHECK(r.planText == "шаги плана");
    REQUIRE(r.actions.is_array());
    CHECK(r.actions.size() == 1); // action без oldText/newText отфильтрован
    CHECK(r.actions[0].value("newText", "") == "long add");
    CHECK(fake->last.wantJson == true);
}

TEST_CASE("HttpMainModelBroker reports provider down", "[api]") {
    auto fake = std::make_shared<FakeApi>();
    fake->ok = false;
    fake->error = "HTTP 500";
    HttpMainModelBroker broker(fake, "");
    GenerateRequest req; req.primitiveStr = "SANDBOX_FIX";
    GenerateResult r = broker.generate(req);
    CHECK(!r.ok);
    CHECK(r.modelUnavailable);
}
