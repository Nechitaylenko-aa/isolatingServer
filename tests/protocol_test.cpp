#include <catch2/catch_test_macros.hpp>

#include <string>

#include <nlohmann/json.hpp>

#include "cli/CProtocolServer.h"

TEST_CASE("protocol maps the session context payload", "[protocol][conversation]")
{
    fcpp::SApiRequest request;
    std::string id;
    std::string error;

    // decisions.md №20: session.open/resume/close are gone — the address of
    // the dialogue belongs to the agent (project.open creates and returns
    // it), not to a client-driven lifecycle. session.update_context is the
    // one session.* method left: it mutates the one session a process has.
    REQUIRE(fcpp::CProtocolServer::parse_request(
        R"({"id":"context-1","method":"session.update_context","params":{"sessionId":"s-1","context":{"goal":"inspect","open_questions":["provenance"]}}})",
        request, id, error));
    REQUIRE(id == "context-1");
    REQUIRE(request.m_method == fcpp::EApiMethod::SessionUpdateContext);
    REQUIRE(request.m_session_id == "s-1");
    const nlohmann::json context = nlohmann::json::parse(request.m_context_json);
    REQUIRE(context["goal"] == "inspect");
    REQUIRE(context["open_questions"][0] == "provenance");
}

TEST_CASE("protocol parses question shrink requests", "[protocol][chat]")
{
    fcpp::SApiRequest request;
    std::string id;
    std::string error;

    REQUIRE(fcpp::CProtocolServer::parse_request(
        R"({"id":"shrink-1","method":"question.shrink","params":{"session_id":"s-1","question_id":42}})",
        request, id, error));
    REQUIRE(request.m_method == fcpp::EApiMethod::QuestionShrink);
    REQUIRE(request.m_session_id == "s-1");
    REQUIRE(request.m_question_id == 42);
}

TEST_CASE("protocol rejects non-string method instead of throwing", "[protocol]")
{
    fcpp::SApiRequest request;
    std::string id;
    std::string error;

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"bad-1","method":42})", request, id, error));
    REQUIRE(error == "field 'method' must be a string");
}

TEST_CASE("protocol response preserves conversation session identifier", "[protocol][conversation]")
{
    fcpp::SApiResponse response;
    response.m_ok = true;
    response.m_state = "READY";
    response.m_session_id = "session-123";

    const nlohmann::json serialized =
        nlohmann::json::parse(fcpp::CProtocolServer::serialize_response("req-123", response));
    REQUIRE(serialized["id"] == "req-123");
    REQUIRE(serialized["result"]["session_id"] == "session-123");
    REQUIRE(serialized["result"]["state"] == "READY");
}

TEST_CASE("protocol maps project profile setter", "[protocol][profile]")
{
    fcpp::SApiRequest request;
    std::string id;
    std::string error;

    REQUIRE(fcpp::CProtocolServer::parse_request(
        R"({"id":"profile-1","method":"project.profile.set","params":{"profile":"C++20; keep the public API stable"}})",
        request, id, error));
    REQUIRE(id == "profile-1");
    REQUIRE(request.m_method == fcpp::EApiMethod::ProjectProfileSet);
    REQUIRE(request.m_profile_text == "C++20; keep the public API stable");
}

TEST_CASE("protocol rejects malformed parameter types and non-object params", "[protocol][validation]")
{
    fcpp::SApiRequest request;
    std::string id;
    std::string error;

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"bad-params","method":"status","params":[]})", request, id, error));
    REQUIRE(error == "field 'params' must be an object");

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"bad-text","method":"ask","params":{"text":42}})", request, id, error));
    REQUIRE(error == "field 'params.text' must be a string");

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"bad-stream","method":"ask","params":{"text":"hello","stream":"yes"}})",
        request, id, error));
    REQUIRE(error == "field 'params.stream' must be a boolean");

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"bad-cursor","method":"ask","params":{"text":"hello","editor":{"cursor":{"line":-1}}}})",
        request, id, error));
    REQUIRE(error == "field 'params.editor.cursor.line' must be a non-negative 32-bit integer");
}

