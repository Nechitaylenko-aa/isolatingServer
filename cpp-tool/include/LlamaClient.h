#pragma once

#include <string>

class LlamaClient {
public:
    static std::string query(const std::string& prompt);

private:
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);
};
