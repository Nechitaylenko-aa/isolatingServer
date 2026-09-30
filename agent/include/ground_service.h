#pragma once
#include "json.hpp"
#include <string>
#include <vector>

namespace cppagent {

using json = nlohmann::json;

struct GroundResult {
    json locate;          // полный ответ queryLocateSymbol (или {ok:false, error:...})
    json enclosingMethod; // locate["enclosing_method"] или null
    json enclosingClass;  // locate["enclosing_class"]
    json symbolRefs{json{{{"ok", false}}}};      // querySymbolRefs или {ok:false}
    json classOutline{json{{{"ok", false}}}};    // queryClassOutline или {ok:false}
    json scaleFacts;      // DeterministicScale.facts (approx пока)
    std::string suggestedMode{"sandbox"}; // "sandbox"|"quarry" от estimateScaleApprox
    bool ok{false};       // true если locate ok
    json error;           // при !ok — locate целиком или flags error
};

// Сервис Ground: единственное место, которое знает про core/compile_commands
// и про libclang-детали. Оркестратор и handlers его только вызывают.
// INFRA его вообще не вызывает, UNDERSTAND может вызвать лёгкий путь.
class GroundService {
public:
    explicit GroundService(std::string compileCommandsPath)
        : compileCommandsPath_(std::move(compileCommandsPath)) {}

    // Полный Ground: flags -> locate -> symbolRefs/classOutline -> scale
    GroundResult buildGround(const std::string& file, int line, int col);

    // Лёгкий Ground для UNDERSTAND: только flags + locate, без symbolRefs/outline/scale
    GroundResult buildGroundLight(const std::string& file, int line, int col);

    // Низкоуровнево: только флаги для файла (нужно для тестов/отладки)
    bool resolveFlags(const std::string& file, std::vector<std::string>& outFlags, json& outError);

    const std::string& compileCommandsPath() const { return compileCommandsPath_; }

private:
    std::string compileCommandsPath_;
};

} // namespace cppagent
