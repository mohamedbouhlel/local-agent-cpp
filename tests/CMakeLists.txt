# Harness de tests (active via -DAGENT_BUILD_TESTS=ON a la racine).
# Aucun framework externe : micro-harnais maison (tests/test_support.hpp).

find_package(Threads REQUIRED)

add_executable(agent_tests
  test_main.cpp
  mock_http_server.cpp
  mock_http_server_test.cpp
  sse_parser_test.cpp
  workspace_tools_test.cpp
  write_authorization_test.cpp
  filesystem_sandbox_test.cpp
  http_client_test.cpp
  agent_events_test.cpp
  ui_test.cpp
  ui_renderer_test.cpp
  ui_metrics_test.cpp
  agent_streaming_test.cpp
  agent_e2e_test.cpp
  agent_worker_test.cpp
  tui_test.cpp
  tui_navigation_test.cpp
  bounded_history_test.cpp
  prompt_reader_test.cpp
)

target_link_libraries(agent_tests PRIVATE agent_core agent_tui Threads::Threads)
target_compile_options(agent_tests PRIVATE -Wall -Wextra -Wpedantic)

# Un test CTest par groupe : resultats lisibles par groupe et code de retour
# distinct par groupe. Le TIMEOUT transforme un eventuel blocage en ECHEC
# exploitable au lieu d'un job CI suspendu (constat R-01).
add_executable(tui_pty_driver tui_pty_driver.cpp)
target_link_libraries(tui_pty_driver PRIVATE agent_tui Threads::Threads)
find_library(UTIL_LIBRARY util)
if(UTIL_LIBRARY)
  target_link_libraries(agent_tests PRIVATE ${UTIL_LIBRARY})
endif()

set(test_groups sse workspace http agent ui ui3 ui4 tui2 tui3 tui4a tui4b1 repl mock)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list(APPEND test_groups sandbox)
endif()
foreach(group IN LISTS test_groups)
  add_test(NAME ${group} COMMAND agent_tests --filter=${group})
  set_tests_properties(${group} PROPERTIES TIMEOUT 30)
endforeach()
# tui3 includes 30 independent PTY shutdown repetitions. A clean run took
# 31.58 s on the qualification host, so keep the repetition count and allow
# bounded scheduler variance instead of timing out a passing group at 30 s.
set_tests_properties(tui3 PROPERTIES TIMEOUT 60)
