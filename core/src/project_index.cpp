#include "project_index.h"
#include "clang_index.h"
#include "compile_commands.h"
#include <clang-c/Index.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>

namespace cpptool {

namespace {

std::string toHex(size_t h) {
    char buf[17];
    snprintf(buf, sizeof(buf), "%016zx", h);
    return std::string(buf);
}

} // anon

std::string ProjectIndex::hashFileContent(const std::string& absPath) {
    std::ifstream in(absPath, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss; ss << in.rdbuf();
    std::string data = ss.str();
    size_t h = std::hash<std::string>{}(data);
    // примешиваем размер чтобы отличать коллизии
    h ^= std::hash<size_t>{}(data.size()) + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
    return toHex(h);
}

std::string ProjectIndex::hashFlags(const std::vector<std::string>& flags) {
    std::string joined;
    for (auto& f : flags) { joined += f; joined.push_back('\0'); }
    size_t h = std::hash<std::string>{}(joined);
    return toHex(h);
}

std::unordered_map<std::string, FileCacheEntry> JsonFileStore::load() {
    std::unordered_map<std::string, FileCacheEntry> out;
    std::ifstream in(cachePath_);
    if (!in) return out;
    try {
        json j; in >> j;
        for (auto& [path, val] : j.items()) {
            FileCacheEntry e;
            e.sourceHash = val.value("sourceHash", "");
            e.flagsHash = val.value("flagsHash", "");
            e.publicMethods = val.value("publicMethods", -1);
            if (val.contains("refsCount") && val["refsCount"].is_object()) {
                for (auto& [usr, cnt] : val["refsCount"].items())
                    e.refsCount[usr] = cnt.get<int>();
            }
            out[path] = std::move(e);
        }
    } catch (...) {
        // битый кэш — начинаем с пустого, не падаем
    }
    return out;
}

void JsonFileStore::save(const std::unordered_map<std::string, FileCacheEntry>& entries) {
    namespace fs = std::filesystem;
    fs::path p(cachePath_);
    if (p.has_parent_path()) fs::create_directories(p.parent_path());
    json j = json::object();
    for (auto& [path, e] : entries) {
        json refs = json::object();
        for (auto& [usr, cnt] : e.refsCount) refs[usr] = cnt;
        j[path] = json{{"sourceHash", e.sourceHash}, {"flagsHash", e.flagsHash}, {"publicMethods", e.publicMethods}, {"refsCount", refs}};
    }
    std::ofstream out(cachePath_);
    if (out) out << j.dump(2);
}

ProjectIndex::ProjectIndex(std::string compileCommandsPath, std::string cachePath)
    : compileCommandsPath_(std::move(compileCommandsPath))
    , store_(std::make_unique<JsonFileStore>(std::move(cachePath))) {}

void ProjectIndex::ensureLoaded() {
    if (loaded_) return;
    mem_ = store_->load();
    loaded_ = true;
}

void ProjectIndex::persist() { store_->save(mem_); }

FileCacheEntry ProjectIndex::buildEntryForFile(const std::string& absPath, const std::vector<std::string>& flags) {
    FileCacheEntry e;
    e.sourceHash = hashFileContent(absPath);
    e.flagsHash = hashFlags(flags);
    e.publicMethods = -1;
    // Парсим TU и считаем USR вхождений
    auto unitOpt = ParsedUnit::parse(absPath, flags);
    if (!unitOpt) return e;
    ParsedUnit& unit = *unitOpt;
    CXCursor root = unit.rootCursor();
    // Обход всех курсоров в TU, считаем USR
    struct Ctx { FileCacheEntry* e; };
    Ctx ctx{&e};
    auto visitor = [](CXCursor c, CXCursor /*parent*/, CXClientData data) -> CXChildVisitResult {
        auto* ctx = static_cast<Ctx*>(data);
        std::string usr = cursorUSR(c);
        if (!usr.empty()) ctx->e->refsCount[usr]++;
        return CXChildVisit_Recurse;
    };
    clang_visitChildren(root, visitor, &ctx);
    return e;
}

ProjectIndex::BlastRefsResult ProjectIndex::totalRefsForUSR(const std::string& targetUSR) {
    ensureLoaded();
    BlastRefsResult r;
    auto idxOpt = CompileCommandsIndex::load(compileCommandsPath_);
    if (!idxOpt) {
        r.ok = false;
        r.error = "не удалось загрузить compile_commands.json: " + compileCommandsPath_;
        return r;
    }
    // Собираем список всех .cpp файлов из базы — для каждого нужен entry
    // CompileCommandsIndex хранит byFile_ приватно, поэтому перечитываем json напрямую
    // чтобы получить список путей (простой и надёжный способ без friend).
    std::vector<std::pair<std::string, std::vector<std::string>>> files;
    {
        std::ifstream in(compileCommandsPath_);
        if (!in) { r.ok = false; r.error = "не удалось открыть compile_commands.json"; return r; }
        json j; try { in >> j; } catch (const std::exception& e) { r.ok = false; r.error = std::string("compile_commands.json не JSON: ") + e.what(); return r; }
        for (auto& e : j) {
            std::string f = e.value("file", "");
            if (f.empty()) continue;
            auto flagsOpt = idxOpt->flagsFor(f);
            if (!flagsOpt) continue;
            files.emplace_back(f, *flagsOpt);
        }
    }
    r.filesTotal = static_cast<int>(files.size());
    for (auto& [absPath, flags] : files) {
        std::string curHash = hashFileContent(absPath);
        std::string curFlagsHash = hashFlags(flags);
        auto it = mem_.find(absPath);
        bool hit = (it != mem_.end() && it->second.sourceHash == curHash && it->second.flagsHash == curFlagsHash);
        FileCacheEntry entry;
        if (hit) {
            entry = it->second;
            r.fromCache++;
        } else {
            entry = buildEntryForFile(absPath, flags);
            mem_[absPath] = entry;
            r.filesScanned++;
        }
        auto it2 = entry.refsCount.find(targetUSR);
        if (it2 != entry.refsCount.end()){ r.totalRefs += it2->second; r.distinctFilesWithRefs++; }
    }
    // Сохраняем обновлённый кэш на диск (только если что-то перепарсили)
    if (r.filesScanned > 0) persist();
    return r;
}

} // namespace cpptool
