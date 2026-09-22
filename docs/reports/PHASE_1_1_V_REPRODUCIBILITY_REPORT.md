# P1.1-V — Audit versionnabilité et reproductibilité de la suite de tests

> Itération d'**audit + préparation**. **Aucun commit, aucun push, aucun
> staging.** Aucune modification de `src/`, `include/`, du protocole HTTP/SSE,
> du mock ou des tests. Un seul fichier créé : ce rapport.

## État initial

| Élément | Valeur observée |
|---|---|
| Branche | `main` |
| HEAD | `1c46372e822c0d982b11628425fdf697cb0653a8` (`docs(streaming): add pre-implementation audit report`) — **et non** `4f33b62` : un commit a été créé depuis l'audit streaming |
| `git status --short --branch` | `## main` · ` M CMakeLists.txt` · `?? docs/README.md`, `?? docs/context/`, `?? docs/history/`, `?? docs/questions/`, `?? docs/roadmap/`, `?? docs/reports/PHASE_1_1_{REPORT,REVIEW_REPORT,R2_MOCK_TIMEOUT_REPORT}.md`, `?? tests/` |
| `git diff --stat` | ` CMakeLists.txt | 8 ++++++++` — **modification préexistante** (P1.1), préservée |
| `git diff -- CMakeLists.txt` | `option(AGENT_BUILD_TESTS ... OFF)` + `if(...) enable_testing() add_subdirectory(tests) endif()` |
| `git ls-files tests` | **AUCUN fichier de tests suivi par Git** |
| `git ls-files docs` | `docs/reports/streaming-audit.md` (seul document versionné) |

Documents de gouvernance relus avant action : `AGENTS.md`, `README.md`,
`USAGE.md`, `docs/context/PROJECT_CONTEXT.md`, `docs/history/PROJECT_HISTORY.md`,
`docs/roadmap/ROADMAP.md`, `docs/questions/OPEN_QUESTIONS.md`,
`docs/reports/PHASE_1_1_REVIEW_REPORT.md` (R-01 → R-09),
`docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md` (L1 → L6).

## Inventaire

### Fichiers de tests (10 fichiers, 2 158 lignes, **tous non suivis**)

| Fichier | Lignes | Rôle | Cible | Groupe | Dépendances |
|---|---|---|---|---|---|
| `tests/test_support.hpp` | 157 | micro-harnais : registre de cas, `TEST`, 5 assertions, `capture_error`, `TempDir` RAII | en-tête | — | `<filesystem>`, `<sstream>`, `<functional>` |
| `tests/test_main.cpp` | 49 | exécuteur `main`, `--filter=<groupe>`, résumé, codes 0/1/2 | `agent_tests` | — | `test_support.hpp` |
| `tests/mock_http_server.hpp` | 76 | API du serveur HTTP loopback scriptable | en-tête | — | `<atomic>`, `<thread>`, `<mutex>` |
| `tests/mock_http_server.cpp` | 291 | serveur POSIX (bind loopback, port éphémère, `SO_RCVTIMEO`, `408`, arrêt prompt) | `agent_tests` | — | POSIX sockets, `<sys/time.h>`, `<cerrno>` |
| `tests/sse_parser_test.cpp` | 192 | framing SSE | `agent_tests` | `sse` | `sse_parser.hpp` |
| `tests/workspace_tools_test.cpp` | 473 | sandbox + 4 outils + registre | `agent_tests` | `workspace` | `workspace_tools.hpp`, `tool_registry.hpp`, `<unistd.h>` (`geteuid`) |
| `tests/http_client_test.cpp` | 124 | `post`, non-2xx, streaming, coupures | `agent_tests` | `http` | `http_client.hpp`, `mock_http_server.hpp` |
| `tests/agent_streaming_test.cpp` | 587 | reconstruction tool_calls + historique, bout en bout via mock | `agent_tests` | `agent` | `agent.hpp`, `mock_http_server.hpp`, `nlohmann/json` |
| `tests/mock_http_server_test.cpp` | 184 | **tests R-01** : silence, requête incomplète, 408, arrêt prompt | `agent_tests` | `mock` | idem + sockets brutes |
| `tests/CMakeLists.txt` | 25 | cible unique + 5 `add_test` + `TIMEOUT 30` | — | — | `find_package(Threads)` |

### Cibles, groupes CTest et dépendances

