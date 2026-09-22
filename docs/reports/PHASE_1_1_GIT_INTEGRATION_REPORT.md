# P1.1-GIT-1 — Intégration Git contrôlée

> Intégration **explicite** des tests et de la documentation validés de P1.1 dans
> l'index Git. **Aucun `git add .` / `git add -A`. Aucun commit. Aucun push.**
> Aucune source de production touchée, aucun fichier supprimé/restauré/écrasé.
> Ce rapport est créé **avant** le staging ; ses sections « Résultats du staging »
> et « Verdict » sont complétées **après** les commandes, et le rapport est
> **restagé** à chaque modification.

## OBJECTIF ET PÉRIMÈTRE

Stager **exactement** les chemins autorisés par P1.1-V3 (21 entrées + le rapport
V3) plus ce rapport d'intégration, puis vérifier que l'index ne contient **rien
d'autre**.

**Dans le périmètre** : `CMakeLists.txt` (modification additive de P1.1, off par
défaut), les **10** fichiers de `tests/`, les **5** documents de référence de
`docs/` (`README.md` + `context/`, `history/`, `questions/`, `roadmap/`), les
**7** rapports de `docs/reports/` (5 rapports de phase P1.1 + P1.1-V3 + ce
rapport).

**Hors périmètre** : `src/`, `include/`, `README.md`, `AGENTS.md`, `CLAUDE.md`,
`USAGE.md`, `.env.example`, `.gitignore`, `workspace/`, `.env`, `build/`, et
`docs/reports/streaming-audit.md` (**déjà suivi**).

## ÉTAT GIT INITIAL

```text
$ pwd
/home/oper/projects/local-agent-cpp
$ git branch --show-current
main
$ git rev-parse HEAD
1c46372e822c0d982b11628425fdf697cb0653a8

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
?? docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md
?? docs/reports/PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md
?? docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md
?? docs/roadmap/
?? tests/

$ git diff --cached --stat
  → (vide : index VIDE)

$ git diff --stat
 CMakeLists.txt | 8 ++++++++
 1 file changed, 8 insertions(+)
```

**Conformité au rapport V3** : branche `main` ✅ ; HEAD `1c46372` **identique**
✅ ; index **vide** ✅ ; `CMakeLists.txt` **toujours** la modification auditée
(`git diff --numstat` → **`8  0`**) ✅ ; **aucun changement non prévu** ✅.
⇒ **Aucune divergence : le staging peut être engagé.**

## CONTRÔLES AVANT STAGING

| Contrôle | Commande | Résultat observé |
|---|---|---|
| Existence des chemins prévus | boucle `[ -f ]` sur les 22 chemins préexistants | **22/22 présents** |
| Fichiers de `tests/`/`docs/` non couverts | `comm -23` (présents vs liste autorisée ∪ `streaming-audit.md`) | **AUCUN écart** — tout est couvert |
| Marqueurs CMake | `grep -n "AGENT_BUILD_TESTS\|enable_testing()\|add_subdirectory(tests)"` | `:35-39` — `AGENT_BUILD_TESTS` (2), `enable_testing()` (1), `add_subdirectory(tests)` (1) |
| Modification préexistante | `git diff --numstat -- CMakeLists.txt` | **`8  0`** — additif, conforme à l'audit |

## LISTE EXHAUSTIVE DES FICHIERS PRÉVUS (23)

**1 — Modification de configuration (1)**

```text
CMakeLists.txt
```

**2 — Suite de tests (10)**

```text
tests/CMakeLists.txt            tests/test_support.hpp       tests/test_main.cpp
tests/mock_http_server.hpp      tests/mock_http_server.cpp   tests/mock_http_server_test.cpp
tests/sse_parser_test.cpp       tests/workspace_tools_test.cpp
tests/http_client_test.cpp      tests/agent_streaming_test.cpp
```

**3 — Documents de référence (5)**

```text
docs/README.md
docs/context/PROJECT_CONTEXT.md   docs/history/PROJECT_HISTORY.md
docs/questions/OPEN_QUESTIONS.md  docs/roadmap/ROADMAP.md
```

**4 — Rapports (7)**

```text
docs/reports/PHASE_1_1_REPORT.md                             (livrable P1.1)
docs/reports/PHASE_1_1_REVIEW_REPORT.md                      (revue P1.1-R)
docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md             (remédiation R-01)
docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md           (audit reproductibilité)
docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md      (correctifs E2/E3)
docs/reports/PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md (préparation V3)
docs/reports/PHASE_1_1_GIT_INTEGRATION_REPORT.md             (ce rapport)
```

