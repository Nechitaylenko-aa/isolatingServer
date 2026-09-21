#pragma once

#include <clang-c/Index.h>
#include <string>
#include "Types.h"

class ClangParser {
public:
    explicit ClangParser(const std::string& filename);
    ~ClangParser();

    MethodInfo findMethodAtPosition(int line, int col);
    ClassInfo getClassInfo(const std::string& class_name);
    ClassInfo getClassInfoByName(const std::string& class_name);  // Новый метод

private:
    std::string filename_;
    CXIndex index;
    CXTranslationUnit tu;

    std::string readFile();
    std::string extractBody(int start_line, int end_line);
    //CXSourceRange getMethodBodyRange(CXCursor methodCursor);
};
