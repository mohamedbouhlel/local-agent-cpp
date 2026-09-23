# Enregistrement des arbitrages REPL (A-1 → A-9) — rapport

> Itération **exclusivement documentaire**. **Aucune** modification de `.cpp`,
> `.h`, `CMakeLists.txt` ou des tests ; **aucune** implémentation de
> `PromptReader` ; aucun changement de comportement du REPL ; **aucun** build,
> **aucun** test, **aucun** commit. Seuls documents touchés : la **spécification
> devenue norme** (enregistrement) et **ce rapport**.

## 1. DATE, BRANCHE, HEAD ET ÉTAT DU WORKTREE

| Élément | Valeur observée |
|---|---|
| Date / heure | **2026-09-23 15:42 CEST** |
| Branche | `main` |
| HEAD | `f62ca3680384b42ff4cdd8287a44eae4e3b15be2` (`f62ca36 docs: record agent test validation`) |
| Nombre de commits | **39** — inchangé |
| Index (staged) | **vide** |
| `git diff --stat` (initial) | `docs/README.md | 8 ++++++++` — **préexistant** (indexation issue de la ratification, préservée) |
| Fichiers non suivis (initiaux) | `docs/specifications/repl-multiline-requirements-proposal.md`, `docs/reports/2026-09-23-repl-multiline-preparation.md`, `…-requirements-definition.md`, `…-requirements-ratification.md`, `…-final-review-and-implementation-plan.md`, `docs/reports/revue-proposition-repl-multiline-2026-09-23.md` |

### 1.1 Écart avec l'« état de référence » annoncé — expliqué, non bloquant

L'énoncé indique un **worktree propre** à la préparation. **L'état réel est un
worktree non propre**, mais **entièrement expliqué** : `docs/README.md` porte
l'indexation de la spécification (mission de ratification) et **6** documents
sont **non suivis** (artefacts des missions documentaires précédentes).
**Aucun fichier inattendu**, **aucune modification non attribuée** ⇒ vérification
effectuée, péremètre établi, **aucun blocage**. **Aucun fichier préexistant n'a
été écrasé.**

## 2. DOCUMENTS CONSULTÉS

