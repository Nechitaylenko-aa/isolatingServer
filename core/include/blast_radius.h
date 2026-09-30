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

// Кросс-TU с кэшем через ProjectIndex (L1 mem + L2 JsonFileStore).
// compileCommandsPath — путь к compile_commands.json, cachePath — путь к кэшу (напр. build/.cpp-tool-cache/index.json).
// Если cachePath пустой — берётся <dir_of_compileCommands>/.cpp-tool-cache/index.json.
json queryBlastRadiusCross(const std::string& compileCommandsPath,
                           const std::string& file,
                           const std::string& className,
                           const std::string& methodUSR,
                           const std::vector<std::string>& flags,
                           const std::string& cachePath = "");
} // namespace cpptool
