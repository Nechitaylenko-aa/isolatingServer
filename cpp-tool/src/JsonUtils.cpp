#include "JsonUtils.h"

std::string JsonUtils::escape(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else if (c == '\b') result += "\\b";
        else if (c == '\f') result += "\\f";
        else result += c;
    }
    return result;
}
