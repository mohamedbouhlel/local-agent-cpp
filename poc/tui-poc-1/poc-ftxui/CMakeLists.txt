cmake_minimum_required(VERSION 3.20)
project(poc_ftxui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG f921fad208912747c17d129a8ef75ec7624b6eec
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ftxui)

find_package(Threads REQUIRED)
add_executable(poc-ftxui main.cpp ../common/event_queue.hpp ../common/terminal_sanitizer.hpp ../common/ui_helpers.hpp)
target_include_directories(poc-ftxui PRIVATE ../common)
target_link_libraries(poc-ftxui PRIVATE
    ftxui::screen ftxui::dom ftxui::component Threads::Threads)
target_compile_options(poc-ftxui PRIVATE -Wall -Wextra -Wpedantic)
add_executable(sanitizer-tests ../common/sanitizer_tests.cpp)
target_include_directories(sanitizer-tests PRIVATE ../common)
target_compile_options(sanitizer-tests PRIVATE -Wall -Wextra -Wpedantic)
