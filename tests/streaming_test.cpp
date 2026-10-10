#include <catch2/catch_test_macros.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/CAgentApi.h"
#include "core/CConfigManager.h"
#include "fcpp-agent/IAgentApi.h"
#include "fcpp-agent/IModelApi.h"
#include "llm/CModelOpenAICompat.h"

namespace
{

struct SMockHttpResponse
{
    std::string m_body;
    std::string m_content_type = "application/json";
    std::size_t m_extra_content_length = 0;
};

int g_stream_test_counter = 0;

class CTempDir
{
public:
    explicit CTempDir(const std::string& prefix)
        : m_path(std::filesystem::temp_directory_path()
                 / (prefix + "-" + std::to_string(static_cast<long>(::getpid()))
                    + "-" + std::to_string(g_stream_test_counter++)))
    {
        std::filesystem::create_directories(m_path);
    }

    ~CTempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    const std::filesystem::path& path() const
    {
        return m_path;
    }

    CTempDir(const CTempDir&) = delete;
    CTempDir& operator=(const CTempDir&) = delete;

private:
    std::filesystem::path m_path;
};

class CHomeScope
{
public:
    explicit CHomeScope(const std::filesystem::path& home)
    {
        const char* previous = std::getenv("HOME");
        if (previous != nullptr)
        {
            m_previous = previous;
            m_had_previous = true;
        }
        REQUIRE(::setenv("HOME", home.string().c_str(), 1) == 0);
    }

    ~CHomeScope()
    {
        if (m_had_previous)
        {
            ::setenv("HOME", m_previous.c_str(), 1);
        }
        else
        {
            ::unsetenv("HOME");
        }
    }

    CHomeScope(const CHomeScope&) = delete;
    CHomeScope& operator=(const CHomeScope&) = delete;

private:
    std::string m_previous;
    bool m_had_previous = false;
};

class CMockOpenAIServer
{
public:
    explicit CMockOpenAIServer(std::vector<SMockHttpResponse> responses)
        : m_responses(std::move(responses))
    {
        m_listener = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_listener < 0)
        {
            throw std::runtime_error("cannot create mock HTTP socket");
        }

        int reuse = 1;
        (void)::setsockopt(m_listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (::bind(m_listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
            || ::listen(m_listener, 8) != 0)
        {
            ::close(m_listener);
            m_listener = -1;
            throw std::runtime_error("cannot bind mock HTTP socket");
        }

        socklen_t address_size = sizeof(address);
        if (::getsockname(m_listener, reinterpret_cast<sockaddr*>(&address), &address_size) != 0)
        {
            ::close(m_listener);
            m_listener = -1;
            throw std::runtime_error("cannot inspect mock HTTP socket");
        }
        m_port = ntohs(address.sin_port);
        m_thread = std::thread([this]() { serve(); });
    }

    ~CMockOpenAIServer()
    {
        m_stopping.store(true);
        if (m_listener >= 0)
        {
            (void)::shutdown(m_listener, SHUT_RDWR);
        }
        if (m_thread.joinable())
        {
            m_thread.join();
        }
        if (m_listener >= 0)
        {
            ::close(m_listener);
        }
    }

    std::string base_url() const
    {
        return "http://127.0.0.1:" + std::to_string(m_port) + "/v1";
    }

    std::vector<std::string> requests() const
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_requests;
    }

    CMockOpenAIServer(const CMockOpenAIServer&) = delete;
    CMockOpenAIServer& operator=(const CMockOpenAIServer&) = delete;

private:
    static std::string read_request(int client)
    {
        std::string request;
        char buffer[4096];
        while (true)
        {
            const ssize_t received = ::recv(client, buffer, sizeof(buffer), 0);
            if (received <= 0)
            {
                break;
            }
            request.append(buffer, static_cast<std::size_t>(received));

            const std::size_t header_end = request.find("\r\n\r\n");
            if (header_end == std::string::npos)
            {
                continue;
            }

            std::size_t content_length = 0;
            const std::string headers = request.substr(0, header_end);
            const std::string marker = "Content-Length:";
            const std::size_t length_pos = headers.find(marker);
            if (length_pos != std::string::npos)
            {
                const std::size_t value_pos = length_pos + marker.size();
                content_length = static_cast<std::size_t>(
                    std::stoull(headers.substr(value_pos)));
            }
            if (request.size() >= header_end + 4U + content_length)
            {
                break;
            }
        }
        return request;
    }

