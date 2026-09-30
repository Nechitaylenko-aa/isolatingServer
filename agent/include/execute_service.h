#pragma once
#include "json.hpp"
#include <string>
#include <vector>

namespace cppagent {

using json = nlohmann::json;

struct EditAction {
    std::string file;      // абсолютный путь
    std::string oldText;   // что заменить (пусто = вставка в конец)
    std::string newText;   // на что заменить
};

struct ApplyResult {
    bool ok{false};
    std::vector<std::string> appliedFiles;
    std::string checkpointId; // папка с бекапами, для undo
    std::string error;
};

// Ответственность: применить патч к файлам на диске, без git.
// Бекапы — копии файлов в /tmp/cpp-tool-checkpoint/<id>/ относительными путями.
class ExecuteService {
public:
    explicit ExecuteService(std::string checkpointRoot = "/tmp/cpp-tool-checkpoint")
        : checkpointRoot_(std::move(checkpointRoot)) {}
    ApplyResult apply(const std::vector<EditAction>& edits);
    bool undo(const std::string& checkpointId, std::string& error);
private:
    std::string checkpointRoot_;
    static std::string newId();
};

} // namespace cppagent
