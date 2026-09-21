#include "Parser.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <set>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

// Глобальные переменные для findMethodVisitor
static MethodInfo found_method;
static bool method_found = false;
static int target_line = 0;
static ClassInfo any_class_info;
static std::string target_class_name;

// Класс для поиска
static ClassInfo target_class;


// Функция для извлечения include директорий из файла
std::vector<std::string> extractIncludePaths(const std::string& filename) {
    std::set<std::string> includeDirs;
    std::ifstream file(filename);
    std::string line;

    // Регулярка для #include "path/to/file.h"
    std::regex userIncludeRegex("#include\\s+\"([^\"]+)\"");

    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, userIncludeRegex)) {
            std::string includePath = match[1].str();

            // Извлекаем директорию
            size_t lastSlash = includePath.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                std::string dir = includePath.substr(0, lastSlash);
                includeDirs.insert(dir);
            }
        }
    }

    return std::vector<std::string>(includeDirs.begin(), includeDirs.end());
}

// Найти реальный путь к директории с заголовком
std::string resolveIncludePath(const std::string& filename, const std::string& includeDir) {
    namespace fs = std::filesystem;
    fs::path currentFile(filename);
    fs::path currentDir = currentFile.parent_path();

    // Ищем относительно текущей директории
    fs::path fullPath = currentDir / includeDir;
    if (fs::exists(fullPath) && fs::is_directory(fullPath)) {
        return fullPath.string();
    }

    // Ищем вверх по дереву
    for (int i = 0; i < 3; i++) {
        if (currentDir.empty()) break;

        fullPath = currentDir / includeDir;
        if (fs::exists(fullPath) && fs::is_directory(fullPath)) {
            return fullPath.string();
        }

        currentDir = currentDir.parent_path();
    }

    return "";
}



CXChildVisitResult classVisitor(CXCursor cursor, CXCursor parent, CXClientData data) {
    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) {
        CXString name = clang_getCursorSpelling(cursor);
        std::string cursor_name = clang_getCString(name);
        clang_disposeString(name);

        if (cursor_name == target_class.name) {
            clang_visitChildren(cursor, [](CXCursor child, CXCursor parent, CXClientData data) {
                ClassInfo* cls = static_cast<ClassInfo*>(data);
                CXCursorKind child_kind = clang_getCursorKind(child);

                if (child_kind == CXCursor_FieldDecl) {
                    CXString member_name = clang_getCursorSpelling(child);
                    CXType member_type = clang_getCursorType(child);
                    CXString type_spelling = clang_getTypeSpelling(member_type);

                    MemberInfo member;
                    member.name = clang_getCString(member_name);
                    member.type = clang_getCString(type_spelling);
                    cls->members.push_back(member);

                    clang_disposeString(member_name);
                    clang_disposeString(type_spelling);
                } else if (child_kind == CXCursor_CXXMethod) {
                    unsigned access = clang_getCXXAccessSpecifier(child);
                    if (access == CX_CXXPublic) {
                        CXString method_name = clang_getCursorSpelling(child);
                        cls->public_methods.push_back(clang_getCString(method_name));
                        clang_disposeString(method_name);
                    }
                }
                return CXChildVisit_Continue;
            }, data);
            return CXChildVisit_Break;
        }
    }
    return CXChildVisit_Continue;
}

// Реализация ClangParser
ClangParser::ClangParser(const std::string& filename) : filename_(filename) {
    index = clang_createIndex(1, 0);

    // Базовые аргументы
    std::vector<const char*> args;
    args.push_back("-std=c++17");
    args.push_back("-I.");  // текущая директория

    // Извлекаем include директории из файла
    auto includeDirs = extractIncludePaths(filename);
    std::set<std::string> resolvedPaths;

    for (const auto& dir : includeDirs) {
        std::string resolved = resolveIncludePath(filename, dir);
        if (!resolved.empty() && resolvedPaths.find(resolved) == resolvedPaths.end()) {
            resolvedPaths.insert(resolved);
            args.push_back("-I");
            // Нужно сохранить строки, чтобы они не уничтожились
            char* pathCopy = strdup(resolved.c_str());
            args.push_back(pathCopy);
        }
    }

    tu = clang_parseTranslationUnit(
        index,
        filename.c_str(),
                                    args.data(),
                                    args.size(),
                                    nullptr, 0,
                                    CXTranslationUnit_DetailedPreprocessingRecord
    );

    // Очищаем strdup память
    for (size_t i = 0; i < args.size(); i++) {
        if (i > 1 && args[i] != "-I") {
            free((void*)args[i]);
        }
    }
}

ClangParser::~ClangParser() {
    if (tu) clang_disposeTranslationUnit(tu);
    if (index) clang_disposeIndex(index);
}

std::string ClangParser::readFile() {
    FILE* f = fopen(filename_.c_str(), "r");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content;
    content.resize(size);
    fread(&content[0], 1, size, f);
    fclose(f);
    return content;
}

std::string ClangParser::extractBody(int start_line, int end_line) {
    std::string content = readFile();
    std::istringstream stream(content);
    std::string line;
    std::string result;
    int current_line = 1;

    while (std::getline(stream, line)) {
        if (current_line >= start_line && current_line <= end_line) {
            result += line + "\n";
        }
        current_line++;
    }
    return result;
}