- **Une seule cible de test** : `agent_tests` (6 sources + `mock_http_server.cpp`), liée à
  `agent_core` **PUBLIC** et `Threads::Threads` ; `-Wall -Wextra -Wpedantic`.
- **5 groupes CTest**, un processus par groupe : `sse` (16 cas), `workspace` (25),
  `http` (6), `agent` (12), `mock` (5) → **64 cas / 239 assertions**.
- **Dépendances héritées de la production, aucune nouvelle** : `agent_core`
  (donc `libcurl` système + `nlohmann_json` via FetchContent) et `Threads`
  (POSIX, partie de la libc). Aucun framework de test externe.

## Audit

### Analyse statique — reproductibilité

| Point audité | Constat | Preuve |
|---|---|---|
| Chemins absolus machine | **aucun `/home/`** dans `tests/` ni `CMakeLists.txt` | `grep -Rn "/home/"` → 0 |
| Chemins `/tmp/` | **un seul cas** : un test de **rejet** de chemin absolu référence `/tmp/agent_cpp_escape.txt` en dur | `tests/workspace_tools_test.cpp:235,237` (cf. **E3**) |
| Ports | **éphémères** : `sin_port = 0` puis `getsockname` ; jamais de port fixe | `mock_http_server.cpp:115,128` |
| Adresses | **loopback uniquement** `INADDR_LOOPBACK` / `127.0.0.1` | `mock_http_server.cpp:114,156`, `mock_http_server_test.cpp:45` |
| Délais | `piece_delay_us = 5000` et un `sleep_for(200 ms)` | `http_client_test.cpp:62`, `mock_http_server_test.cpp:176` (cf. **E4**) |
| Répertoires temporaires | `temp_directory_path() / ("agent_cpp_test_" + tag)`, `remove_all` avant + en destructeur | `test_support.hpp:84-94` |
| **Unicité des tags** | **38 tags, 38 distincts** → aucun risque de collision *intra*-suite | `grep` + `sort -u` |
| **Partage inter-processus** | le chemin temporaire est **global et déterministe** (tag fixe, ni PID ni build) | `test_support.hpp:84` (cf. **E2**) |
| Variables d'environnement | **aucune lecture** dans `tests/` | `grep -Rn "getenv|TMPDIR|environ"` → 0 |
| `TMPDIR` | influence seulement l'**emplacement** des temporaires (via `temp_directory_path()`), pas le résultat | cf. **E5** |
| Service externe | **aucun** : le seul serveur est le mock in-process sur loopback | conception des groupes `http`/`agent`/`mock` |
| Fichiers de données de test | **aucune fixture externe** : tout est généré à la volée dans `TempDir` | revue des 5 fichiers de groupe |
| Hypothèse CWD | **aucune** : aucun `chdir`, aucune ouverture de chemin relatif | vérifié par exécution depuis 3 CWD (§Validation) |
| Ordre d'exécution | **aucune dépendance** : pas d'état statique mutable hors registre ; chaque groupe tourne dans son **propre processus** | revue + 4 builds × N runs |
| Nettoyage | `TempDir` et `PermissionRestorer` en RAII, destructeurs non lançants | `test_support.hpp:80-97`, `workspace_tools_test.cpp:74-92` |
| Parallélisation intra-build | `ctest --parallel` exécute 5 **processus** distincts, tags disjoints, ports éphémères | §Validation |

### Audit CMake — ce qui est intégré, ce qui reste

**Déjà intégré** : option `AGENT_BUILD_TESTS` **OFF par défaut** (`CMakeLists.txt:36`),
`enable_testing()` + `add_subdirectory(tests)` conditionnels (`:37-40`) ;
`tests/CMakeLists.txt` déclare la cible, les 5 groupes et un `TIMEOUT 30`.

**Reste à intégrer** : **rien sur le plan technique**. Le seul manque est
**administratif** : `tests/` n'est **pas suivi par Git** (`git ls-files tests` → 0).
Un `git add tests/` (itération ultérieure) suffirait ; aucun fichier ne dépend
d'un artefact extérieur.

**Clone propre** : vérifié **avec réseau** (§Validation, build « net ») — la seule
dépendance externe est celle de la production (`libcurl` + téléchargement
`nlohmann/json` par FetchContent, `CMakeLists.txt:13-17`). **Sans réseau et sans
cache**, la configuration échouerait — c'est un prérequis **hérité**, pas
introduit par les tests (cf. **E6**).

