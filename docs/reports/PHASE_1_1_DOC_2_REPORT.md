# P1.1-DOC-2 — Cohérence documentaire après commit (rapport)

> Itération de **correction documentaire** uniquement. **Aucun changement dans
> `src/`, `include/`, les tests ou la configuration CMake.** Aucun `git add`,
> aucun commit, aucun `amend`, aucun `reset`/`rebase`, aucun push. Aucun fichier
> supprimé. Les modifications sont **laissées non commitées**, en attente d'une
> autorisation explicite d'intégration Git.

## 1. OBJECTIF ET COMMIT DE RÉFÉRENCE

**Objectif** : finaliser la cohérence documentaire de P1.1 en corrigeant les deux
écarts identifiés lors de P1.1-GIT-2 (`GC-3`, `GC-4`) :

- **`GC-3`** — `docs/README.md` n'indexait que **2 rapports** sur l'ensemble des
  rapports P1.1 ;
- **`GC-4`** — `AGENTS.md` affirmait encore « Pas de suite de tests pour
  l'instant » alors que la suite est désormais **committée**.

**Commit de référence attendu et confirmé** :
`1abe663904b1864a3fafeeefb4520bfb6517a795`
(`test: integrate P1.1 test suite and reports`), branche `main`.

## 2. ÉTAT INITIAL GIT

| Élément | Valeur observée au démarrage |
|---|---|
| Répertoire | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `1abe663904b1864a3fafeeefb4520bfb6517a795` — **conforme** au commit de référence |
| Nombre de commits | **29** |
| Index (`git diff --cached --name-status`) | **vide** |
| Modifications non stagées (`git diff --name-status`) | **vide** |
| Fichiers non suivis | **1 seul** : `docs/reports/PHASE_1_1_GIT_COMMIT_REPORT.md` |
| Remote | **aucun** (`git remote -v` vide, `git branch -r` = 0) |

Aucun écart significatif avec l'état attendu : l'itération a pu démarrer sans
constat bloquant.

## 3. FICHIERS EXAMINÉS ET MODIFIÉS

### 3.1 Modifiés — 2 fichiers

| Fichier | Nature | Justification |
|---|---|---|
| `docs/README.md` | modification | indexer tous les rapports P1.1 (`GC-3`) + préciser la convention de nommage observée |
| `AGENTS.md` | modification | remplacer l'affirmation obsolète « Pas de suite de tests » (`GC-4`) |

### 3.2 Examinés, **non modifiés**

| Fichier | Constat |
|---|---|
| `docs/reports/PHASE_1_1_GIT_COMMIT_REPORT.md` | **factuellement exact** (cf. §5.5) ⇒ aucune correction nécessaire |
| `docs/reports/PHASE_1_1_GIT_INTEGRATION_REPORT.md` | cohérent ; référencé dans l'index |
| `docs/reports/PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md` | cohérent ; référencé |
| `docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md` | cohérent ; référencé |
| `docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md` | cohérent ; référencé |
| `docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md` | cohérent ; référencé |
| `docs/reports/PHASE_1_1_REVIEW_REPORT.md` | cohérent ; référencé |
| `docs/reports/PHASE_1_1_REPORT.md` | cohérent ; référencé |
| `docs/reports/streaming-audit.md` | cohérent ; référencé |
| `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/test_main.cpp`, `README.md`, `USAGE.md`, `CLAUDE.md` | **lus pour vérification uniquement**, aucune modification |

## 4. ÉCARTS CORRIGÉS

### 4.1 `GC-3` — index `docs/README.md` incomplet

**Avant** : la table « Rapports de phase » ne listait que **2** rapports
(`PHASE_1_1_REPORT.md`, `PHASE_1_1_REVIEW_REPORT.md`) — soit **2 sur 9** fichiers
présents dans `docs/reports/` au démarrage de l'itération.

**Après** : la table liste **10 entrées — soit 10/10 fichiers** de `docs/reports/`
à l'état final (voir la correction de complétude en §4.4), chacune avec une
description courte et factuelle :

