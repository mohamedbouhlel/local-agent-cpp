cmake_minimum_required(VERSION 3.20)
project(tui_async_poc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TUI_ASYNC_ENABLE_TSAN "Build the isolated prototype with ThreadSanitizer" OFF)
find_package(Threads REQUIRED)

add_library(tui_async src/agent_worker.cpp)
target_include_directories(tui_async PUBLIC include ../../include)
target_link_libraries(tui_async PUBLIC Threads::Threads)
target_compile_options(tui_async PRIVATE -Wall -Wextra -Wpedantic)

add_executable(tui_async_tests tests/worker_tests.cpp)
target_link_libraries(tui_async_tests PRIVATE tui_async)
target_compile_options(tui_async_tests PRIVATE -Wall -Wextra -Wpedantic)

if(TUI_ASYNC_ENABLE_TSAN)
  foreach(target tui_async tui_async_tests)
    target_compile_options(${target} PRIVATE -fsanitize=thread -fno-omit-frame-pointer -g)
    target_link_options(${target} PRIVATE -fsanitize=thread)
  endforeach()
endif()

enable_testing()
add_test(NAME tui_async_tests COMMAND tui_async_tests)
set_tests_properties(tui_async_tests PROPERTIES TIMEOUT 20)
