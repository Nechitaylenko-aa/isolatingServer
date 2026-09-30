#include "clang_index.h"
#include <sstream>

namespace cpptool {

std::string cxStringToStd(CXString s) {
    const char* cstr = clang_getCString(s);
    std::string result = cstr ? cstr : "";
    clang_disposeString(s);
    return result;
}

std::optional<ParsedUnit> ParsedUnit::parse(const std::string& file,
                                             const std::vector<std::string>& args) {
    CXIndex idx = clang_createIndex(/*excludeDeclarationsFromPCH=*/0, /*displayDiagnostics=*/0);

    std::vector<const char*> cargs;
    cargs.reserve(args.size());
    for (auto& a : args) cargs.push_back(a.c_str());

    unsigned options = CXTranslationUnit_DetailedPreprocessingRecord;

    CXTranslationUnit tu = nullptr;
    CXErrorCode err = clang_parseTranslationUnit2(
        idx, file.c_str(),
        cargs.data(), static_cast<int>(cargs.size()),
        nullptr, 0,
        options,
        &tu
    );

    if (err != CXError_Success || tu == nullptr) {
        if (tu) clang_disposeTranslationUnit(tu);
        clang_disposeIndex(idx);
        return std::nullopt;
    }

    return ParsedUnit(idx, tu);
}

ParsedUnit::ParsedUnit(ParsedUnit&& other) noexcept
    : idx_(other.idx_), tu_(other.tu_) {
    other.idx_ = nullptr;
    other.tu_ = nullptr;
}

ParsedUnit::~ParsedUnit() {
    if (tu_) clang_disposeTranslationUnit(tu_);
    if (idx_) clang_disposeIndex(idx_);
}

CXCursor ParsedUnit::rootCursor() const {
    return clang_getTranslationUnitCursor(tu_);
}

std::vector<std::string> ParsedUnit::diagnostics() const {
    std::vector<std::string> out;
    unsigned n = clang_getNumDiagnostics(tu_);
    for (unsigned i = 0; i < n; ++i) {
        CXDiagnostic diag = clang_getDiagnostic(tu_, i);
        out.push_back(cxStringToStd(clang_getDiagnosticSpelling(diag)));
        clang_disposeDiagnostic(diag);
    }
    return out;
}

std::string cursorUSR(CXCursor c) { return cxStringToStd(clang_getCursorUSR(c)); }
std::string cursorSpelling(CXCursor c) { return cxStringToStd(clang_getCursorSpelling(c)); }
std::string cursorKindName(CXCursor c) { return cxStringToStd(clang_getCursorKindSpelling(clang_getCursorKind(c))); }

std::string cursorFile(CXCursor c) {
    CXSourceLocation loc = clang_getCursorLocation(c);
    CXFile file; unsigned line, col, offset;
    clang_getSpellingLocation(loc, &file, &line, &col, &offset);
    if (!file) return "";
    return cxStringToStd(clang_getFileName(file));
}

unsigned cursorLine(CXCursor c) {
    CXSourceLocation loc = clang_getCursorLocation(c);
    CXFile file; unsigned line, col, offset;
    clang_getSpellingLocation(loc, &file, &line, &col, &offset);
    return line;
}

unsigned cursorColumn(CXCursor c) {
    CXSourceLocation loc = clang_getCursorLocation(c);
    CXFile file; unsigned line, col, offset;
    clang_getSpellingLocation(loc, &file, &line, &col, &offset);
    return col;
}

std::string buildMethodSignature(CXCursor methodCursor) {
    std::ostringstream sig;

    CXCursorKind k = clang_getCursorKind(methodCursor);
    bool isCtorOrDtor = (k == CXCursor_Constructor || k == CXCursor_Destructor);

    if (clang_CXXMethod_isStatic(methodCursor)) sig << "static ";
    if (clang_CXXMethod_isVirtual(methodCursor)) sig << "virtual ";

    if (!isCtorOrDtor) {
        CXType resultType = clang_getCursorResultType(methodCursor);
        sig << cxStringToStd(clang_getTypeSpelling(resultType)) << " ";
    }
    sig << cursorSpelling(methodCursor) << "(";

    int numArgs = clang_Cursor_getNumArguments(methodCursor);
    for (int i = 0; i < numArgs; ++i) {
        if (i > 0) sig << ", ";
        CXCursor argCursor = clang_Cursor_getArgument(methodCursor, i);
        CXType argType = clang_getCursorType(argCursor);
        sig << cxStringToStd(clang_getTypeSpelling(argType));
    }
    sig << ")";

    if (clang_CXXMethod_isConst(methodCursor)) sig << " const";
    if (clang_CXXMethod_isPureVirtual(methodCursor)) sig << " = 0";

    return sig.str();
}

} // namespace cpptool
