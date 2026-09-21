# Agent CPP

Agent IA minimal en C++, connecté à un LLM local compatible OpenAI.

## Prérequis
- CMake >= 3.20
- Compilateur C++20
- libcurl (dépendance système) :
  - Debian/Ubuntu : sudo apt install libcurl4-openssl-dev
  - macOS : brew install curl
- nlohmann/json : récupéré automatiquement via FetchContent, rien à installer
- Un serveur local exposant /v1/chat/completions (llama.cpp, Ollama, LM Studio, vLLM...)

## Configuration
cp .env.example .env
# éditer .env avec LLM_BASE_URL / LLM_MODEL / LLM_API_KEY

## Build
cmake -B build
cmake --build build

## Run
set -a; source .env; set +a
./build/agent
