cmake_minimum_required(VERSION 3.20)
set(TARGET_NAME app-server)

# Устанавливаем стандарт C++ (например, C++17, C++20, C++23)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(TARGET_NAME ${TARGET_NAME}d)
endif()

project(${TARGET_NAME})

#include(FetchContent)
#FetchContent_Declare(
#        databased
#        GIT_REPOSITORY git@github.com:Nechitaylenko-aa/lib-database.git
#        GIT_TAG dev
#)

#FetchContent_MakeAvailable(databased)

find_package(Catch2 3 REQUIRED)

file(GLOB_RECURSE src
        sources/*.h
        sources/*.cpp
        )

add_executable(${TARGET_NAME}
        main.cpp
        ${src}

        )

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    add_definitions("-DFreeBSD")
endif()

set(OCTO_INC ${octo-encryption-cpp_SOURCE_DIR}/include/octo-encryption-cpp)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OCTO_INC}
        )


target_link_libraries(${TARGET_NAME} PRIVATE
        database
        octo-encryption-cpp
        ${MEASURE_LIB}
        ${CORE_LIB}
        pthread
        stdc++
        PRIVATE Catch2::Catch2
        )

target_compile_options(${TARGET_NAME} PRIVATE
        -Wall
        -Wextra
        -pedantic
        -std=c++17
        -D_REENTRANT
        )


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${TARGET_NAME} PRIVATE
            -g
            -O0
            -DDEBUG
            -D_DEBUG
            )
else()
    target_compile_options(${TARGET_NAME} PRIVATE
            -O2
            -DNDEBUG
            )
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_compile_options(${TARGET_NAME} PRIVATE
            -D_XOPEN_SOURCE=700
            -D__BSD_VISIBLE
            )
endif()


set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        )


#add_custom_target(server
#        DEPENDS ${TARGET_NAME}
#        )


#if(BUILD_TESTS)
#    enable_testing()
#    add_subdirectory(tests)
#endif()
