# P1.1-V3 — Préparation de l'intégration Git

> Itération de **préparation, en lecture seule vis-à-vis de Git**.
> **Aucun `git add`, aucun commit, aucun push, aucun `reset`/`restore`/`checkout`/`clean`.**
> Aucun fichier de code ou de configuration modifié. **Seul fichier créé : ce rapport.**
> Aucun résultat n'est inventé : chaque sortie ci-dessous provient d'une commande réellement exécutée.

## OBJECTIF

Auditer précisément (1) les fichiers de tests et les rapports à conserver, (2) la
modification préexistante de `CMakeLists.txt`, (3) la liste exacte des fichiers à
intégrer ultérieurement, (4) l'absence de fichier hors périmètre, et produire ce
rapport durable.

## ÉTAT GIT INITIAL

```text
$ pwd
/home/oper/projects/local-agent-cpp

$ git status --short --branch
## main
 M CMakeLists.txt
?? docs/README.md
?? docs/context/PROJECT_CONTEXT.md
?? docs/history/PROJECT_HISTORY.md
?? docs/questions/OPEN_QUESTIONS.md
?? docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md
?? docs/reports/PHASE_1_1_REPORT.md
?? docs/reports/PHASE_1_1_REVIEW_REPORT.md
?? docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md
?? docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md
?? docs/roadmap/ROADMAP.md
?? tests/CMakeLists.txt
?? tests/agent_streaming_test.cpp
?? tests/http_client_test.cpp
?? tests/mock_http_server.cpp
?? tests/mock_http_server.hpp
?? tests/mock_http_server_test.cpp
?? tests/sse_parser_test.cpp
?? tests/test_main.cpp
?? tests/test_support.hpp
?? tests/workspace_tools_test.cpp

$ git rev-parse HEAD
1c46372e822c0d982b11628425fdf697cb0653a8
$ git branch --show-current
main
```

**Total** : `1` fichier suivi modifié + `20` fichiers non suivis = **21 entrées**.

### Vérification du diff existant

```text
$ git diff --stat
 CMakeLists.txt | 8 ++++++++
 1 file changed, 8 insertions(+)

$ git diff -- src include
  → aucune sortie  ⇒ src/ et include/ INCHANGÉS

$ git diff -- README.md AGENTS.md CLAUDE.md USAGE.md
  → aucune sortie  ⇒ les 4 documents racine INCHANGÉS
```

**Confirmations** : la seule modification de fichier suivi est `CMakeLists.txt` ;
**aucun fichier de production** concerné ; **aucun** des quatre documents racine
touché.

## AUDIT CMakeLists.txt

```text
$ git diff -- CMakeLists.txt
@@ -30,3 +30,11 @@ add_executable(agent
   src/main.cpp
 )
 target_link_libraries(agent PRIVATE agent_core)
+
+# Harness de tests, desactive par defaut : l'executable de production n'en
+# depend jamais. Activer explicitement avec -DAGENT_BUILD_TESTS=ON.
+option(AGENT_BUILD_TESTS "Build the agent test harness" OFF)
+if(AGENT_BUILD_TESTS)
+  enable_testing()
+  add_subdirectory(tests)
+endif()
```

| Marqueur | Occurrences totales | Ajoutées par le diff |
|---|---|---|
| `AGENT_BUILD_TESTS` | 3 | 3 |
| `enable_testing()` | 1 | 1 |
| `add_subdirectory(tests)` | 1 | 1 |

Référence autoritative : `git diff --numstat` → **`8  0`** (8 insertions,
**0 suppression**). *(Un comptage naïf de `^+`/`^-` sur la sortie du diff
renverrait 9/1 : les lignes d'en-tête `+++`/`---` sont comptées à tort — la
valeur `--numstat` fait foi.)*

- **Ce qui était préexistant** : les lignes 1 à 32 (cibles `agent_core` et
  `agent`, `find_package(CURL)`, FetchContent `nlohmann/json`) — **non modifiées**.
