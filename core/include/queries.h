#pragma once
#include "json.hpp"
#include <memory>
#include <string>
#include <vector>

namespace cpptool {

using json = nlohmann::json;

class CTuCache;

// Общий экземпляр кэша на путь БД (см. CTuCache::shared). Единая точка входа
// для всех query-функций: вместо `CTuCache cache(path)` — `makeQueryCache(path)`,
// чтобы в рамках одного прогона скан проекта выполнялся один раз, а не на
// каждый под-запрос.
std::shared_ptr<CTuCache> makeQueryCache(const std::string& compileCommandsPath);

// query.locate_symbol: file+line+col -> USR/kind/enclosing method/enclosing class.
// Источник — CTuCache (SQLite): ensure_project_indexed + чистый SELECT, ни одного
// ParsedUnit::parse внутри. file может быть заголовком без своей записи в
// compile_commands.json — decl для него заполнится из ЛЮБОГО TU, который его
// #include-ит, при условии что индекс уже строился хотя бы раз.
json queryLocateSymbol(const std::string& file, unsigned line, unsigned col,
                        const std::string& compileCommandsPath);

// query.class_outline: имя класса (среди ВСЕХ decl проекта) -> base_classes[] +
// public_methods[]. file — необязательная подсказка для разрешения неоднозначности
// при нескольких классах с одинаковым именем (предпочитается совпадение по файлу).
json queryClassOutline(const std::string& className, const std::string& compileCommandsPath,
                       const std::string& fileHint = "");

// query.locate_usr: USR -> актуальные line/col определения символа.
// decl теперь глобален по usr — file/flags больше не нужны вовсе, поиск не
// привязан к тому, в каком TU символ был впервые встречен.
// Возвращает {ok, line, column, file, is_definition} либо {ok:false, error:{code:"usr_not_found"}}.
json queryLocateByUSR(const std::string& usr, const std::string& compileCommandsPath);

// query.symbol_refs: ищет все ссылки на символ с данным USR внутри ОДНОГО TU.
// Ограничение: не агрегирует по всему проекту — для этого пришлось бы
// перепарсивать каждый .cpp из compile_commands.json, что дорого при каждом
// запросе. См. ТЗ: полноценный кросс-файловый индекс ссылок — отдельная
// задача (постоянный индекс с инкрементальным обновлением по source_hash),
// а не то, что можно честно сделать одним query по одному файлу.
json querySymbolRefs(const std::string& targetUSR, const std::string& file,
                      const std::vector<std::string>& flags);

// query.call_chain: USR метода -> кто его ВЫЗЫВАЕТ (входящие рёбра графа
// вызовов по всему проекту, из таблицы calls кэша).
// Возвращает {ok, target_usr, direction:"incoming", calls:[{usr,signature,file,line,call_file,call_line}]}.
json queryCallChain(const std::string& targetUSR, const std::string& compileCommandsPath);

// query.call_subtree: USR метода -> кого ВЫЗЫВАЕТ он сам (исходящие рёбра).
// Возвращает {ok, target_usr, direction:"outgoing", calls:[...]}.
json queryCallSubtree(const std::string& targetUSR, const std::string& compileCommandsPath);

// query.dependency_map: файл -> кто его подключает (dependents) и что
// подключает он сам (dependencies), из таблицы tu_dep кэша. file может быть
// заголовком без своей записи в compile_commands.json.
json queryDependencyMap(const std::string& file, const std::string& compileCommandsPath);

// query.todo_scan: находит TODO/FIXME/HACK/XXX в файле (если задан) или во всех
// файлах класса (classUsr). {ok, markers:[{file,line,marker,text}], files_scanned}.
json queryTodoScan(const std::string& compileCommandsPath, const std::string& file,
                   const std::string& classUsr);

// query.doc_check: проверяет наличие заголовочного doc-комментария (///, //!,
// /**, /*!) в начале файла(ов). {ok, undocumented:[{file,line}], checked:[...]}.
json queryDocCheck(const std::string& compileCommandsPath, const std::string& file,
                   const std::string& classUsr);

// query.type_usage: где используется тип (класс/структура) с данным USR —
// список мест из refs. {ok, locations:[{file,line,column}]}.
json queryTypeUsage(const std::string& typeUsr, const std::string& compileCommandsPath);

// query.complexity_hotspot: методы области (file или classUsr) с метриками
// (длина, макс. вложенность, длинные строки), отсортированные по убыванию.
json queryComplexityHotspot(const std::string& compileCommandsPath, const std::string& file,
                            const std::string& classUsr);

// query.duplicate_find: ищет методы с одинаковой нормализованной сигнатурой
// (потенциальные дубликаты). {ok, groups:[{signature, members:[{file,line,name}]}]}.
json queryDuplicateFind(const std::string& compileCommandsPath, const std::string& file,
                        const std::string& classUsr);

// query.virtual_overrides: USR метода (базового ИЛИ любого sibling-override) ->
// все переопределения базового метода во всех TU проекта. Источник — overrides+decl
// в CTuCache; если targetUSR сам override, подъём к базовому делает сам CTuCache
// (один SELECT), повторный парсинг TU для этого больше не требуется.
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
