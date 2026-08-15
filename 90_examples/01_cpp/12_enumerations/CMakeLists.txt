cmake_minimum_required(VERSION 3.20)
project(cpp_enumerations LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(battle_types STATIC src/BattleTypes.cpp)
target_include_directories(battle_types PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(enumerations src/main.cpp)
target_link_libraries(enumerations PRIVATE battle_types)

include(CTest)
add_executable(enumerations_tests tests/BattleTypesTests.cpp)
target_link_libraries(enumerations_tests PRIVATE battle_types)
add_test(NAME enumerations_tests COMMAND enumerations_tests)

