# PROJECT_CONTEXT.md — contexte de référence (local-agent-cpp)

> Audit réalisé le 2026-09-22 à HEAD `4f33b62`, arbre de travail propre.
> Ce document décrit l'état **réel** du code à HEAD. Chaque affirmation est
> adossée à un fichier/ligne ou à un diff Git identifié. Les éléments
> incertains sont explicitement marqués `[DÉDUCTION]`, `[NON VÉRIFIÉ]` ou
> renvoyés vers [`OPEN_QUESTIONS.md`](../questions/OPEN_QUESTIONS.md).

## 1. Identité du projet

- **Nom** : `local-agent-cpp` (cible CMake `project(agent_cpp CXX)`).
- **Nature** : agent conversationnel IA minimal en C++20, en ligne de commande.
- **Principe** : boucle REPL locale → `POST /v1/chat/completions`
  (OpenAI-compatible, **streaming SSE**) → réponse ; le modèle peut appeler
  automatiquement des outils fichiers sandboxés.
- **Cible d'exécution** : machine locale, serveur LLM local
  (README cite llama.cpp, Ollama, LM Studio, vLLM comme exemples — **aucun
  serveur précis n'est attesté comme cible effective dans le dépôt**, voir
  [`OPEN_QUESTIONS.md`](../questions/OPEN_QUESTIONS.md)).
- **Portée** : 27 commits, du 2026-09-21 22:26:19 +0200 au 2026-09-22
  14:07:50 +0200. Branche unique `main`, **aucun remote, aucun tag, aucun
  stash**. 19 fichiers suivis.

## 2. État Git à HEAD

| Élément | Valeur |
|---|---|
| HEAD | `4f33b62 docs: document streaming behavior and tool-call feedback` |
| Branche | `main` (seule branche) |
| Remotes / tags / stash | aucun |
| Modifications locales suivies | aucune (`git status` propre) |
| Fichiers suivis | 19 |
| Fichier local non suivi | `.env` (ignoré par `.gitignore`) — contient les 4 variables, `LLM_API_KEY` **non vide** (valeur non inspectée) |
| Dossier `build/` | présent, ignoré par Git |

## 3. Structure du build (faits confirmés)

`CMakeLists.txt` (32 lignes) définit :

- `find_package(CURL REQUIRED)` — libcurl système.
- FetchContent `nlohmann/json` **v3.11.3** (archive `json.tar.xz`).
- `add_library(agent_core STATIC)` : `agent.cpp`, `http_client.cpp`,
  `sse_parser.cpp`, `tool_registry.cpp`, `workspace_tools.cpp`.
  - `target_include_directories(agent_core PUBLIC include)`
  - `target_link_libraries(agent_core PUBLIC CURL::libcurl nlohmann_json::nlohmann_json)`
- `add_executable(agent src/main.cpp)` + `target_link_libraries(agent PRIVATE agent_core)`.
- `CMAKE_CXX_STANDARD 20`, `CMAKE_EXPORT_COMPILE_COMMANDS ON`.

Conséquence pratique (documentée dans AGENTS.md) : un test/harnais se compile
en liant `agent_core`, sans relister les `.cpp`.

## 4. Inventaire des fichiers à HEAD

### 4.1 Bibliothèque `agent_core`

| Fichier | Lignes | Responsabilité |
|---|---|---|
| `include/agent.hpp` | 43 | `Message`, `AgentConfig`, `AgentCallbacks`, classe `Agent` |
| `src/agent.cpp` | 297 | historique, payload, reconstruction des deltas SSE, dispatch des outils |
| `include/http_client.hpp` | 29 | `HttpError`, classe `HttpClient` (`post`, `post_streaming`) |
| `src/http_client.cpp` | 158 | wrapper libcurl (RAII), helper `perform_request` commun |
| `include/sse_parser.hpp` | 21 | classe `SseParser` (`feed`) |
| `src/sse_parser.cpp` | 107 | framing SSE pur (aucune connaissance OpenAI) |
| `include/tool.hpp` | 13 | `struct Tool {name, description, schema, handler}` |
| `include/tool_registry.hpp` | 19 | classe `ToolRegistry` |
| `src/tool_registry.cpp` | 53 | enregistrement/exécution/schémas des outils |
| `include/workspace_tools.hpp` | 7 | `register_workspace_tools(...)` |
| `src/workspace_tools.cpp` | 396 | sandbox + 4 outils fichiers |

### 4.2 Exécutable et documentation

