/**
 * @file CClangFactory.cpp
 * @brief Реализация фабрики backend'а libclang.
 */

#include "index/CClangFactory.h"

#include <filesystem>
#include <memory>

#include "index/CClang22.h"

namespace fcpp
{

std::unique_ptr<IClang> create_clang_backend(const std::filesystem::path& clang_path)
{
    (void)clang_path;

    auto backend = std::make_unique<CClang22>();
    if (!backend->is_available())
    {
        return nullptr;
    }
    return backend;
}

} // namespace fcpp