### Nouveaux tests du mock (P1.1-R2) — vérification ciblée

- Bornage par `SO_RCVTIMEO` (`mock_http_server.cpp:200-201`) et réponse **408**
  contrôlée (`:89-95`, appel `:241`) : stables sur **9 exécutions du groupe `mock`**
  (3 builds × 3) + `--parallel`.
- Réveil de la connexion active au destructeur (`:142-145`) : arrêts mesurés à
  **0 ms** dans les cas dédiés.
- **Aucune fragilité nouvelle** observée ; le groupe `mock` (~1,2 s) est le plus
  lent mais reste borné par des délais explicites (300 ms / 5 s) et par le
  `TIMEOUT 30` CTest.
- Seule réserve : le `sleep_for(200 ms)` de `mock_http_server_test.cpp:176`
  (cf. **E4**).

## Validation

### Commandes réellement exécutées et résultats

| # | Commande | Résultat observé |
|---|---|---|
| 1 | 3 × `cmake -S . -B /tmp/p111v-{a,b,c} -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=<repo>/build/_deps/json-src -DFETCHCONTENT_FULLY_DISCONNECTED=ON` | **configure=0** (×3), **hors ligne** |
| 2 | 3 × `cmake --build /tmp/p111v-{a,b,c}` | **build=0** (×3), **0 erreur / 0 avertissement** |
| 3 | 3 × `ctest -N` (un par build) | **5 tests découverts** (`sse`, `workspace`, `http`, `agent`, `mock`) — identiques sur les 3 builds |
| 4 | **9 ×** `agent_tests` (3 par build) | **64/64 cas · 239 assertions · 0 échec** à chaque fois (code 0) |
| 5 | **9 ×** `agent_tests --filter=mock` | **5/64 cas · 0 échec** à chaque fois (code 0) |
| 6 | 3 × `ctest` | `100% tests passed, 0 tests failed out of 5` (×3) |
| 7 | `ctest --parallel 2` puis `--parallel 4`, puis `--parallel 4` ×3 | `100% tests passed, 0 tests failed out of 5` — **5 passes**, aucune instabilité |
| 8 | `agent_tests` depuis **3 CWD** (`/`, `/tmp`, dépôt) | **64/64, code 0** dans les 3 cas ⇒ **aucune hypothèse sur le répertoire courant** |
| 9 | **Build « clone propre »** : `cmake -S . -B /tmp/p111v-net -DAGENT_BUILD_TESTS=ON` **sans aucun artefact préexistant** | configure **0**, json récupéré par FetchContent, build **0**, **0 avertissement**, `ctest -N` → **5 tests**, `agent_tests` → **64/64 · 239 assertions · 0 échec**, `ctest` → **5/5** |
| 10 | `cmake -S . -B /tmp/p111v-off -DAGENT_BUILD_TESTS=OFF` + build | **0 / 0** ; cibles `agent` + `agent_core` **seulement** ; `ctest -N` → **Total Tests: 0** ; aucun objet de test compilé |
| 11 | **Test de collision** : `--filter=workspace` lancé **simultanément** depuis les builds `a` et `b`, **6 itérations** | **6/6 itérations en ÉCHEC** dans les **deux** processus (1 à 9 assertions en échec) ⇒ **E2 démontré** |
| 12 | Vérification du dépôt après tous les runs | `workspace/` = `.gitkeep` seul ; `git status` inchangé |

**Bilan quantitatif** : **4 builds indépendants** (3 hors ligne + 1 « clone
propre »), **10 exécutions complètes** (9 + 1), **9 exécutions du groupe `mock`**,
**4 passes CTest complètes**, **5 passes CTest parallèles**, **3 CWD** testés,
**1 test de collision**, **1 build `OFF`**. **Aucune exécution annoncée non
réalisée.**

### Détail de la collision (E2) — preuve