- **Pourquoi cette modification appartient à P1.1** : elle introduit l'option
  `AGENT_BUILD_TESTS` (**OFF par défaut**), `enable_testing()` et
  `add_subdirectory(tests)` — c'est-à-dire l'intégration CMake de la suite de
  tests produite par P1.1.
- **Pourquoi elle doit être conservée** : sans elle, `tests/` n'est jamais
  configuré ni compilé, et les 5 groupes CTest n'existent pas.
- **Pourquoi ce n'est pas une modification de production** : elle est **additive
  et gardée par un `if()`** ; `agent` et `agent_core` conservent exactement leurs
  sources et leurs liaisons. Vérifié en P1.1-V : en mode `OFF`, **0 cible de
  test** n'est créée et `ctest -N` renvoie `Total Tests: 0`.

**Aucune ligne modifiée** par cette itération.

## INVENTAIRE TESTS

`git ls-files tests` → **vide** : **aucun** fichier de tests n'est suivi.
`find tests -type f | sort` → **10 fichiers**, tous en `??`.

| Fichier | Octets | Lignes | Présent | Suivi | Données temp. figées | Secret / chemin perso |
|---|---|---|---|---|---|---|
| `tests/CMakeLists.txt` | 900 | 25 | ✅ | non | — | aucun |
| `tests/test_support.hpp` | 6 605 | 165 | ✅ | non | aucun | aucun |
| `tests/test_main.cpp` | 1 793 | 49 | ✅ | non | aucun | aucun |
| `tests/mock_http_server.hpp` | 2 754 | 76 | ✅ | non | aucun | aucun |
| `tests/mock_http_server.cpp` | 8 848 | 291 | ✅ | non | aucun | aucun |
| `tests/mock_http_server_test.cpp` | 6 335 | 184 | ✅ | non | aucun | aucun |
| `tests/sse_parser_test.cpp` | 5 878 | 192 | ✅ | non | aucun | aucun |
| `tests/workspace_tools_test.cpp` | 18 447 | 481 | ✅ | non | aucun | aucun |
| `tests/http_client_test.cpp` | 4 190 | 124 | ✅ | non | aucun | aucun |
| `tests/agent_streaming_test.cpp` | 22 103 | 587 | ✅ | non | aucun | aucun |

**Contrôle secrets / chemins personnels** (commande demandée) :

```text
$ grep -RInE '/home/|/root/|/Users/|[A-Za-z]:\\|LLM_API_KEY=|API_KEY=|TOKEN=|PASSWORD=' tests
  → AUCUN résultat
```

**Dépendances** (includes réels) :

- en-têtes du projet via `include/` (PUBLIC d'`agent_core`) : `agent.hpp`,
  `http_client.hpp`, `sse_parser.hpp`, `tool_registry.hpp`, `workspace_tools.hpp` ;
- `<nlohmann/json.hpp>` (transitif via `agent_core` PUBLIC) ;
- en-têtes locaux `test_support.hpp` / `mock_http_server.hpp` résolus par la
  règle « répertoire du fichier inclus » — **aucun `target_include_directories`
  supplémentaire** n'est déclaré pour `agent_tests` ;
- POSIX : `<unistd.h>`, `<sys/socket.h>`, `<netinet/*>`, `<arpa/inet.h>`,
  `<sys/time.h>` (mock loopback et `getpid()`), `Threads` (CMake).

**Références à `src/`, `include/`, `workspace/`** : seulement **2 commentaires**
d'en-tête de fichier (`http_client_test.cpp:1`, `sse_parser_test.cpp:1`) —
informatifs, **aucun chemin utilisé à l'exécution**.

**Cohérence avec `tests/CMakeLists.txt`** : les **7 `.cpp`** listés dans
`add_executable` correspondent **exactement** aux 7 `.cpp` présents
(`comm -23` → **aucun manquant**) ; les 2 `.hpp` et `CMakeLists.txt` ne sont pas
des sources compilées. **Aucun fichier inattendu** dans `tests/`.

## INVENTAIRE DOCUMENTATION

`find docs -type f | sort` → **11 fichiers**. `git ls-files docs` →
`docs/reports/streaming-audit.md` (**seul document déjà suivi**).

| Fichier | Suivi Git | Rôle |
|---|---|---|
| `docs/README.md` | non | index de `docs/` |
| `docs/context/PROJECT_CONTEXT.md` | non | état de référence (P0) |
| `docs/history/PROJECT_HISTORY.md` | non | reconstitution chronologique (P0) |
| `docs/roadmap/ROADMAP.md` | non | trajectoire (P0) |
| `docs/questions/OPEN_QUESTIONS.md` | non | questions ouvertes (P0) |
| `docs/reports/streaming-audit.md` | **oui** | audit streaming (commité en `1c46372`) |
| `docs/reports/PHASE_1_1_REPORT.md` | non | livrable P1.1 |
| `docs/reports/PHASE_1_1_REVIEW_REPORT.md` | non | revue P1.1-R |
| `docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md` | non | remédiation R-01 |
| `docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md` | non | audit reproductibilité |
| `docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md` | non | correctifs E2/E3 |

`.gitignore` **n'exclut ni `tests/` ni `docs/`** (vérifié :
`git check-ignore tests` → NON, `git check-ignore docs` → NON).

