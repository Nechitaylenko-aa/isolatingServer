#include "build_graph.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <optional>

namespace fs = std::filesystem;

namespace cpptool {

namespace {

// Имя цели закодировано в id как "<name>::@<hash>" — так CMake File API
// ссылается на зависимости (сами объекты dependencies несут только id).
std::string targetNameFromId(const std::string& id) {
    auto pos = id.find("::@");
    return pos == std::string::npos ? id : id.substr(0, pos);
}

// Находит самый свежий index-*.json в reply-каталоге (File API может
// оставлять несколько поколений при повторных запросах).
std::optional<fs::path> findLatestIndex(const fs::path& replyDir) {
    std::optional<fs::path> best;
    std::filesystem::file_time_type bestTime;
    if (!fs::exists(replyDir)) return std::nullopt;
    for (auto& entry : fs::directory_iterator(replyDir)) {
        auto name = entry.path().filename().string();
        if (name.rfind("index-", 0) == 0 && entry.path().extension() == ".json") {
            auto t = entry.last_write_time();
            if (!best || t > bestTime) { best = entry.path(); bestTime = t; }
        }
    }
    return best;
}

json readJsonFile(const fs::path& p) {
    std::ifstream in(p);
    json j;
    in >> j;
    return j;
}

// Пишет пустой query-файл codemodel-v2 (см. CMake File API, "Shared Stateless
// Query Files") и, если ответа ещё нет, перезапускает конфигурацию CMake на
// уже существующем build-каталоге (использует закэшированные аргументы —
// быстро, без пересборки). Это единственное место в query.build_graph, где
// есть побочный эффект — намеренно, иначе граф целей нельзя получить вообще
// при первом обращении к уже настроенному без File API проекту.
bool ensureCodemodelReply(const fs::path& buildDir) {
    fs::path queryDir = buildDir / ".cmake/api/v1/query";
    fs::path markerFile = queryDir / "codemodel-v2";
    fs::path replyDir = buildDir / ".cmake/api/v1/reply";

    bool markerJustCreated = false;
    if (!fs::exists(markerFile)) {
        fs::create_directories(queryDir);
        std::ofstream(markerFile).close();
        markerJustCreated = true;
    }

    if (!markerJustCreated && findLatestIndex(replyDir)) {
        return true; // уже есть ответ от предыдущего запроса — не дёргаем cmake зря
    }

    std::string cmd = "cmake \"" + buildDir.string() + "\" > /dev/null 2>&1";
    int rc = std::system(cmd.c_str());
    return rc == 0 && findLatestIndex(replyDir).has_value();
}

} // namespace

json queryBuildGraph(const std::string& buildDirStr) {
    fs::path buildDir(buildDirStr);
    if (!fs::exists(buildDir / "CMakeCache.txt")) {
        return json{{"ok", false}, {"error", {{"code", "not_a_build_dir"},
            {"message", buildDirStr + " не похож на сконфигурированный build-каталог (нет CMakeCache.txt)"}}}};
    }

    if (!ensureCodemodelReply(buildDir)) {
        return json{{"ok", false}, {"error", {{"code", "cmake_reconfigure_failed"},
            {"message", "не удалось получить codemodel через CMake File API"}}}};
    }

    fs::path replyDir = buildDir / ".cmake/api/v1/reply";
    auto indexPath = findLatestIndex(replyDir);
    json index = readJsonFile(*indexPath);

    if (!index.contains("reply") || !index["reply"].contains("codemodel-v2")) {
        return json{{"ok", false}, {"error", {{"code", "codemodel_missing_in_reply"}}}};
    }
    std::string codemodelFile = index["reply"]["codemodel-v2"]["jsonFile"].get<std::string>();
    json codemodel = readJsonFile(replyDir / codemodelFile);

    json targets = json::array();

    for (auto& cfg : codemodel["configurations"]) {
        for (auto& tRef : cfg["targets"]) {
            std::string tFile = tRef["jsonFile"].get<std::string>();
            json t = readJsonFile(replyDir / tFile);

            std::string type = t.value("type", "UNKNOWN");
            if (type == "UTILITY") continue; // CTest/dashboard-цели и т.п., не реальные модули сборки

            json deps = json::array();
            if (t.contains("dependencies")) {
                for (auto& d : t["dependencies"]) {
                    deps.push_back(targetNameFromId(d["id"].get<std::string>()));
                }
            }

            int compiledSources = 0, headerOnlySources = 0;
            if (t.contains("sources")) {
                for (auto& s : t["sources"]) {
                    if (s.contains("compileGroupIndex")) ++compiledSources;
                    else ++headerOnlySources;
                }
            }

            targets.push_back({
                {"name", t.value("name", tRef.value("name", ""))},
                {"type", type},
                {"declared_deps", deps},
                {"compiled_sources_count", compiledSources},
                {"header_only_sources_count", headerOnlySources}
            });
        }
    }

    return json{{"ok", true}, {"targets", targets}};
}

} // namespace cpptool