| Rapport indexé | Source de la description |
|---|---|
| `streaming-audit.md` | en-tête du rapport (audit en lecture seule de l'état réel du streaming) |
| `PHASE_1_1_REPORT.md` | en-tête + faits du harness (64 cas / 239 assertions, 5 groupes) |
| `PHASE_1_1_REVIEW_REPORT.md` | en-tête (revue indépendante : représentativité, déterminisme, couverture, faux positifs) |
| `PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md` | en-tête (remédiation R-01 : `SO_RCVTIMEO`, réponse `408`, réveil au destructeur) |
| `PHASE_1_1_V_REPRODUCIBILITY_REPORT.md` | en-tête (audit versionnabilité/reproductibilité, écarts E1–E3) |
| `PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md` | en-tête (correction E2/E3 : suffixe de PID, chemin absolu supprimé) |
| `PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md` | en-tête (préparation en lecture seule de l'intégration Git) |
| `PHASE_1_1_GIT_INTEGRATION_REPORT.md` | en-tête (staging explicite des 23 fichiers, sans commit ni push) |
| `PHASE_1_1_GIT_COMMIT_REPORT.md` | en-tête + §5.5 (commit `1abe663`, 23 fichiers) |
| `PHASE_1_1_DOC_2_REPORT.md` | ce rapport (correction documentaire `GC-3`/`GC-4`) — **ajouté en §4.4** |

La section **« Convention de nommage observée »** a été complétée d'une ligne
factuelle pour couvrir les rapports d'itération d'un même jalon
(`..._R2_...`, `..._V_...`, `..._V2_...`, `..._V3_...`, `..._GIT_INTEGRATION_...`,
`..._GIT_COMMIT_...`) et l'audit thématique (`<sujet>-audit.md`). La structure
existante (documents de référence / rapports de phase / hors `docs/`) est
**préservée**.

### 4.2 `GC-4` — `AGENTS.md` §`## Test` obsolète

**Avant** :

```
## Test
Pas de suite de tests pour l'instant.
(À ajouter : Catch2 ou GoogleTest si le projet grossit)
Un harnais manuel peut lier `agent_core` directement (voir Architecture) sans
relister les .cpp.
```

**Après** : formulation factuelle décrivant la suite **existante** et son
activation, chaque élément adossé au dépôt :

| Affirmation dans `AGENTS.md` | Source vérifiée |
|---|---|
| suite dans `tests/`, micro-harnais maison, aucun framework externe | `tests/test_support.hpp`, `tests/test_main.cpp`, `tests/CMakeLists.txt:1-2` |
| mock HTTP loopback | `tests/mock_http_server.hpp` / `.cpp` |
| 64 cas / 239 assertions | harnais (`test_main.cpp` imprime le résumé) et rapports P1.1 |
| 5 groupes `sse`, `workspace`, `http`, `agent`, `mock` | `tests/CMakeLists.txt:22` (`foreach(group …)`) |
| désactivée par défaut, activation `-DAGENT_BUILD_TESTS=ON` | `CMakeLists.txt:35-39` (`option(… OFF)` + `if(…)`) |
| un test CTest par groupe, `TIMEOUT 30` | `tests/CMakeLists.txt:23-24` |
| `--filter=<groupe>`, retour **2** si aucun cas exécuté | `tests/test_main.cpp:10-14,42-45` |
| un test lie `agent_core` sans relister les `.cpp` | `AGENTS.md` §Architecture (déjà établi) |

La ligne obsolète suggérant « Catch2 ou GoogleTest » a été retirée : elle
contredisait la règle `AGENTS.md:112` (« Ne pas ajouter de framework externe sans
le demander explicitement »). **Aucune consigne de projet n'a été ajoutée** au-delà
de ce qui est démontré par le dépôt.

### 4.3 `GC-1` (rappel) — rapport post-commit non suivi

`docs/reports/PHASE_1_1_GIT_COMMIT_REPORT.md` reste **non suivi** : l'itération
P1.1-DOC-2 n'effectue aucun `git add`. Cet état est attendu et inchangé.

### 4.4 P1.1-DOC-2-FIX — complétude réelle de l'index (auto-référence)

**Écart constaté** : la version initiale de ce rapport annonçait « **9/9**
rapports indexés ». Or la **création de ce rapport** porte `docs/reports/` à
**10** fichiers : l'index ne couvrait alors que **9/10**, ce rapport n'étant pas
lui-même référencé.

| Contrôle | Valeur relevée |
|---|---|
| Fichiers réels dans `docs/reports/` (`ls`) | **10** |
| Entrées `reports/` dans l'index (avant correction) | **9** |
| Écart | **1** : `PHASE_1_1_DOC_2_REPORT.md` non indexé |

**Correction** : ajout d'une **10ᵉ** entrée dans `docs/README.md`, avec lien
relatif `reports/PHASE_1_1_DOC_2_REPORT.md` (cible vérifiée existante) et
description factuelle. L'index couvre désormais **10/10** fichiers de
`docs/reports/`.

**Leçon retenue** : un rapport qui modifie le périmètre qu'il décrit (ici, en
s'ajoutant à `docs/reports/`) doit se compter **lui-même** dans son inventaire
final. Le décompte est donc porté à **10/10** dans tout ce rapport.

## 5. COMMANDES DE VALIDATION ET RÉSULTATS EXACTS

### 5.1 `git diff --check`

```
$ git diff --check
(aucune sortie)
```

⇒ **silencieux** : aucune erreur d'espace blanc / de fin de fichier.

### 5.2 Vérification des liens documentaires ciblés

Extraction de tous les liens Markdown relatifs de `docs/README.md` et test
d'existence de chaque cible (résolution relative à `docs/`) :

| Contrôle | Résultat |
|---|---|
| Liens de `docs/README.md` | **18/18 OK** (état final, cf. §5.6) — `../README.md`, `../USAGE.md`, `../AGENTS.md`, `../CLAUDE.md`, `context/PROJECT_CONTEXT.md`, `history/PROJECT_HISTORY.md`, `roadmap/ROADMAP.md`, `questions/OPEN_QUESTIONS.md`, et les **10** rapports de `reports/` |
| Liens cassés introduits | **0** |
| Liens Markdown de `AGENTS.md` | aucun (chemins cités entre accents graves) ⇒ rien à résoudre |

### 5.3 État Git et périmètre modifié

```
$ git status --short --branch
## main
 M AGENTS.md
 M docs/README.md
?? docs/reports/PHASE_1_1_GIT_COMMIT_REPORT.md

$ git diff --stat
 AGENTS.md      | 17 +++++++++++++----
 docs/README.md | 17 ++++++++++++++---
 2 files changed, 27 insertions(+), 7 deletions(-)

$ git status --porcelain -- src include tests CMakeLists.txt
(aucune sortie)
```

⇒ **aucun fichier de `src/`, `include/`, `tests/` ni `CMakeLists.txt` modifié**.
Seuls `AGENTS.md` et `docs/README.md` sont modifiés ; aucun fichier n'est stagé.

### 5.4 Relecture finale des fichiers modifiés

`git diff` relu intégralement pour `AGENTS.md` et `docs/README.md` : les
modifications correspondent exactement aux corrections `GC-3` et `GC-4`
décrites en §4 ; aucune modification parasite.

### 5.5 Exactitude du rapport post-commit (contrôle §2.C)

`docs/reports/PHASE_1_1_GIT_COMMIT_REPORT.md` a été recoupé avec le dépôt ; les
affirmations vérifiables sont **toutes exactes** :

| Affirmation du rapport | Vérification |
|---|---|
| 23 fichiers, `M` ×1 + `A` ×22 + `D` ×0 | `git show --name-status … 1abe663` → `22 A` + `1 M` = **23** |
| 5 060 insertions | `git show --stat … 1abe663` → `23 files changed, 5060 insertions(+)` |
| HEAD parent `1c46372e82…` | `git rev-parse 1abe663^` → `1c46372e822c0d982b11628425fdf697cb0653a8` |
| auteur `oper <oper@operius.rev.sfr.net>`, date `Wed Sep 23 00:17:52 2026 +0200` | `git log -1 --pretty=…` → identique |
| aucun remote ⇒ aucun push | `git remote -v` vide, `git branch -r` = 0 |
| `GC-3` : `docs/README.md:22-25` n'indexe que 2 rapports | index relu → **confirmé** (état d'époque) |
| `GC-4` : `AGENTS.md:18` affirme « Pas de suite de tests… » | ligne relue → **confirmé** (état d'époque) |

⇒ **aucune erreur factuelle ni de cohérence vérifiable** : le rapport post-commit
n'a **pas** été modifié. Ses constats `GC-3`/`GC-4` y restent consignés comme
**preuve historique** de l'état à leur instant ; ils sont **résolus par la
présente itération** (§4.1, §4.2) et le rapport est désormais **indexé** (§4.1).

### 5.6 Re-exécution après correction de complétude (P1.1-DOC-2-FIX, §4.4)

Commandes **relancées** après l'ajout de l'entrée manquante :

```
$ git diff --check
(aucune sortie)                      # silencieux

$ ls -1 docs/reports/*.md | wc -l    # 10
$ grep -cE '^\| \[`reports/' docs/README.md   # 10

$ git status --short --branch
## main
 M AGENTS.md
 M docs/README.md
?? docs/reports/PHASE_1_1_DOC_2_REPORT.md
?? docs/reports/PHASE_1_1_GIT_COMMIT_REPORT.md

$ git diff --stat
 AGENTS.md      | 17 +++++++++++++----
 docs/README.md | 18 +++++++++++++++---
 2 files changed, 28 insertions(+), 7 deletions(-)

$ git status --porcelain -- src include tests CMakeLists.txt
(aucune sortie)                      # aucun fichier source/test/CMake touché
```

| Contrôle | Résultat final |
|---|---|
| Complétude de l'index | **10 / 10** fichiers de `docs/reports/` indexés |
| Liens de `docs/README.md` | **18 / 18 OK**, dont `reports/PHASE_1_1_DOC_2_REPORT.md` (nouvelle cible) |
| `git diff --check` | **silencieux** |
| Périmètre modifié | **2 fichiers** : `AGENTS.md`, `docs/README.md` |
| `src/`, `include/`, `tests/`, `CMakeLists.txt` | **aucune modification** |
| HEAD / commits / remote | `1abe663…` / **29** / **aucun** — inchangés |

## 6. LIMITES ET POINTS NON VÉRIFIÉS

| # | Limite / point non vérifié | Motif |
|---|---|---|
| L-1 | La suite de tests **n'a pas été réexécutée** | contrainte explicite de l'itération : la validation post-commit est déjà documentée en P1.1-GIT-2 ; aucun changement de code/test ne la justifie |
| L-2 | Le compte « 64 cas / 239 assertions » n'a **pas été re-mesuré** | il provient du harnais et des rapports P1.1 ; repris tel quel dans l'index et `AGENTS.md`, sans changement de code |
| L-3 | Aucun **rendu Markdown** n'a été contrôlé (ni GitHub, ni outil tiers) | la vérification porte sur l'**existence des cibles** de liens, non sur le rendu visuel |
| L-4 | `docs/reports/PHASE_1_1_GIT_COMMIT_REPORT.md` reste **non suivi** (`GC-1`) | non corrigé volontairement : aucun `git add` autorisé dans cette itération |
| L-5 | Signature de commit (`GC-2`, identité auto-détectée) non traitée | hors périmètre documentaire ; aucune action requise |

Aucun de ces points n'est bloquant ; aucun changement de code, test ou CMake n'a
été nécessaire (aucun blocage documentaire n'est apparu).

## 7. ÉTAT GIT FINAL

| Élément | Valeur |
|---|---|
| Branche | `main` |
| HEAD | `1abe663904b1864a3fafeeefb4520bfb6517a795` — **inchangé** |
| Nombre de commits | **29** — **inchangé** |
| Index | **vide** (aucun `git add`) |
| Fichiers modifiés (non stagés) | `AGENTS.md`, `docs/README.md` |
| Fichiers non suivis | `docs/reports/PHASE_1_1_DOC_2_REPORT.md` (ce rapport), `docs/reports/PHASE_1_1_GIT_COMMIT_REPORT.md` |
| Fichiers supprimés | **aucun** |
| Remote / push | aucun remote ; **aucun push** |

## 8. VERDICT

### PASS

**Justification** :

1. **`GC-3` corrigé** : `docs/README.md` indexe désormais **10/10** fichiers
   présents dans `docs/reports/`, avec une description factuelle et un lien
   vérifié pour chacun ; la structure documentaire existante est préservée.
2. **`GC-4` corrigé** : `AGENTS.md` §`## Test` décrit la suite **existante** et son
   activation, chaque affirmation étant adossée à un fichier/ligne du dépôt ;
   la mention obsolète « Pas de suite de tests » et la suggestion de framework
   externe ont disparu (cette dernière contredisait `AGENTS.md:112`).
3. **Rapport post-commit examiné** : factuellement exact, non modifié ; il est
   désormais **référencé dans l'index**.
4. **Cohérence et liens** : **18/18** liens de `docs/README.md` résolvent ;
   **0** lien cassé introduit ; les instructions de test d'`AGENTS.md` sont
   conformes à la configuration CMake (`option(… OFF)`, `enable_testing()`,
   `add_subdirectory(tests)`, 5 groupes CTest `TIMEOUT 30`).
5. **Périmètre respecté** : **2 fichiers** modifiés (`AGENTS.md`,
   `docs/README.md`) ; **aucun** fichier de `src/`, `include/`, `tests/` ni
   `CMakeLists.txt` touché ; **aucun** fichier supprimé ; `git diff --check`
   silencieux ; HEAD et nombre de commits inchangés ; aucun `git add`, commit,
   amend, reset, rebase ou push.
6. **Rapport durable créé et vérifié** avant conclusion (présent, non vide,
   contenu conforme aux résultats observés).
7. **Complétude de l'index vérifiée et corrigée** (`P1.1-DOC-2-FIX`, §4.4) :
   l'annonce initiale « 9/9 » était incomplète une fois ce rapport ajouté à
   `docs/reports/` ; l'entrée manquante a été ajoutée et le décompte final
   **10/10** est établi par `ls -1 docs/reports/*.md | wc -l` (**10**) comparé au
   nombre d'entrées `reports/` de l'index (**10**).

Réserves documentées et **non bloquantes** : voir §6 (`L-1` → `L-5`), notamment
la non-réexécution de la suite de tests (volontaire, cf. contrainte) et le
maintien de `PHASE_1_1_GIT_COMMIT_REPORT.md` non suivi.

**Les modifications restent non commitées**, en attente d'une autorisation
explicite d'intégration Git.
