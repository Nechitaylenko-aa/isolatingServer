#include "queries.h"
#include "tu_cache.h"

namespace cpptool {

json queryClassOutline(const std::string& className, const std::string& compileCommandsPath,
                       const std::string& fileHint) {
    auto cache = makeQueryCache(compileCommandsPath);
    if (!cache->opened()) {
        return json{{"ok", false}, {"error", {{"code", "cache_open_failed"},
            {"message", "не удалось открыть кэш: " + cache->open_error()}}}};
    }
    auto stats = cache->ensure_project_indexed(compileCommandsPath);
    if (!stats.ok) {
        return json{{"ok", false}, {"error", {{"code", "index_failed"}, {"message", stats.error}}}};
    }
    return json::parse(cache->class_outline_json(className, /*byUsr=*/false, fileHint));
}

} // namespace cpptool
