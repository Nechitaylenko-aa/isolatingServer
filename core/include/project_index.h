#pragma once
#include "json.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace cpptool {

using json = nlohmann::json;

// Кэш уровня TU: по одному entry на .cpp из compile_commands.json.
// L1 — в памяти (unordered_map), L2 — json-файл на диске (JsonFileStore).
// Ключ инвалидации — sourceHash (хеш содержимого) + flagsHash.
struct FileCacheEntry {
    std::string sourceHash;
    std::string flagsHash;
    // сколько раз каждый USR встретился в этом TU (только для кэша refs)
    std::unordered_map<std::string,int> refsCount;
    // для S5: можно хранить public_methods если файл содержит класс, иначе -1
    int publicMethods{-1};
};

// Интерфейс персистентности — чтобы заменить JsonFileStore на SqliteStore в S5 без правок GroundService.
class IndexStore {
public:
    virtual ~IndexStore() = default;
    virtual std::unordered_map<std::string, FileCacheEntry> load() = 0;
    virtual void save(const std::unordered_map<std::string, FileCacheEntry>& entries) = 0;
};

class JsonFileStore : public IndexStore {
public:
    explicit JsonFileStore(std::string cachePath) : cachePath_(std::move(cachePath)) {}
    std::unordered_map<std::string, FileCacheEntry> load() override;
    void save(const std::unordered_map<std::string, FileCacheEntry>& entries) override;
    const std::string& path() const { return cachePath_; }
private:
    std::string cachePath_;
};

// Основной индекс проекта. Знает только про core/compile_commands + ParsedUnit,
// ничего не знает про agent/ModelBroker.
class ProjectIndex {
public:
    // cachePath — напр. "build/.cpp-tool-cache/index.json" или "/tmp/cpp-tool-cache.json"
    // compileCommandsPath — путь к compile_commands.json
    ProjectIndex(std::string compileCommandsPath, std::string cachePath);

    // Сколько раз USR встречается во всём проекте (сумма по всем .cpp).
    // Перепарсивает только грязные файлы (sourceHash/flagsHash изменился), остальные — из кэша.
    // Возвращает {totalRefs, fromCache, filesScanned, filesTotal, error?}
    struct BlastRefsResult {
        int totalRefs{0};
        int distinctFilesWithRefs{0}; // во скольких .cpp (TU) USR встретился хотя бы раз
        int filesScanned{0};
        int filesTotal{0}; // total_files_in_project — знаменатель для относительного порога
        int fromCache{0};
        std::string error;
        bool ok{true};
    };
    BlastRefsResult totalRefsForUSR(const std::string& targetUSR);

    // Хелперы для хешей (публичны для тестов)
    static std::string hashFileContent(const std::string& absPath);
    static std::string hashFlags(const std::vector<std::string>& flags);

private:
    std::string compileCommandsPath_;
    std::unique_ptr<IndexStore> store_;
    std::unordered_map<std::string, FileCacheEntry> mem_;
    bool loaded_{false};
    void ensureLoaded();
    void persist();
    FileCacheEntry buildEntryForFile(const std::string& absPath, const std::vector<std::string>& flags);
};

} // namespace cpptool
