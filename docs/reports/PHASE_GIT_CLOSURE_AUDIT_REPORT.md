# Audit de clôture Git — séparation R0 / P3.3 (rapport)

> Audit **en lecture seule**. Aucun fichier source, test ou document existant
> modifié par cette itération (hormis ce rapport et sa ligne d'index). **Aucun
> staging, aucun commit.** Les modifications préexistantes sont **intactes**.

## 1. OBJECTIF ET PÉRIMÈTRE

Déterminer, **preuves à l'appui**, la répartition des modifications non commitées
entre : **R0 (code/tests)**, **P3.3 (historique)**, **documentation R0**, et
identifier les **fichiers mixtes** ainsi que les **risques de mélange avant
staging**.

**Hors périmètre** : toute modification, tout staging, tout commit.

Gouvernance relue : `AGENTS.md` (commits atomiques, Conventional Commits,
« ne pas ajouter de framework externe »).

## 2. HEAD ET BRANCHE CONSTATÉS

| Élément | Valeur |
|---|---|
| Branche | `main` |
| HEAD | `47acd5d70f937f96b6db1f8d03f9e7fb132744eb` (`docs: reconcile HttpClient::post() decision (keep, tested)`) |
| Derniers commits | `51b7d2c` (test: add end-to-end SSE mock scenarios), `db88cdf` (docs: finalize P1.1 documentation), `1abe663`, `1c46372` |

## 3. ÉTAT GIT INITIAL

| Contrôle | Résultat |
|---|---|
| `git status --short` | **16 `M`** + **3 `??`** (voir §4) |
| `git diff --cached --stat` | **vide** ⇒ **rien n'est stagé** |
| `git diff --stat` | `16 files changed, 536 insertions(+), 62 deletions(-)` |
| `git ls-files --others --exclude-standard` | exactement **3** fichiers (les 3 rapports) |
| `git stash list` / `git remote -v` / `git tag` | **vides** ⇒ aucun stash, aucun remote, aucun tag |

## 4. INVENTAIRE DÉTAILLÉ

### 4.1 Fichiers suivis modifiés (16)

| Fichier | Statut | Périmètre | Justification (preuve) | Chevauchement | Risque staging |
|---|---|---|---|---|---|
| `include/agent.hpp` | `M` | **R0 code** | ajout `AgentError`, `ToolOutcome`, `on_tool_result` (invariants R0) | aucun | faible |
| `include/sse_parser.hpp` | `M` | **R0 code** | ajout `flush()`, `buffered_bytes()` | aucun | faible |
| `src/agent.cpp` | `M` | **R0 code** | complétude `[DONE]`, validation tool calls, `on_tool_result` | aucun | faible |
| `src/http_client.cpp` | `M` | **R0 code** | `CONNECTTIMEOUT`, message de timeout | aucun | faible |
| `src/main.cpp` | `M` | **R0 code** | `AgentError` + récapitulatif d'exécution | aucun | faible |
| `src/sse_parser.cpp` | `M` | **R0 code** | `flush()` | aucun | faible |
| `src/workspace_tools.cpp` | `M` | **R0 code** | `verify_written_file()` | aucun | faible |
| `tests/agent_e2e_test.cpp` | `M` | **R0 tests** | test d'état réel du fichier | aucun | faible |
| `tests/agent_streaming_test.cpp` | `M` | **R0 tests** | 5 tests R0 (+1 remplacé) | aucun | faible |
| `tests/http_client_test.cpp` | `M` | **R0 tests** | test de timeout | aucun | faible |
| `tests/sse_parser_test.cpp` | `M` | **R0 tests** | 2 tests `flush()` | aucun | faible |
| `tests/workspace_tools_test.cpp` | `M` | **R0 tests** | test d'écriture vérifiée | aucun | faible |
| `AGENTS.md` | `M` | **Documentation R0** | 2 hunks : invariants `## Décisions` (R0) + compteur `## Test` 64/239→75/298 | **aucun** (P3.3 n'a pas touché `AGENTS.md`) | faible |
| `docs/README.md` | `M` | **MIXTE** (P3.3 + R0) | 1 hunk = **3 lignes ajoutées** : `PHASE_3_3_…` (**P3.3**), `PHASE_R0_EXECUTION_…` (**R0**), `PHASE_R0_DOCUMENTATION_…` (**R0 doc**) | **oui** (§6) | **moyen** |
| `docs/questions/OPEN_QUESTIONS.md` | `M` | **MIXTE** (P3.3 + R0) | **1 hunk unique** (`@@ -10,11 +10,19 @@`) : réécriture Q1 par **P3.3** **et** compteur 75/298 par **R0 doc**, **entrelacés** | **oui** (§6) | **élevé** |
| `docs/roadmap/ROADMAP.md` | `M` | **MIXTE** (P3.3 + R0) | **4 hunks** : `@@ -28,9 +28,24 @@` = §A mélangé (P3.3 + R0 doc) ; `@@ -44,7 +59,8 @@` (§B.2), `@@ -62,6 +78,7 @@` (§B.9), `@@ -158,11 +175,11 @@` (P6/P8) = **R0 doc purs** | **oui** (§6) | **moyen** |

**Attribuabilité du code/tests** : `grep -rln "P3.3" src include tests` → **aucune
référence P3.3** dans le code ou les tests ⇒ les 12 fichiers `src/`/`include/`/
`tests/` sont **entièrement R0**.

### 4.2 Fichiers non suivis (3) — contenu examiné

| Fichier | Statut | Périmètre | Preuve (en-tête du fichier) |
|---|---|---|---|
| `docs/reports/PHASE_3_3_TEST_DOCUMENTATION_RECONCILIATION_REPORT.md` | `??` | **P3.3 historique** | `# P3.3 — Réconciliation de la documentation de tests (dette E-1) — rapport` |
| `docs/reports/PHASE_R0_EXECUTION_LOOP_RELIABILITY_REPORT.md` | `??` | **R0 (fiabilisation)** | `# Phase R0 — Fiabilisation de la boucle d'exécution (rapport)` |
| `docs/reports/PHASE_R0_DOCUMENTATION_RECONCILIATION_REPORT.md` | `??` | **R0 (documentation)** | `# Phase R0 — Réconciliation documentaire des compteurs de tests (rapport)` |

## 5. MATRICE DE SÉPARATION

| Périmètre | Fichiers | Statut | Confiance | Risque |
|---|---|---|---|---|
| **R0 code/tests** | `include/agent.hpp`, `include/sse_parser.hpp`, `src/agent.cpp`, `src/http_client.cpp`, `src/main.cpp`, `src/sse_parser.cpp`, `src/workspace_tools.cpp`, `tests/agent_e2e_test.cpp`, `tests/agent_streaming_test.cpp`, `tests/http_client_test.cpp`, `tests/sse_parser_test.cpp`, `tests/workspace_tools_test.cpp` (**12**) | `M` | **Élevée** (aucune trace P3.3) | Faible — **entièrement attribuables** |
| **P3.3 historique** | `docs/reports/PHASE_3_3_TEST_DOCUMENTATION_RECONCILIATION_REPORT.md` (**1**) | `??` | **Élevée** (en-tête P3.3) | Faible — **entièrement attribuable** |
| **Documentation R0** | `AGENTS.md`, `docs/reports/PHASE_R0_EXECUTION_LOOP_RELIABILITY_REPORT.md`, `docs/reports/PHASE_R0_DOCUMENTATION_RECONCILIATION_REPORT.md` (**3**) | `M`/`??` | **Élevée** (AGENTS.md sans trace P3.3 ; titres R0) | Faible — **entièrement attribuables** |
| **Autres / indéterminés** | — | — | — | **Aucun** fichier non attribué |
| **Fichiers MIXTES** (P3.3 **et** R0 doc dans le même fichier) | `docs/README.md`, `docs/questions/OPEN_QUESTIONS.md`, `docs/roadmap/ROADMAP.md` (**3**) | `M` | **Élevée** (attribution établie **hunk par hunk**, §4.1) | **Moyen à élevé** : non séparables sans édition de hunks |

### 5.1 Précision sur les fichiers mixtes

| Fichier | Séparabilité par hunk | Séparabilité par ligne |
|---|---|---|
| `docs/README.md` | **non** (1 seul hunk de 3 lignes, 1 P3.3 + 2 R0) | **oui** (lignes distinctes) — nécessite `git add -p` (interactif) |
| `docs/roadmap/ROADMAP.md` | **partielle** (3 hunks R0 purs, 1 hunk mixte) | **oui**, mais délicate (le hunk mixte contient paragraphes P3.3 et lignes R0 doc) |
| `docs/questions/OPEN_QUESTIONS.md` | **non** (1 hunk mixte) | **non en pratique** : la ligne de compteur R0 est **insérée dans le bloc réécrit par P3.3** ⇒ les deux contributions sont **textuellement liées** |

## 6. RISQUES ET AMBIGUÏTÉS

| # | Risque | Détail |
|---|---|---|
| R-1 | **Mélange P3.3 / R0 doc dans un même fichier** | 3 fichiers (`README.md`, `OPEN_QUESTIONS.md`, `ROADMAP.md`). Un `git add` de ces fichiers **emporte les deux périmètres** ; un `git add .`/`-A` (interdit) mêlerait **aussi** le code/tests R0 |
| R-2 | **Séparation P3.3 ↔ R0 doc non réalisable de façon fiable** | `OPEN_QUESTIONS.md` : P3.3 et R0 doc sont **dans le même bloc** ; une séparation exigerait une réécriture manuelle du bloc ⇒ **non recommandée** |
| R-3 | **`docs/README.md` indexe les 3 rapports** | l'index ne peut pas décrire un rapport sans citer son périmètre ⇒ toute indexation est par nature « mixte » |
| R-4 | **Code/tests R0 non commités** | 12 fichiers ; un commit R0 emporterait aussi la doc R0 si l'on utilise un staging trop large |
| R-5 | **Aucun risque de perte** | aucun stash/remote/tag ; aucune modification écrasée (§7) |

## 7. VÉRIFICATIONS EFFECTUÉES

| # | Commande | Résultat |
|---|---|---|
| V-1 | `git status --short` / `--branch --short` | voir §3 : `## main`, 16 `M`, 3 `??` |
| V-2 | `git diff --stat` | `16 files changed, 536 insertions(+), 62 deletions(-)` |
| V-3 | `git diff --cached --stat` | **vide** ⇒ **aucun fichier stagé** |
| V-4 | `git log -5 --oneline --decorate` | HEAD `47acd5d (HEAD -> main)` |
| V-5 | `git ls-files --others --exclude-standard` | **3** fichiers (§4.2) |
| V-6 | `git diff -U0/-U2` par fichier | attribution **hunk par hunk** (§4.1, §5.1) |
| V-7 | `grep -rln "P3.3" src include tests` | **aucune** occurrence ⇒ code/tests = R0 pur |
| V-8 | `head -1` des 3 rapports non suivis | périmètre confirmé (§4.2) |
| V-9 | `git diff --check` | **silencieux** |
| V-10 | `git stash list` / `remote -v` / `tag` | **vides** |

**Confirmation explicite** : **aucun** fichier stagé ; **aucun** commit créé ;
**aucune** modification préalable écrasée (l'inventaire §4 correspond
exactement à l'état de travail observé au début de l'audit, plus ce rapport).

## 8. RECOMMANDATION DE PROCHAINE ÉTAPE

> Aucun staging n'est effectué ici. Les stratégies ci-dessous sont des
> **propositions** soumises à validation explicite.

**Séparation retenue (fiable, vérifiable) — 2 commits :**

1. **Commit « code/tests R0 »** — les **12** fichiers `src/`/`include/`/`tests/`
   (périmètre **entièrement R0**, confiance élevée). Message suggéré :
   `fix: enforce stream completeness and verifiable tool execution`.
2. **Commit « documentation »** — `AGENTS.md`, `docs/README.md`,
   `docs/questions/OPEN_QUESTIONS.md`, `docs/roadmap/ROADMAP.md` et les **3**
   rapports. Message suggéré : `docs: reconcile R0 invariants, test counts and P3.3 notes`.

   Ce second commit **regroupe nécessairement P3.3 et la documentation R0**,
   car trois fichiers sont **textuellement entrelacés** (§5.1). C'est une
   **limite établie**, pas un choix arbitraire.

**Alternative (3 commits) — uniquement si une séparation manuelle est acceptée :**

3. Séparer d'abord `P3.3` (`PHASE_3_3…md` + les lignes Q1/§A/README ajoutées par
   P3.3) puis `R0 doc` (le reste). **Non recommandée** : elle exige une édition
   de hunks/lignes (`git add -p`, **interactif**, non exécutable ici) et un
   démêlage manuel de `OPEN_QUESTIONS.md` (R-2), source d'erreur.

**Interdits rappelés** : pas de `git add .` / `git add -A` ; stager les chemins
**explicitement**, par liste.

## 9. VERDICT

### VALIDÉ

**Justification** :

1. L'inventaire est **complet et prouvé** : 16 fichiers suivis modifiés +
   3 non suivis, chacun attribué à un périmètre avec sa **preuve** (§4), y
   compris les 3 fichiers non suivis dont le **contenu a été examiné** (§4.2).
2. La **matrice de séparation** (§5) distingue les **16 fichiers entièrement
   attribuables** (12 R0 code/tests + `AGENTS.md` + 2 rapports R0 + 1 rapport
   P3.3) des **3 fichiers mixtes**, avec leur **séparabilité** établie
   hunk/ligne (§5.1).
3. Les **risques** sont explicites (§6), notamment **R-2** : le démêlage
   P3.3 ↔ R0 doc de `OPEN_QUESTIONS.md` n'est **pas fiable**.
4. **Aucune ambiguïté bloquante** : une stratégie de séparation **fiable**
   (2 commits, §8) existe, fondée sur des preuves Git.

**Réserves** : le périmètre « documentation » **regroupera P3.3 et R0** (R-1,
R-3) ; toute séparation plus fine (3 commits) nécessite une intervention
manuelle non fiable (R-2). Ces réserves sont **informatives** et n'empêchent pas
la clôture.

## 10. STAGING / COMMIT

**Aucun staging, aucun commit effectué.** État final : identique à l'inventaire
§4, augmenté de ce rapport (`??`) et de sa ligne d'index dans `docs/README.md`.
Une validation explicite est requise avant toute opération de staging.
