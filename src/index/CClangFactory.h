/**
 * @file CClangFactory.h
 * @brief Фабрика backend'а libclang (ADR-11).
 */

#ifndef FCPP_AGENT_CCLANGFACTORY_H
#define FCPP_AGENT_CCLANGFACTORY_H

#include <filesystem>
#include <memory>

#include "fcpp-agent/IClang.h"

namespace fcpp
{

/**
 * @brief Создать backend IClang.
 *
 * Дефолт — clang 22 (ADR-06). Аргумент clang_path зарезервирован для мульти-
 * версионного выбора; при сборке используется слинкованная версия libclang.
 * @param clang_path Путь к libclang/инструменту (пусто — дефолт).
 * @return Backend либо nullptr при недоступности.
 */
std::unique_ptr<IClang> create_clang_backend(const std::filesystem::path& clang_path);

} // namespace fcpp

#endif // FCPP_AGENT_CCLANGFACTORY_H