## INVENTAIRE RAPPORTS

`find docs/reports -maxdepth 1 -type f | sort` → **6 fichiers**.

| Rapport | Octets | Lignes | Verdict présent | Nom exact attendu | Suivi |
|---|---|---|---|---|---|
| `PHASE_1_1_REPORT.md` | 19 625 | 317 | oui — `## 11. Verdict` / `**PASS.**` | ✅ | non |
| `PHASE_1_1_REVIEW_REPORT.md` | 27 059 | 426 | oui — `### PASS AVEC RÉSERVES` | ✅ | non |
| `PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md` | 12 328 | 202 | oui — `### PASS` | ✅ | non |
| `PHASE_1_1_V_REPRODUCIBILITY_REPORT.md` | 18 516 | 241 | oui — `### PASS AVEC RÉSERVES` | ✅ | non |
| `PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md` | 10 669 | 200 | oui — `### PASS` | ✅ | non |
| `streaming-audit.md` | 15 483 | 254 | *(pas de bloc Verdict : se termine par `## 6. Recommandation`)* | ✅ | **oui** |

- **Doublons** : `md5sum` → **6 empreintes distinctes**, **aucun doublon**.
- **Écrasement entre itérations** : **aucun**. `PHASE_1_1_REVIEW_REPORT.md` a été
  **volontairement régénéré** lors de P1.1-R (même fichier, même jalon, format
  imposé) et le signale explicitement dans son propre contenu ; les 5 autres
  rapports ont été créés une seule fois, à des horodatages distincts.
- **Cohérence chronologique** (mtime) : `17:07` REPORT → `21:53` REVIEW →
  `22:14` R2 → `22:35` streaming-audit → `22:44` V → `23:14` V2 — conforme à
  l'ordre des itérations. Aucun rapport postérieur à une itération qui l'aurait
  ignoré.
- **Verdict** : présent dans les **5 rapports de phase**. `streaming-audit.md` est
  un **audit** (constats + recommandations) et non un rapport de phase : il ne
  porte pas de bloc `## Verdict` — écart de forme **signalé**, non bloquant.
- **Rapport de cette itération** : `PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md`
  → **absent** au démarrage ⇒ **créé** (nom exact, aucun écrasement).

## FICHIERS À INTÉGRER

**Liste A** — à intégrer ultérieurement, **confirmée par l'état réel** (21
fichiers existants ; +1 pour ce rapport, créé par l'itération) :

