#include <catch2/catch_test_macros.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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

std::string request_body(const std::string& request)
{
    const std::size_t separator = request.find("\r\n\r\n");
    REQUIRE(separator != std::string::npos);
    return request.substr(separator + 4U);
}


} // namespace

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

