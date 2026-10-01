#include "execute_service.h"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>

namespace cppagent {
namespace fs = std::filesystem;

namespace {

// Нормализация: заменяем все пробелы/табы/переносы на один пробел,
// обрезаем края. Для сравнения oldText с содержимым файла.
std::string normalizeWhitespace(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool prevSpace = false;
    for(char c : s) {
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if(!prevSpace) { out += ' '; prevSpace = true; }
        } else {
            out += c;
            prevSpace = false;
        }
    }
    // trim
    size_t b = out.find_first_not_of(' ');
    size_t e = out.find_last_not_of(' ');
    if(b == std::string::npos) return "";
    return out.substr(b, e - b + 1);
}

// Убираем все пробелы/табы/переносы — для поиска по содержимому
std::string stripAllWhitespace(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for(char c : s)
        if(c != ' ' && c != '\t' && c != '\n' && c != '\r') out += c;
    return out;
}

struct FuzzyMatch {
    bool found = false;
    size_t pos = 0;        // позиция в оригинальном content
    size_t len = 0;        // длина куска в оригинальном content
    std::string matchType; // "exact" | "normalized" | "prefix"
    double score = 0.0;    // 1.0 = exact, ниже = fuzzy
};

// Номер строки (1-based) для смещения в content.
static size_t lineOfOffset(const std::string& content, size_t off) {
    size_t line = 1;
    for (size_t i = 0; i < off && i < content.size(); ++i)
        if (content[i] == '\n') ++line;
    return line;
}

// Выбирает вхождение needle в haystack, ближайшее по строке к hintLine (1-based).
// mappingIdx переводит позицию в haystack в смещение в content.
// Если hintLine == 0 — первое вхождение (прежнее поведение).
template <typename Fn>
static size_t nearestOccurrence(const std::string& haystack, const std::string& needle,
                                const std::string& content, size_t hintLine, Fn mappingIdx) {
    if (needle.empty()) return std::string::npos;
    std::vector<size_t> hits;
    size_t p = haystack.find(needle);
    while (p != std::string::npos) {
        hits.push_back(p);
        p = haystack.find(needle, p + needle.size());
        if (hits.size() > 512) break; // защита от патологического входа
    }
    if (hits.empty()) return std::string::npos;
    if (hintLine == 0 || hits.size() == 1) return hits.front();
    size_t best = hits.front(), bestDist = (size_t)-1;
    for (size_t h : hits) {
        size_t cOff = mappingIdx(h);
        if (cOff > content.size()) continue;
        size_t ln = lineOfOffset(content, cOff);
        size_t d = (ln > hintLine) ? (ln - hintLine) : (hintLine - ln);
        if (d < bestDist) { bestDist = d; best = h; }
    }
    return best;
}

FuzzyMatch fuzzyFindOldText(const std::string& content, const std::string& oldText, int hintLine) {
    FuzzyMatch result;
    const size_t hint = (hintLine > 0) ? (size_t)hintLine : 0;

    // 1) Точный поиск — при нескольких вхождениях берём ближайшее к курсору
    size_t pos = nearestOccurrence(content, oldText, content, hint, [](size_t x){ return x; });
    if(pos != std::string::npos)
        return {true, pos, oldText.size(), "exact", 1.0};

    // 2) Нормализация пробелов — ищем normalized(oldText) в normalized(content)
    std::string normContent = normalizeWhitespace(content);
    std::string normTarget  = normalizeWhitespace(oldText);
    if(normTarget.empty()) return result;

    {
        // charMap[i] = индекс в content для i-го символа normContent
        std::vector<size_t> charMap;
        charMap.reserve(normContent.size());
        bool prevSpace = false;
        for(size_t ci = 0; ci < content.size(); ++ci) {
            char c = content[ci];
            if(c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                if(!prevSpace) { charMap.push_back(ci); prevSpace = true; }
            } else {
                charMap.push_back(ci);
                prevSpace = false;
            }
        }
        size_t normPos = nearestOccurrence(normContent, normTarget, content, hint,
                                           [&charMap](size_t x){ return x < charMap.size() ? charMap[x] : charMap.empty() ? (size_t)0 : charMap.back(); });
        if(normPos != std::string::npos && normPos < charMap.size() && (normPos + normTarget.size() - 1) < charMap.size()) {
            size_t startInContent = charMap[normPos];
            size_t endInContent = charMap[normPos + normTarget.size() - 1] + 1;
            // Проверяем что не выходим за границы
            if(endInContent <= content.size())
                return {true, startInContent, endInContent - startInContent, "normalized", 0.9};
        }
    }

    // 3) Prefix-match: берём первые 20+ осмысленных символов oldText,
    //    ищем stripAllWhitespace в stripAllWhitespace(content)
    std::string strippedTarget = stripAllWhitespace(oldText);
    if(strippedTarget.size() < 10) return result; // слишком короткий префикс, много ложных срабатываний

    std::string strippedContent = stripAllWhitespace(content);
    size_t prefixLen = std::min<size_t>(strippedTarget.size(), 30);
    std::string prefix = strippedTarget.substr(0, prefixLen);
    // В strip-пространстве свои индексы — переводим их в content и берём ближайшее к курсору
    auto stripToContent = [&content](size_t stripPos) {
        size_t contentIdx = 0, stripIdx = 0;
        while (contentIdx < content.size() && stripIdx < stripPos) {
            char c = content[contentIdx];
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r') ++stripIdx;
            ++contentIdx;
        }
        return contentIdx;
    };
    size_t stripPos = nearestOccurrence(strippedContent, prefix, content, hint, stripToContent);
    if(stripPos == std::string::npos) return result;

    // stripPos — индекс в strippedContent. Нужно найти соответствующий индекс в content.
    size_t contentIdx = 0;
    size_t stripIdx = 0;
    while(contentIdx < content.size() && stripIdx < stripPos) {
        char c = content[contentIdx];
        if(c != ' ' && c != '\t' && c != '\n' && c != '\r') ++stripIdx;
        ++contentIdx;
    }
    // contentIdx — начало совпавшего куска в content. Длина?
    // Ищем конец: strippedTarget целиком в strippedContent?
    size_t stripEnd = stripPos + strippedTarget.size();
    if(stripEnd > strippedContent.size()) return result; // не влезает
    size_t contentEndIdx = contentIdx;
    while(contentEndIdx < content.size() && stripIdx < stripEnd) {
        char c = content[contentEndIdx];
        if(c != ' ' && c != '\t' && c != '\n' && c != '\r') ++stripIdx;
        ++contentEndIdx;
    }
    return {true, contentIdx, contentEndIdx - contentIdx, "prefix", 0.7};
}

// Отступ (пробелы/табы) первой строки куска.
std::string firstLineIndent(const std::string& s) {
    size_t nl = s.find('\n');
    std::string first = s.substr(0, nl == std::string::npos ? s.size() : nl);
    size_t i = 0;
    while(i < first.size() && (first[i] == ' ' || first[i] == '\t')) ++i;
    return first.substr(0, i);
}

// Отступ строки, содержащей смещение pos в content.
std::string indentOfLineAt(const std::string& content, size_t pos) {
    size_t ls = 0;
    if(pos > 0) { size_t nl = content.rfind('\n', pos - 1); ls = (nl == std::string::npos) ? 0 : nl + 1; }
    size_t i = ls;
    while(i < content.size() && (content[i] == ' ' || content[i] == '\t')) ++i;
    return content.substr(ls, i - ls);
}

// Выравнивает newText по отступу найденного места в файле. Модель регулярно
// возвращает newText без ведущих пробелов (переносит в нулевую колонку), тогда как
// oldText в файле стоит с отступом. match_type normalized/exact это не ловит —
// нормализация ищет совпадение при любых отступах, а вставка идёт как есть.
// Считаем delta между отступом файла и отступом oldText и применяем ко всем
// непустым строкам newText — относительные отступы внутри сохраняются.
std::string reindentReplacement(const std::string& content, const FuzzyMatch& fm,
                                const std::string& newText) {
    if(newText.empty()) return newText;
    // Желаемый отступ первой строки = отступ строки в ФАЙЛЕ, где стоит найденное место.
    // Но fm.pos указывает по-разному: "exact" — на начало oldText (может включать
    // ведущие пробелы), "normalized" — на первый НЕпробельный символ (отступ уже
    // остаётся в content перед fm.pos). Поэтому считаем, сколько отступа УЖЕ в префиксе:
    // prefixIndentChars = от lineStart до min(fm.pos, indentEnd). Остаток (desired - уже)
    // должна донести первая строка замены.
    size_t lineStart = 0;
    if(fm.pos > 0){ size_t nl = content.rfind('\n', fm.pos - 1); lineStart = (nl == std::string::npos) ? 0 : nl + 1; }
    size_t indentEnd = lineStart;
    while(indentEnd < content.size() && (content[indentEnd] == ' ' || content[indentEnd] == '\t')) ++indentEnd;
    std::string desiredIndent = content.substr(lineStart, indentEnd - lineStart);
    size_t alreadyInPrefix = (std::min(fm.pos, indentEnd) > lineStart) ? (std::min(fm.pos, indentEnd) - lineStart) : 0;
    std::string wantFirst = (alreadyInPrefix <= desiredIndent.size())
                            ? desiredIndent.substr(alreadyInPrefix) : std::string();

    std::string newIndent = firstLineIndent(newText);
    if(wantFirst == newIndent) return newText; // уже совпадает — не трогаем

    std::string addPrefix;
    bool remove = false;
    size_t removeLen = 0;
    if(wantFirst.size() >= newIndent.size() && wantFirst.compare(0, newIndent.size(), newIndent) == 0){
        addPrefix = wantFirst.substr(newIndent.size());
    } else if(newIndent.size() > wantFirst.size() && newIndent.compare(0, wantFirst.size(), wantFirst) == 0){
        remove = true;
        removeLen = newIndent.size() - wantFirst.size();
    } else {
        return newText; // несопоставимые отступы (таб vs пробел) — не угадываем
    }

    std::string out;
    std::istringstream iss(newText);
    std::string line;
    bool first = true;
    while(std::getline(iss, line)){
        if(!first) out += '\n';
        first = false;
        if(line.empty()) continue;
        if(remove){
            size_t k = 0;
            while(k < removeLen && k < line.size() && (line[k] == ' ' || line[k] == '\t')) ++k;
            out += line.substr(k);
        } else {
            out += addPrefix + line;
        }
    }
    if(!newText.empty() && newText.back() == '\n') out += '\n';
    return out;
}

} // anon fuzzy