| # | Chemin | Nature | Justification |
|---|---|---|---|
| 1 | `CMakeLists.txt` | **modifié** (M) | intégration CMake de la suite (option `AGENT_BUILD_TESTS`, OFF par défaut) |
| 2 | `tests/CMakeLists.txt` | nouveau | cible `agent_tests`, 5 groupes CTest, `TIMEOUT 30` |
| 3-10 | `tests/{test_support.hpp, test_main.cpp, mock_http_server.hpp, mock_http_server.cpp, mock_http_server_test.cpp, sse_parser_test.cpp, workspace_tools_test.cpp, http_client_test.cpp, agent_streaming_test.cpp}` | nouveaux | sources du harnais (10 fichiers au total avec `tests/CMakeLists.txt`) |
| 11 | `docs/README.md` | nouveau | index documentaire |
| 12 | `docs/context/PROJECT_CONTEXT.md` | nouveau | document de référence |
| 13 | `docs/history/PROJECT_HISTORY.md` | nouveau | document de référence |
| 14 | `docs/questions/OPEN_QUESTIONS.md` | nouveau | document de référence |
| 15 | `docs/roadmap/ROADMAP.md` | nouveau | document de référence |
| 16 | `docs/reports/PHASE_1_1_REPORT.md` | nouveau | rapport de phase |
| 17 | `docs/reports/PHASE_1_1_REVIEW_REPORT.md` | nouveau | rapport de phase |
| 18 | `docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md` | nouveau | rapport de phase |
| 19 | `docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md` | nouveau | rapport de phase |
| 20 | `docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md` | nouveau | rapport de phase |
| 21 | `docs/reports/PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md` | nouveau (cette itération) | ce rapport |

**Aucune granularité implicite** : les 10 fichiers de `tests/` doivent être
intégrés **ensemble** (la cible `agent_tests` référence les 7 `.cpp` ; les 2
`.hpp` sont inclus par eux).

## FICHIERS HORS PÉRIMÈTRE

**Liste B** — à conserver, **hors intégration P1.1** (inchangés par cette itération) :

```text
README.md            AGENTS.md        CLAUDE.md        USAGE.md
.env.example         .gitignore       workspace/.gitkeep
src/*.cpp (6)        include/*.hpp (6)
.env                 (ignoré par Git) build/            (ignoré par Git)
```

Motif : ces fichiers sont **déjà suivis et non modifiés** (`git diff` vide pour
`README.md`, `AGENTS.md`, `CLAUDE.md`, `USAGE.md` et pour `src/`/`include/`), ou
**volontairement ignorés** par `.gitignore` (`build/`, `.env`). Ils ne doivent
**pas** figurer dans le futur commit d'intégration de la suite de tests.

## FICHIERS INATTENDUS

**Liste C** — **aucun fichier non expliqué**.

Un seul fichier mérite une mention explicite : `docs/reports/streaming-audit.md`
— **déjà suivi par Git** (commité en `1c46372`). Il n'appartient donc **pas** à la
liste A (rien à intégrer) et n'est **pas** un inattendu : c'est un élément **déjà
intégré**. Il ne doit pas être re-stagé « pour rien ».

Contrôle complémentaire : la somme `1 (M) + 20 (??)` couvre **exactement** l'état
`git status --short --untracked-files=all` — **aucune entrée orpheline**.

## CONTRÔLES EFFECTUÉS

