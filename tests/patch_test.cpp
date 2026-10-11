#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "core/CPatchApplier.h"
#include "core/CPatchParser.h"
#include "core/CPatchValidator.h"
#include "fcpp-agent/SPatch.h"

namespace
{

/** @brief Счётчик временных каталогов. */
int g_patch_counter = 0;

/**
 * @brief Создать уникальный временный каталог.
 * @param prefix Префикс.
 * @return Путь.
 */
std::filesystem::path make_temp_dir(const std::string& prefix)
{
    const std::filesystem::path base =
        std::filesystem::temp_directory_path()
        / (prefix + "-" + std::to_string(static_cast<long>(::getpid())) + "-" + std::to_string(g_patch_counter++));
    std::filesystem::create_directories(base);
    return base;
}

/**
 * @brief Прочитать файл.
 * @param path Путь.
 * @return Содержимое.
 */
std::string slurp(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return content;
}

} // namespace

TEST_CASE("patch parser разбирает unified diff", "[patch]")
{
    const std::string diff =
        "--- a/src/a.cpp\n"
        "+++ b/src/a.cpp\n"
        "@@ -1,4 +1,5 @@\n"
        " int main()\n"
        " {\n"
        "-    return 1;\n"
        "+    int x = 1;\n"
        "+    return x;\n"
        " }\n";

    fcpp::SPatchSet patch;
    REQUIRE(fcpp::CPatchParser::parse(diff, patch));
    REQUIRE(patch.m_files.size() == 1);
    REQUIRE(patch.m_files.front().m_old_path == "src/a.cpp");
    REQUIRE(patch.m_files.front().m_hunks.size() == 1);
}

TEST_CASE("patch validator применяет hunk и отклоняет конфликт", "[patch]")
{
    const std::filesystem::path root = make_temp_dir("fcpp-patch-root");
    std::filesystem::create_directories(root / "src");
    std::ofstream(root / "src" / "a.cpp") << "int main()\n{\n    return 1;\n}\n";

    const std::string diff =
        "--- a/src/a.cpp\n"
        "+++ b/src/a.cpp\n"
        "@@ -1,4 +1,5 @@\n"
        " int main()\n"
        " {\n"
        "-    return 1;\n"
        "+    int x = 1;\n"
        "+    return x;\n"
        " }\n";

    fcpp::SPatchSet patch;
    REQUIRE(fcpp::CPatchParser::parse(diff, patch));

    fcpp::CPatchValidator validator(root);
    fcpp::SPatchValidation validation;
    REQUIRE(validator.validate(patch, validation));
    REQUIRE(validation.m_changes.size() == 1);
    REQUIRE(validation.m_changes.front().m_updated.find("int x = 1;") != std::string::npos);

    fcpp::CPatchApplier applier;
    std::string message;
    REQUIRE(applier.apply(validation.m_changes, message));
    REQUIRE(slurp(root / "src" / "a.cpp").find("int x = 1;") != std::string::npos);

    // Конфликт: тот же патч повторно не применится (контекст уже изменён).
    fcpp::SPatchValidation second;
    REQUIRE_FALSE(validator.validate(patch, second));

    // Откат (COMPILATION FAIL) возвращает исходное содержимое.
    REQUIRE(applier.revert(validation.m_changes, message));
    REQUIRE(slurp(root / "src" / "a.cpp").find("return 1;") != std::string::npos);
    REQUIRE(slurp(root / "src" / "a.cpp").find("int x = 1;") == std::string::npos);

    std::filesystem::remove_all(root);
}

TEST_CASE("patch validator отклоняет выход за пределы проекта", "[patch]")
{
    const std::filesystem::path root = make_temp_dir("fcpp-patch-escape");
    std::ofstream(root / "keep.cpp") << "int a;\n";

    const std::string diff =
        "--- a/../escape.cpp\n"
        "+++ b/../escape.cpp\n"
        "@@ -1,1 +1,1 @@\n"
        "-int a;\n"
        "+int b;\n";

    fcpp::SPatchSet patch;
    REQUIRE(fcpp::CPatchParser::parse(diff, patch));

    fcpp::CPatchValidator validator(root);
    fcpp::SPatchValidation validation;
    REQUIRE_FALSE(validator.validate(patch, validation));

    std::filesystem::remove_all(root);
}