Exécuter **deux builds distincts simultanément** sur le même groupe produit des
échecs dans les deux processus, avec des erreurs caractéristiques d'un
répertoire partagé : `file already exists: a.txt (pass overwrite: true to
replace it)`, `file does not exist`, `failed to open file for reading`,
`workspace path escapes sandbox` remplacé par `file does not exist`
(le répertoire du lien symbolique ayant été supprimé par l'autre processus).

**Mécanisme** : `tests/test_support.hpp:84` construit
`temp_directory_path() / ("agent_cpp_test_" + tag)` avec un **tag fixe** — ni PID,
ni identifiant de build — et `TempDir` fait `remove_all` avant `create_directories`
(`:86-87`). Deux processus exécutant le **même cas** se détruisent donc
mutuellement leur répertoire de travail.

**Portée** : cela n'affecte **pas** `ctest --parallel` (un seul build : les 5
groupes ont des tags **disjoints**), mais affecte **plusieurs builds/checkouts
exécutés en parallèle** sur la même machine, ou deux développeurs sur la même
machine.

### Conditions particulières rencontrées (non contournées)

- **Réseau requis pour FetchContent** (prérequis hérité) : la validation hors
  ligne a utilisé `FETCHCONTENT_SOURCE_DIR_JSON` + `FETCHCONTENT_FULLY_DISCONNECTED=ON`
  pointant sur `build/_deps/json-src` ; le build « clone propre » a, lui, utilisé
  le réseau — **disponible** dans cet environnement (téléchargement réussi).
- **`TMPDIR`** : sur cette machine `TMPDIR=/home/oper/tmp`, donc
  `temp_directory_path()` y résout. Aucun test ne dépend de sa **valeur**.
- **`uid` non-root (1000)** : condition requise pour que les cas de permissions
  s'exécutent (ils se *skippent* sous `root`) — **aucun SKIP** observé.

## Écarts

