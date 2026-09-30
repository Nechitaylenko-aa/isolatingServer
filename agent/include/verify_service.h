#pragma once
#include "json.hpp"
#include <string>

namespace cppagent {
using json = nlohmann::json;
struct VerifyResult {
    bool ok{false};
    std::string log;
    std::string error;
};
// Проверки после Execute: compile (быстро) и ctest (для TEST_GEN).
class VerifyService {
public:
    explicit VerifyService(std::string buildDir) : buildDir_(std::move(buildDir)) {}
    // Быстрая проверка: cmake --build (или g++ -fsyntax-only если buildDir пустой)
    VerifyResult verifyCompile(const std::string& file = "");
    VerifyResult verifyTests(const std::string& filter = "");
private:
    std::string buildDir_;
};
} // namespace cppagent
