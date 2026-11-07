cmake_minimum_required(VERSION 3.16)
project(word_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(wordengine src/dawg.cpp src/wordsearch.cpp)
target_include_directories(wordengine PUBLIC include)
if(MSVC)
  target_compile_options(wordengine PRIVATE /W4)
else()
  target_compile_options(wordengine PRIVATE -Wall -Wextra)
endif()

add_executable(wsearch apps/wsearch.cpp)
target_link_libraries(wsearch PRIVATE wordengine)
add_executable(bench apps/bench.cpp)
target_link_libraries(bench PRIVATE wordengine)

# The original lab, built unchanged for comparison.
add_executable(lab11 archive/lab11/LAB11.cpp)
target_include_directories(lab11 PRIVATE archive/lab11)

enable_testing()
add_executable(tests tests/test_main.cpp)
target_link_libraries(tests PRIVATE wordengine)
add_test(NAME tests COMMAND tests)

if(MINGW)
  foreach(t wsearch bench lab11 tests)
    target_link_options(${t} PRIVATE -static)
  endforeach()
endif()
