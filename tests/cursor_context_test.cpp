/**
 * @file cursor_context_test.cpp
 * @brief Срез курсорного контекста строится окном вокруг строки курсора.
 */

#include "core/CCursorContextAnalyzer.h"
#include "core/CLogger.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fcpp-agent/IClang.h"
#include "fcpp-agent/IModelApi.h"
#include "index/CClangFactory.h"

namespace
{

/** @brief Модель, всегда отвечающая YES на вопрос о применимости курсора. */
class CYesModel final : public fcpp::IModelApi
{
public:
    bool is_available() const override
    {
        return true;
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>&) override
    {
        return fcpp::SModelReply{true, "YES", {}, "stop", {}};
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
};

/** @brief Бэкенд clang, возвращающий заранее заданный символ. */
class CFixedClang final : public fcpp::IClang
{
public:
    explicit CFixedClang(fcpp::SClangSymbol symbol)
        : m_symbol(std::move(symbol))
    {
    }

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
        return m_symbol;
    }

private:
    fcpp::SClangSymbol m_symbol;
};

/** @brief Создать временный каталог для теста. */
std::filesystem::path make_temp_dir(const std::string& name)
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / name;
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
    std::filesystem::create_directories(dir, ignored);
    return dir;
}

/** @brief Записать файл из count строк вида "line N". */
void write_lines(const std::filesystem::path& path, std::size_t count)
{
    std::ofstream output(path);
    REQUIRE(output.is_open());
    for (std::size_t index = 1; index <= count; ++index)
    {
        output << "int value_" << index << " = " << index << ";\n";
    }
}

} // namespace

TEST_CASE("cursor slice is a window around the cursor, not the whole entity",
          "[cursor][context]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-cursor-slice");
    const std::filesystem::path source = dir / "big.cpp";
    write_lines(source, 400);

    // Сущность покрывает весь файл, но срез должен остаться окном вокруг строки 200.
    fcpp::SClangSymbol symbol{};
    symbol.m_name = "CBig";
    symbol.m_qualified_name = "CBig";
    symbol.m_signature = "class CBig";
    symbol.m_usr = "c:@S@CBig";
    symbol.m_location.m_file = source.string();
    symbol.m_location.m_line = 1;
    symbol.m_location.m_column = 1;
    symbol.m_location.m_end_line = 400;

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Error);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CYesModel>();
        });

    const CFixedClang clang(symbol);
    const fcpp::CCursorContextAnalyzer analyzer(clang, llm);

    fcpp::SCursorSliceLimits limits;
    limits.m_radius_lines = 5;
    limits.m_max_bytes = 100000;

    const fcpp::SCursorContextResult result = analyzer.assess(
        "What does this line do?", source, 200, 5, {}, limits);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_context_applicable);
    REQUIRE_FALSE(result.m_slice_truncated);

    // Окно: строки 195..205 — 11 строк, курсор внутри.
    REQUIRE(result.m_source_context.find("195: ") != std::string::npos);
    REQUIRE(result.m_source_context.find("200: ") != std::string::npos);
    REQUIRE(result.m_source_context.find("205: ") != std::string::npos);
    // Строки далеко за пределами окна в срез не попадают.
    REQUIRE(result.m_source_context.find("100: ") == std::string::npos);
    REQUIRE(result.m_source_context.find("300: ") == std::string::npos);

    // Весь класс (400 строк) в промпт не уходит.
    REQUIRE(result.m_source_context.size() < 2000);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("cursor slice respects the byte limit and reports truncation",
          "[cursor][context]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-cursor-slice-limit");
    const std::filesystem::path source = dir / "wide.cpp";
    write_lines(source, 200);

    fcpp::SClangSymbol symbol{};
    symbol.m_name = "CWide";
    symbol.m_location.m_file = source.string();
    symbol.m_location.m_line = 100;
    symbol.m_location.m_column = 1;
    symbol.m_location.m_end_line = 200;

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Error);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CYesModel>();
        });

    const CFixedClang clang(symbol);
    const fcpp::CCursorContextAnalyzer analyzer(clang, llm);

    fcpp::SCursorSliceLimits limits;
    limits.m_radius_lines = 50;
    limits.m_max_bytes = 300;

    const fcpp::SCursorContextResult result = analyzer.assess(
        "Explain", source, 100, 1, {}, limits);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_slice_truncated);
    REQUIRE(result.m_source_context.size() <= limits.m_max_bytes);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("clang backend resolves a class declared in a C++ header",
          "[cursor][clang][header]")
{
    // Regression: libclang picks the language by extension and parses `.h` as C,
    // where `class` and `namespace` do not exist. The AST is then empty and the
    // cursor never resolves. Header extensions must be parsed as C++ headers.
    const std::filesystem::path dir = make_temp_dir("fcpp-header-language");
    const std::filesystem::path header = dir / "sample.h";
    {
        std::ofstream output(header);
        REQUIRE(output.is_open());
        output << "#ifndef SAMPLE_H\n#define SAMPLE_H\n"
                  "namespace demo\n{\n"
                  "class CWidget\n"
                  "{\n"
                  "public:\n"
                  "    int value() const;\n"
                  "};\n"
                  "}\n"
                  "#endif\n";
    }

    const std::unique_ptr<fcpp::IClang> clang = fcpp::create_clang_backend({});
    REQUIRE(clang != nullptr);
    REQUIRE(clang->is_available());

    // Line 5, column 7 points at "CWidget" (line 4 is the namespace brace).
    const std::optional<fcpp::SClangSymbol> symbol =
        clang->cursor_at(header, 5, 7, {});
    REQUIRE(symbol.has_value());
    REQUIRE(symbol->m_name == "CWidget");
    REQUIRE(symbol->m_qualified_name == "demo::CWidget");

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}

TEST_CASE("cursor slice stays inside the file at the first line",
          "[cursor][context]")
{
    const std::filesystem::path dir = make_temp_dir("fcpp-cursor-slice-head");
    const std::filesystem::path source = dir / "head.cpp";
    write_lines(source, 10);

    fcpp::SClangSymbol symbol{};
    symbol.m_name = "head";
    symbol.m_location.m_file = source.string();
    symbol.m_location.m_line = 1;
    symbol.m_location.m_column = 1;
    symbol.m_location.m_end_line = 10;

    fcpp::CLogger logger(dir / "logs", fcpp::ELogLevel::Error);
    fcpp::SLlmRoleSet roles;
    fcpp::CLlmLayer llm(roles, logger,
        [](fcpp::ELlmRole) -> std::unique_ptr<fcpp::IModelApi>
        {
            return std::make_unique<CYesModel>();
        });

    const CFixedClang clang(symbol);
    const fcpp::CCursorContextAnalyzer analyzer(clang, llm);

    fcpp::SCursorSliceLimits limits;
    limits.m_radius_lines = 5;
    limits.m_max_bytes = 100000;

    const fcpp::SCursorContextResult result = analyzer.assess(
        "Explain", source, 1, 1, {}, limits);

    REQUIRE(result.m_ok);
    REQUIRE(result.m_source_context.find("1: ") != std::string::npos);
    REQUIRE(result.m_source_context.find("6: ") != std::string::npos);
    REQUIRE(result.m_source_context.find("7: ") == std::string::npos);

    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
}
