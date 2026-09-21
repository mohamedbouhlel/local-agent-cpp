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
- LLM_BASE_URL : endpoint local (ex: http://localhost:8080/v1/chat/completions)
- LLM_MODEL    : nom du modèle chargé côté serveur
- LLM_API_KEY  : optionnelle, pour serveurs locaux qui l'exigent

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
- Tool-calling : hors scope de cette itération. Sera ajouté dans un prompt
  dédié une fois le REPL de base fonctionnel.
- Erreurs réseau : si le serveur local est injoignable (curl échoue), 
  afficher un message clair mentionnant LLM_BASE_URL plutôt que de laisser
  remonter une exception brute.

## Current state
Structure de build uniquement. Tout le code métier (main.cpp, agent.hpp)
reste à écrire.

## Instructions pour l'agent
- Ne pas ajouter de framework externe sans le demander explicitement
- Garder chaque fichier court et une seule responsabilité
- Commits atomiques, un changement logique par commit, format Conventional
  Commits (feat, fix, refactor, chore, docs, build, test)
