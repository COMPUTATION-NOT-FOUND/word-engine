cmake_minimum_required(VERSION 3.16)
project(word_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(wordengine src/dawg.cpp src/wordsearch.cpp src/board.cpp src/movegen.cpp src/game.cpp src/ai.cpp)
target_include_directories(wordengine PUBLIC include)
if(MSVC)
  target_compile_options(wordengine PRIVATE /W4)
else()
  target_compile_options(wordengine PRIVATE -Wall -Wextra)
endif()

if(EMSCRIPTEN)
  # Browser build: just the engine module the web page loads (see web/ and .github/workflows/pages.yml).
  add_executable(engine web/bindings.cpp)
  target_link_libraries(engine PRIVATE wordengine)
  target_link_options(engine PRIVATE -lembind -sMODULARIZE=1 -sEXPORT_NAME=createEngine
    -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=worker -O3)
  return()
endif()

add_executable(wsearch apps/wsearch.cpp)
target_link_libraries(wsearch PRIVATE wordengine)
add_executable(bench apps/bench.cpp)
target_link_libraries(bench PRIVATE wordengine)
add_executable(tessera apps/tessera.cpp)
target_link_libraries(tessera PRIVATE wordengine)

# The original lab, built unchanged for comparison (only when its files are present).
set(WE_TARGETS wsearch bench tessera tests)
if(EXISTS "${CMAKE_SOURCE_DIR}/archive/lab11/LAB11.cpp")
  add_executable(lab11 archive/lab11/LAB11.cpp)
  target_include_directories(lab11 PRIVATE archive/lab11)
  list(APPEND WE_TARGETS lab11)
endif()

enable_testing()
add_executable(tests tests/test_main.cpp)
target_link_libraries(tests PRIVATE wordengine)
target_compile_definitions(tests PRIVATE WE_DICT="${CMAKE_SOURCE_DIR}/data/enable1.txt")
add_test(NAME tests COMMAND tests)

if(MINGW)
  foreach(t ${WE_TARGETS})
    target_link_options(${t} PRIVATE -static)
  endforeach()
endif()
