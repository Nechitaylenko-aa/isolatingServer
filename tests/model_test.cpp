#include <catch2/catch_test_macros.hpp>
#include "core/CModelLevels.h"

#include <string>
#include <utility>
#include <vector>

#include "core/CIntentAnalyzer.h"
#include "core/CAnswerQuality.h"
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

TEST_CASE("answer quality: parses accepted low-risk assessment", "[model][quality]")
{
    fcpp::SAnswerQualityAssessment assessment;
    REQUIRE(fcpp::parse_answer_quality(
        R"({"verdict":"ACCEPT","risk_score":12,"reason":"claims are supported"})",
        assessment));
    REQUIRE(assessment.m_valid);
    REQUIRE(assessment.m_risk_score == 12);
    REQUIRE_FALSE(assessment.m_escalate);
}

TEST_CASE("answer quality: high risk escalates even when verdict says accept", "[model][quality]")
{
    fcpp::SAnswerQualityAssessment assessment;
    REQUIRE(fcpp::parse_answer_quality(
        R"({"verdict":"ACCEPT","risk_score":35,"reason":"insufficient evidence"})",
        assessment));
    REQUIRE(assessment.m_escalate);
}

TEST_CASE("answer quality: accepts only one strict JSON object", "[model][quality]")
{
    fcpp::SAnswerQualityAssessment assessment;
    const std::string valid =
        R"({"verdict":"ACCEPT","risk_score":12,"reason":"supported"})";

    REQUIRE(fcpp::parse_answer_quality(" \n" + valid + " \t", assessment));
    REQUIRE_FALSE(fcpp::parse_answer_quality("review: " + valid, assessment));
    REQUIRE_FALSE(fcpp::parse_answer_quality(valid + " trailing text", assessment));
    REQUIRE_FALSE(fcpp::parse_answer_quality(valid + " " + valid, assessment));
    REQUIRE_FALSE(fcpp::parse_answer_quality(
        R"({"verdict":"ACCEPT","risk_score":12,"reason":"supported","extra":true})",
        assessment));
    REQUIRE_FALSE(fcpp::parse_answer_quality(
        R"({"verdict":"ACCEPT","risk_score":12,"reason":"   "})", assessment));
}

TEST_CASE("answer quality: threshold boundary and explicit escalation are deterministic", "[model][quality]")
{
    fcpp::SAnswerQualityAssessment assessment;
    REQUIRE(fcpp::parse_answer_quality(
        R"({"verdict":"ACCEPT","risk_score":34,"reason":"below threshold"})", assessment));
    REQUIRE_FALSE(assessment.m_escalate);

    REQUIRE(fcpp::parse_answer_quality(
        R"({"verdict":"ACCEPT","risk_score":35,"reason":"at threshold"})", assessment));
    REQUIRE(assessment.m_escalate);

    REQUIRE(fcpp::parse_answer_quality(
        R"({"verdict":"ESCALATE","risk_score":0,"reason":"explicit escalation"})", assessment));
    REQUIRE(assessment.m_escalate);
}

TEST_CASE("answer quality: malformed assessment is rejected", "[model][quality]")
{
    fcpp::SAnswerQualityAssessment assessment;
    REQUIRE_FALSE(fcpp::parse_answer_quality("probably fine", assessment));
    REQUIRE_FALSE(fcpp::parse_answer_quality(
        R"({"verdict":"ACCEPT","risk_score":101,"reason":"out of range"})",
        assessment));
}

TEST_CASE("model levels: same_as_main collapses when the light model name is inherited", "[model][levels]")
{
    fcpp::SConfig config;
    config.m_model_main.m_url = "http://127.0.0.1:8080/v1";
    config.m_model_main.m_name = "main";
    config.m_model_light.m_url = "same_as_main";
    config.m_model_light.m_name.clear();

    const std::vector<fcpp::EModelLevel> levels = fcpp::available_model_levels(config);
    REQUIRE(levels.size() == 1);
    REQUIRE(levels.front() == fcpp::EModelLevel::Lite);
}

TEST_CASE("model levels: explicit light model name remains a distinct escalation level", "[model][levels]")
{
    fcpp::SConfig config;
    config.m_model_main.m_url = "http://127.0.0.1:8080/v1";
    config.m_model_main.m_name = "main";
    config.m_model_light.m_url = "same_as_main";
    config.m_model_light.m_name = "lite";

    const std::vector<fcpp::EModelLevel> levels = fcpp::available_model_levels(config);
    REQUIRE(levels.size() == 2);
    REQUIRE(levels[0] == fcpp::EModelLevel::Lite);
    REQUIRE(levels[1] == fcpp::EModelLevel::Main);
}

TEST_CASE("model levels: remote provider type participates in endpoint identity", "[model][levels]")
{
    fcpp::SConfig config;
    config.m_model_main.m_url = "http://127.0.0.1:8080/v1";
    config.m_model_main.m_name = "shared";
    config.m_model_light.m_url = "same_as_main";
    config.m_model_light.m_name.clear();
    config.m_model_remote.m_url = config.m_model_main.m_url;
    config.m_model_remote.m_name = config.m_model_main.m_name;
    config.m_model_remote.m_key = "test-key";
    config.m_remote_type = fcpp::EModelRemoteType::Claude;

    const std::vector<fcpp::EModelLevel> levels = fcpp::available_model_levels(config);
    REQUIRE(levels.size() == 2);
    REQUIRE(levels[0] == fcpp::EModelLevel::Lite);
    REQUIRE(levels[1] == fcpp::EModelLevel::Remote);
}

TEST_CASE("model levels: final candidate cannot pass review without a higher-level reviewer", "[model][levels][quality]")
{
    const std::vector<fcpp::EModelLevel> one_level = {fcpp::EModelLevel::Lite};
    REQUIRE_FALSE(fcpp::has_higher_review_level(one_level, fcpp::EModelLevel::Lite));

    const std::vector<fcpp::EModelLevel> two_levels = {
        fcpp::EModelLevel::Lite, fcpp::EModelLevel::Main
    };
    REQUIRE(fcpp::has_higher_review_level(two_levels, fcpp::EModelLevel::Lite));
    REQUIRE_FALSE(fcpp::has_higher_review_level(two_levels, fcpp::EModelLevel::Main));
    REQUIRE_FALSE(fcpp::has_higher_review_level({}, fcpp::EModelLevel::Main));
}
