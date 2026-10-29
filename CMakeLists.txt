cmake_minimum_required(VERSION 3.20)

project(fcpp-agent VERSION 0.1.0 LANGUAGES C CXX)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# --- Language standard ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Default build type ---
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Strict warnings ---
add_compile_options(-Wall -Wextra -Werror)

find_package(Threads REQUIRED)

include(FetchContent)

# --- nlohmann/json (header-only) ---
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

# --- SQLite3 (amalgamation) ---
FetchContent_Declare(
    sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450100.zip
)

FetchContent_MakeAvailable(nlohmann_json sqlite3)

# --- libclang (target backend clang 22, ADR-06/ADR-11) ---
# Hard requirement: absence is a configuration FAIL, not a degradation.
# FreeBSD: /usr/local/llvm22, /usr/local
# Ubuntu:  /usr/lib/llvm-22, /usr/include, /usr/lib/x86_64-linux-gnu
find_path(LIBCLANG_INCLUDE_DIR clang-c/Index.h
    HINTS
        /usr/lib/llvm-22/include
        /usr/lib/llvm-21/include
        /usr/lib/llvm-20/include
        /usr/lib/llvm-19/include
        /usr/lib/llvm-18/include
    PATHS
        /usr/local/llvm22/include
        /usr/local/include
        /usr/include
    REQUIRED
)
find_library(LIBCLANG_LIBRARY NAMES clang libclang clang-22 libclang-22
    HINTS
        /usr/lib/llvm-22/lib
        /usr/lib/llvm-21/lib
        /usr/lib/llvm-20/lib
        /usr/lib/llvm-19/lib
        /usr/lib/llvm-18/lib
        /usr/lib/x86_64-linux-gnu
    PATHS
        /usr/local/llvm22/lib
        /usr/local/lib
        /usr/lib
    REQUIRED
)
get_filename_component(LIBCLANG_LIB_DIR "${LIBCLANG_LIBRARY}" DIRECTORY)
message(STATUS "libclang include: ${LIBCLANG_INCLUDE_DIR}")
message(STATUS "libclang library: ${LIBCLANG_LIBRARY}")

# --- libcurl (HTTP transport for LLM providers, 6b) ---
# FreeBSD: /usr/local/include, /usr/local/lib
# Ubuntu:  /usr/include/x86_64-linux-gnu, /usr/lib/x86_64-linux-gnu
find_path(LIBCURL_INCLUDE_DIR curl/curl.h
    HINTS
        /usr/include/x86_64-linux-gnu
    PATHS
        /usr/local/include
        /usr/include
    REQUIRED
)
find_library(LIBCURL_LIBRARY NAMES curl libcurl
    HINTS
        /usr/lib/x86_64-linux-gnu
    PATHS
        /usr/local/lib
        /usr/lib
    REQUIRED
)
message(STATUS "libcurl include: ${LIBCURL_INCLUDE_DIR}")
message(STATUS "libcurl library: ${LIBCURL_LIBRARY}")

# The SQLite amalgamation archive unpacks into a versioned subdirectory.
file(GLOB_RECURSE SQLITE3_C_FILE "${sqlite3_SOURCE_DIR}/sqlite3.c")
if(NOT SQLITE3_C_FILE)
    message(FATAL_ERROR "sqlite3.c not found under ${sqlite3_SOURCE_DIR}")
endif()
get_filename_component(SQLITE3_DIR "${SQLITE3_C_FILE}" DIRECTORY)

add_library(sqlite3 STATIC ${SQLITE3_C_FILE})
target_include_directories(sqlite3 PUBLIC ${SQLITE3_DIR})
target_compile_options(sqlite3 PRIVATE -w)