| Fichier | Lignes | Rôle |
|---|---|---|
| `src/main.cpp` | 104 | config env, création de `workspace/`, boucle REPL, callbacks d'affichage, gestion d'erreurs |
| `CMakeLists.txt` | 32 | build (voir §3) |
| `README.md` | 35 | prérequis, config, build, run, renvoi vers `USAGE.md` |
| `USAGE.md` | 254 | guide d'utilisation (config, REPL, outils, limites, dépannage) |
| `AGENTS.md` | 106 | instructions projet, décisions d'architecture, état |
| `CLAUDE.md` | 2 | simple renvoi vers `AGENTS.md` |
| `.env.example` | 4 | documente les 4 variables |
| `.gitignore` | 9 | ignore `build/`, objets, `.cache/`, `compile_commands.json`, `.vscode/`, `.DS_Store`, `.env` |
| `workspace/.gitkeep` | 1 | conserve le dossier sandbox dans Git |

**Absent du dépôt** (vérifié par `git ls-files`) : suite de tests, CI,
licence, `.clang-format`, `Doxyfile`.

## 5. Configuration (faits confirmés — `src/main.cpp`)

Toutes les variables sont **optionnelles** (aucune n'est obligatoire) ; une
valeur vide est traitée comme absente (`getenv_or_default`).

| Variable | Défaut | Remarque |
|---|---|---|
| `LLM_BASE_URL` | `http://localhost:8080/v1/chat/completions` | endpoint POST |
| `LLM_MODEL` | `local-model` | champ `model` du payload |
| `LLM_API_KEY` | `""` | si non vide → en-tête `Authorization: Bearer <clé>` |
| `LLM_TIMEOUT_SECONDS` | `60` | entier strictement positif ; sinon `stderr` `Invalid LLM_TIMEOUT_SECONDS value, using 60 seconds.` et repli sur 60 |

Lecture exclusivement via `std::getenv()` ; **aucun parsing `.env` en C++**
(le `.env` est chargé par le shell : `set -a; source .env; set +a`).

## 6. Comportement du REPL (faits confirmés — `src/main.cpp`)

- Bannière : `agent-cpp ready. Type 'exit' to quit.`
- Invite : `> `
- `exit` **ou** fin de flux (`Ctrl-D`) → fin de boucle.
- Ligne vide → ignorée, pas d'appel réseau.
- Affichage **progressif** : `on_text_delta` écrit chaque fragment sur
  `std::cout` avec `flush()` ; un `\n` est ajouté au retour de `send()`.
- Pendant un cycle d'outils : `on_tool_call` affiche
  `[appel de <nom>...]` sur une ligne dédiée.
- Message user → ajouté à l'historique **avant** la boucle d'itérations.
- Erreurs (le REPL ne s'arrête pas) :
  - serveur injoignable / statut non-2xx → `stderr`
    `Impossible de contacter LLM_BASE_URL (<url>): <détail>`
  - autre exception → `stderr` `Reponse du serveur inattendue: <détail>`
  - si du texte a déjà été streamé, un `\n` est émis avant le message
    d'erreur pour qu'il démarre sur sa propre ligne.

## 7. Protocole HTTP et streaming (faits confirmés)

- `HttpClient::post_streaming(url, headers, body, timeout, on_data)` :
  - callback d'écriture cumulant le corps **et** appelant `on_data(chunk)`
    par fragment ;
  - retourne le corps complet accumulé ;
  - mêmes erreurs que `post()` : `HttpError("HTTP POST failed: ...")` sur
    échec curl, `HttpError("HTTP POST failed with status <N>: <corps>")` sur
    statut non-2xx ;
  - `CURLOPT_TIMEOUT = timeout_seconds` (pas de `CONNECTTIMEOUT` distinct).
- `post()` reste défini et fonctionnel, mais **n'est plus appelé** par
  `Agent` (voir dette technique, [`PROJECT_HISTORY.md`](../history/PROJECT_HISTORY.md) §5).
- `SseParser::feed(chunk) -> vector<string>` : bufferise, découpe aux
  séparateurs d'événement (`\n\n` ou `\r\n\r\n`), ignore les commentaires
  `:` et les autres champs, retourne les payloads `data:` (préfixe retiré).
  `[DONE]` est retourné tel quel. **Aucun `flush()`** exposé.
- Reconstruction dans `agent.cpp` :
  - `content` concaténé ; `on_text_delta` appelé immédiatement ;
  - `tool_calls` accumulés dans un `std::map<std::size_t, ToolCallAccumulator>`
    indexé par le champ `index` ; `id` **assigné**, `name`/`arguments`
    **concaténés** ; `arguments` vide normalisé en `"{}"` ;
  - fragment JSON invalide → **ignoré silencieusement** ;
  - message final `{role:"assistant", content, tool_calls?}` au format
    attendu par l'historique.
- **Invariant** : si `post_streaming` lève, le message assistant partiel
  n'est jamais ajouté à l'historique ; seul le message user du tour reste.

## 8. Outils (faits confirmés — `src/workspace_tools.cpp`)

Constantes : `max_read_chars = 50'000`, `max_write_chars = 200'000`,
`binary_probe_bytes = 4096`, `max_search_results = 50`.

Sandbox : racine `std::filesystem::absolute("workspace")` **relative au
répertoire de lancement**, créée au démarrage. `sandbox_path()` rejette :
chemin vide, chemin absolu, tout composant `..`, et toute sortie du dossier
(vérification `is_inside_workspace`). Les composants commençant par `.` sont
« cachés ».

| Outil | Entrées | Comportement | Limite |
|---|---|---|---|
| `list_files` | aucune | liste récursive des fichiers **réguliers non cachés**, chemins relatifs | pas de plafond de nombre |
| `read_file` | `path` (req.) | rejette fichier inexistant / non régulier / binaire (NUL dans les 4096 premiers octets) | troncature à 50 000 car. + `[truncated: file exceeds 50000 characters]` |
| `search_files` | `query` (req.) | recherche **littérale**, sensible à la casse, format `chemin:ligne:contenu`, ignore fichiers cachés/binaires | 50 résultats + `[truncated: reached 50 search results]` |
| `write_file` | `path`, `content` (req.), `overwrite` (opt., défaut `false`) | écriture **atomique** (temporaire `<nom>.tmp.<ticks>.<compteur>` puis `rename`), permissions préservées à l'écrasement, répertoires parents créés | rejet en bloc si `content` > 200 000 car. (pas de troncature) |

Messages d'erreur notables de `write_file` : `file already exists: <chemin>
(pass overwrite: true to replace it)`, `cannot write file: path is an existing
directory: <chemin>`, `content is <N> characters, exceeding the maximum write
size of 200000; write the file in smaller parts instead`,
`failed to read existing file permissions: ...`,
`failed to apply existing file permissions to temporary file: ...`,
`failed to replace target file: ...`. Les erreurs d'outil sont transformées
en `tool error: <message>` **renvoyées au modèle** (pas affichées à
l'utilisateur).

## 9. Codes d'erreur et invariants transverses

- `HttpError` (dérive `std::runtime_error`) : seul type distingué par le REPL.
- Échec d'un outil → jamais d'exception propagée à l'appelant : converti en
  message `tool error: ...` dans l'historique.
- Dépassement de `max_tool_iterations` (5) →
  `std::runtime_error("tool call loop exceeded maximum iterations")`, affiché
  par le REPL sous la forme générique `Reponse du serveur inattendue: ...`.
- `trim_history()` raisonne **en tours complets** débutant sur un message
  `role=user` ; il ne supprime jamais le dernier tour utilisateur, même
  surdimensionné (limitation assumée, documentée dans AGENTS.md).

## 10. État de validation (distinction essentielle)

Trois niveaux à ne pas confondre :

1. **Implémenté** : présent dans le code à HEAD (tout ce qui précède).
2. **Validé de façon reproductible depuis le dépôt** : **néant**. Il n'existe
   aucune suite de tests, aucun script de validation, aucune CI dans le
   dépôt ; `AGENTS.md` confirme « Pas de suite de tests pour l'instant ».
3. **Validé hors dépôt (sessions de travail, non reproductible)** : les
   itérations documentées dans l'historique de session ont utilisé des
   harnais C++ et des mock serveurs créés dans des répertoires temporaires
   (`/tmp`, `$TMPDIR`) **puis supprimés**. Ces validations ne sont ni
   versionnées ni rejouables telles quelles ; elles ne doivent pas être
   présentées comme des preuves présentes dans le dépôt. `[NON VÉRIFIÉ]` dans
   le cadre de cet audit repo-only.

## 11. Navigation rapide pour une prochaine session

- Comprendre la boucle : `src/agent.cpp` (`Agent::send`, `apply_stream_delta`,
  `build_assistant_message`, `trim_history`).
- Comprendre le réseau : `src/http_client.cpp` (`perform_request`,
  `streaming_write_callback`) et `src/sse_parser.cpp`.
- Comprendre le sandbox : `src/workspace_tools.cpp` (`sandbox_path`,
  `TemporaryFileGuard`, les 4 handlers).
- Comprendre l'affichage : `src/main.cpp` (callbacks, catch HttpError).
- Règles de travail : `AGENTS.md` (RAII, un fichier = une responsabilité,
  Conventional Commits, pas de framework externe sans accord explicite).
