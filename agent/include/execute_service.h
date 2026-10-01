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
    int hintLine = 0;      // строка-подсказка (из cursor): при нескольких вхождениях oldText
                           // выбираем ближайшее к ней, а не первое попавшееся
};

struct ApplyResult {
    bool ok{false};
    std::vector<std::string> appliedFiles;
    std::string checkpointId; // папка с бекапами, для undo
    std::string error;
    // Тип матчинга oldText по каждому применённому файлу: "exact" | "normalized" | "prefix" | "insert".
    // Пригодится для диагностики качества actions от main-модели (нормализация/fuzzy — сигнал что модель
    // не скопировала oldText буквально из файла).
    // ВАЖНО: присваивание, НЕ brace-init — иначе {json::array()} создаст [[ ]] (массив с одним элементом).
    json matchInfo = json::array();
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