| ID | Gravité | Preuve | Impact | Correction | Portée |
|---|---|---|---|---|---|
| **E1** | **MAJEUR** | `git ls-files tests` → **0 fichier** ; `tests/` apparaît en `??` | La suite **n'est pas versionnée** : un clone propre ne l'a pas et ne peut pas la reconstruire. Elle ne peut pas encore servir de « base officielle de validation » au sens versionné. | `git add tests/` (+ `tests/CMakeLists.txt`) et commit `test:` — **interdit dans cette itération** | **Hors périmètre ici** ; à faire dans une itération dédiée |
| **E2** | **MAJEUR** | Collision **reproduite 6/6** entre deux builds concurrents (§Validation #11) ; mécanisme en `test_support.hpp:84-87` | Deux exécutions simultanées de la même suite se corrompent mutuellement ⇒ faux échecs possibles en CI multi-build ou en usage partagé. N'affecte **pas** `ctest --parallel` ni l'usage séquentiel. | Dériver le répertoire du **PID/build** (ex. suffixe `getpid()` ou variable injectée) — **non appliqué** | **Hors périmètre** (itération dédiée) |
| **E3** | **MINEUR** | `/tmp/agent_cpp_escape.txt` en dur dans un test de rejet : `workspace_tools_test.cpp:235,237` | Chemin absolu **dépendant de la machine** dans un répertoire **partagé** : si ce fichier existait (autre processus, exécution antérieure), l'assertion `!exists` échouerait. **Non reproduit** ici (le fichier était absent). | Utiliser le `TempDir` du cas comme cible de rejet — **non appliqué** | **Hors périmètre** (itération dédiée) |
| **E4** | **MINEUR** | `http_client_test.cpp:62` (`piece_delay_us = 5000`), `mock_http_server_test.cpp:176` (`sleep_for(200 ms)`) | Tester la progressivité exige du temps réel ⇒ sensibilité théorique à la charge. **Aucune instabilité observée** (9 + 9 exécutions, 5 passes parallèles). | Conserver la synchronisation observée (lecture bloquante) plutôt qu'un délai — **non appliqué** | **Hors périmètre** |
| **E5** | **INFO** | `test_support.hpp:84` (`temp_directory_path()`) | L'**emplacement** des temporaires dépend de `TMPDIR`/du système ; le **résultat** n'en dépend pas. Un `TMPDIR` non inscriptible ferait échouer les cas (pas de repli). | Aucune | — |
| **E6** | **INFO** | `CMakeLists.txt:13-17` (FetchContent) ; build « clone propre » réussi **avec réseau** | Un clone **sans réseau et sans cache** ne peut pas configurer (échec à l'étape FetchContent). Prérequis **hérité de la production**, identique pour `agent` seul. | Documenter le prérequis (option `FETCHCONTENT_SOURCE_DIR_JSON` / vendoring) | **Hors périmètre** |
| **E7** | **INFO** | `CMAKE_BUILD_TYPE` vide dans le cache | Tests exécutés **sans optimisation** ni `-DNDEBUG` : plus lent, sans incidence sur la validité. | Aucune | — |
| **E8** | **INFO** | Mécanisme `add_test` + `set_tests_properties(... TIMEOUT 30)` (`tests/CMakeLists.txt:22-24`) | Un blocage futur devient un **échec** CTest au lieu d'un job suspendu — garde-fou **positif**, déjà en place. | Aucune | — |
| **E9** | **INFO** | `docs/reports/PHASE_1_1_REVIEW_REPORT.md` (R-02 → R-08) ; `docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md` (L1, L2, L5) | Dettes connues du mock et de la suite, **inchangées** : `pending_responses()` utilisé seulement par les nouveaux tests, 500 silencieux sur file vide, mock mono-thread. | — | **Hors périmètre** (R-02 → R-08 non traités) |

**Aucun écart BLOQUANT.** Aucune correction appliquée (conformément à §10).

## Réponse à la question centrale

**`tests/` est reproductible : OUI, démontré** — 4 builds indépendants
(dont un sans aucun artefact préexistant), 10 exécutions complètes vertes,
5 passes CTest parallèles, indépendance au CWD vérifiée, mode `OFF` intact,
aucun accès réseau hors prérequis hérité documenté, aucune dépendance à l'ordre
d'exécution, aucune donnée externe.

**`tests/` est versionnable : OUI techniquement, NON en l'état** — la suite est
autonome et n'a besoin d'**aucun** fichier nouveau pour être intégrée, mais
**aucun de ses 10 fichiers n'est suivi par Git** (**E1**). Trois écarts sont par
ailleurs documentés avant de la déclarer « base officielle » : collision entre
exécutions concurrentes (**E2**, reproduit), chemin partagé en dur (**E3**),
sensibilité temporelle bornée (**E4**).

## État final

```bash
$ git status --short --branch
## main
 M CMakeLists.txt
?? docs/README.md
?? docs/context/
?? docs/history/
?? docs/questions/
?? docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md
?? docs/reports/PHASE_1_1_REPORT.md
?? docs/reports/PHASE_1_1_REVIEW_REPORT.md
?? docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md
?? docs/roadmap/
?? tests/

$ git rev-parse HEAD
1c46372e822c0d982b11628425fdf697cb0653a8
```

- **Fichiers modifiés par cette itération** : **un seul**, créé —
  `docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md`.
- `src/`, `include/`, `tests/`, `CMakeLists.txt` : **non modifiés**
  (` M CMakeLists.txt` = modification **préexistante** de P1.1, inchangée).
- **Aucun staging** (`git diff --cached` vide), **aucun commit**, **aucun push** ;
  HEAD **identique** à l'état initial (`1c46372`).
- Tous les artefacts de validation (`/tmp/p111v-*`, journaux) ont été **supprimés**.

## Verdict

### PASS AVEC RÉSERVES

**Justification** — les critères d'acceptation de §9 sont satisfaits dans
l'usage nominal : configuration depuis un build propre ✅ (4 builds, dont un sans
artefact préexistant), compilation ✅ (0 avertissement), découverte CTest ✅
(5/5 groupes sur les 4 builds), suite complète ✅ (10 × 64/64), répétitions ✅
(9 + 9 + parallèles), `AGENT_BUILD_TESTS=OFF` ✅ (0 test, production intacte),
absence de dépendance cachée à un artefact de build ✅, absence de dépendance à
l'ordre ✅, stabilité du groupe `mock` ✅, reproductibilité multi-builds
**séquentielle** ✅.

Le verdict n'est pas `PASS` en raison de deux limites **démontrées ou
structurelles**, plus une fragilité mineure : **E1** (la suite n'est **pas
versionnée** — critère « versionnable » non encore réalisé, correction interdite
dans cette itération), **E2** (collision **reproduite 6/6** entre exécutions
concurrentes ; le critère « reproductible depuis plusieurs builds » n'est donc
acquis qu'en exécution **séquentielle**), **E3** (chemin absolu partagé en dur →
critère « aucun chemin absolu dépendant de la machine » partiellement satisfait).
Aucun écart `BLOQUANT` ; aucune régression ; aucune correction appliquée.