TEST_CASE("protocol preserves a decoded session id when later ask validation fails", "[protocol][validation][session]")
{
    fcpp::SApiRequest request;
    std::string id;
    std::string error;

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"bad-ask","method":"ask","params":{"session_id":"session-kept"}})",
        request, id, error));
    REQUIRE(request.m_session_id == "session-kept");

    fcpp::SApiResponse response;
    response.m_ok = false;
    response.m_error = fcpp::EErrorKind::InvalidRequest;
    response.m_message = error;
    response.m_session_id = request.m_session_id;
    const nlohmann::json document = nlohmann::json::parse(
        fcpp::CProtocolServer::serialize_response(id, response));
    REQUIRE(document["error"]["session_id"] == "session-kept");
}

TEST_CASE("protocol accepts ask with or without a client-supplied session id",
          "[protocol][validation][session]")
{
    // decisions.md №20: session_id — необязательный флаг клиента, не параметр
    // адресации. Агент сам резолвит активную сессию открытого проекта;
    // анонимный ask (без session_id вовсе) — обычный, валидный запрос.
    fcpp::SApiRequest request;
    std::string id;
    std::string error;

    REQUIRE(fcpp::CProtocolServer::parse_request(
        R"({"id":"anon-ask","method":"ask","params":{"text":"что делает этот метод?"}})",
        request, id, error));
    REQUIRE(request.m_session_id.empty());

    REQUIRE(fcpp::CProtocolServer::parse_request(
        R"({"id":"session-ask","method":"ask","params":{"session_id":"s-1","text":"что делает этот метод?"}})",
        request, id, error));
    REQUIRE(request.m_session_id == "s-1");
}

TEST_CASE("protocol rejects missing required fields and invalid identifiers", "[protocol][validation]")
{
    fcpp::SApiRequest request;
    std::string id;
    std::string error;

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"missing-root","method":"open","params":{}})", request, id, error));
    REQUIRE(error == "missing or empty required field: params.projectRoot");

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"missing-target","method":"tool","params":{"tool":"refs"}})", request, id, error));
    REQUIRE(error == "missing or empty required field: params.target");

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"missing-session","method":"session.update_context","params":{"context":{}}})",
        request, id, error));
    REQUIRE(error == "missing or empty required field: params.session_id");

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":"too-large","method":"question.shrink","params":{"session_id":"s","question_id":70000}})",
        request, id, error));
    REQUIRE(error == "field 'params.question_id' is outside the valid 16-bit range");

    REQUIRE_FALSE(fcpp::CProtocolServer::parse_request(
        R"({"id":7,"method":"status"})", request, id, error));
    REQUIRE(error == "field 'id' must be a non-empty string");

    REQUIRE(fcpp::CProtocolServer::parse_request(
        R"({"id":"status-no-params","method":"status"})", request, id, error));
}

TEST_CASE("protocol never serializes Ok for a failed response", "[protocol][errors]")
{
    fcpp::SApiResponse response;
    response.m_ok = false;
    response.m_error = fcpp::EErrorKind::Ok;
    response.m_message.clear();
    response.m_session_id = "session-preserved";

    const nlohmann::json document = nlohmann::json::parse(
        fcpp::CProtocolServer::serialize_response("failed-ask", response));
    REQUIRE(document.contains("error"));
    REQUIRE(document["error"]["code"] == "NotUnderstood");
    REQUIRE_FALSE(document["error"]["message"].get<std::string>().empty());
    REQUIRE(document["error"]["session_id"] == "session-preserved");
    REQUIRE_FALSE(document.contains("result"));
}

TEST_CASE("protocol preserves NotUnderstood as a typed failure", "[protocol][errors]")
{
    fcpp::SApiResponse response;
    response.m_ok = false;
    response.m_error = fcpp::EErrorKind::NotUnderstood;
    response.m_message = "request not understood";

    const nlohmann::json document = nlohmann::json::parse(
        fcpp::CProtocolServer::serialize_response("not-understood", response));
    REQUIRE(document["error"]["code"] == "NotUnderstood");
    REQUIRE(document["error"]["message"] == "request not understood");
}