std::string ExecuteService::newId() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::random_device rd; std::mt19937_64 g(rd());
    return std::to_string(now) + "-" + std::to_string(g() % 100000);
}

ApplyResult ExecuteService::apply(const std::vector<EditAction>& edits) {
    ApplyResult r;
    if (edits.empty()) {
        r.ok = true;
        r.error = "";
        return r;
    }
    std::string id = newId();
    fs::path chkDir = fs::path(checkpointRoot_) / id;
    std::error_code ec;
    fs::create_directories(chkDir, ec);
    if (ec) { r.error = "не удалось создать checkpoint dir: " + ec.message(); return r; }
    r.checkpointId = id;

    // rollback того, что уже применили в этом же вызове, если провалимся на следующей правке.
    // Бэкапы уже лежат в chkDir — просто восстанавливаем из них всё, что успело примениться.
    std::vector<std::pair<std::string,std::string>> appliedBackups; // {origFile, bakPath}
    auto rollbackApplied = [&](){
        for (auto& [origFile, bakPath] : appliedBackups) {
            std::error_code rec;
            fs::copy_file(bakPath, origFile, fs::copy_options::overwrite_existing, rec);
        }
        r.appliedFiles.clear();
        r.matchInfo = json::array();
    };

    for (auto& e : edits) 
    {
        fs::path filePath(e.file);
        if (!fs::exists(filePath)) { r.error = "файл не найден: " + e.file; rollbackApplied(); return r; }
        // backup: копия с сохранением относительного пути (по abs -> _abs_...)
        // safe-имя через хеш чтобы не путать '_' в пути с '/' (как в /tmp/cpp_tool_test_...)
        size_t h = std::hash<std::string>{}(e.file);
        std::string safe = std::to_string(h) + ".bak";
        fs::path bak = chkDir / safe;
        fs::copy_file(filePath, bak, fs::copy_options::overwrite_existing, ec);
        if (ec) { r.error = "backup failed " + e.file + ": " + ec.message(); rollbackApplied(); return r; }
        // манифест: какой хеш -> какой оригинальный путь
        {
            fs::path mf = chkDir / "manifest.json";
            json m;
            if (fs::exists(mf)) { std::ifstream min(mf); try{ min>>m; }catch(...){ m=json::object(); } }
            m[safe] = e.file;
            std::ofstream mo(mf, std::ios::trunc); mo << m.dump(2);
        }

        std::ifstream in(e.file);
        std::ostringstream ss; ss << in.rdbuf();
        std::string content = ss.str();
        std::string out;
        std::string matchType;
        double matchScore = 1.0;
        if (e.oldText.empty()) {
            out = content + e.newText;
            matchType = "insert";
        } else {
            FuzzyMatch fm = fuzzyFindOldText(content, e.oldText, e.hintLine);
            if (!fm.found) { r.error = "oldText не найден в " + e.file + " (ни exact, ни normalized, ни prefix не сработали)"; rollbackApplied(); return r; }
            matchType = fm.matchType;
            matchScore = fm.score;
            std::string replacement = reindentReplacement(content, fm, e.newText);
            out = content.substr(0, fm.pos) + replacement + content.substr(fm.pos + fm.len);
        }
        std::ofstream outFile(e.file, std::ios::trunc);
        if (!outFile) 
        { 
            r.error = "не удалось записать " + e.file; 
            rollbackApplied();
            return r; 
        }

        outFile << out;
        appliedBackups.push_back({e.file, bak.string()});
        r.appliedFiles.push_back(e.file);
        r.matchInfo.push_back(json{{"file", e.file}, {"match_type", matchType}, {"score", matchScore}});
    }
    r.ok = true;
    return r;
}

bool ExecuteService::undo(const std::string& checkpointId, std::string& error) 
{
    fs::path chkDir = fs::path(checkpointRoot_) / checkpointId;
    if (!fs::exists(chkDir)) { error = "checkpoint не найден: " + checkpointId; return false; }
    fs::path mf = chkDir / "manifest.json";
    if (!fs::exists(mf)) { error = "манифест не найден в checkpoint " + checkpointId; return false; }
    json m; { std::ifstream in(mf); try{ in>>m; }catch(const std::exception& e){ error=std::string("манифест не JSON: ")+e.what(); return false; } }
    
    for (auto& [safe, origJson] : m.items()) 
    {
        std::string orig = origJson.get<std::string>();
        fs::path bak = chkDir / safe;
        if (!fs::exists(bak)) { error = "бекап не найден: " + safe; return false; }
        std::error_code ec;
        fs::copy_file(bak, fs::path(orig), fs::copy_options::overwrite_existing, ec);
        if (ec) 
        { 
            error = "undo failed " + orig + ": " + ec.message(); 
            return false; 
        }
    }
    return true;
}

} // namespace cppagent
