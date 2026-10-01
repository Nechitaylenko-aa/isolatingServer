set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.19)
project(cli_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- libclang (для core) ---
# Авто-детект: Debian/Ubuntu -> /usr/lib/llvm-19, FreeBSD -> /usr/local/llvm19
if(NOT LIBCLANG_LIBRARY_DIR)
    if(EXISTS "/usr/lib/llvm-19/lib/libclang.so")
        set(LIBCLANG_LIBRARY_DIR "/usr/lib/llvm-19/lib")
    elseif(EXISTS "/usr/lib/llvm-19/lib/libclang-19.so.1")
        set(LIBCLANG_LIBRARY_DIR "/usr/lib/llvm-19/lib")
    elseif(EXISTS "/usr/lib/x86_64-linux-gnu/libclang-19.so.19")
        set(LIBCLANG_LIBRARY_DIR "/usr/lib/x86_64-linux-gnu")
    else()
        set(LIBCLANG_LIBRARY_DIR "/usr/local/llvm19/lib")
    endif()
endif()
if(NOT LIBCLANG_INCLUDE_DIR)
    if(EXISTS "/usr/lib/llvm-19/include/clang-c/Index.h")
        set(LIBCLANG_INCLUDE_DIR "/usr/lib/llvm-19/include")
    else()
        set(LIBCLANG_INCLUDE_DIR "/usr/local/llvm19/include")
    endif()
endif()
# Debian/Ubuntu: libclang-19.so.19 лежит в /usr/lib/x86_64-linux-gnu без .so-симлинка без -dev.
# Поэтому пробуем несколько имён и даём фолбэк на явный путь.
find_library(LIBCLANG_LIB
        NAMES clang libclang clang-19 libclang-19
        PATHS ${LIBCLANG_LIBRARY_DIR} /usr/lib/llvm-19/lib /usr/lib/x86_64-linux-gnu
        )
if(NOT LIBCLANG_LIB AND EXISTS "/usr/lib/x86_64-linux-gnu/libclang-19.so.19")
    set(LIBCLANG_LIB "/usr/lib/x86_64-linux-gnu/libclang-19.so.19")
endif()
if(NOT LIBCLANG_LIB AND EXISTS "/usr/lib/llvm-19/lib/libclang-19.so.1")
    set(LIBCLANG_LIB "/usr/lib/llvm-19/lib/libclang-19.so.1")
endif()
if(NOT LIBCLANG_LIB)
    message(FATAL_ERROR "libclang not found. Установи libclang-dev или укажи -DLIBCLANG_LIBRARY_DIR=/usr/lib/x86_64-linux-gnu")
endif()
message(STATUS "LIBCLANG_LIB=${LIBCLANG_LIB}")

find_package(CURL REQUIRED)
include_directories(/usr/include)

# --- core: слой 0 (libclang). НИЧЕГО не знает о моделях — ни один файл
# здесь не инклюдит model_broker.h/orchestrator.h и не линкует CURL. ---
add_library(core STATIC
        core/src/clang_index.cpp
        core/src/query_locate_symbol.cpp
        core/src/query_class_outline.cpp
        core/src/query_symbol_refs.cpp
        core/src/query_virtual_overrides.cpp
        core/src/query_clangd_implementations.cpp
        core/src/query_find_implementors.cpp
        core/src/compile_commands.cpp
        core/src/build_graph.cpp
        core/src/blast_radius.cpp
        core/src/project_index.cpp
        )
target_include_directories(core PUBLIC core/include third_party ${LIBCLANG_INCLUDE_DIR} /usr/local/llvm19/include)
target_link_libraries(core PUBLIC ${LIBCLANG_LIB})
set_target_properties(core PROPERTIES BUILD_RPATH "${LIBCLANG_LIBRARY_DIR}")

# --- agent: слой 3-5 (модели, оркестратор). НЕ линкует libclang напрямую —
# зовёт core только через его публичные json-возвращающие функции (queries.h,
# compile_commands.h, build_graph.h), которые agent/src/orchestrator.cpp
# инклюдит из core/include. Разделение осталось, просто не через процесс. ---
add_library(agent STATIC
        agent/src/model_broker.cpp
        agent/src/orchestrator.cpp
        agent/src/handlers.cpp
        agent/src/ground_service.cpp
        agent/src/task_classification.cpp
        agent/src/main_model_broker.cpp
        agent/src/execute_service.cpp
        agent/src/verify_service.cpp
        )
target_include_directories(agent PUBLIC
        agent/include
        third_party
        /usr/local/include
        )
target_link_libraries(agent PUBLIC CURL::libcurl core)

# --- единый бинарник ---
add_executable(cli_agent cli/main.cpp)
target_link_libraries(cli_agent PRIVATE core agent)

# --- тесты (Catch2 3.x, S4.1) — некошерно руками, только assert --- 
enable_testing()
list(APPEND CMAKE_PREFIX_PATH "/usr/lib/cmake/Catch2")
find_package(Catch2 3 QUIET)
if(Catch2_FOUND)
    add_executable(agent_tests 
        agent/tests/test_execute_service.cpp
        agent/tests/test_orchestrator_integration.cpp
    )
    target_link_libraries(agent_tests PRIVATE agent core Catch2::Catch2WithMain)
    target_include_directories(agent_tests PRIVATE agent/include core/include third_party ${LIBCLANG_INCLUDE_DIR})
    include(Catch)
    catch_discover_tests(agent_tests)
    message(STATUS "Catch2 found — agent_tests enabled")
else()
    message(STATUS "Catch2 not found — agent_tests skipped (install libcatch2-dev)")
endif()
