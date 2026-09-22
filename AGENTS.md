# AGENTS.md

## Project
Agent IA minimal en C++ : boucle input utilisateur -> appel LLM local
compatible OpenAI (endpoint /v1/chat/completions) -> réponse.

## Setup
cmake -B build

## Build
cmake --build build

## Run
set -a; source .env; set +a
./build/agent

## Test
Pas de suite de tests pour l'instant.
(À ajouter : Catch2 ou GoogleTest si le projet grossit)

## Code style
- C++20
- RAII partout, pas de raw pointers sauf nécessité justifiée
- En-têtes dans include/, implémentation dans src/
- Un fichier .cpp par responsabilité (client HTTP, parsing JSON, boucle agent)
- Pas de dépendances lourdes : libcurl + nlohmann/json suffisent

## Architecture cible
- main.cpp             -> boucle principale
- agent.hpp/.cpp        -> état de l'agent, historique de conversation
- http_client.hpp/.cpp  -> wrapper autour de libcurl

## Config LLM local
- LLM_BASE_URL : endpoint local (défaut :
  http://localhost:8080/v1/chat/completions)
- LLM_MODEL    : nom du modèle chargé côté serveur (défaut : local-model)
- LLM_API_KEY  : optionnelle, pour serveurs locaux qui l'exigent
- LLM_TIMEOUT_SECONDS : optionnelle, timeout des requêtes HTTP en secondes
  (défaut : 60)

## Credentials
- Fichier .env (non commité) chargé dans le shell avant de lancer l'agent
  (set -a; source .env; set +a) — pas de parsing dotenv en C++ pour l'instant
- .env.example committé comme documentation des variables attendues
- Le code lit uniquement via std::getenv(), jamais de valeur en dur

## Décisions d'architecture (v1)
- Streaming : désactivé (stream: false). http_client doit quand même être
  conçu avec une interface qui pourrait accepter un callback de streaming
  plus tard, sans réécriture complète.
- Historique de conversation : borné aux 20 derniers messages pour éviter
  de dépasser la fenêtre de contexte du modèle local. Troncature simple
  (drop des plus anciens), pas de résumé.
- Limitation connue — tour surdimensionné : trim_history() ne retire jamais
  le dernier tour utilisateur, même s'il dépasse à lui seul
  max_history_messages (20). La troncature raisonne en tours complets (un
  tour commence sur un message role=user) et s'arrête avant le tour en
  cours. Un échange qui enchaîne beaucoup de tool_calls peut donc
  temporairement dépasser le cap de 20 messages. C'est un compromis assumé,
  pas un bug : couper au milieu d'un tour casserait l'appairage
  assistant (tool_calls) -> tool (tool_call_id) attendu par l'API. Le
  dépassement devient plus probable si max_tool_iterations est augmenté
  au-delà de sa valeur actuelle (5).
- Tool-calling : actif via ToolRegistry (dispatch des tool_calls dans
  Agent::send, borné par max_tool_iterations = 5). Quatre outils sandboxés
  dans workspace/ sont exposés au modèle : list_files, read_file,
  search_files et write_file. write_file écrit de façon autonome (aucune
  confirmation humaine), mais refuse d'écraser un fichier existant si
  overwrite n'est pas explicitement true.
- Erreurs réseau : si le serveur local est injoignable (curl échoue), 
  afficher un message clair mentionnant LLM_BASE_URL plutôt que de laisser
  remonter une exception brute.

## Current state
REPL minimal fonctionnel : lecture utilisateur, appel chat completions
OpenAI-compatible non-streamé, historique borné et tool-calling actif avec
quatre outils sandboxés (list_files, read_file, search_files, write_file).

## Instructions pour l'agent
- Ne pas ajouter de framework externe sans le demander explicitement
- Garder chaque fichier court et une seule responsabilité
- Commits atomiques, un changement logique par commit, format Conventional
  Commits (feat, fix, refactor, chore, docs, build, test)
