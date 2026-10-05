#include "queries.h"
#include "tu_cache.h"

namespace cpptool {

json queryVirtualOverrides(const std::string& compileCommandsPath,
                           const std::string& targetUSR) {
    if (targetUSR.empty())
        return json{{"ok", false}, {"error", {{"code", "usr_empty"}, {"message", "пустой USR"}}}};

    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened()) {
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    }
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok) {
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};
    }
    json result = json::parse(cache->get_overrides_json(targetUSR));
    // files_scanned/files_total — статистика индексации (могла случиться давно,
    // не обязательно только что; общий кэш может быть уже тёплым).
    result["files_scanned"] = stats.filesScanned;
    result["files_total"] = stats.filesTotal;
    return result;
}

} // namespace cpptool
