/**
 * @file file_route_test.cpp
 * @brief Файл без позиции курсора — отдельный маршрут дерева.
 */

#include "core/CCursorContextAnalyzer.h"
#include "core/CLogger.h"
#include "core/CRequestOrchestrator.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fcpp-agent/IClang.h"
#include "fcpp-agent/IModelApi.h"

namespace
{

/** @brief Модель с фиксированным ответом на YES/NO и отдельным ответом на запрос. */
class CFixedAnswerModel final : public fcpp::IModelApi
{
public:
    explicit CFixedAnswerModel(std::string yes_no, std::string answer = "answer")
        : m_yes_no(std::move(yes_no))
        , m_answer(std::move(answer))
    {
    }

    bool is_available() const override
    {
        return true;
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>& messages) override
    {
        const std::string system =
            messages.empty() ? std::string{} : messages.front().m_content;
        if (system.find("concerns the supplied file") != std::string::npos)
        {
            return fcpp::SModelReply{true, m_yes_no, {}, "stop", {}};
        }
        return fcpp::SModelReply{true, m_answer, {}, "stop", {}};
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>& messages,
                               const std::vector<fcpp::SModelTool>&) override
    {
        return complete(messages);
    }

    fcpp::SModelReply complete_stream(const std::vector<fcpp::SModelMessage>& messages,
                                      const fcpp::SStreamCallback&) override
    {
        return complete(messages);
    }

private:
    std::string m_yes_no;
    std::string m_answer;
};

/** @brief Бэкенд clang, который не находит сущность (позиция не задана). */
class CNoSymbolClang final : public fcpp::IClang
{
public:
    bool is_available() const override
    {
        return true;
    }

    std::string version() const override
    {
        return "test";
    }

    bool extract(const std::filesystem::path&, const std::vector<std::string>&,
                 fcpp::SClangExtract&) const override
    {
        return true;
    }

    std::optional<fcpp::SClangSymbol> cursor_at(
        const std::filesystem::path&, std::uint32_t, std::uint32_t,
        const std::vector<std::string>&,
        fcpp::EFocusKind = fcpp::EFocusKind::Innermost) const override
    {
        return std::nullopt;
    }
};

/** @brief Создать временный каталог. */
std::filesystem::path make_temp_dir(const std::string& name)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
    std::filesystem::create_directories(dir, ignored);
    return dir;
}

/** @brief Записать файл с заданным содержимым. */
void write_file(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream output(path);
    REQUIRE(output.is_open());
    output << content;
}

} // namespace

TEST_CASE("file without cursor position is a separate route", "[file][route]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-file-route");
    const std::filesystem::path file = dir / "notes.md";
    write_file(file, "# Notes\n\nThis document describes the build steps.\n");

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Error);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CFixedAnswerModel>("YES", "file answer");
        });

    CNoSymbolClang clang;
    fcpp::CCursorContextAnalyzer analyzer(clang, llm);
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "file-route-yes";
    request.m_text = "What is described in this document?";
    request.m_cursor_file = file.string();
    // No line/column: the editor sent a file, not a position.

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::FileContextRequired);
    REQUIRE(result.m_message == "file answer");
    REQUIRE_FALSE(result.m_file_context.empty());
    REQUIRE(result.m_file_context.find("build steps") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("file judged irrelevant is reported, not answered generically",
          "[file][route]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-file-route-no");
    const std::filesystem::path file = dir / "notes.md";
    write_file(file, "# Notes\n\nThis document describes the build steps.\n");

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Error);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CFixedAnswerModel>("NO", "generic answer");
        });

    CNoSymbolClang clang;
    fcpp::CCursorContextAnalyzer analyzer(clang, llm);
    fcpp::CRequestOrchestrator orchestrator(logger, nullptr, &analyzer, &llm);

    fcpp::SOrchestrationRequest request;
    request.m_request_id = "file-route-no";
    request.m_text = "What is RAII?";
    request.m_cursor_file = file.string();

    const fcpp::SOrchestrationResult result = orchestrator.run(request);

    // No cursor, no history, file irrelevant: the only applicable outcome is the
    // generic route, and the file must not be silently attached to it.
    REQUIRE(result.m_ok);
    REQUIRE(result.m_route == fcpp::EOrchestrationRoute::NoApplicableContext);
    REQUIRE(result.m_file_context.empty());

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("unreadable file makes the file assessment fail visibly",
          "[file][route]")
{
    fcpp::CLogger logger(make_temp_dir("fcpp-file-route-missing") / "logs",
                         fcpp::ELogLevel::Error);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CFixedAnswerModel>("YES", "answer");
        });

    const CNoSymbolClang clang;
    const fcpp::CCursorContextAnalyzer analyzer(clang, llm);

    const fcpp::SFileContextResult result = analyzer.assess_file(
        "Explain this file", "/nonexistent/path/to/file.txt");

    REQUIRE_FALSE(result.m_ok);
    REQUIRE_FALSE(result.m_error.empty());
    REQUIRE_FALSE(result.m_context_applicable);
}

TEST_CASE("large file content is truncated and marked", "[file][route]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-file-route-large");
    const std::filesystem::path file = dir / "big.txt";
    write_file(file, std::string(5000, 'x'));

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Error);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CFixedAnswerModel>("YES", "answer");
        });

    const CNoSymbolClang clang;
    const fcpp::CCursorContextAnalyzer analyzer(clang, llm);

    fcpp::SFileSliceLimits limits;
    limits.m_max_bytes = 1000;

    const fcpp::SFileContextResult result =
        analyzer.assess_file("Explain this file", file, limits);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_context_applicable);
    REQUIRE(result.m_truncated);
    REQUIRE(result.m_content.size() < 1200);
    REQUIRE(result.m_content.find("TRUNCATED") != std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}