| Document | Usage |
|---|---|
| `docs/specifications/repl-multiline-requirements-proposal.md` | **document retenu** pour l'enregistrement (normatif, contient déjà `D-1→D-8`) |
| `docs/reports/2026-09-23-repl-multiline-final-review-and-implementation-plan.md` | **source des formulations** `A-1`→`A-9` (rapport de revue, §4 et §5) — **non modifié** |
| `docs/reports/2026-09-23-repl-multiline-requirements-ratification.md` | contexte de ratification `D-1`→`D-8` |
| `docs/reports/2026-09-23-repl-multiline-preparation.md` | préparation (point d'intégration) |
| `docs/reports/revue-proposition-repl-multiline-2026-09-23.md` | revue antérieure du mainteneur |
| `docs/README.md` | index documentaire (section « Spécifications ») |
| `docs/context/PROJECT_CONTEXT.md`, `docs/history/PROJECT_HISTORY.md`, `docs/roadmap/ROADMAP.md`, `docs/questions/OPEN_QUESTIONS.md` | recherche des identifiants `A-*` |
| `AGENTS.md` | conventions du dépôt |

## 3. ARBITRAGES ENREGISTRÉS

> **Décisions ratifiées par le mainteneur le 2026-09-23 ; enregistrées le
> 2026-09-23.** Formulations exactes récupérées dans le dépôt
> (rapport de revue, §4/§5) : **aucune** n'a été inventée, complétée ni
> reformulée.

| ID | Décision ratifiée | Exigence / limite | Effet normatif enregistré |
|---|---|---|---|
| **A-1** | **`multiline`** | `REPL-03` ; §0 | Normalisation **CRLF → `\n` limitée au contenu de bloc multiline** ; en mode simple, `REPL-02` prime (ligne transmise telle quelle) |
| **A-2** | Confirmée (recommandation précédente) | `REPL-03` | Ne supprimer **qu'un seul `\r` final** par ligne collectée ; tout autre `\r` **conservé** |
| **A-3** | Confirmée (recommandation précédente) | `REPL-01`, `L-3` | Les tests **ne dépendent pas du libellé** de l'invite : asserter l'**état**, pas la chaîne |
| **A-4** | Confirmée (recommandation précédente) | `REPL-08` | **Aucun message requis** avant la fin du REPL sur EOF ; note `stderr` **optionnelle**, non normative |
| **A-5** | Confirmée (recommandation précédente) | `REPL-01`, `REPL-04` | **Asymétrie explicite** en mode simple : `:multi` = **commande**, `:end` = **contenu** (ratifiée par `D-1`/`D-2`) |
| **A-6** | **`exact`** | `REPL-07` | `exit` reconnu par **correspondance exacte** de la ligne en mode simple (pas de `trim`) ; littéral en multiline |
| **A-7** | Confirmée (recommandation précédente) | `REPL-03`, `REPL-06` | **`content = join(lignes, "\n")`** (0 ligne ⇒ bloc vide ; 1 ligne vide ⇒ `""` ; 2 ⇒ `"\n"`) |
| **A-8** | **`none`** | `L-1` | **Aucune limite de taille** ; `L-1` se lit « **aucune limite imposée** » (parité avec le mode simple) |
| **A-9** | **`rename`** | Nommage du document | Renommer `…-proposal.md` → **`repl-multiline-requirements.md`** — **ratifié** ; **application non réalisée** dans cette itération (cf. §5) |

**A-2 à A-5 et A-7** sont **explicitement enregistrés comme « confirmés
conformément aux recommandations précédentes »**, dont la formulation exacte est
conservée au §4/§5 du rapport de revue cité — **sans réinterprétation**.

## 4. DISTINCTION DÉCISIONS RATIFIÉES / IMPLÉMENTATION FUTURE

| | Statut |
|---|---|
| **Décisions `A-1`→`A-9`** | **RATIFIÉES** et **enregistrées** (spécification §6). |
| **`D-1`→`D-8`** | **RATIFIÉES** (spécification §4), antérieures à cette itération. |
| **`PromptReader`** | **NON implémenté** — reste **à réaliser**. |
| **Mode multiline du REPL** | **NON implémenté** — le REPL actuel lit toujours **une ligne** (`src/main.cpp:71`). |
| **Tests `repl`** | **NON créés**, **NON exécutés** — seul un **plan** de 22 cas existe (rapport de revue §6.5). |
| **Modifications CMake** (ajout à `agent_core`, groupe CTest `repl`) | **NON réalisées** — autorisées par `D-5` pour l'implémentation future. |
| **A-9 (`rename`)** | **Ratifié, NON appliqué** (§5). |

**Aucune proposition future n'a été transformée en implémentation réalisée.**

## 5. APPLICATIONS NON RÉALISÉES (assumées)

| # | Point | Justification |
|---|---|---|
| N-1 | **A-9 `rename` non appliqué** | La présente itération **enregistre** les arbitrages ; le renommage est une **opération documentaire distincte**. Impact identifié et borné : **1 lien vivant** (`docs/README.md`), à mettre à jour lors de l'application. Décision : **ne pas renommer** ici, pour préserver la traçabilité en un seul point et ne pas anticiper l'itération dédiée. |
| N-2 | **Corps des exigences `REPL-01`→`REPL-09` non réécrit** | Les arbitrages sont **consignés** avec une **règle de préséance** explicite (spécification §6) ; réécrire le corps ratifié pour « intégrer » les arbitrages serait une refonte non demandée. Une consolidation rédactionnelle pourra être faite ultérieurement. |
| N-3 | **`L-1` non réécrit** | `A-8` (= `none`) est enregistré avec la lecture à appliquer (« aucune limite imposée ») via la règle de préséance. |

## 6. FICHIERS MODIFIÉS / NON MODIFIÉS

**Modifié (1)** : `docs/specifications/repl-multiline-requirements-proposal.md` —
ajout de la section **« 6. Registre des arbitrages ratifiés (A-1 → A-9) »**
(+~65 lignes) ; **aucune autre modification** du document (corps des exigences,
matrice et registre `D-1`→`D-8` **inchangés**).

**Créé (1)** : `docs/reports/2026-09-23-repl-arbitrages-ratification.md` (ce rapport).

**Explicitement NON modifiés** :

* tous les `.cpp` (`src/*.cpp`) et `.h` (`include/*.hpp`) ;
* `CMakeLists.txt` (racine) et `tests/CMakeLists.txt` ;
* tous les fichiers de `tests/` ;
* `AGENTS.md` ;
* `docs/README.md` (déjà modifié par la ratification ; **non retouché ici**) ;
* les rapports **historiques** et de **revue** (`…preparation.md`,
  `…definition.md`, `…ratification.md`, `…final-review-and-implementation-plan.md`,
  `revue-proposition-…md`) : **aucune modification rétroactive** ;
* `docs/context/PROJECT_CONTEXT.md`, `docs/history/PROJECT_HISTORY.md`,
  `docs/roadmap/ROADMAP.md`, `docs/questions/OPEN_QUESTIONS.md` (seulement
  **consultés**).

## 7. VÉRIFICATIONS EFFECTUÉES

| # | Commande | Résultat |
|---|---|---|
| V-1 | `git branch --show-current` / `git log -1 --oneline` / `git status --short` | `main` / `f62ca36 …` / `M docs/README.md` + 6 non suivis (§1) |
| V-2 | `git rev-list --count HEAD` | **39** — inchangé |
| V-3 | `find docs -maxdepth 3 -type f` | **35** fichiers à l'état initial (**36** après création de ce rapport) ; document retenu : `docs/specifications/repl-multiline-requirements-proposal.md` (§2) |
| V-4 | `grep -RniE 'A-1\|…\|A-9'` (docs) | occurrences `A-*` **d'autres phases** identifiées (`PHASE_3_2`, `PHASE_GIT_CLOSURE`, `PHASE_1_1_GIT_3`) ; les `A-1`→`A-9` **du REPL** sont dans le rapport de revue (§2) ⇒ **formulations récupérées dans le dépôt** |
| V-5 | `sed` du §4 du rapport de revue | les **9** formulations `A-1`→`A-9` extraites **sans invention** |
| V-6 | `test -e` du chemin de ce rapport | **absent** ⇒ création sans écrasement |
| V-7 | `grep -n "^## 6. Registre des arbitrages"` (spécification) | **présent** (ligne 220) |
| V-8 | `git diff --check` | **silencieux** |
| V-9 | `git status --porcelain -- src include tests CMakeLists.txt` | **vide** ⇒ **aucune** source/test/CMake modifiée |
| V-10 | `git diff --cached --stat` | **vide** ⇒ rien de stagé |
| V-11 | Relecture intégrale du document modifié et de ce rapport | **effectuée** |

**Aucun build, aucun test lancé, aucun commit** : aucune exigence n'est déclarée
« vérifiée par test ».

## 8. VERDICT

### ARBITRAGES RATIFIÉS — ENREGISTREMENT DOCUMENTAIRE VALIDÉ

**Justification** :

1. Les **arbitrages `A-1` à `A-9`** sont **enregistrés** dans le document normatif
   approprié, avec : mention de **décisions ratifiées**, **datation**
   (2026-09-23), **séparation décisions / contexte**, **conservation des
   identifiants**, mention explicite que **`A-2` à `A-5` et `A-7` sont confirmés
   conformément aux recommandations précédentes**, et **aucune** proposition
   future présentée comme réalisée.
2. **Aucune formulation inventée** : les formulations exactes proviennent du
   **dépôt** (rapport de revue, §4/§5) — vérifié (`V-4`, `V-5`).
3. **Périmètre respecté** : 1 document modifié + ce rapport ; **aucun** `.cpp`,
   `.h`, `CMakeLists.txt` ou test touché (`V-9`) ; index vide (`V-10`) ;
   `git diff --check` silencieux (`V-8`).
4. **Aucun commit** ; HEAD et nombre de commits **inchangés** (`V-2`).
5. **Rapport durable** créé, relu, et **documents relus** (`V-11`).

**Réserves assumées** (et non bloquantes) : **N-1** (`A-9 rename` ratifié mais
**non appliqué**), **N-2** (corps des exigences **non réécrit**, traité par la
**règle de préséance**), **N-3** (`L-1` lu via `A-8`).

**Arrêt** : itération **documentaire** terminée ; **aucune** implémentation, **aucun**
commit. La prochaine étape relève d'une **consigne d'exécution séparée** — non
anticipée ici.