**Explicitement exclus** : `docs/reports/streaming-audit.md` — **déjà suivi**
(commité en `1c46372`) : ni restagé, ni modifié.

## RÉSULTATS DU STAGING

Quatre commandes `git add --` (chemins **explicites**, jamais `.` ni `-A`), toutes
en **rc=0** :

```bash
git add -- CMakeLists.txt                              # rc=0
git add -- tests/… (10 chemins)                        # rc=0
git add -- docs/README.md docs/context/… (5 chemins)   # rc=0
git add -- docs/reports/… (7 chemins)                  # rc=0
```

Résultat observé (`git diff --cached --stat`) : **23 fichiers** — `1` modification
(`M`) + `22` ajouts (`A`) ; **4 824 insertions** pour les 22 fichiers **hors ce
rapport** :

```text
 CMakeLists.txt                                     |   8 +
 docs/README.md                                     |  44 ++
 docs/context/PROJECT_CONTEXT.md                    | 213 ++++++++
 docs/history/PROJECT_HISTORY.md                    | 226 ++++++++
 docs/questions/OPEN_QUESTIONS.md                   | 187 +++++++
 docs/reports/PHASE_1_1_GIT_INTEGRATION_REPORT.md   | (auto-référence, non figée)
 docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md   | 202 +++++++
 docs/reports/PHASE_1_1_REPORT.md                   | 317 +++++++++++
 docs/reports/PHASE_1_1_REVIEW_REPORT.md            | 426 +++++++++++++++
 docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX...   | 200 +++++++
 docs/reports/PHASE_1_1_V3_GIT_INTEGRATION_PREP...   | 374 +++++++++++++
 docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md | 241 +++++++++
 docs/roadmap/ROADMAP.md                            | 212 ++++++++
 tests/CMakeLists.txt                               |  25 +
 tests/agent_streaming_test.cpp                     | 587 +++++++++++++++++++++
 tests/http_client_test.cpp                         | 124 +++++
 tests/mock_http_server.cpp                         | 291 ++++++++++
 tests/mock_http_server.hpp                         |  76 +++
 tests/mock_http_server_test.cpp                    | 184 +++++++
 tests/sse_parser_test.cpp                          | 192 +++++++
 tests/test_main.cpp                                |  49 ++
 tests/test_support.hpp                             | 165 +++++++
 tests/workspace_tools_test.cpp                     | 481 +++++++++++++++++
 23 files changed  (total global NON figé ici : ce rapport en fait partie)
 4 824 insertions pour les 22 fichiers HORS ce rapport
```

**Auto-référence** : ce rapport étant lui-même stagé, son propre nombre de lignes
— et donc le total global d'insertions — varie à chaque complétion du rapport.
Ces deux valeurs sont donc **volontairement non figées** dans ce document ; les
chiffres **stables** (nombre de fichiers `23`, répartition `1 M + 22 A`,
`4 824` insertions pour les 22 autres fichiers) sont exacts et vérifiables par
`git diff --cached --stat` / `--numstat`.

`git status --short` après staging : `M ` pour `CMakeLists.txt` et `A ` pour les
22 nouveaux fichiers — **aucune** entrée hors périmètre.

## VÉRIFICATIONS DU STAGING

| Contrôle | Commande | Résultat observé |
|---|---|---|
| Liste staged ≡ liste autorisée | `comm -23` / `comm -13` + `diff -q` | **identiques** (23 vs 23) ; non-autorisés : *vide* ; non-stagés : *vide* ; `diff -q` → **OUI** |
| Les 10 fichiers de tests | `grep -c '^tests/'` | **10 / 10** |
| Les 7 rapports (dont celui-ci) | `grep -c '^docs/reports/'` | **7 / 7** |
| Aucune source de production | `grep -cE '^(src\|include)/'` | **0** (attendu 0) |
| `streaming-audit.md` non restagé | `grep -c 'streaming-audit'` | **0** (attendu 0) |
| Aucun fichier hors périmètre | `grep -vcE '^(CMakeLists.txt\|tests/\|docs/)'` | **0** (attendu 0) |
| Conformité de forme | `git diff --cached --check` | **aucune sortie** ⇒ aucune erreur de blanc |
| Nature des entrées | `git diff --cached --name-status` | `M` ×1 (`CMakeLists.txt`) + `A` ×22 |
| Diff de `CMakeLists.txt` stagé | `git diff --cached -- CMakeLists.txt` | additif `+8 / −0` : `option(AGENT_BUILD_TESTS … OFF)`, `enable_testing()`, `add_subdirectory(tests)` |

