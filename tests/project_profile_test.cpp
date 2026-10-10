#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "fcpp-agent/CProjectProfile.h"

namespace
{

class CUnusedModel final : public fcpp::IModelApi
{
public:
    bool is_available() const override
    {
        return true;
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>&) override
    {
        return {};
    }

    fcpp::SModelReply complete(const std::vector<fcpp::SModelMessage>&,
                               const std::vector<fcpp::SModelTool>&) override
    {
        return {};
    }

    fcpp::SModelReply complete_stream(const std::vector<fcpp::SModelMessage>&,
                                      const fcpp::SStreamCallback&) override
    {
        return {};
    }
};

} // namespace

TEST_CASE("project profile preserves its text and zip is a no-op", "[profile]")
{
    fcpp::CProjectProfile profile;
    REQUIRE(profile.get().empty());

    profile.set("C++20; do not change the public API");
    REQUIRE(profile.get() == "C++20; do not change the public API");

    CUnusedModel model;
    profile.zip(model);
    REQUIRE(profile.get() == "C++20; do not change the public API");
}

TEST_CASE("project profile can be replaced explicitly", "[profile]")
{
    fcpp::CProjectProfile profile;
    profile.set("first profile");
    profile.set("second profile");

    REQUIRE(profile.get() == "second profile");
}