# --- Core library ---
add_library(fcpp_core STATIC
    src/core/CConfigManager.cpp
    src/core/CProjectProfile.cpp
    src/core/CProjectPaths.cpp
    src/core/CExecutor.cpp
    src/core/CHash.cpp
    src/core/CCompileCommandsLocator.cpp
    src/core/CCompileCommandsParser.cpp
    src/core/CProjectRegistry.cpp
    src/core/CProjectManager.cpp
    src/index/CSchemaInitializer.cpp
    src/index/CSQLiteIndex.cpp
    src/index/CClang22.cpp
    src/index/CClangFactory.cpp
    src/index/CSqlCompilerIndex.cpp
    src/core/SError.cpp
    src/core/SConfig.cpp
    src/core/SStaleReport.cpp
    src/core/CLogger.cpp
    src/core/SLogEvent.cpp
    src/core/CDoctor.cpp
    src/core/CPatchParser.cpp
    src/core/CPatchValidator.cpp
    src/core/CPatchApplier.cpp
    src/core/CAgentApi.cpp
    # Conversation persistence and prompt context budgeting implementations.
    src/core/CConversationStore.cpp
    src/core/CContextManager.cpp
    src/core/CChatContextSource.cpp
    src/core/CProjectProfileContextSource.cpp
    src/core/CCodeContextSource.cpp
    src/core/SIntent.cpp
    src/core/SSubtasks.cpp
    src/core/SRequestOutcome.cpp
    src/core/CRequestPipeline.cpp
    src/core/CRequestOrchestrator.cpp
    src/core/CCursorContextAnalyzer.cpp
    src/core/CHistorian.cpp
    src/core/SLlmRoleConfig.cpp
    src/llm/CLlmLayer.cpp
    src/core/CIntentAnalyzer.cpp
    src/core/CAnswerService.cpp
    src/core/CAnswerQuality.cpp
    src/core/SEvidence.cpp
    src/core/CEvidenceCollector.cpp
    src/core/SCascade.cpp
    src/core/CModelCascade.cpp
    src/core/CModelLevels.cpp
    src/core/CEscalatingIntentAnalyzer.cpp
    src/core/CSubtaskComposer.cpp
    src/core/CReActAgent.cpp
    src/llm/CEndpointUtil.cpp
    src/llm/CUrlTransport.cpp
    src/llm/CModelOpenAICompat.cpp
    src/llm/CModelClaude.cpp
    src/llm/CModelFactory.cpp
    src/tools/CToolContext.cpp
    src/tools/CIndexTool.cpp
    src/tools/CToolRegistry.cpp
)
target_include_directories(fcpp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(fcpp_core PRIVATE
    ${LIBCLANG_INCLUDE_DIR}
    ${LIBCURL_INCLUDE_DIR}
)
target_compile_features(fcpp_core PUBLIC cxx_std_20)
target_compile_definitions(fcpp_core PRIVATE
    FCPP_AGENT_SCHEMA_FILE="${CMAKE_CURRENT_SOURCE_DIR}/schema/current.sql"
)
target_link_libraries(fcpp_core PUBLIC
    nlohmann_json::nlohmann_json
    sqlite3
    ${LIBCLANG_LIBRARY}
    ${LIBCURL_LIBRARY}
    Threads::Threads
)
target_link_options(fcpp_core PUBLIC "-Wl,-rpath,${LIBCLANG_LIB_DIR}")

# --- Executable ---
add_executable(fcpp-agent
    src/main.cpp
    src/cli/CProtocolServer.cpp
)
target_link_libraries(fcpp-agent PRIVATE fcpp_core)

# --- Tests (Catch2) ---
enable_testing()
find_package(Catch2 3 REQUIRED)

add_executable(fcpp_tests
    tests/project_manager_test.cpp
    tests/compile_commands_test.cpp
    tests/compiler_index_test.cpp
    tests/tools_test.cpp
    tests/stale_test.cpp
    tests/stage8a_test.cpp
    tests/patch_test.cpp
    tests/agent_api_test.cpp
    tests/protocol_test.cpp
    src/cli/CProtocolServer.cpp
    tests/conversation_store_test.cpp
    tests/context_manager_test.cpp
    tests/chat_context_source_test.cpp
    tests/context_sources_test.cpp
    tests/pipeline_test.cpp
    tests/model_test.cpp
    tests/project_profile_test.cpp
    tests/evidence_test.cpp
    tests/cascade_test.cpp
    tests/subtask_test.cpp
    tests/streaming_test.cpp
    tests/request_orchestrator_test.cpp
    tests/historian_test.cpp
)
target_link_libraries(fcpp_tests PRIVATE fcpp_core Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(fcpp_tests)
