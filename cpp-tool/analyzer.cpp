#include "Parser.h"
#include "LlamaClient.h"
#include "JsonUtils.h"
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <regex>


std::set<std::string> extractMemberAccesses(const std::string& body, const ClassInfo& current_class) {
    std::set<std::string> members;

    // Сначала соберём все имена членов класса для быстрого поиска
    std::set<std::string> memberNames;
    for (const auto& member : current_class.members) {
        memberNames.insert(member.name);
    }


    std::regex memberRegex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s*(->|\.))");
    std::smatch match;
    std::string::const_iterator searchStart(body.cbegin());

    while (std::regex_search(searchStart, body.cend(), match, memberRegex)) {
        std::string candidate = match[1].str();
        if (memberNames.find(candidate) != memberNames.end()) {
            members.insert(candidate);
        }
        searchStart = match[0].second;
    }

    return members;
}

// Очистить тип от указателей и пробелов
std::string cleanType(const std::string& type) {
    std::string result = type;
    // Убираем * в конце
    if (!result.empty() && result.back() == '*') {
        result.pop_back();
    }
    // Убираем пробелы
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    // Убираем const и &
    if (result.find("const ") == 0) {
        result = result.substr(6);
    }
    if (!result.empty() && result.back() == '&') {
        result.pop_back();
    }
    return result;
}

// Проверить, является ли тип пользовательским (не примитивом)
bool isUserType(const std::string& type) {
    static const std::set<std::string> primitives = {
        "int", "float", "double", "char", "bool", "void",
        "size_t", "uint32_t", "uint64_t", "int32_t", "int64_t"
    };

    std::string cleaned = cleanType(type);

    // Если содержит :: или начинается с большой буквы — скорее всего пользовательский
    if (cleaned.find("::") != std::string::npos) {
        return true;
    }
    if (!cleaned.empty() && isupper(cleaned[0])) {
        return true;
    }
    if (primitives.find(cleaned) != primitives.end()) {
        return false;
    }
    // Если это std::vector или подобное — извлекаем внутренний тип
    if (cleaned.find("std::vector<") == 0) {
        size_t start = cleaned.find('<') + 1;
        size_t end = cleaned.rfind('>');
        if (end != std::string::npos) {
            std::string inner = cleaned.substr(start, end - start);
            return isUserType(inner);
        }
    }

    return false;
}

std::string buildContext(const MethodInfo& method, const ClassInfo& current_class,
                         ClangParser& parser, const std::string& goal) {
    std::stringstream ss;

    // 1. Код метода
    ss << "```cpp\n" << method.full_code << "\n```\n\n";

    // 2. Члены класса (только используемые)
    auto usedMembers = extractMemberAccesses(method.body, current_class);
    if (!usedMembers.empty()) {
        ss << "Члены класса, используемые в методе:\n";
        for (const auto& memberName : usedMembers) {
            for (const auto& member : current_class.members) {
                if (member.name == memberName) {
                    ss << "- `" << member.name << "`: " << member.type;
                    if (member.type.find('*') != std::string::npos) {
                        ss << " (указатель, возможен nullptr)";
                    }
                    ss << "\n";
                    break;
                }
            }
        }
        ss << "\n";
    }

    // 3. Публичные методы используемых типов (только если есть)
    for (const auto& memberName : usedMembers) {
        for (const auto& member : current_class.members) {
            if (member.name == memberName) {
                std::string memberType = cleanType(member.type);
                ClassInfo typeInfo = parser.getClassInfoByName(memberType);
                if (!typeInfo.public_methods.empty()) {
                    ss << "Публичные методы `" << memberType << "`:\n";
                    for (const auto& m : typeInfo.public_methods) {
                        ss << "- " << m << "\n";
                    }
                    ss << "\n";
                }
                break;
            }
        }
    }

    // 4. Задание
    ss << goal << "\n";

    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        return 1;
    }

    std::string filename = argv[1];
    int line = std::stoi(argv[2]);
    int col = std::stoi(argv[3]);
    std::string goal = argc > 4 ? argv[4] : "Добавить проверки на nullptr и обработку ошибок";

    ClangParser parser(filename);
    MethodInfo method = parser.findMethodAtPosition(line, col);

    if (method.class_name.empty()) {
        std::cout << R"({"error": "Method not found at line )" << line << "\"}\n";
        return 1;
    }

    ClassInfo current_class = parser.getClassInfo(method.class_name);

    std::string context = buildContext(method, current_class, parser, goal);

    std::string result = LlamaClient::query(context);


    std::cout << "{\n";
    std::cout << R"(  "method": ")" << JsonUtils::escape(method.method_name) << "\",\n";
    std::cout << R"(  "class": ")" << JsonUtils::escape(method.class_name) << "\",\n";
    std::cout << R"(  "signature": ")" << JsonUtils::escape(method.signature) << "\",\n";
    std::cout << "  \"body_start_line\": " << method.body_start_line << ",\n";
    std::cout << "  \"body_start_col\": " << method.body_start_col << ",\n";
    std::cout << "  \"body_end_line\": " << method.body_end_line << ",\n";
    std::cout << "  \"body_end_col\": " << method.body_end_col << ",\n";
    std::cout << R"(  "context": ")" << JsonUtils::escape(context) << "\",\n";
    std::cout << R"(  "suggestion": ")" << JsonUtils::escape(result) << "\"\n";
    std::cout << "}\n";

    return 0;
}
