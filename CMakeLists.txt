cmake_minimum_required(VERSION 3.20)  # Лучше 3.20, как у всех

set(LIB_CORE core)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set(LIB_CORE ${LIB_CORE}d)
endif()

project(${LIB_CORE})

set(CORE_LIB ${PROJECT_NAME} CACHE INTERNAL "")

# --- Пути ---
set(PUBLIC_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(PRIVATE_SRC_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- Публичные заголовки ---
file(GLOB_RECURSE PUBLIC_HEADERS
        ${PUBLIC_HEADER_DIR}/*.h
        )
include_directories(
        include
)
# --- Исходники core ---
file(GLOB_RECURSE CORE_SOURCES
        ${PRIVATE_SRC_DIR}/*.cpp
        automation/*.cpp
        automation/*.h
        )

# --- Компоненты и оборудование ---
file(GLOB_RECURSE COMPONENT_SOURCES
        components/*.cpp
        equipment/*.cpp
        )

# --- Заголовки компонентов (для установки) ---
file(GLOB_RECURSE COMPONENT_HEADERS
        components/*.h
        equipment/*.h
        )
file(GLOB_RECURSE logger
        ../server/sources/logger-common/logger.cpp
        )

# --- Сборка библиотеки ---
add_library(${PROJECT_NAME} STATIC
        ${PUBLIC_HEADERS}
        ${COMPONENT_HEADERS}
        ${CORE_SOURCES}
        ${COMPONENT_SOURCES}
        ${logger}
        )

# --- Настройка инклудов ---
target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${PUBLIC_HEADER_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/equipment/base/include
        ${CMAKE_CURRENT_SOURCE_DIR}/equipment/water/include
        ${CMAKE_CURRENT_SOURCE_DIR}/components/water/include

        ${CMAKE_SOURCE_DIR}/libmeasures/include
        ${CMAKE_SOURCE_DIR}/libmultilang/include


        ${CMAKE_BINARY_DIR}/libmeasures
        ${CMAKE_BINARY_DIR}/libmultilang

        ../interfaces/include
        ../saver/include
        /usr/include
        /usr/local/include
        ${CMAKE_CURRENT_BINARY_DIR}
        )

# --- Скрываем символы ---
set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        )

# --- Генерируем export header ---
include(GenerateExportHeader)

GENERATE_EXPORT_HEADER(${PROJECT_NAME}
        BASE_NAME ${PROJECT_NAME}
        EXPORT_MACRO_NAME CORE_EXPORT
        EXPORT_FILE_NAME core_export.h
        STATIC_DEFINE CORE_STATIC
        )

# --- Линковка зависимостей ---
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        ${MEASURE_LIB}
        ${LIB_LANG}
        PRIVATE
        # внутренние зависимости

        )

# --- Установка ---
#install(TARGETS ${PROJECT_NAME}
#        EXPORT coreTargets
#        LIBRARY DESTINATION lib
#        ARCHIVE DESTINATION lib
#        RUNTIME DESTINATION bin
#        )

#install(DIRECTORY ${PUBLIC_HEADER_DIR}/
#        DESTINATION include
#        FILES_MATCHING PATTERN "*.h"
#        )

#install(FILES ${CMAKE_CURRENT_BINARY_DIR}/core_export.h
#        DESTINATION include
#        )

#install(EXPORT coreTargets
#        FILE coreTargets.cmake
#        NAMESPACE core::
#        DESTINATION lib/cmake/core
#        )