MethodInfo ClangParser::findMethodAtPosition(int line, int col) {
    method_found = false;
    target_line = line;
    found_method = MethodInfo();

    CXFile file = clang_getFile(tu, filename_.c_str());
    CXSourceLocation location = clang_getLocation(tu, file, line, col);
    CXCursor cursor = clang_getCursor(tu, location);

    if (clang_Cursor_isNull(cursor)) {
        return found_method;
    }

    while (!clang_Cursor_isNull(cursor)) {
        CXCursorKind kind = clang_getCursorKind(cursor);
        CXString kind_name = clang_getCursorKindSpelling(kind);
        CXString cursor_name = clang_getCursorSpelling(cursor);

        clang_disposeString(kind_name);
        clang_disposeString(cursor_name);

        if (kind == CXCursor_CXXMethod || kind == CXCursor_FunctionDecl ||
            kind == CXCursor_Constructor || kind == CXCursor_Destructor) {

            CXString name = clang_getCursorSpelling(cursor);
            found_method.method_name = clang_getCString(name);
            clang_disposeString(name);

            CXString display = clang_getCursorDisplayName(cursor);
            found_method.signature = clang_getCString(display);
            clang_disposeString(display);

            CXType return_type = clang_getCursorResultType(cursor);
            if (return_type.kind != CXType_Void) {
                CXString ret_spelling = clang_getTypeSpelling(return_type);
                found_method.return_type = clang_getCString(ret_spelling);
                clang_disposeString(ret_spelling);
            } else {
                found_method.return_type = "void";
            }

            if (kind == CXCursor_CXXMethod) {
                CXCursor parent_cursor = clang_getCursorSemanticParent(cursor);
                CXString parent_name = clang_getCursorSpelling(parent_cursor);
                found_method.class_name = clang_getCString(parent_name);
                clang_disposeString(parent_name);
            }

            CXSourceRange range = clang_getCursorExtent(cursor);
            CXSourceLocation start = clang_getRangeStart(range);
            CXSourceLocation end = clang_getRangeEnd(range);
            unsigned start_line, end_line;
            clang_getSpellingLocation(start, nullptr, &start_line, nullptr, nullptr);
            clang_getSpellingLocation(end, nullptr, &end_line, nullptr, nullptr);
            found_method.start_line = start_line;
            found_method.end_line = end_line;

            // ========== ДОБАВЛЯЕМ КООРДИНАТЫ ТЕЛА ==========
            // Получаем точные координаты тела метода
            CXSourceLocation body_start_loc = clang_getRangeStart(range);
            CXSourceLocation body_end_loc = clang_getRangeEnd(range);

            clang_getSpellingLocation(body_start_loc, nullptr,
                                      &found_method.body_start_line,
                                      &found_method.body_start_col, nullptr);
            clang_getSpellingLocation(body_end_loc, nullptr,
                                      &found_method.body_end_line,
                                      &found_method.body_end_col, nullptr);
            // =============================================

            int num_args = clang_Cursor_getNumArguments(cursor);
            for (int i = 0; i < num_args; i++) {
                CXCursor arg = clang_Cursor_getArgument(cursor, i);
                CXString arg_name = clang_getCursorSpelling(arg);
                CXType arg_type = clang_getCursorType(arg);
                CXString type_spelling = clang_getTypeSpelling(arg_type);

                std::string param = std::string(clang_getCString(type_spelling));
                std::string param_name = clang_getCString(arg_name);
                if (!param_name.empty()) {
                    param += " " + param_name;
                }
                found_method.params.push_back(param);

                clang_disposeString(arg_name);
                clang_disposeString(type_spelling);
            }

            method_found = true;
            break;
        }

        cursor = clang_getCursorSemanticParent(cursor);
    }

    if (method_found) {
        found_method.full_code = extractBody(found_method.start_line, found_method.end_line);

        size_t bracePos = found_method.full_code.find('{');
        if (bracePos != std::string::npos) {
            found_method.body = found_method.full_code.substr(bracePos);
            std::string clean_signature = found_method.full_code.substr(0, bracePos);
            while (!clean_signature.empty() &&
                   (clean_signature.back() == ' ' || clean_signature.back() == '\t')) {
                clean_signature.pop_back();
            }
            found_method.signature = clean_signature;
        }
    }

    return found_method;
}

ClassInfo ClangParser::getClassInfo(const std::string& class_name) {
    target_class = ClassInfo();
    target_class.name = class_name;

    CXCursor cursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(cursor, classVisitor, &target_class);

    return target_class;
}


CXChildVisitResult anyClassVisitor(CXCursor cursor, CXCursor parent, CXClientData data) {
    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) {
        CXString name = clang_getCursorSpelling(cursor);
        std::string cursor_name = clang_getCString(name);
        clang_disposeString(name);

        if (cursor_name == target_class_name) {
            // Собираем публичные методы
            clang_visitChildren(cursor, [](CXCursor child, CXCursor parent, CXClientData data) {
                ClassInfo* cls = static_cast<ClassInfo*>(data);
                CXCursorKind child_kind = clang_getCursorKind(child);

                if (child_kind == CXCursor_CXXMethod) {
                    unsigned access = clang_getCXXAccessSpecifier(child);
                    if (access == CX_CXXPublic) {
                        CXString method_name = clang_getCursorSpelling(child);
                        cls->public_methods.push_back(clang_getCString(method_name));
                        clang_disposeString(method_name);
                    }
                }
                return CXChildVisit_Continue;
            }, data);
            return CXChildVisit_Break;
        }
    }
    return CXChildVisit_Continue;
}

ClassInfo ClangParser::getClassInfoByName(const std::string& class_name) {
    any_class_info = ClassInfo();
    any_class_info.name = class_name;
    target_class_name = class_name;

    CXCursor cursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(cursor, anyClassVisitor, &any_class_info);

    return any_class_info;
}
