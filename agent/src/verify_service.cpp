#include "verify_service.h"
#include <cstdio>
#include <array>
#include <sstream>

namespace cppagent {

// ВАЖНО: cmd не должен содержать собственный '| tail'/'| head' — иначе $? в full
// это код возврата tail, а не сборки/ctest (упавший билд через '| tail' даёт 0).
static std::string execCapture(const std::string& cmd, int& exitCode) {
    std::array<char, 4096> buf{};
    std::string out;
    // { cmd; } без сабшелла — $? отражает статус самой команды
    std::string full = "{ " + cmd + "; } 2>&1; echo __EXIT__:$?";
    FILE* p = popen(full.c_str(), "r");
    if (!p) { exitCode = -1; return "popen failed"; }
    while (fgets(buf.data(), (int)buf.size(), p)) out += buf.data();
    pclose(p);
    exitCode = 0;
    auto pos = out.rfind("__EXIT__:");
    if (pos != std::string::npos) {
        std::string codeStr = out.substr(pos + 9);
        while (!codeStr.empty() && (codeStr.back() == '\n' || codeStr.back() == '\r' || codeStr.back() == ' '))
            codeStr.pop_back();
        try { exitCode = std::stoi(codeStr); } catch(...) { exitCode = 0; }
        out = out.substr(0, pos);
    }
    // оставляем последние 80 строк, чтобы не раздувать JSON
    {
        std::vector<std::string> lines;
        std::istringstream iss(out);
        std::string line;
        while (std::getline(iss, line)) lines.push_back(line);
        if ((int)lines.size() > 80) lines.erase(lines.begin(), lines.end() - 80);
        out.clear();
        for (auto& l : lines) { out += l; out += '\n'; }
    }
    return out;
}

VerifyResult VerifyService::verifyCompile(const std::string& /*file*/) {
    VerifyResult r;
    if (buildDir_.empty()) {
        // Пропуск — это НЕ успех. Иначе клиент видит compile_ok:true на несостоявшейся правке.
        r.ok = false;
        r.skipped = true;
        r.log = "no buildDir — проверка компиляции пропущена (buildDir не разрешился из --cc)";
        return r;
    }
    int ec = 0;
    r.log = execCapture("cmake --build " + buildDir_ + " -j4", ec);
    r.ok = (ec == 0);
    if (!r.ok) r.error = "compile failed (exit " + std::to_string(ec) + ")";
    return r;
}

VerifyResult VerifyService::verifyTests(const std::string& filter) {
    VerifyResult r;
    if (buildDir_.empty()) {
        r.ok = false;
        r.skipped = true;
        r.log = "no buildDir — проверка тестов пропущена (buildDir не разрешился из --cc)";
        return r;
    }
    std::string cmd = "ctest --test-dir " + buildDir_;
    if (!filter.empty()) cmd += " -R " + filter;
    int ec = 0;
    r.log = execCapture(cmd, ec);
    // нет тестов в проекте — это не провал сборки, но и не успешная проверка: отделяем через skipped
    if (r.log.find("No tests were found") != std::string::npos) {
        r.ok = false; r.skipped = true;
        return r;
    }
    r.ok = (ec == 0);
    if (!r.ok) r.error = "ctest failed (exit " + std::to_string(ec) + ")";
    return r;
}

} // namespace cppagent
