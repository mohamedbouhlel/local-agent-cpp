# PROJECT_CONTEXT.md — photographie technique du projet

> **Statut : RÉFÉRENCE COURANTE DATÉE, avec annexe historique préservée**
>
> - Observation distante : **2026-11-05** (Europe/Paris).
> - Dépôt : `mohamedbouhlel/local-agent-cpp`, branche `main`.
> - **Commit exact examiné : `add64b893ad447e76fb12b90e266ec8eab2f2956`** (fusion de PR #5).
> - Méthode : lecture du code, de CMake, des tests et de la documentation versionnés au commit indiqué.
> - **Validation : structure et comportements vérifiés par inspection ; aucun build, CTest ou scénario LLM exécuté lors de M2.** Les chiffres d'assertions et succès des tests sont *rapportés* par les documents du dépôt/PR #5, pas reproduits ici.
> - Worktree local, état des remotes locaux et environnement d'exécution : **NON VÉRIFIÉS**.
>
> Cette photographie **remplace l'ancienne introduction comme description courante**. L'audit à `4f33b62` est intégralement conservé plus bas sous « Archive historique » : ses constats ne concernent **que cet ancien commit**. L'historique chronologique reste dans [PROJECT_HISTORY.md](../history/PROJECT_HISTORY.md). Les propositions et questions restent dans [ROADMAP.md](../roadmap/ROADMAP.md) et [OPEN_QUESTIONS.md](../questions/OPEN_QUESTIONS.md), qui peuvent encore contenir des relevés antérieurs tant que M3 n'est pas exécuté.

## Suivi de réconciliation — M5 au `2b54563f650e0995603472261ecfa306f53bd654` (2026-11-05)

> **FAIT VÉRIFIÉ PAR LECTURE DU CODE, NON TESTÉ EN M5 :** après la
> photographie M2 au SHA `add64b8`, les PR #6 (M2), #7 (M3) et #9 (M4)
> ont été fusionnées. La PR #10 a ensuite modifié
> `src/filesystem_sandbox.cpp` : la règle Landlock du workspace retire
> `LANDLOCK_ACCESS_FS_EXECUTE`, ce qui interdit l'exécution de fichiers
> depuis le workspace par le processus confiné. Le fichier
> `tests/filesystem_sandbox_test.cpp` a été étendu avec un scénario
> d'exécution refusée **dans un test existant**.
>
> L'arbre de `main` au commit `2b54563f650e0995603472261ecfa306f53bd654` compte **88 fichiers suivis**,
> dont **40 rapports** et **13 fichiers de tests**. Le nombre de déclarations
> `TEST(...)` reste **118** ; `tests/CMakeLists.txt` déclare toujours
> **7 groupes sous Linux**. Les **403 assertions et 7/7 réussis** restent des
> résultats **rapportés avant la PR #10** : **aucun build ni CTest n'a été
> rejoué pendant M5**, et la nouvelle restriction Landlock n'est pas
> certifiée par cet audit documentaire.
>
> Les passages datés de M2 ci-dessous, y compris leurs compteurs et mentions
> de dettes documentaires alors ouvertes, restent exacts pour leur **SHA
> d'observation** et ne sont pas réécrits rétroactivement. Voir
> [le rapport M5](../reports/2026-11-05-m5-documentation-final-consistency.md)
> pour les preuves et les limites.

## 1. Identité, environnement et périmètre vérifié

**FAIT VÉRIFIÉ AU SHA** — Agent conversationnel CLI en C++20. `src/main.cpp` charge la configuration, prépare `workspace/` relativement au répertoire de lancement, enregistre les outils, active le confinement filesystem, puis lance la boucle REPL. L'agent appelle un endpoint compatible OpenAI `POST /v1/chat/completions` en streaming SSE (sources : `src/main.cpp`, `src/agent.cpp`, `src/http_client.cpp`).

**FAIT VÉRIFIÉ AU SHA** — Le programme actuel exige **Linux** pour démarrer en mode normal : `apply_filesystem_sandbox()` refuse explicitement le démarrage hors Linux. Cela ne constitue pas une décision de portabilité future. Le `README.md` fournit encore une instruction de dépendance macOS : ne pas en déduire que l'exécution macOS est prise en charge (source : `src/filesystem_sandbox.cpp`).

**FAIT VÉRIFIÉ AU SHA** — Le dépôt distant comporte 85 fichiers suivis, dont 37 rapports dans `docs/reports/` et 13 fichiers dans `tests/` (inventaire de l'arbre Git du commit). Ce nombre **n'est pas** celui de l'audit historique `4f33b62` (19 fichiers).

## 2. Build, composants et dépendances

**FAIT VÉRIFIÉ AU SHA** — `CMakeLists.txt` impose CMake >= 3.20, C++20, `CURL::libcurl` et `nlohmann_json::nlohmann_json` ; nlohmann/json **v3.11.3** est récupéré par `FetchContent`. Il construit :
- la bibliothèque statique `agent_core` (9 sources) : `agent.cpp`, `filesystem_sandbox.cpp`, `http_client.cpp`, `prompt_reader.cpp`, `sse_parser.cpp`, `tool_registry.cpp`, `workspace_tools.cpp`, `workspace_search.cpp`, `workspace_write.cpp` ;
- l'exécutable `agent` à partir de `src/main.cpp`, lié à `agent_core` ;
- une suite de tests **désactivée par défaut**, activable par `-DAGENT_BUILD_TESTS=ON`.

Le guide d'installation et de lancement reste [README.md](../../README.md) ; les instructions d'utilisation détaillées restent [USAGE.md](../../USAGE.md). Les commandes d'exécution ci-dessous sont des **instructions issues du dépôt, non rejouées pendant M2** :

```sh
cmake -S . -B build
cmake --build build
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

## 3. Configuration et fonctionnement de l'agent

**FAIT VÉRIFIÉ AU SHA** — `src/main.cpp` lit les variables d'environnement (pas de parseur `.env` dans le programme) :
- `LLM_BASE_URL` : `http://localhost:8080/v1/chat/completions` par défaut ;
- `LLM_MODEL` : `local-model` par défaut ;
- `LLM_API_KEY` : vide par défaut, transmise via le header Bearer si renseignée ;
- `LLM_TIMEOUT_SECONDS` : 60 s par défaut ; une valeur invalide ou non positive déclenche le retour à 60 s avec avertissement.

**FAIT VÉRIFIÉ AU SHA** — Le REPL est simple ligne ou multiligne explicite : `:multi` ouvre un bloc, `:end` le termine, un bloc de zéro ligne est ignoré et un EOF avant le terminateur abandonne la saisie sans envoi (`src/prompt_reader.cpp`, `src/main.cpp`). L'implémentation existe, même si la [spécification approuvée](../specifications/repl-multiline-requirements.md) porte encore la phrase historique « aucune implémentation » (à réconcilier hors M2).

**FAIT VÉRIFIÉ AU SHA** — `Agent::send()` consomme les événements SSE, reconstitue le texte et les `tool_calls` à partir des deltas, appelle `SseParser::flush()`, exige `[DONE]`, rejette `finish_reason="length"` et les appels d'outils dépourvus d'identifiant/nom valide ; les erreurs de protocole sont distinguées des erreurs de transport (`src/agent.cpp`, `include/agent.hpp`). L'historique est limité à **20 messages**, coupé sur les frontières de tours ; la boucle limite à **5** itérations d'outils (`src/agent.cpp`). Ces limites ne sont pas présentées comme des valeurs configurables.

**FAIT VÉRIFIÉ AU SHA** — `HttpClient::post_streaming()` utilise libcurl ; le timeout de connexion est plafonné à `min(timeout_total, 10 s)`. `HttpClient::post()` non-streamé subsiste, sans appelant de production observé, conformément à la décision P3.1 antérieure (`src/http_client.cpp`, `src/agent.cpp`, rapport P3.1). La CLI affiche les succès/échecs effectifs des outils séparément du texte généré par le modèle (`src/main.cpp`, `include/agent.hpp`).

## 4. Outils fichiers et limites de sécurité

**FAIT VÉRIFIÉ AU SHA** — Quatre outils sont enregistrés dans `src/workspace_tools.cpp` : `list_files`, `read_file`, `search_files`, `write_file`. `read_file` limite le contenu à **50 000 caractères/octets stockés dans la chaîne**, `search_files` limite les résultats à **50 correspondances** et `write_file` refuse un contenu supérieur à **200 000 caractères/octets stockés dans la chaîne**. Les chemins absolus et composants `..` sont refusés. `write_file` requiert `overwrite: true` pour remplacer un fichier existant.

**FAIT VÉRIFIÉ AU SHA** — La préparation de `workspace/` refuse une racine de workspace qui est un lien symbolique ; le confinement **Landlock** est ensuite imposé au processus avant le REPL et échoue en mode fermé s'il ne peut pas être activé (`src/filesystem_sandbox.cpp`, `src/main.cpp`). Les autorisations de lecture hors workspace sont limitées à quelques chemins explicitement listés pour TLS et résolution réseau, lorsqu'ils existent.

**FAIT VÉRIFIÉ AU SHA** — La lecture et la recherche suivent les chemins au moyen de descripteurs et de `O_NOFOLLOW`, et rejettent les fichiers à liens physiques multiples (`st_nlink != 1`) ; l'écriture utilise des opérations relatives aux descripteurs (`openat`, `linkat` ou `renameat`), une vérification du contenu temporaire puis de l'identité du fichier publié (`src/workspace_tools.cpp`, `src/workspace_search.cpp`, `src/workspace_write.cpp`).

**LIMITES / NON VÉRIFIÉ** — Landlock ne constitue ni une isolation réseau, ni une isolation des autres processus du même utilisateur ; des manipulations concurrentes du workspace peuvent subsister. Une vérification d'identité **après publication** peut signaler un problème sans garantir l'absence d'effets préalables. La visibilité atomique d'un renommage ne prouve pas la durabilité après crash : **aucun `fsync` observé** dans `src/workspace_write.cpp`. Se reporter à [l'analyse d'isolation](../reports/2026-11-04-isolation-systeme.md) pour les essais **historiques**, sans les confondre avec une preuve de sécurité exhaustive du code courant.

## 5. Tests et état de validation

**FAIT VÉRIFIÉ AU SHA** — Le harnais maison versionné dans `tests/` repose sur `agent_tests` et un mock HTTP loopback. Le code contient **118 déclarations `TEST(...)`**, réparties entre **7 groupes CTest sous Linux** (`sse`, `workspace`, `http`, `agent`, `repl`, `mock`, `sandbox`) ; chaque test CTest a un timeout de **30 s** (`tests/CMakeLists.txt`). Le groupe `sandbox` est conditionné à Linux.

**VALIDATION RAPPORTÉE, NON REJOUÉE À M2** — `AGENTS.md` et la PR #5 rapportent **118 cas, 403 assertions et 7/7 groupes réussis**. Les **118 déclarations** et la configuration des **7 groupes** sont vérifiées dans le code ; les **403 assertions exécutées** et le verdict **7/7** ne sont pas certifiés par une nouvelle exécution M2. La PR #5 documente aussi une limite sur la vérification positive HTTPS/CA. **Aucune CI GitHub Actions** n'est présente dans l'arbre consulté.

## 6. Incertitudes, décisions et maintenance documentaire

- **NON VÉRIFIÉ** : environnement local, compilabilité sur toutes les distributions, test avec un vrai serveur LLM, cible serveur/modèle unique, résistance à toutes les courses concurrentes, durabilité après crash.
- **DÉCISION APPROUVÉE PRÉEXISTANTE** : exigences `REPL-01` à `REPL-09` ; conservation de `HttpClient::post()` en P3.1. Le code observé n'autorise pas à présumer d'autres choix produit.
- **DOCUMENTS NON ENCORE RÉCONCILIÉS À M2** : `ROADMAP.md`, `OPEN_QUESTIONS.md`, `PROJECT_HISTORY.md`, `AGENTS.md`, `README.md`, `USAGE.md` et la spécification REPL peuvent encore présenter des chiffres ou des formulations d'étapes antérieures.
- Cette section de référence devra être revue lors d'un changement de code/configuration/tests, avec un **nouveau SHA** et un rapport daté. Les rapports anciens ne doivent pas être réécrits pour leur attribuer rétroactivement un état ultérieur.

---

## Archive historique — audit initial du 22 septembre 2026 à `4f33b62`

> **STATUT : HISTORIQUE.** Le texte ci-dessous est conservé intégralement comme artefact de l'audit initial. Les expressions « HEAD », « actuel », « aucune suite de tests », « aucun remote » et les comptes de fichiers y désignent **exclusivement `4f33b62`**. Elles ne décrivent pas le HEAD `add64b893ad447e76fb12b90e266ec8eab2f2956`. Ne pas interpréter les liens de fichiers ou numéros de lignes anciens comme garantis sur le code actuel.

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
- `post()` est **conservé** (décision P3.1) : défini et fonctionnel, **sans**
  appelant en **production** (`Agent` utilise `post_streaming`), mais **couvert
  par 4 tests** (`tests/http_client_test.cpp`). L'absence d'appelant interne ne
  justifie pas sa suppression : il délègue à `perform_request()` (partagé) et
  reste disponible comme API non-streamée.
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
