#include "queries.h"
#include "tu_cache.h"

namespace cpptool {

json queryLocateSymbol(const std::string& file, unsigned line, unsigned col,
                        const std::string& compileCommandsPath) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened()) {
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    }
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok) {
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};
    }
    json result = json::parse(cache->find_enclosing_json(file, line, col));
    return result;
}

json queryLocateByUSR(const std::string& usr, const std::string& compileCommandsPath) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened()) {
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    }
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok) {
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};
    }
    return json::parse(cache->get_location_by_usr_json(usr));
}

} // namespace cpptool
