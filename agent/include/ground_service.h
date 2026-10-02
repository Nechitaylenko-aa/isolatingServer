#pragma once
#include "json.hpp"
#include "task_classification.h"
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
    // queryVirtualOverrides — заполняется, только если enclosingMethod.is_virtual==true.
    // {ok:false} по умолчанию значит "не искали" (метод не виртуальный), а не "не нашли" —
    // эти два случая различает вызывающий код по virtualOverridesChecked.
    json virtualOverrides{json{{{"ok", false}}}};
    bool virtualOverridesChecked{false}; // true если enclosingMethod был виртуальным и поиск запускался
    // Слой 2 (ТЗ, arch.md): где мы физически находимся в коде. Вычисляется
    // детерминированно из тех же фактов, что раньше шли в DetailKind (его
    // заменяет — DetailKind было 4 значения только под UNDERSTAND/virtual-кейсы,
    // CursorContext — 10 значений, общих для всех Primitive и всех Detail).
    CursorContext cursorContext{CursorContext::NO_CONTEXT};
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

    // Резолв якоря: USR -> актуальные line/col. Якорь на USR переживает сдвиги строк,
    // которые вносит любая правка файла выше символа (line/col как снимок — не переживает).
    // Возвращает {ok, line, column, is_definition} или {ok:false, error:{...}}.
    json resolveCursorByUsr(const std::string& file, const std::string& usr);

    const std::string& compileCommandsPath() const { return compileCommandsPath_; }

private:
    std::string compileCommandsPath_;
};

} // namespace cppagent
