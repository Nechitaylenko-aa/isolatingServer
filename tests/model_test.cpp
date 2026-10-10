#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/CIntentAnalyzer.h"
#include "fcpp-agent/IModelApi.h"
#include "fcpp-agent/SConfig.h"
#include "fcpp-agent/SIntent.h"
#include "llm/CEndpointUtil.h"
#include "llm/CModelFactory.h"
#include "llm/CModelOpenAICompat.h"

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
