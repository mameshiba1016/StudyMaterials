cmake_minimum_required(VERSION 3.20)
project(cpp_compile_and_link VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 全翻訳単位へ同じ警告LevelとUTF-8解釈を適用する。
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# CombatMath.cppをCompileし、静的Library combat_mathへ格納する。
add_library(combat_math STATIC
    src/CombatMath.cpp
)
target_include_directories(combat_math PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# main.cppのObjectとcombat_math LibraryをLinkしてExecutableを作る。
add_executable(compile_and_link
    src/main.cpp
)
target_link_libraries(compile_and_link PRIVATE combat_math)

include(CTest)
if(BUILD_TESTING)
    add_executable(compile_and_link_tests
        tests/CombatMathTests.cpp
    )
    target_link_libraries(compile_and_link_tests PRIVATE combat_math)
    add_test(NAME compile_and_link_tests COMMAND compile_and_link_tests)
endif()
