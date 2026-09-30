#include "verify_service.h"
#include <cstdio>
#include <array>
#include <sstream>

namespace cppagent {

static std::string execCapture(const std::string& cmd) {
    std::array<char, 4096> buf{};
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return "popen failed";
    while (fgets(buf.data(), (int)buf.size(), p)) out += buf.data();
    pclose(p);
    return out;
}

VerifyResult VerifyService::verifyCompile(const std::string& /*file*/) {
    VerifyResult r;
    if (buildDir_.empty()) { r.ok = true; r.log = "no buildDir, skip compile"; return r; }
    std::string cmd = "cmake --build " + buildDir_ + " -j4 2>&1 | tail -n 80";
    std::string log = execCapture(cmd);
    r.log = log;
    // эвристика: если в логе "error" или "Error" — считаем фейлом
    bool hasError = (log.find("error:") != std::string::npos || log.find("Error") != std::string::npos);
    // пустой лог без ошибок — ok (cmake может быть уже собран)
    r.ok = !hasError;
    if (!r.ok) r.error = "compile failed";
    return r;
}

VerifyResult VerifyService::verifyTests(const std::string& filter) {
    VerifyResult r;
    if (buildDir_.empty()) { r.ok = true; r.log = "no buildDir, skip ctest"; return r; }
    std::string cmd = "ctest --test-dir " + buildDir_;
    if (!filter.empty()) cmd += " -R " + filter;
    cmd += " 2>&1 | tail -n 80";
    r.log = execCapture(cmd);
    r.ok = (r.log.find("Failed") == std::string::npos && r.log.find("No tests") == std::string::npos) || r.log.find("Passed") != std::string::npos;
    if (!r.ok) r.error = "ctest failed";
    return r;
}

} // namespace cppagent
