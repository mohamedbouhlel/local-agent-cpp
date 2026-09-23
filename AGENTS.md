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
Suite de tests versionnée dans `tests/` : micro-harnais maison (`test_support.hpp`,
`test_main.cpp`, aucun framework externe) accompagné d'un mock HTTP loopback
(`mock_http_server.*`). 64 cas / 239 assertions répartis en 5 groupes : `sse`,
`workspace`, `http`, `agent`, `mock`.
Désactivée par défaut, elle ne pèse jamais sur l'exécutable de production ;
activation explicite :
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON
cmake --build build-tests
cd build-tests && ctest
Chaque groupe est un test CTest distinct (`TIMEOUT 30`). L'exécutable
`agent_tests` accepte aussi `--filter=<groupe>` et retourne 2 si aucun cas n'est
exécuté (filtre sans correspondance). Un test ou un harnais lie `agent_core`
directement (voir Architecture) sans relister les .cpp.

## Code style
- C++20
- RAII partout, pas de raw pointers sauf nécessité justifiée
- En-têtes dans include/, implémentation dans src/
- Un fichier .cpp par responsabilité (client HTTP, parsing JSON, boucle agent)
- Pas de dépendances lourdes : libcurl + nlohmann/json suffisent

## Architecture
- Bibliothèque statique `agent_core` : toute la logique (hors boucle main) —
  - agent.hpp/.cpp          -> état de l'agent, historique, dispatch tool_calls
  - http_client.hpp/.cpp    -> wrapper autour de libcurl
  - tool_registry.hpp/.cpp  -> registre d'outils
  - workspace_tools.hpp/.cpp -> outils sandboxés dans workspace/
  Son include path (`include/`) est PUBLIC, et elle expose transitivement
  CURL::libcurl et nlohmann_json::nlohmann_json (les en-têtes publics
  incluent déjà `<nlohmann/json.hpp>`).
- Exécutable `agent` : `src/main.cpp` uniquement (config + boucle REPL), lié
  à `agent_core` en PRIVATE. Plus d'include path à redéclarer (hérité).
- Conséquence : compiler un test ou un harnais revient à lier `agent_core`
  (`target_link_libraries(<cible> PRIVATE agent_core)`) — plus besoin de
  relister les .cpp un par un ni de redéclarer les dépendances.

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
- Streaming : actif (stream: true). Les réponses sont consommées en SSE
  (src/sse_parser.cpp, framing uniquement) et le message assistant est
  reconstruit depuis les deltas OpenAI (content concaténé ; tool_calls
  accumulés par index dans src/agent.cpp). Feedback minimal pendant un cycle
  d'outils : callback on_tool_call -> "[appel de <outil>...]".
- Invariant streaming : si la requête échoue en plein flux (coupure, statut
  non-2xx), le message assistant partiellement reconstruit n'est JAMAIS ajouté
  à l'historique — seul le message user du tour y reste, pour pouvoir
  retenter sans historique corrompu.
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
  overwrite n'est pas explicitement true. L'écriture est atomique : contenu
  écrit dans un fichier temporaire du même répertoire puis substitué via
  rename(). Compromis assumé : si la cible écrasée avait des hard links,
  rename() casse le lien — le chemin écrit pointe vers le nouvel inode, les
  autres liens conservent l'ancien contenu.
- Erreurs réseau : si le serveur local est injoignable (curl échoue), 
  afficher un message clair mentionnant LLM_BASE_URL plutôt que de laisser
  remonter une exception brute.

## Current state
REPL minimal fonctionnel : lecture utilisateur, appel chat completions
OpenAI-compatible en streaming (affichage progressif), historique borné et
tool-calling actif avec quatre outils sandboxés (list_files, read_file,
search_files, write_file).

## Instructions pour l'agent
- Ne pas ajouter de framework externe sans le demander explicitement
- Garder chaque fichier court et une seule responsabilité
- Commits atomiques, un changement logique par commit, format Conventional
  Commits (feat, fix, refactor, chore, docs, build, test)