| # | Contrôle | Commande | Résultat |
|---|---|---|---|
| 1 | Référence | `pwd`, `git status --short --branch`, `git rev-parse HEAD`, `git branch --show-current` | `/home/oper/projects/local-agent-cpp`, `main`, HEAD `1c46372`, 21 entrées |
| 2 | Diff suivi | `git diff --stat` ; `git diff -- src include` ; `git diff -- README.md AGENTS.md CLAUDE.md USAGE.md` | `CMakeLists.txt` `8 ++++++++` uniquement ; **src/include inchangés** ; **4 docs racine inchangés** |
| 3 | Inventaire Git | `git ls-files tests` ; `git ls-files docs` ; `git status --short --untracked-files=all` | tests : **0 suivi** ; docs : **1 suivi** ; 21 entrées détaillées |
| 4 | Présence physique | `find tests -type f` ; `find docs -type f` | tests **10/10** ; docs **11** ; **0 fichier inattendu** |
| 5 | Secrets / chemins perso | `grep -RInE '/home/\|/root/\|/Users/\|[A-Za-z]:\\\\\|LLM_API_KEY=\|API_KEY=\|TOKEN=\|PASSWORD=' tests` | **aucun résultat** |
| 6 | Données temp. figées | `grep -RnE '\.(log\|tmp\|bak\|swp)"' tests` | aucun nom figé |
| 7 | Cohérence CMake ↔ fichiers | `comm -23` (.cpp présents vs listés) | **aucun fichier manquant** |
| 8 | Rapports | `find docs/reports -maxdepth 1 -type f` ; `md5sum` ; `ls -la` | 6 rapports, **0 doublon**, noms exacts, verdicts présents (5/5 phases), chronologie cohérente |
| 9 | Marqueurs CMake | `grep -c` des 3 marqueurs + `git diff --numstat` | `AGENT_BUILD_TESTS` ×3, `enable_testing()` ×1, `add_subdirectory(tests)` ×1 ; **8 insertions / 0 suppression** |
| 10 | Références doc | `grep -RIn "PHASE_1_1"` (28 occ.) ; `grep -RInE 'PROJECT_CONTEXT\.md\|PROJECT_HISTORY\.md\|ROADMAP\.md\|OPEN_QUESTIONS\.md'` (31 occ.) | **aucune référence incorrecte** : voir classification au §ÉCARTS |
| 11 | Propreté | `find . -maxdepth 3 -type f \( -name '*.log' -o -name '*.tmp' -o -name '*.bak' -o -name '*.swp' \)` | **aucun** fichier (y compris sous `build/`) |
| 12 | **Index Git** | `git diff --cached --stat` ; `git diff --cached` | **les deux vides** ⇒ **aucun staging** |

**Classification des 31 références documentaires** :
(a) **correctes** — liens relatifs et chemins `docs/...` (index `docs/README.md:13-16`,
rapports V:20-21, REVIEW:31, `OPEN_QUESTIONS.md`, `PROJECT_CONTEXT.md`,
`ROADMAP.md:9`, `streaming-audit.md:206`) ; (b) **titres de document** —
`PROJECT_HISTORY.md:1`, `OPEN_QUESTIONS.md:1`, `PROJECT_CONTEXT.md:1`,
`ROADMAP.md:1` ; (c) **anciens chemins conservés volontairement** — bloc de code
reproduisant un `git status` d'époque dans `PHASE_1_1_REPORT.md:21-24` (bloc de
**preuve/audit**, déjà justifié en P1.1-DOC) ; (d) **références incorrectes** :
**aucune**.

## ÉCARTS / RISQUES

