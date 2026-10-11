#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

#include "core/CIntentAnalyzer.h"
#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/SConfig.h"
#include "fcpp-agent/SIntent.h"
#include "llm/CEndpointUtil.h"
#include "llm/CModelFactory.h"
#include "llm/CModelOpenAICompat.h"


namespace
{

class CFixedReplyModel final : public fcpp::IModelApi
{
public:
    explicit CFixedReplyModel(std::string text)
        : m_text(std::move(text))
    {
    }

    bool is_available() const override { return true; }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>&) override
    {
        return fcpp::SModelReply{true, m_text, {}, "stop", {}};
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>&,
                               const std::vector<fcpp::SModelTool>&) override
    {
        const std::vector<fcpp::SModelMessage> empty_messages;
        return complete(empty_messages);
    }

    fcpp::SModelReply complete_stream(const std::vector<fcpp::SModelMessage>&,
                                      const fcpp::SStreamCallback&) override
    {
        const std::vector<fcpp::SModelMessage> empty_messages;
        return complete(empty_messages);
    }

private:
    std::string m_text;
};

} // namespace

TEST_CASE("intent analyzer: general engineering scope does not require a subject",
          "[model][intent]")
{
    const std::string xml =
        "<intent><scope>GENERAL_ENGINEERING</scope><area>DESIGN</area>"
        "<relation>SELF</relation><projection>EXPLANATION</projection>"
        "<subject kind=\"SYMBOL\" name=\"\"/></intent>";
    fcpp::SIntent intent;
    std::string error;

    REQUIRE(fcpp::CIntentAnalyzer::parse_intent_xml(xml, intent, error));
    REQUIRE(intent.m_scope == fcpp::ERequestScope::GeneralEngineering);
    REQUIRE(intent.m_subject.m_name.empty());
}

TEST_CASE("intent analyzer: deictic reference falls back to cursor symbol when model omits name",
          "[model][intent]")
{
    const std::string xml =
        "<intent><area>UNDERSTAND</area><relation>SELF</relation>"
        "<projection>EXPLANATION</projection>"
        "<subject kind=\"CLASS\" name=\"\"/></intent>";
    CFixedReplyModel model(xml);
    fcpp::SIntent intent;
    std::string error;

    REQUIRE(fcpp::CIntentAnalyzer{}.analyze(
        model, "Объясни этот класс", "[cursor symbol: fcpp::CWidget (CLASS)]",
        intent, error));

    REQUIRE(intent.m_subject.m_name == "fcpp::CWidget");
    REQUIRE(intent.m_subject.m_kind == fcpp::ESubjectKind::Class);
    REQUIRE(intent.m_subject.m_selection == fcpp::ESubjectSelection::Cursor);
}

TEST_CASE("intent analyzer: global question does not inherit cursor symbol",
          "[model][intent]")
{
    const std::string xml =
        "<intent><area>UNDERSTAND</area><relation>SELF</relation>"
        "<projection>EXPLANATION</projection>"
        "<subject kind=\"SYMBOL\" name=\"\"/></intent>";
    CFixedReplyModel model(xml);
    fcpp::SIntent intent;
    std::string error;

    REQUIRE(fcpp::CIntentAnalyzer{}.analyze(
        model, "Объясни архитектуру проекта", "[cursor symbol: fcpp::CWidget (CLASS)]",
        intent, error));

    REQUIRE(intent.m_subject.m_name.empty());
    REQUIRE(intent.m_subject.m_selection == fcpp::ESubjectSelection::Context);
}

TEST_CASE("endpoint: base_url до v1 + путь api", "[model]")
{
    REQUIRE(fcpp::CEndpointUtil::join("https://routerai.ru/api/v1", "/chat/completions")
            == "https://routerai.ru/api/v1/chat/completions");
    REQUIRE(fcpp::CEndpointUtil::join("https://routerai.ru/api/v1/", "chat/completions")
            == "https://routerai.ru/api/v1/chat/completions");
    REQUIRE(fcpp::CEndpointUtil::join("https://api.anthropic.com/v1", "/messages")
            == "https://api.anthropic.com/v1/messages");
}

TEST_CASE("intent analyzer: разбор XML", "[model]")
{
    const std::string xml =
        "<intent>\n"
        "  <area>UNDERSTAND</area>\n"
        "  <relation>CALLERS</relation>\n"
        "  <projection>LOCATIONS</projection>\n"
        "  <subject kind=\"METHOD\" name=\"ModelProfile::load\"/>\n"
        "</intent>";

    fcpp::SIntent intent;
    std::string error;
    REQUIRE(fcpp::CIntentAnalyzer::parse_intent_xml(xml, intent, error));
    REQUIRE(intent.m_area == fcpp::EArea::Understand);
    REQUIRE(intent.m_relation == fcpp::ERelation::Callers);
    REQUIRE(intent.m_projection == fcpp::EProjection::Locations);
    REQUIRE(intent.m_subject.m_kind == fcpp::ESubjectKind::Method);
    REQUIRE(intent.m_subject.m_name == "ModelProfile::load");
    REQUIRE(intent.m_effect == fcpp::EEffect::ReadOnly); // выведено детерминированно
}

TEST_CASE("intent analyzer: некорректный XML отклоняется", "[model]")
{
    fcpp::SIntent intent;
    std::string error;
    REQUIRE_FALSE(fcpp::CIntentAnalyzer::parse_intent_xml("<intent>garbage</intent>", intent, error));
    REQUIRE_FALSE(error.empty());
}

TEST_CASE("model factory: light same_as_main наследует main", "[model]")
{
    fcpp::SConfig config;
    config.m_model_main.m_url = "http://127.0.0.1:8080/v1";
    config.m_model_main.m_name = "qwen";
    config.m_model_light.m_url = "same_as_main";

    std::unique_ptr<fcpp::IModelApi> light = fcpp::CModelFactory::create(config, fcpp::EModelRoleKind::Light);
    REQUIRE(light != nullptr);
    REQUIRE(light->is_available());

    // Remote без имени модели недоступен (не сконфигурирован).
    fcpp::SConfig empty_config;
    std::unique_ptr<fcpp::IModelApi> remote = fcpp::CModelFactory::create(empty_config, fcpp::EModelRoleKind::Remote);
    REQUIRE(remote != nullptr);
    REQUIRE_FALSE(remote->is_available());
}
