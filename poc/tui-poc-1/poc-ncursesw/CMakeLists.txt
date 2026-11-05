cmake_minimum_required(VERSION 3.20)
project(poc_ncursesw LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

add_executable(poc-ncursesw main.cpp ../common/event_queue.hpp ../common/terminal_sanitizer.hpp ../common/ui_helpers.hpp)
target_include_directories(poc-ncursesw PRIVATE ../common ${CURSES_INCLUDE_DIRS})
target_link_libraries(poc-ncursesw PRIVATE ${CURSES_LIBRARIES} Threads::Threads)
target_compile_options(poc-ncursesw PRIVATE -Wall -Wextra -Wpedantic)
add_executable(sanitizer-tests ../common/sanitizer_tests.cpp)
target_include_directories(sanitizer-tests PRIVATE ../common)
target_compile_options(sanitizer-tests PRIVATE -Wall -Wextra -Wpedantic)