**Aucune différence entre la liste réelle du staging et la liste autorisée** :
aucune résolution nécessaire.

## ÉCARTS / RISQUES

| ID | Gravité | Constat | Preuve | Suite |
|---|---|---|---|---|
| **GIT1-1** | INFO | Le rapport a été **écrit avant** le staging (§2), puis **complété après** (§5) : il a donc transitoirement porté l'état `AM` (staged + modifié) avant d'être **restagé** | `git status --short` avant restage | Conforme à la procédure ; aucun impact |
| **GIT1-2** | INFO | `docs/reports/streaming-audit.md` était **déjà suivi** : ni restagé, ni modifié | `grep -c 'streaming-audit'` sur la liste staged → **0** | Aucun impact |
| **GIT1-3** | MINEUR *(reporté de P1.1-V3)* | `tests/` n'est **plus** « non versionné » une fois l'index validé, mais **rien n'est encore commité** : un clone propre ne verra ces fichiers qu'**après commit** (interdit ici) | `git diff --cached` non vide, `git diff --exit-code HEAD` | **Commit à venir**, sur autorisation explicite |
| **GIT1-4** | MINEUR *(reporté de P1.1-V3)* | `docs/README.md:22-25` n'indexe que **2 rapports sur 5** et ne mentionne pas `streaming-audit.md` | lecture de `docs/README.md` | Correction ultérieure (hors périmètre de cette itération) |
| **GIT1-5** | MINEUR *(reporté de P1.1-V3)* | `AGENTS.md:18` affirme encore « Pas de suite de tests pour l'instant » alors que 64 cas existent ; `AGENTS.md` est **hors liste A** ⇒ divergence persistante | `grep -n "test" AGENTS.md` → `:18` | Mise à jour d'`AGENTS.md` à prévoir |

**Aucun écart bloquant.** `src/`, `include/`, `README.md`, `AGENTS.md`,
`CLAUDE.md`, `USAGE.md`, `.env.example`, `.gitignore`, `workspace/` : **inchangés**.

## CONTRÔLE FINAL

Valeurs **stables** (indépendantes de la longueur de ce rapport), observées après
le dernier restage :

| Commande | Résultat observé |
|---|---|
| `git status --short --branch` | `## main` + `1` entrée `M ` staged + `22` entrées `A ` staged ; **arbre de travail propre** |
| `git diff --cached --name-only \| wc -l` | **23** |
| `git diff --cached --check` | **aucune sortie** ⇒ aucune erreur de blanc |
| `git diff --cached --name-status \| grep -c '^M'` | **1** (`CMakeLists.txt`) |
| `git diff --cached --name-status \| grep -c '^A'` | **22** |
| `git rev-parse HEAD` | `1c46372e822c0d982b11628425fdf697cb0653a8` (**inchangé**) |
| `git log -1 --oneline` | `1c46372 docs(streaming): add pre-implementation audit report` (**aucun commit ajouté**) |

**Aucun commit, aucun push** n'a été effectué : l'intégration est **staged**
uniquement, en attente d'une autorisation explicite.

## VERDICT

### PASS

**Justification** — les conditions du verdict sont réunies :

1. **Liste staged exacte** : **23 fichiers** (= 21 entrées de P1.1-V3 + le rapport
   V3 + ce rapport), **strictement identique** à la liste autorisée
   (`comm`/`diff -q` → identiques) ; `0` entrée non autorisée, `0` entrée
   autorisée manquante.
2. **Contrôles valides** : 10/10 fichiers de tests, 7/7 rapports, **0** source
   `src/`/`include/`, **0** fichier hors périmètre, `streaming-audit.md` non
   restagé, `git diff --cached --check` silencieux, `CMakeLists.txt` stagé
   additif `+8 / −0`.
3. **Aucun écrasement ni suppression** : `name-status` = `M` ×1 + `A` ×22.
4. **Rapport durable créé et vérifié** avant et après staging.
5. **Aucun commit, aucun push** ; HEAD **inchangé** (`1c46372`).

Réserves documentées et **non bloquantes** : `GIT1-3` (`tests/` sera réellement
versionné **au commit** — étape suivante, non autorisée ici), `GIT1-4`
(index `docs/README.md` incomplet), `GIT1-5` (`AGENTS.md:18` obsolète).
