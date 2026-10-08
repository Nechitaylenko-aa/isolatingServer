/**
 * @file CDoctor.cpp
 * @brief Реализация preflight-проверки окружения (ADR-06).
 */

#include "core/CDoctor.h"

#include <iostream>
#include <memory>
#include <string>

#include "core/CExecutor.h"
#include "fcpp-agent/IClang.h"
#include "index/CClangFactory.h"

namespace fcpp
{

namespace
{

/**
 * @brief Извлечь первую непустую строку вывода.
 * @param text Текст.
 * @return Первая строка.
 */
std::string first_line(const std::string& text)
{
    const std::size_t newline = text.find('\n');
    return (newline == std::string::npos) ? text : text.substr(0, newline);
}

} // namespace

bool CDoctor::probe(const std::string& tool, std::string& out_version)
{
    CExecutor executor;

    SCommandArgs args;
    args.m_tool = tool;
    args.m_arguments = {"--version"};
    args.m_effect = EFFECT_READ_ONLY;
    args.m_timeout_seconds = 10;

    const SExecutionResult result = executor.execute(args, nullptr);
    if (result.m_allowed && result.m_exit_code == 0)
    {
        const std::string output = result.m_stdout.empty() ? result.m_stderr : result.m_stdout;
        out_version = first_line(output);
        return true;
    }
    return false;
}

bool CDoctor::run(const SConfig& config) const
{
    std::cout << "fcpp-agent: doctor\n\n";
    std::cout << "Инструмент      Статус   Версия\n";
    std::cout << "-----------------------------------------------\n";

    bool critical_ok = true;

    const std::string cmake = config.m_cmake_path.empty() ? "cmake" : config.m_cmake_path.string();
    std::string cmake_version;
    const bool cmake_found = probe(cmake, cmake_version);
    std::cout << "cmake           " << (cmake_found ? "OK     " : "MISSING") << "  "
              << (cmake_found ? cmake_version : "(critical)") << '\n';
    critical_ok = critical_ok && cmake_found;

    std::unique_ptr<IClang> clang = create_clang_backend(config.m_clang_path);
    const bool clang_found = (clang != nullptr && clang->is_available());
    std::cout << "libclang(>=22)  " << (clang_found ? "OK     " : "MISSING") << "  "
              << (clang_found ? clang->version() : "(critical)") << '\n';
    critical_ok = critical_ok && clang_found;

    const std::string ninja = config.m_ninja_path.empty() ? "ninja" : config.m_ninja_path.string();
    std::string ninja_version;
    const bool ninja_found = probe(ninja, ninja_version);
    std::cout << "ninja           " << (ninja_found ? "OK     " : "MISSING") << "  "
              << (ninja_found ? ninja_version : "(important)") << '\n';

    const std::string ctest = "ctest";
    std::string ctest_version;
    const bool ctest_found = probe(ctest, ctest_version);
    std::cout << "ctest           " << (ctest_found ? "OK     " : "MISSING") << "  "
              << (ctest_found ? ctest_version : "(important)") << '\n';

    const std::string tidy = config.m_clang_tidy_path.empty() ? "clang-tidy" : config.m_clang_tidy_path.string();
    std::string tidy_version;
    const bool tidy_found = probe(tidy, tidy_version);
    std::cout << "clang-tidy      " << (tidy_found ? "OK     " : "MISSING") << "  "
              << (tidy_found ? tidy_version : (config.m_enable_tidy ? "(enabled, missing)" : "(disabled)")) << '\n';

    std::cout << '\n';
    if (!critical_ok)
    {
        std::cerr << "fcpp-agent: FAIL: отсутствуют критичные инструменты (cmake, libclang).\n";
        return false;
    }

    std::cout << "Критичные инструменты доступны.\n";
    return true;
}

} // namespace fcpp
