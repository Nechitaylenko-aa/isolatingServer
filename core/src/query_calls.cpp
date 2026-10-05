#include "queries.h"
#include "tu_cache.h"

namespace cpptool {

namespace {
json callsQuery(const std::string& targetUSR, const std::string& compileCommandsPath, bool incoming) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened()) {
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    }
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok)
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};
    return json::parse(cache->get_calls_json(targetUSR, incoming));
}
} // namespace

std::shared_ptr<CTuCache> makeQueryCache(const std::string& compileCommandsPath) {
    return CTuCache::shared(defaultCachePathFor(compileCommandsPath));
}

json queryCallChain(const std::string& targetUSR, const std::string& compileCommandsPath) {
    return callsQuery(targetUSR, compileCommandsPath, true);
}

json queryCallSubtree(const std::string& targetUSR, const std::string& compileCommandsPath) {
    return callsQuery(targetUSR, compileCommandsPath, false);
}

} // namespace cpptool
