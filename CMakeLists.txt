set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
cmake_minimum_required(VERSION 3.19)
project(cli_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- libclang: жёстко одна версия, llvm22 (FreeBSD 14.4) ---
# Мультиверсионность сознательно не поддерживается: отличаются не только
# версии API, но и поведение — строковый формат USR и результат разбора AST
# между версиями clang не совпадают, а это тихий отказ кэша, а не ошибка.
# Версия задаётся здесь и попадает в кэш (см. core/src/tu_cache.cpp).
if(NOT LIBCLANG_DIR)
    set(LIBCLANG_DIR "/usr/local/llvm22")
endif()
if(NOT EXISTS "${LIBCLANG_DIR}/include/clang-c/Index.h")
    message(FATAL_ERROR "clang-c/Index.h не найден в ${LIBCLANG_DIR}/include. Установи llvm22 или укажи -DLIBCLANG_DIR=/путь/к/llvm")
endif()

find_library(LIBCLANG_LIB NAMES clang libclang PATHS "${LIBCLANG_DIR}/lib" NO_DEFAULT_PATH)
if(NOT LIBCLANG_LIB)
    message(FATAL_ERROR "libclang не найден в ${LIBCLANG_DIR}/lib")
endif()
message(STATUS "LIBCLANG_LIB=${LIBCLANG_LIB} (llvm ${LIBCLANG_DIR})")

# --- sqlite3: постоянный кэш TU (core/src/tu_cache.cpp) ---
find_path(SQLITE3_INCLUDE_DIR sqlite3.h PATHS /usr/local/include /usr/include)
find_library(SQLITE3_LIB NAMES sqlite3 PATHS /usr/local/lib /usr/lib)
if(NOT SQLITE3_INCLUDE_DIR OR NOT SQLITE3_LIB)
    message(FATAL_ERROR "sqlite3 не найден. Установи databases/sqlite3 (FreeBSD)")
endif()
message(STATUS "SQLITE3_LIB=${SQLITE3_LIB}")

find_package(CURL REQUIRED)

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
        core/src/db_manager.cpp
        core/src/hash.cpp
        core/src/tu_cache.cpp
        )
target_include_directories(core PUBLIC core/include third_party "${LIBCLANG_DIR}/include")
target_include_directories(core PRIVATE ${SQLITE3_INCLUDE_DIR})
target_link_libraries(core PUBLIC ${LIBCLANG_LIB} ${SQLITE3_LIB})
set_target_properties(core PROPERTIES BUILD_RPATH "${LIBCLANG_DIR}/lib")

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

# --- тесты (Catch2 3.x) ---
enable_testing()
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake/Catch2")
find_package(Catch2 3 QUIET)
if(Catch2_FOUND)
    include(Catch) # даёт catch_discover_tests — без него CMake не знает команду

    add_executable(core_tests
        core/tests/test_hash.cpp
        core/tests/test_db_manager.cpp
        core/tests/test_tu_cache.cpp
    )
    target_link_libraries(core_tests PRIVATE core Catch2::Catch2WithMain)
    target_include_directories(core_tests PRIVATE core/include third_party "${LIBCLANG_DIR}/include")
    catch_discover_tests(core_tests)

    add_executable(agent_tests
        agent/tests/test_execute_service.cpp
        agent/tests/test_orchestrator_integration.cpp
    )
    target_link_libraries(agent_tests PRIVATE agent core Catch2::Catch2WithMain)
    target_include_directories(agent_tests PRIVATE agent/include core/include third_party "${LIBCLANG_DIR}/include")
    catch_discover_tests(agent_tests)
    message(STATUS "Catch2 found — core_tests и agent_tests включены")
else()
    message(STATUS "Catch2 not found — тесты пропущены (установи devel/catch2 или укажи CMAKE_PREFIX_PATH)")
endif()
