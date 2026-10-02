#pragma once
#include "json.hpp"
#include <string>
#include <vector>

namespace cpptool {
using json = nlohmann::json;

// Внутри одного TU (дешёвый, для UNDERSTAND/light).
json queryBlastRadius(const std::string& file,
                      const std::string& className,
                      const std::string& methodUSR,
                      const std::vector<std::string>& flags);

// Кросс-TU с постоянным кэшем на SQLite (CTuCache).
// compileCommandsPath — путь к compile_commands.json, cachePath — путь к файлу БД
// (напр. build/.cpp-tool-cache/index.db).
// Если cachePath пустой — берётся <dir_of_compileCommands>/.cpp-tool-cache/index.db.
json queryBlastRadiusCross(const std::string& compileCommandsPath,
                           const std::string& file,
                           const std::string& className,
                           const std::string& methodUSR,
                           const std::vector<std::string>& flags,
                           const std::string& cachePath = "");
} // namespace cpptool