TEST_CASE("patch validator rejects sibling-prefix and symlink escapes", "[patch][security]")
{
    const std::filesystem::path root = make_temp_dir("fcpp-patch-boundary");
    const std::filesystem::path sibling(root.string() + "-evil");
    const std::filesystem::path outside = make_temp_dir("fcpp-patch-outside");
    std::filesystem::create_directories(sibling);
    std::ofstream(sibling / "pwn.cpp") << "int a;\n";
    std::ofstream(outside / "pwn.cpp") << "int a;\n";

    auto parse_and_validate = [&root](const std::string& old_path, const std::string& new_path)
    {
        const std::string diff = "--- a/" + old_path + "\n"
            + "+++ b/" + new_path + "\n"
            + "@@ -1,1 +1,1 @@\n-int a;\n+int b;\n";
        fcpp::SPatchSet patch;
        REQUIRE(fcpp::CPatchParser::parse(diff, patch));
        fcpp::CPatchValidator validator(root);
        fcpp::SPatchValidation validation;
        REQUIRE_FALSE(validator.validate(patch, validation));
        REQUIRE(validation.m_message.find("path escapes project root") != std::string::npos);
    };

    parse_and_validate("../" + sibling.filename().string() + "/pwn.cpp",
                       "../" + sibling.filename().string() + "/pwn.cpp");

    std::error_code symlink_error;
    std::filesystem::create_directory_symlink(outside, root / "linked", symlink_error);
    REQUIRE_FALSE(symlink_error);
    parse_and_validate("linked/pwn.cpp", "linked/pwn.cpp");

    std::filesystem::remove_all(root);
    std::filesystem::remove_all(sibling);
    std::filesystem::remove_all(outside);
}

TEST_CASE("patch validator checks deletion hunks against the original file", "[patch][delete]")
{
    const std::filesystem::path root = make_temp_dir("fcpp-patch-delete");
    std::ofstream(root / "keep.cpp") << "int a;\n";
    fcpp::CPatchValidator validator(root);
    fcpp::SPatchValidation validation;

    const std::string missing_hunk =
        "--- a/keep.cpp\n"
        "+++ /dev/null\n";
    fcpp::SPatchSet patch;
    REQUIRE(fcpp::CPatchParser::parse(missing_hunk, patch));
    REQUIRE_FALSE(validator.validate(patch, validation));
    REQUIRE(validation.m_message.find("delete patch has no hunks") != std::string::npos);

    const std::string mismatched_hunk =
        "--- a/keep.cpp\n"
        "+++ /dev/null\n"
        "@@ -1,1 +0,0 @@\n"
        "-int different;\n";
    REQUIRE(fcpp::CPatchParser::parse(mismatched_hunk, patch));
    REQUIRE_FALSE(validator.validate(patch, validation));
    REQUIRE(validation.m_message.find("delete patch conflict") != std::string::npos);

    const std::string valid_hunk =
        "--- a/keep.cpp\n"
        "+++ /dev/null\n"
        "@@ -1,1 +0,0 @@\n"
        "-int a;\n";
    REQUIRE(fcpp::CPatchParser::parse(valid_hunk, patch));
    REQUIRE(validator.validate(patch, validation));
    REQUIRE(validation.m_changes.size() == 1);
    REQUIRE(validation.m_changes[0].m_is_delete);

    std::filesystem::remove_all(root);
}


TEST_CASE("patch parser rejects inconsistent hunk counts and overflowing ranges", "[patch][parser]")
{
    const std::string inconsistent =
        "--- a/file.cpp\n"
        "+++ b/file.cpp\n"
        "@@ -1,3 +1,4 @@\n"
        "-old\n"
        "+new\n";
    fcpp::SPatchSet patch;
    REQUIRE_FALSE(fcpp::CPatchParser::parse(inconsistent, patch));
    REQUIRE(patch.m_files.empty());

    const std::string overflow =
        "--- a/file.cpp\n"
        "+++ b/file.cpp\n"
        "@@ -4294967297,1 +1,1 @@\n"
        "-old\n"
        "+new\n";
    REQUIRE_FALSE(fcpp::CPatchParser::parse(overflow, patch));
    REQUIRE(patch.m_files.empty());
}
