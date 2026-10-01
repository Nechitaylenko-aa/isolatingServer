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

// query.locate_usr: USR -> актуальные line/col определения символа в указанном файле.
// Нужен потому, что cursor.line — снимок: любая правка файла выше сдвигает строки,
// и сохранённый line/col начинает указывать в пустоту. Якорь на USR переживает сдвиги.
// Возвращает {ok, line, column, file} либо {ok:false, error:{code:"usr_not_found"}}.
json queryLocateByUSR(const std::string& file, const std::string& usr,
                      const std::vector<std::string>& flags);

// query.symbol_refs: ищет все ссылки на символ с данным USR внутри ОДНОГО TU.
// Ограничение: не агрегирует по всему проекту — для этого пришлось бы
// перепарсивать каждый .cpp из compile_commands.json, что дорого при каждом
// запросе. См. ТЗ: полноценный кросс-файловый индекс ссылок — отдельная
// задача (постоянный индекс с инкрементальным обновлением по source_hash),
// а не то, что можно честно сделать одним query по одному файлу.
json querySymbolRefs(const std::string& targetUSR, const std::string& file,
                      const std::vector<std::string>& flags);

// query.virtual_overrides: USR метода -> все определения-переопределения во ВСЕХ TU проекта.
// Нужен для C++-иерархий: UNDERSTAND под курсором видит только базовый/диспетчерский метод,
// а реализации чистого virtual лежат в классах-потомках в других .cpp. Матч по USR базового
// метода через clang_getOverriddenCursors (не по имени — имена ненадёжны).
json queryVirtualOverrides(const std::string& compileCommandsPath,
                           const std::string& targetUSR);

// query.find_implementors: по имени базового класса и имени метода находит все
// конкретные реализации этого метода в наследниках (транзитивно), итерируясь по
// всем TU из compile_commands.json. Работает без фонового индекса clangd и
// без необходимости собираемости всего проекта.
// Возвращает {ok, implementors:[{class,signature,file,line,usr}], descendants, files_scanned, files_total}
json queryFindImplementors(const std::string& compileCommandsPath,
                           const std::string& baseClassName,
                           const std::string& methodName);

// query.clangd_implementations: запускает clangd как subprocess, отправляет
// textDocument/implementation (LSP) и возвращает все реализации виртуального метода.
// Работает даже для заголовков, которых нет в compile_commands.json, т.к. clangd
// строит персистентный индекс всего проекта. Для больших проектов (500+ TU) это
// быстрее, чем перепарсивать все TU через libclang напрямую.
// line и col — 1-based.
json queryClangdImplementations(const std::string& compileCommandsPath,
                                const std::string& file,
                                unsigned line,
                                unsigned col);

} // namespace cpptool
