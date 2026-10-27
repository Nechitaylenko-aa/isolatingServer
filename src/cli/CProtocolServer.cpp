/**
 * @file CProtocolServer.cpp
 * @brief Реализация протокольного диспетчера (JSON-RPC поверх stdio).
 */

#include "cli/CProtocolServer.h"

#include <iostream>
#include <cstdint>
#include <limits>
#include <string>

#include <nlohmann/json.hpp>

#include "fcpp-agent/ITool.h"

namespace fcpp
{

namespace
{

/**
 * @brief Сопоставить строковое имя метода протокола внутреннему EApiMethod.
 * @param name Имя метода (protocol-строка).
 * @param out Выходной метод.
 * @return true если метод распознан.
 */
bool method_from_name(const std::string& name, EApiMethod& out)
{
    if (name == "open")
    {
        out = EApiMethod::ProjectOpen;
        return true;
    }
    if (name == "close")
    {
        out = EApiMethod::ProjectClose;
        return true;
    }
    if (name == "status")
    {
        out = EApiMethod::ProjectStatus;
        return true;
    }
    if (name == "ask")
    {
        out = EApiMethod::Ask;
        return true;
    }
    if (name == "question.shrink")
    {
        out = EApiMethod::QuestionShrink;
        return true;
    }
    if (name == "session.open")
    {
        out = EApiMethod::SessionOpen;
        return true;
    }
    if (name == "session.resume")
    {
        out = EApiMethod::SessionResume;
        return true;
    }
    if (name == "session.close")
    {
        out = EApiMethod::SessionClose;
        return true;
    }
    if (name == "session.update_context")
    {
        out = EApiMethod::SessionUpdateContext;
        return true;
    }
    if (name == "project.profile.set")
    {
        out = EApiMethod::ProjectProfileSet;
        return true;
    }
    if (name == "config.reload")
    {
        out = EApiMethod::ConfigReload;
        return true;
    }
    if (name == "project.list")
    {
        out = EApiMethod::ProjectList;
        return true;
    }
    if (name == "tool")
    {
        out = EApiMethod::Tool;
        return true;
    }
    if (name == "patch")
    {
        out = EApiMethod::Patch;
        return true;
    }
    if (name == "index.status")
    {
        out = EApiMethod::IndexStatus;
        return true;
    }
    if (name == "index.refresh")
    {
        out = EApiMethod::IndexRefresh;
        return true;
    }
    if (name == "index.rebuild")
    {
        out = EApiMethod::IndexRebuild;
        return true;
    }
    if (name == "build" || name == "build.build")
    {
        out = EApiMethod::Build;
        return true;
    }
    if (name == "tests" || name == "tests.run")
    {
        out = EApiMethod::Tests;
        return true;
    }
    if (name == "doctor")
    {
        out = EApiMethod::Doctor;
        return true;
    }
    if (name == "agent.status")
    {
        out = EApiMethod::AgentStatus;
        return true;
    }
    if (name == "agent.capabilities")
    {
        out = EApiMethod::AgentCapabilities;
        return true;
    }
    return false;
}

enum class EExpectedJsonType
{
    String,
    Object,
    Boolean,
    Integer,
    UnsignedInteger
};

bool validate_optional_type(const nlohmann::json& object,
                            const std::string& key,
                            const std::string& prefix,
                            EExpectedJsonType expected,
                            std::string& out_error)
{
    if (!object.contains(key))
    {
        return true;
    }

    const nlohmann::json& value = object[key];
    bool valid = false;
    const char* expected_name = "valid value";
    switch (expected)
    {
        case EExpectedJsonType::String:
            valid = value.is_string();
            expected_name = "a string";
            break;
        case EExpectedJsonType::Object:
            valid = value.is_object();
            expected_name = "an object";
            break;
        case EExpectedJsonType::Boolean:
            valid = value.is_boolean();
            expected_name = "a boolean";
            break;
        case EExpectedJsonType::Integer:
            valid = value.is_number_integer();
            expected_name = "an integer";
            break;
        case EExpectedJsonType::UnsignedInteger:
            valid = value.is_number_unsigned();
            expected_name = "a non-negative 32-bit integer";
            break;
    }

    if (!valid)
    {
        out_error = "field '" + prefix + "." + key + "' must be " + expected_name;
        return false;
    }
    return true;
}

bool validate_uint16_field(const nlohmann::json& object,
                           const std::string& key,
                           const std::string& prefix,
                           std::string& out_error)
{
    if (!object.contains(key))
    {
        return true;
    }
    const nlohmann::json& value = object[key];
    if (!value.is_number_integer())
    {
        out_error = "field '" + prefix + "." + key + "' must be a non-zero 16-bit integer";
        return false;
    }

    std::uint64_t number = 0;
    if (value.is_number_unsigned())
    {
        number = value.get<std::uint64_t>();
    }
    else
    {
        const std::int64_t signed_number = value.get<std::int64_t>();
        if (signed_number <= 0)
        {
            out_error = "field '" + prefix + "." + key + "' must be a non-zero 16-bit integer";
            return false;
        }
        number = static_cast<std::uint64_t>(signed_number);
    }

    if (number == 0 || number > std::numeric_limits<std::uint16_t>::max())
    {
        out_error = "field '" + prefix + "." + key + "' is outside the valid 16-bit range";
        return false;
    }
    return true;
}

bool validate_uint32_field(const nlohmann::json& object,
                           const std::string& key,
                           const std::string& prefix,
                           std::string& out_error)
{
    if (!object.contains(key))
    {
        return true;
    }
    const nlohmann::json& value = object[key];
    if (!value.is_number_unsigned())
    {
        out_error = "field '" + prefix + "." + key + "' must be a non-negative 32-bit integer";
        return false;
    }
    if (value.get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max())
    {
        out_error = "field '" + prefix + "." + key + "' is outside the valid 32-bit range";
        return false;
    }
    return true;
}

} // namespace

CProtocolServer::CProtocolServer(const CConfigManager& config_manager,
                                 std::istream& input,
                                 std::ostream& output)
    : m_api(config_manager)
    , m_input(input)
    , m_output(output)
{
}

bool CProtocolServer::parse_request(const std::string& text,
                                    SApiRequest& out_request,
                                    std::string& out_id,
                                    std::string& out_error)
{
    out_request = SApiRequest{};
    out_id.clear();
    out_error.clear();

    nlohmann::json document;
    try
    {
        document = nlohmann::json::parse(text);
    }
    catch (const nlohmann::json::exception& error)
    {
        out_error = std::string("invalid json: ") + error.what();
        return false;
    }

    if (!document.is_object() || !document.contains("method"))
    {
        out_error = "missing field: method";
        return false;
    }

    if (!document.contains("id") || !document["id"].is_string()
            || document["id"].get<std::string>().empty())
    {
        out_error = "field 'id' must be a non-empty string";
        return false;
    }
    out_id = document["id"].get<std::string>();

    if (!document["method"].is_string())
    {
        out_error = "field 'method' must be a string";
        return false;
    }
    const std::string method_name = document["method"].get<std::string>();
    out_request.m_request_id = out_id;

    if (!method_from_name(method_name, out_request.m_method))
    {
        out_error = "unknown method: " + method_name;
        return false;
    }

    if (document.contains("params") && !document["params"].is_object())
    {
        out_error = "field 'params' must be an object";
        return false;
    }
    const nlohmann::json empty_params = nlohmann::json::object();
    const nlohmann::json& params = document.contains("params") ? document["params"] : empty_params;

    const struct SFieldTypeCheck
    {
        const char* m_key;
        EExpectedJsonType m_type;
    } field_checks[] = {
        {"projectRoot", EExpectedJsonType::String}, {"path", EExpectedJsonType::String},
        {"text", EExpectedJsonType::String}, {"answer", EExpectedJsonType::Object},
        {"question_id", EExpectedJsonType::Integer}, {"profile", EExpectedJsonType::String},
        {"session_id", EExpectedJsonType::String}, {"sessionId", EExpectedJsonType::String},
        {"projectId", EExpectedJsonType::String},
        {"tool", EExpectedJsonType::String}, {"target", EExpectedJsonType::String},
        {"payload", EExpectedJsonType::String}, {"limit", EExpectedJsonType::UnsignedInteger},
        {"assumeYes", EExpectedJsonType::Boolean}, {"buildAfter", EExpectedJsonType::Boolean},
        {"stream", EExpectedJsonType::Boolean}, {"editor", EExpectedJsonType::Object}
    };
    for (const SFieldTypeCheck& field : field_checks)
    {
        if (!validate_optional_type(params, field.m_key, "params", field.m_type, out_error))
        {
            return false;
        }
    }
    if (!validate_uint16_field(params, "question_id", "params", out_error)
            || !validate_uint32_field(params, "limit", "params", out_error))
    {
        return false;
    }
    if (params.contains("session_id") && params.contains("sessionId")
            && params["session_id"] != params["sessionId"])
    {
        out_error = "fields 'params.session_id' and 'params.sessionId' must match when both are provided";
        return false;
    }

    if (params.contains("answer"))
    {
        const nlohmann::json& answer = params["answer"];
        if (!validate_optional_type(answer, "question_id", "params.answer",
                                    EExpectedJsonType::Integer, out_error)
                || !validate_optional_type(answer, "option", "params.answer",
                                           EExpectedJsonType::String, out_error)
                || !validate_uint16_field(answer, "question_id", "params.answer", out_error))
        {
            return false;
        }
    }
    if (params.contains("editor"))
    {
        const nlohmann::json& editor = params["editor"];
        if (!validate_optional_type(editor, "file", "params.editor",
                                    EExpectedJsonType::Object, out_error)
                || !validate_optional_type(editor, "cursor", "params.editor",
                                           EExpectedJsonType::Object, out_error))
        {
            return false;
        }
        if (editor.contains("file")
                && !validate_optional_type(editor["file"], "path", "params.editor.file",
                                           EExpectedJsonType::String, out_error))
        {
            return false;
        }
        if (editor.contains("cursor"))
        {
            const nlohmann::json& cursor = editor["cursor"];
            if (!validate_optional_type(cursor, "line", "params.editor.cursor",
                                        EExpectedJsonType::UnsignedInteger, out_error)
                    || !validate_optional_type(cursor, "column", "params.editor.cursor",
                                               EExpectedJsonType::UnsignedInteger, out_error)
                    || !validate_uint32_field(cursor, "line", "params.editor.cursor", out_error)
                    || !validate_uint32_field(cursor, "column", "params.editor.cursor", out_error))
            {
                return false;
            }
        }
    }

    if (params.contains("projectRoot") && params["projectRoot"].is_string())
    {
        out_request.m_project_root = params["projectRoot"].get<std::string>();
    }
    if (params.contains("path") && params["path"].is_string())
    {
        out_request.m_path = params["path"].get<std::string>();
    }
    if (params.contains("text") && params["text"].is_string())
    {
        out_request.m_text = params["text"].get<std::string>();
    }
    if (params.contains("answer") && params["answer"].is_object())
    {
        const nlohmann::json& answer = params["answer"];
        if (answer.contains("question_id"))
        {
            const nlohmann::json& value = answer["question_id"];
            const std::uint64_t question_id = value.is_number_unsigned()
                ? value.get<std::uint64_t>()
                : static_cast<std::uint64_t>(value.get<std::int64_t>());
            out_request.m_answer_question_id = static_cast<std::uint16_t>(question_id);
        }
        if (answer.contains("option") && answer["option"].is_string())
        {
            out_request.m_selected_option = answer["option"].get<std::string>();
        }
    }
    if (params.contains("question_id"))
    {
        const nlohmann::json& value = params["question_id"];
        const std::uint64_t question_id = value.is_number_unsigned()
            ? value.get<std::uint64_t>()
            : static_cast<std::uint64_t>(value.get<std::int64_t>());
        out_request.m_question_id = static_cast<std::uint16_t>(question_id);
    }
    if (params.contains("profile") && params["profile"].is_string())
    {
        out_request.m_profile_text = params["profile"].get<std::string>();
    }
    if (params.contains("session_id") && params["session_id"].is_string())
    {
        out_request.m_session_id = params["session_id"].get<std::string>();
    }
    else if (params.contains("sessionId") && params["sessionId"].is_string())
    {
        out_request.m_session_id = params["sessionId"].get<std::string>();
    }
    if (params.contains("projectId") && params["projectId"].is_string())
    {
        out_request.m_project_id = params["projectId"].get<std::string>();
    }
    if (params.contains("context"))
    {
        out_request.m_context_json = params["context"].dump();
    }
    if (params.contains("tool") && params["tool"].is_string())
    {
        out_request.m_tool = params["tool"].get<std::string>();
    }
    if (params.contains("target") && params["target"].is_string())
    {
        out_request.m_target = params["target"].get<std::string>();
    }
    if (params.contains("payload") && params["payload"].is_string())
    {
        out_request.m_payload = params["payload"].get<std::string>();
    }
    if (params.contains("limit"))
    {
        out_request.m_limit = params["limit"].get<std::uint32_t>();
    }
    if (params.contains("assumeYes") && params["assumeYes"].is_boolean())
    {
        out_request.m_assume_yes = params["assumeYes"].get<bool>();
    }
    if (params.contains("buildAfter") && params["buildAfter"].is_boolean())
    {
        out_request.m_build_after = params["buildAfter"].get<bool>();
    }
    if (params.contains("stream") && params["stream"].is_boolean())
    {
        out_request.m_stream = params["stream"].get<bool>();
    }

    if (params.contains("editor") && params["editor"].is_object())
    {
        const nlohmann::json& editor = params["editor"];
        if (editor.contains("file") && editor["file"].is_object()
                && editor["file"].contains("path") && editor["file"]["path"].is_string())
        {
            out_request.m_editor_file = editor["file"]["path"].get<std::string>();
        }
        if (editor.contains("cursor") && editor["cursor"].is_object())
        {
            const nlohmann::json& cursor = editor["cursor"];
            if (cursor.contains("line"))
            {
                out_request.m_editor_line = cursor["line"].get<std::uint32_t>();
            }
            if (cursor.contains("column"))
            {
                out_request.m_editor_column = cursor["column"].get<std::uint32_t>();
            }
        }
    }

    const auto require_non_empty = [&](const std::string& value, const char* field) -> bool
    {
        if (!value.empty()) return true;
        out_error = std::string("missing or empty required field: ") + field;
        return false;
    };
    switch (out_request.m_method)
    {
        case EApiMethod::ProjectOpen:
            if (!require_non_empty(out_request.m_project_root, "params.projectRoot")) return false;
            break;
        case EApiMethod::Tool:
            if (!require_non_empty(out_request.m_tool, "params.tool")
                    || !require_non_empty(out_request.m_target, "params.target")) return false;
            break;
        case EApiMethod::Patch:
            if (!require_non_empty(out_request.m_payload, "params.payload")) return false;
            break;
        case EApiMethod::Ask:
            // Жёсткий контракт: диалог адресуется сессией. Без неё ответ не имеет
            // истории, поэтому запрос отклоняется, а не исполняется анонимно.
            if (!require_non_empty(out_request.m_session_id, "params.session_id"))
            {
                return false;
            }
            if (params.contains("answer"))
            {
                if (!out_request.m_text.empty())
                {
                    out_error = "params.text and params.answer are mutually exclusive";
                    return false;
                }
                if (out_request.m_answer_question_id == 0
                        || !require_non_empty(out_request.m_selected_option, "params.answer.option"))
                {
                    if (out_error.empty()) out_error = "params.answer requires a non-zero question_id";
                    return false;
                }
            }
            else if (!require_non_empty(out_request.m_text, "params.text"))
            {
                return false;
            }
            break;
        case EApiMethod::SessionClose:
            if (!require_non_empty(out_request.m_session_id, "params.session_id")) return false;
            break;
        case EApiMethod::SessionUpdateContext:
            if (!require_non_empty(out_request.m_session_id, "params.session_id")
                    || !params.contains("context"))
            {
                if (out_error.empty()) out_error = "params.session_id and params.context are required";
                return false;
            }
            break;
        case EApiMethod::ProjectProfileSet:
            if (!require_non_empty(out_request.m_profile_text, "params.profile")) return false;
            break;
        case EApiMethod::QuestionShrink:
            if (!require_non_empty(out_request.m_session_id, "params.session_id")
                    || out_request.m_question_id == 0)
            {
                if (out_error.empty()) out_error = "params.session_id and non-zero params.question_id are required";
                return false;
            }
            break;
        default:
            break;
    }

    return true;
}

std::string CProtocolServer::serialize_response(const std::string& id,
                                                const SApiResponse& response)
{
    nlohmann::json document;
    document["id"] = id;

    if (response.m_ok)
    {
        nlohmann::json result = nlohmann::json::object();
        if (!response.m_state.empty())
        {
            result["state"] = response.m_state;
        }
        else
        {
            result["state"] = "COMPLETED";
        }
        if (!response.m_project_id.empty())
        {
            result["project_id"] = response.m_project_id;
        }
        if (!response.m_session_id.empty())
        {
            result["session_id"] = response.m_session_id;
        }
        if (!response.m_text.empty())
        {
            result["text"] = response.m_text;
            // Для ask текст ответа дублируем в answer.text (стабильный контракт UI).
            result["answer"] = { {"text", response.m_text} };
        }
        if (response.m_has_question)
        {
            nlohmann::json question;
            question["wait_for_user"] = response.m_question.m_wait_for_user;
            question["questions"] = nlohmann::json::array();
            for (const SQuestion& item : response.m_question.m_questions)
            {
                question["questions"].push_back({
                    {"id", item.m_id},
                    {"text", item.m_text},
                    {"options", item.m_options},
                    {"and", item.m_and}
                });
            }
            result["question"] = question;
        }
        if (response.m_has_shrink_result)
        {
            result["shrunk"] = response.m_shrink_result;
        }
        if (!response.m_names.empty())
        {
            result["names"] = response.m_names;
        }
        if (!response.m_locations.empty())
        {
            nlohmann::json locations = nlohmann::json::array();
            for (const SSymbolLocation& location : response.m_locations)
            {
                locations.push_back({{"file", location.m_file},
                                     {"line", location.m_range.m_line},
                                     {"column", location.m_range.m_column}});
            }
            result["locations"] = locations;
        }
        if (!response.m_symbols.empty())
        {
            nlohmann::json symbols = nlohmann::json::array();
            for (const SSymbol& symbol : response.m_symbols)
            {
                symbols.push_back({{"kind", symbol_kind_name(symbol.m_kind)},
                                   {"name", symbol.m_qualified_name},
                                   {"signature", symbol.m_signature},
                                   {"usr", symbol.m_usr}});
            }
            result["symbols"] = symbols;
        }
        document["result"] = result;
    }
    else
    {
        document["error"] = {
            {"code", error_kind_name(response.m_error)},
            {"message", response.m_message},
            {"details", nlohmann::json::object()}
        };
        // Адрес диалога обязателен и в ошибке: иначе клиент теряет привязку
        // ответа к сессии при любом отказе.
        if (!response.m_session_id.empty())
        {
            document["error"]["session_id"] = response.m_session_id;
        }
    }

    return document.dump();
}

void CProtocolServer::handle_line(const std::string& request_line)
{
    if (request_line.find_first_not_of(" \t\r\n") == std::string::npos)
    {
        return;
    }

    SApiRequest request;
    std::string id;
    std::string parse_error;
    if (!parse_request(request_line, request, id, parse_error))
    {
        SApiResponse error_response;
        error_response.m_ok = false;
        error_response.m_error = EErrorKind::InvalidRequest;
        error_response.m_message = parse_error;
        m_output << serialize_response(id, error_response) << '\n';
        m_output.flush();
        return;
    }

    const SApiResponse response = m_api.execute(request);
    m_output << serialize_response(id, response) << '\n';
    m_output.flush();
}

bool CProtocolServer::open_project(const std::string& project_root)
{
    if (project_root.empty())
    {
        return false;
    }
    SApiRequest request;
    request.m_method = EApiMethod::ProjectOpen;
    request.m_project_root = project_root;
    const SApiResponse response = m_api.execute(request);
    return response.m_ok;
}

int CProtocolServer::run()
{
    // EOF на stdin (родитель закрыл pipe/вышел) завершает процесс — агент не
    // остаётся «сиротой». Это же даёт корректное завершение по Ctrl+D.
    std::string line;
    while (std::getline(m_input, line))
    {
        handle_line(line);
    }
    return 0;
}

} // namespace fcpp
