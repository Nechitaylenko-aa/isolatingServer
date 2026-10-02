#include <catch2/catch_test_macros.hpp>

#include "hash.h"

#include <filesystem>
#include <fstream>

using namespace cpptool;

// Эталонные значения FNV-1a 64-bit — из спецификации алгоритма, а не из нашей
// же реализации: если реализация разойдётся с ними, тест это поймает.
TEST_CASE("fnv1a_64 matches known vectors", "[hash]")
{
    // FNV-1a 64: смещение 0xcbf29ce484222325, простое 0x100000001b3.
    CHECK(fnv1a_64("") == 0xcbf29ce484222325ULL);
    CHECK(fnv1a_64("a") == 0xaf63dc4c8601ec8cULL);
    CHECK(fnv1a_64("foobar") == 0x85944171f73967e8ULL);
}

TEST_CASE("hash_string returns stable 16-char hex", "[hash]")
{
    const std::string h = hash_string("some content");
    CHECK(h.size() == 16);
    CHECK(h == hash_string("some content"));
    CHECK(h != hash_string("some conten"));
    CHECK(hash_string("").size() == 16);
}

TEST_CASE("hash_string differs for UTF-8 multibyte input", "[hash]")
{
    // Кириллица — не однобайтовые символы: хэшируются байты, а не кодпойнты.
    CHECK(hash_string("Проверь") != hash_string("проверь"));
    CHECK(hash_string("Проверь") == hash_string("Проверь"));
}

TEST_CASE("hash_file reads actual content", "[hash]")
{
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / ("cpp_tool_hash_" + std::to_string(::rand()));
    fs::create_directories(dir);
    fs::path file = dir / "sample.txt";

    {
        std::ofstream out(file);
        out << "hello";
    }
    const std::string h1 = hash_file(file.string());
    CHECK(!h1.empty());
    CHECK(h1 == hash_string("hello"));

    {
        std::ofstream out(file, std::ios::trunc);
        out << "hello ";
    }
    CHECK(hash_file(file.string()) != h1);

    fs::remove_all(dir);
}

TEST_CASE("hash_file on missing file is empty, not a valid hash", "[hash]")
{
    // Пустая строка — маркер «содержимое неизвестно». Кэш обязан трактовать
    // её как грязно, поэтому она не должна совпадать ни с одним реальным хэшем.
    CHECK(hash_file("/nonexistent/path/definitely/missing.h").empty());
    CHECK(hash_file("/nonexistent/path/definitely/missing.h") != hash_string(""));
}

TEST_CASE("hash_flags separates arguments", "[hash]")
{
    // Без разделителя ["-ab"] и ["-a","b"] дали бы одинаковый хэш — коллизия,
    // из-за которой смена флагов не инвалидировала бы кэш.
    CHECK(hash_flags({"-ab"}) != hash_flags({"-a", "b"}));
    CHECK(hash_flags({"-a", "b"}) != hash_flags({"-b", "a"}));
    CHECK(hash_flags({"-a", "b"}) == hash_flags({"-a", "b"}));
    CHECK(hash_flags({}).size() == 16);
}