    static void send_all(int client, const std::string& data)
    {
        std::size_t sent = 0;
        while (sent < data.size())
        {
            const ssize_t count = ::send(client, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
            if (count <= 0)
            {
                return;
            }
            sent += static_cast<std::size_t>(count);
        }
    }

    void serve()
    {
        for (const SMockHttpResponse& response : m_responses)
        {
            if (m_stopping.load())
            {
                return;
            }
            const int client = ::accept(m_listener, nullptr, nullptr);
            if (client < 0)
            {
                return;
            }

            const std::string request = read_request(client);
            {
                const std::lock_guard<std::mutex> lock(m_mutex);
                m_requests.push_back(request);
            }

            const std::size_t advertised_length =
                response.m_body.size() + response.m_extra_content_length;
            const std::string headers =
                "HTTP/1.1 200 OK\r\nContent-Type: " + response.m_content_type
                + "\r\nContent-Length: " + std::to_string(advertised_length)
                + "\r\nConnection: close\r\n\r\n";
            send_all(client, headers);
            send_all(client, response.m_body);
            (void)::shutdown(client, SHUT_RDWR);
            ::close(client);
        }
    }

    int m_listener = -1;
    std::uint16_t m_port = 0;
    std::vector<SMockHttpResponse> m_responses;
    mutable std::mutex m_mutex;
    std::vector<std::string> m_requests;
    std::atomic<bool> m_stopping{false};
    std::thread m_thread;
};

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void write_file(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::trunc);
    REQUIRE(output.is_open());
    output << content;
    output.close();
    REQUIRE(output.good());
}

void replace_config_value(std::string& content, const std::string& key, const std::string& value)
{
    std::istringstream input(content);
    std::ostringstream output;
    std::string line;
    bool replaced = false;
    while (std::getline(input, line))
    {
        if (line.rfind(key + "=", 0) == 0)
        {
            line = key + "=\"" + value + "\"";
            replaced = true;
        }
        output << line << '\n';
    }
    REQUIRE(replaced);
    content = output.str();
}

std::filesystem::path make_indexable_project(const std::filesystem::path& project)
{
    std::filesystem::create_directories(project / "src");
    std::filesystem::create_directories(project / "build");
    write_file(project / "CMakeLists.txt",
               "cmake_minimum_required(VERSION 3.20)\nproject(streaming_fixture LANGUAGES CXX)\n");
    write_file(project / "src" / "main.cpp", "int main() { return 0; }\n");
    write_file(project / "build" / "compile_commands.json",
               "[{\"directory\":\"" + project.string() + "\",\"file\":\""
               + (project / "src" / "main.cpp").string()
               + "\",\"arguments\":[\"clang++\",\"-std=c++20\",\"-c\",\""
               + (project / "src" / "main.cpp").string() + "\"]}]\n");
    return project;
}

std::string request_body(const std::string& request)
{
    const std::size_t separator = request.find("\r\n\r\n");
    REQUIRE(separator != std::string::npos);
    return request.substr(separator + 4U);
}

std::string join_chunks(const std::vector<std::string>& chunks)
{
    std::string joined;
    for (const std::string& chunk : chunks)
    {
        joined += chunk;
    }
    return joined;
}

const std::string k_initial_json =
    R"({"choices":[{"finish_reason":"stop","message":{"role":"assistant","content":"draft answer not for display"}}]})";

std::string sse_body(const std::string& first, const std::string& second)
{
    return "data: {\"choices\":[{\"delta\":{\"content\":\"" + first + "\"}}]}\n\n"
           "data: {\"choices\":[{\"delta\":{\"content\":\"" + second + "\"}}]}\n\n"
           "data: [DONE]\n\n";
}

struct SApiStreamResult
{
    bool m_ok = false;
    std::string m_answer;
    std::vector<std::string> m_chunks;
    std::vector<std::string> m_requests;
};

SApiStreamResult run_api_stream(const std::vector<SMockHttpResponse>& responses,
                                bool cancel_on_first_chunk = false)
{
    CMockOpenAIServer server(responses);
    CTempDir home("fcpp-stream-home");
    CHomeScope home_scope(home.path());

    fcpp::CConfigManager config;
    REQUIRE(config.bootstrap());
    std::string config_text = read_file(config.config_path());
    replace_config_value(config_text, "model_main", server.base_url());
    replace_config_value(config_text, "model_main_name", "integration-model");
    write_file(config.config_path(), config_text);
    REQUIRE(config.load());

    CTempDir project("fcpp-stream-project");
    (void)make_indexable_project(project.path());

    fcpp::CAgentApi api(config);
    fcpp::SApiRequest open;
    open.m_method = fcpp::EApiMethod::ProjectOpen;
    open.m_path = project.path().string();
    REQUIRE(api.execute(open).m_ok);

    SApiStreamResult result;
    fcpp::SApiRequest ask;
    ask.m_method = fcpp::EApiMethod::Ask;
    ask.m_text = "Explain the project";
    ask.m_stream = true;
    ask.m_on_stream_chunk = [&result, cancel_on_first_chunk](const std::string& chunk)
    {
        result.m_chunks.push_back(chunk);
        return !cancel_on_first_chunk;
    };

    const fcpp::SApiResponse response = api.execute(ask);
    result.m_ok = response.m_ok;
    result.m_answer = response.m_text;
    result.m_requests = server.requests();
    return result;
}

} // namespace

