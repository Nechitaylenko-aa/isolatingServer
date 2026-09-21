#pragma once

#include <string>
#include <vector>

struct MemberInfo {
    std::string name;
    std::string type;
};

struct ClassInfo {
    std::string name;
    std::vector<MemberInfo> members;
    std::vector<std::string> public_methods;
    std::vector<std::string> base_classes;
};

struct MethodInfo {
    std::string signature;
    std::string body;
    std::string full_code;
    std::string return_type;
    std::string class_name;
    std::string method_name;
    std::vector<std::string> params;
    uint32_t start_line;
    uint32_t end_line;
    uint32_t body_start_line;
    uint32_t body_start_col;
    uint32_t body_end_line;
    uint32_t body_end_col;
};
