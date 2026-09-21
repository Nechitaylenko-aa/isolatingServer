cmake_minimum_required(VERSION 3.10)
project(analyzer)

# Set C++ standard and compiler
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER "clang++")

# Add compile flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# Include directories
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        /usr/local/include
        /usr/local/llvm19/include
)

# Link directories
link_directories(
        /usr/local/lib
        /usr/local/llvm19/lib
)

# Define source files
set(SOURCES
        analyzer.cpp
        src/Parser.cpp
        src/LlamaClient.cpp
        src/JsonUtils.cpp
        )

# Create executable
add_executable(analyzer ${SOURCES})

# Link libraries
target_link_libraries(analyzer
        curl
        clang
        )

# Optional: Set output directory for object files
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
