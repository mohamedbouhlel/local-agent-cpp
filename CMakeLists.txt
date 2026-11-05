cmake_minimum_required(VERSION 3.20)
project(agent_cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# libcurl : dépendance système (voir README pour l'installation)
find_package(CURL REQUIRED)

# nlohmann/json : header-only, récupéré via FetchContent (pas de dépendance système)
include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_library(agent_core STATIC
  src/agent.cpp
  src/filesystem_sandbox.cpp
  src/http_client.cpp
  src/prompt_reader.cpp
  src/sse_parser.cpp
  src/tool_registry.cpp
  src/ui/presentation_state.cpp
  src/ui/terminal_capabilities.cpp
  src/workspace_tools.cpp
  src/workspace_search.cpp
  src/workspace_write.cpp
)
target_include_directories(agent_core PUBLIC include)
target_link_libraries(agent_core PUBLIC CURL::libcurl nlohmann_json::nlohmann_json)

add_executable(agent
  src/main.cpp
)
target_link_libraries(agent PRIVATE agent_core)

# Harness de tests, desactive par defaut : l'executable de production n'en
# depend jamais. Activer explicitement avec -DAGENT_BUILD_TESTS=ON.
option(AGENT_BUILD_TESTS "Build the agent test harness" OFF)
if(AGENT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
