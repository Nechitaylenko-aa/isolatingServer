/**
 * @file CProtocolServer.cpp
 * @brief Реализация протокольного диспетчера (JSON-RPC поверх stdio).
 */

#include "cli/CProtocolServer.h"

#include <iostream>
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

    if (document.contains("id") && document["id"].is_string())
    {
        out_id = document["id"].get<std::string>();
    }

    const std::string method_name = document["method"].get<std::string>();
    if (!method_from_name(method_name, out_request.m_method))
    {
        out_error = "unknown method: " + method_name;
        return false;
    }

    if (!document.contains("params") || !document["params"].is_object())
    {
        // Методы без параметров допустимы (например, status).
        return true;
    }

    const nlohmann::json& params = document["params"];

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
    if (params.contains("limit") && params["limit"].is_number_unsigned())
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
            if (cursor.contains("line") && cursor["line"].is_number_unsigned())
            {
                out_request.m_editor_line = cursor["line"].get<std::uint32_t>();
            }
            if (cursor.contains("column") && cursor["column"].is_number_unsigned())
            {
                out_request.m_editor_column = cursor["column"].get<std::uint32_t>();
            }
        }
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
        if (!response.m_text.empty())
        {
            result["text"] = response.m_text;
            // Для ask текст ответа дублируем в answer.text (стабильный контракт UI).
            result["answer"] = { {"text", response.m_text} };
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
