cmake_minimum_required(VERSION 3.20)
project(cpp_types_variables_initialization VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(character_stats STATIC src/CharacterStats.cpp)
target_include_directories(character_stats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(types_variables_initialization src/main.cpp)
target_link_libraries(types_variables_initialization PRIVATE character_stats)

include(CTest)
if(BUILD_TESTING)
    add_executable(types_variables_initialization_tests tests/CharacterStatsTests.cpp)
    target_link_libraries(types_variables_initialization_tests PRIVATE character_stats)
    add_test(NAME types_variables_initialization_tests COMMAND types_variables_initialization_tests)
endif()
