cmake_minimum_required(VERSION 3.20)

# PROJECT_NAMEと使用言語を定義する。VERSIONは生成物やPackageの版管理にも利用できる。
project(cpp_program_structure VERSION 1.0.0 LANGUAGES CXX)

# Compiler固有拡張ではなく、標準C++20を要求する。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# EncodingとWarningは、このDirectoryで作る全翻訳単位へ適用する。
# LibraryだけをPRIVATE設定すると、main.cppやTest.cppは別OptionでCompileされてしまう。
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# app_coreをLibraryにすることで、実行ProgramとTestから同じ実装を再利用できる。
add_library(app_core STATIC
    src/Application.cpp
)

# PUBLICにすると、app_coreをLinkするTargetへinclude Pathが伝播する。
target_include_directories(app_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# main.cppだけを入口Executableへ置き、処理本体はapp_coreへ委譲する。
add_executable(program_structure
    src/main.cpp
)
target_link_libraries(program_structure PRIVATE app_core)

# CTestを有効化し、Test専用Executableを登録する。
include(CTest)
if(BUILD_TESTING)
    add_executable(program_structure_tests
        tests/ApplicationTests.cpp
    )
    target_link_libraries(program_structure_tests PRIVATE app_core)
    add_test(NAME program_structure_tests COMMAND program_structure_tests)
endif()