TEST_CASE("streaming integration: Agent API forwards live SSE chunks without leaking draft text",
          "[stream][integration]")
{
    const SApiStreamResult result = run_api_stream({
        {k_initial_json, "application/json", 0},
        {sse_body("Hello ", "world"), "text/event-stream", 0}
    });

    REQUIRE(result.m_ok);
    REQUIRE(result.m_answer == "Hello world");
    REQUIRE(join_chunks(result.m_chunks) == result.m_answer);
    REQUIRE(result.m_chunks.size() == 2);
    REQUIRE(join_chunks(result.m_chunks).find("draft answer") == std::string::npos);
    REQUIRE(result.m_requests.size() == 2);

    const nlohmann::json first = nlohmann::json::parse(request_body(result.m_requests[0]));
    const nlohmann::json second = nlohmann::json::parse(request_body(result.m_requests[1]));
    REQUIRE_FALSE(first["stream"].get<bool>());
    REQUIRE(first.contains("tools"));
    REQUIRE(second["stream"].get<bool>());
    REQUIRE_FALSE(second.contains("tools"));
}

TEST_CASE("streaming integration: cancellation stops the request and does not trigger fallback",
          "[stream][integration]")
{
    const SApiStreamResult result = run_api_stream({
        {k_initial_json, "application/json", 0},
        {sse_body("stop-here", "must-not-be-delivered"), "text/event-stream", 0}
    }, true);

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_chunks.size() == 1);
    REQUIRE(result.m_chunks[0] == "stop-here");
    REQUIRE(result.m_requests.size() == 2);
}

TEST_CASE("streaming integration: provider failure after first chunk never mixes a fallback answer",
          "[stream][integration]")
{
    const std::string partial =
        "data: {\"choices\":[{\"delta\":{\"content\":\"partial answer\"}}]}\n\n";
    const SApiStreamResult result = run_api_stream({
        {k_initial_json, "application/json", 0},
        {partial, "text/event-stream", 64}
    });

    REQUIRE_FALSE(result.m_ok);
    REQUIRE(result.m_chunks.size() == 1);
    REQUIRE(result.m_chunks[0] == "partial answer");
    REQUIRE(join_chunks(result.m_chunks).find("draft answer") == std::string::npos);
    REQUIRE(result.m_requests.size() == 2);
}

TEST_CASE("streaming integration: tool-enabled completion preserves tool_calls and buffers text",
          "[stream][integration][tools]")
{
    const std::string tool_reply =
        R"({"choices":[{"finish_reason":"tool_calls","message":{"role":"assistant","content":"","tool_calls":[{"id":"call-1","type":"function","function":{"name":"search","arguments":"{\"name\":\"Widget\"}"}}]}}]})";
    CMockOpenAIServer server({{tool_reply, "application/json", 0}});

    fcpp::CModelOpenAICompat concrete(
        fcpp::SModelEndpoint{server.base_url(), "integration-model", ""}, 0.2, 128);
    fcpp::IModelApi& model = concrete;

    fcpp::SModelMessage user;
    user.m_role = fcpp::EModelRole::User;
    user.m_content = "Find Widget";
    fcpp::SModelTool tool;
    tool.m_name = "search";
    tool.m_description = "Search the index";
    std::size_t callback_count = 0;

    const fcpp::SModelReply reply = model.complete_stream(
        {user}, {tool},
        [&callback_count](const std::string&)
        {
            ++callback_count;
            return true;
        });

    REQUIRE(reply.m_ok);
    REQUIRE(reply.m_tool_calls.size() == 1);
    REQUIRE(reply.m_tool_calls[0].m_id == "call-1");
    REQUIRE(reply.m_tool_calls[0].m_name == "search");
    REQUIRE(reply.m_tool_calls[0].m_arguments == R"({"name":"Widget"})");
    REQUIRE(callback_count == 0);
    REQUIRE(server.requests().size() == 1);
    const nlohmann::json request = nlohmann::json::parse(request_body(server.requests()[0]));
    REQUIRE_FALSE(request["stream"].get<bool>());
    REQUIRE(request.contains("tools"));
}