| ID | Gravité | Constat | Preuve | Impact | Suite |
|---|---|---|---|---|---|
| **V3-1** | **MAJEUR (bloquant pour l'intégration)** | `tests/` n'est **pas versionné** : 0 fichier suivi | `git ls-files tests` → vide | Un clone propre ne possède aucune suite de tests ; elle ne peut pas encore servir de base de validation versionnée. C'est l'écart **E1** de P1.1-V, **toujours ouvert** (traiter exigeait `git add`, interdit ici) | Itération d'intégration dédiée |
| **V3-2** | MINEUR | `docs/README.md` indexe **2** rapports sur **5** (+ `streaming-audit.md` non mentionné) | `docs/README.md:22-25` vs `find docs/reports` | L'index documentaire sera **incomplet** après intégration : liens internes valides mais index partiel | À corriger dans l'itération d'intégration ou une itération docs |
| **V3-3** | MINEUR | `AGENTS.md:18` affirme « Pas de suite de tests pour l'instant » | `grep -n "test" AGENTS.md` → `:18` | Document **contredit par les faits** (64 cas, 239 assertions) ; `AGENTS.md` est **hors liste A**, donc la divergence **persistera** après intégration | Mise à jour d'`AGENTS.md` à prévoir (hors périmètre P1.1) |
| **V3-4** | INFO | `streaming-audit.md` est **déjà suivi** | `git ls-files docs` → ce seul fichier | Aucun impact : ne pas le re-stager | — |
| **V3-5** | INFO | `streaming-audit.md` ne porte pas de bloc `## Verdict` (audit, pas rapport de phase) | `grep -n "^#+" ` → `## 6. Recommandation` | Écart de **forme**, sans incidence sur la traçabilité (constats + recommandations + §Écarts présents) | Arbitrage rédactionnel éventuel |
| **V3-6** | INFO | L'index Git était **vide au démarrage** | `git diff --cached --stat` → vide | Aucune anomalie de staging héritée | — |

**Aucun écart BLOQUANT autre que V3-1** (qui relève du périmètre de l'itération
d'intégration, pas de celle-ci). Aucune correction appliquée — l'itération est
en lecture seule.

## ÉTAT GIT FINAL

```text
$ git status --short --branch
## main
 M CMakeLists.txt
?? docs/README.md
?? docs/context/PROJECT_CONTEXT.md
?? docs/history/PROJECT_HISTORY.md
?? docs/questions/OPEN_QUESTIONS.md
?? docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md
?? docs/reports/PHASE_1_1_REPORT.md
?? docs/reports/PHASE_1_1_REVIEW_REPORT.md
?? docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md
?? docs/reports/PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md
?? docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md
?? docs/roadmap/ROADMAP.md
?? tests/…

$ git diff --stat
 CMakeLists.txt | 8 ++++++++
 1 file changed, 8 insertions(+)

$ git diff --cached --stat
  → (vide : aucun staging)

$ git rev-parse HEAD
1c46372e822c0d982b11628425fdf697cb0653a8
```

- **Seul changement produit par cette itération** : la **création** de
  `docs/reports/PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md` (nouveau
  fichier, apparaît en `??`).
- **Aucun `git add`**, **aucun commit**, **aucun push**, aucun
  `reset`/`restore`/`checkout`/`clean`.
- **Index Git vide** avant et après.
- **HEAD identique** à l'état initial (`1c46372`).
- `README.md`, `AGENTS.md`, `CLAUDE.md`, `USAGE.md`, `src/`, `include/` :
  **inchangés** ; `CMakeLists.txt` : modification **préexistante** conservée
  intacte (`8 0`).

## VERDICT

### PASS

**Justification** — les six conditions du verdict `PASS` sont remplies :

1. **Inventaire complet** : `tests/` (10/10 fichiers, tailles et lignes
   relevées), `docs/` (11 fichiers), `docs/reports/` (6 rapports, 0 doublon,
   0 écrasement, chronologie cohérente), `.gitignore` vérifié (n'exclut ni
   `tests/` ni `docs/`), 21 entrées Git expliquées **sans orphelin**.
2. **Liste d'intégration déterminée** : liste **A** = 21 fichiers confirmés par
   l'état réel (+ ce rapport) ; liste **B** = 18 éléments hors périmètre ;
   liste **C** = **aucun inattendu**.
3. **`CMakeLists.txt` correctement identifié** : modification **additive** de
   `+8 / -0`, **OFF par défaut**, gardée par `if()`, appartenant à P1.1,
   **distincte** de toute modification de production.
4. **Aucun fichier inattendu non expliqué** : `streaming-audit.md` est **déjà
   suivi** (mentionné explicitement, hors liste A).
5. **Index Git inchangé** : `git diff --cached` **vide** avant et après.
6. **Aucun commit, aucun push** ; **rapport durable créé** et vérifié
   (`test -s` OK, sections obligatoires présentes, verdict présent).

**Réserves (non bloquantes)** : V3-1 (`tests/` non versionné — écart **E1** de
P1.1-V, à traiter dans l'itération d'intégration), V3-2 (index `docs/README.md`
incomplet), V3-3 (`AGENTS.md:18` obsolète), et les écarts **E4 → E9** de P1.1-V
toujours ouverts (sensibilité temporelle bornée, `TMPDIR`, prérequis réseau
FetchContent hérité, `CMAKE_BUILD_TYPE` vide, dettes **R-02 → R-08** du mock) —
**tous hors périmètre de cette itération**.
