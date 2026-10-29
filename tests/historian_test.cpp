#include "core/CHistorian.h"

#include <cstddef>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("historian selection accepts a comma-separated list of valid turn numbers",
          "[historian][selection]")
{
    std::vector<std::size_t> indices;
    std::string error;
    REQUIRE(fcpp::CHistorian::decode_relevant_indices("1, 3", 3, indices, error));
    REQUIRE(indices == std::vector<std::size_t>{0, 2});
}

TEST_CASE("historian selection accepts NONE as an empty context",
          "[historian][selection]")
{
    std::vector<std::size_t> indices{0};
    std::string error;
    REQUIRE(fcpp::CHistorian::decode_relevant_indices("NONE", 3, indices, error));
    REQUIRE(indices.empty());
}

TEST_CASE("historian selection rejects malformed or unsafe indices",
          "[historian][selection]")
{
    const std::vector<std::string> invalid{
        "", "none", "0", "4", "1, 1", "1,", "1 2", "1, x", "1,2 explanation"
    };
    for (const std::string& text : invalid)
    {
        DYNAMIC_SECTION("rejects: " << text)
        {
            std::vector<std::size_t> indices;
            std::string error;
            REQUIRE_FALSE(fcpp::CHistorian::decode_relevant_indices(
                text, 3, indices, error));
            REQUIRE_FALSE(error.empty());
        }
    }
}
