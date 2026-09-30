#include "execute_service.h"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>

namespace cppagent {
namespace fs = std::filesystem;

std::string ExecuteService::newId() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::random_device rd; std::mt19937_64 g(rd());
    return std::to_string(now) + "-" + std::to_string(g() % 100000);
}

ApplyResult ExecuteService::apply(const std::vector<EditAction>& edits) {
    ApplyResult r;
    if (edits.empty()) {
        r.ok = true;
        r.error = "";
        return r;
    }
    std::string id = newId();
    fs::path chkDir = fs::path(checkpointRoot_) / id;
    std::error_code ec;
    fs::create_directories(chkDir, ec);
    if (ec) { r.error = "не удалось создать checkpoint dir: " + ec.message(); return r; }
    r.checkpointId = id;

    for (auto& e : edits) 
    {
        fs::path filePath(e.file);
        if (!fs::exists(filePath)) { r.error = "файл не найден: " + e.file; return r; }
        // backup: копия с сохранением относительного пути (по abs -> _abs_...)
        // safe-имя через хеш чтобы не путать '_' в пути с '/' (как в /tmp/cpp_tool_test_...)
        size_t h = std::hash<std::string>{}(e.file);
        std::string safe = std::to_string(h) + ".bak";
        fs::path bak = chkDir / safe;
        fs::copy_file(filePath, bak, fs::copy_options::overwrite_existing, ec);
        if (ec) { r.error = "backup failed " + e.file + ": " + ec.message(); return r; }
        // манифест: какой хеш -> какой оригинальный путь
        {
            fs::path mf = chkDir / "manifest.json";
            json m;
            if (fs::exists(mf)) { std::ifstream min(mf); try{ min>>m; }catch(...){ m=json::object(); } }
            m[safe] = e.file;
            std::ofstream mo(mf, std::ios::trunc); mo << m.dump(2);
        }

        std::ifstream in(e.file);
        std::ostringstream ss; ss << in.rdbuf();
        std::string content = ss.str();
        std::string out;
        if (e.oldText.empty()) {
            out = content + e.newText;
        } 
        else {
            size_t pos = content.find(e.oldText);
            if (pos == std::string::npos) { r.error = "oldText не найден в " + e.file; return r; }
            out = content.substr(0, pos) + e.newText + content.substr(pos + e.oldText.size());
        }
        std::ofstream outFile(e.file, std::ios::trunc);
        if (!outFile) 
        { 
            r.error = "не удалось записать " + e.file; 
            return r; 
        }

        outFile << out;
        r.appliedFiles.push_back(e.file);
    }
    r.ok = true;
    return r;
}

bool ExecuteService::undo(const std::string& checkpointId, std::string& error) 
{
    fs::path chkDir = fs::path(checkpointRoot_) / checkpointId;
    if (!fs::exists(chkDir)) { error = "checkpoint не найден: " + checkpointId; return false; }
    fs::path mf = chkDir / "manifest.json";
    if (!fs::exists(mf)) { error = "манифест не найден в checkpoint " + checkpointId; return false; }
    json m; { std::ifstream in(mf); try{ in>>m; }catch(const std::exception& e){ error=std::string("манифест не JSON: ")+e.what(); return false; } }
    
    for (auto& [safe, origJson] : m.items()) 
    {
        std::string orig = origJson.get<std::string>();
        fs::path bak = chkDir / safe;
        if (!fs::exists(bak)) { error = "бекап не найден: " + safe; return false; }
        std::error_code ec;
        fs::copy_file(bak, fs::path(orig), fs::copy_options::overwrite_existing, ec);
        if (ec) 
        { 
            error = "undo failed " + orig + ": " + ec.message(); 
            return false; 
        }
    }
    return true;
}

} // namespace cppagent
