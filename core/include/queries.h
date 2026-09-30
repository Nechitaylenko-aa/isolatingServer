#pragma once
#include "json.hpp"
#include <string>
#include <vector>

namespace cpptool {

using json = nlohmann::json;

// query.locate_symbol: file+line+col -> USR/kind/spelling/enclosing class
json queryLocateSymbol(const std::string& file, unsigned line, unsigned col,
                        const std::vector<std::string>& flags);

// query.class_outline: file+class_name -> base_classes[] + public_methods[]
json queryClassOutline(const std::string& file, const std::string& className,
                       const std::vector<std::string>& flags);

// query.symbol_refs: ищет все ссылки на символ с данным USR внутри ОДНОГО TU.
// Ограничение: не агрегирует по всему проекту — для этого пришлось бы
// перепарсивать каждый .cpp из compile_commands.json, что дорого при каждом
// запросе. См. ТЗ: полноценный кросс-файловый индекс ссылок — отдельная
// задача (постоянный индекс с инкрементальным обновлением по source_hash),
// а не то, что можно честно сделать одним query по одному файлу.
json querySymbolRefs(const std::string& targetUSR, const std::string& file,
                      const std::vector<std::string>& flags);

} // namespace cpptool
