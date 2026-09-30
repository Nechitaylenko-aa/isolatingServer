#pragma once
#include "json.hpp"
#include <string>

namespace cpptool {

using json = nlohmann::json;

// query.build_graph: читает граф целей CMake через File API (codemodel-v2).
// buildDir — путь к каталогу сборки (тот, где лежит CMakeCache.txt).
// Если реального ответа ещё нет (первый вызов на этом build-каталоге),
// регистрирует запрос и переконфигурирует проект (см. build_graph.cpp) —
// это единственная операция в этом файле, которая может иметь побочный
// эффект (быстрый повторный `cmake <buildDir>`), а не просто читает.
json queryBuildGraph(const std::string& buildDir);

} // namespace cpptool
