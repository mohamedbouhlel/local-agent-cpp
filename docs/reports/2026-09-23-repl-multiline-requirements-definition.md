# Définition des exigences REPL multiline — rapport de mission

> Mission de **définition d'exigences uniquement**. Aucun code, aucun test, aucun
> fichier CMake ou de configuration modifié ; aucun build, aucun test, aucun
> commit. Seuls artefacts créés : les **deux documents** listés en §5.

## 1. DATE ET OBJECTIF

| Élément | Valeur |
|---|---|
| Date / heure | **2026-09-23 15:21 CEST** |
| Dépôt | `/home/oper/projects/local-agent-cpp` |
| Objectif | Produire une **proposition** de spécification fonctionnelle REPL multiline (`REPL-01` → `REPL-09`) et un rapport de mission, **pour arbitrage par le mainteneur**. |
| Nature | **Proposition** — aucune exigence n'est normative sans approbation explicite. |

## 2. BRANCHE, HEAD ET ÉTAT GIT OBSERVÉS

| Élément | Valeur |
|---|---|
| Branche | `main` |
| HEAD | `f62ca3680384b42ff4cdd8287a44eae4e3b15be2` |
| Commit de tête | `f62ca36 docs: record agent test validation` |
| Base attendue (`f62ca36`) | **CONFIRMÉE** (`git cat-file -t f62ca36` → `commit`) |
| `git status --short` (initial) | **un seul** `??` : `docs/reports/2026-09-23-repl-multiline-preparation.md` (artefact de la mission précédente) |
| `git diff --stat` / `--cached --stat` | **vides** (aucun changement suivi, rien de stagé) |
| Autres non suivis | **aucun** (`.env`, `build/` ignorés — **non lus**) |
| Remote / stash | aucun |

**Gate P0 : VALIDÉ** — base conforme, aucun changement inexpliqué, rapport de
préparation **présent** (13 620 o, verdict interne `BLOQUÉE` : spécification
REPL-01→09 introuvable) et **non modifié** par cette mission.

## 3. SOURCES CONSULTÉES

| Source | Nature |
|---|---|
| `docs/reports/2026-09-23-repl-multiline-preparation.md` | rapport de préparation (point de départ) — lu intégralement |
| `src/main.cpp` | REPL réel : `std::getline` (l.71), `exit`/EOF, ligne vide, `agent.send()` (l.107) |
| `AGENTS.md` | conventions : code style, architecture, tests (75 cas / 298 assertions, 5 groupes) |
| `docs/reports/2026-09-23-reanalyse-boucle-agentique.md` | constat : **aucun** mécanisme multi-ligne |
| `docs/README.md`, arborescence `docs/` | conventions documentaires (`docs/<type>/`, minuscules plurielles) |
| `tests/CMakeLists.txt`, `tests/test_main.cpp` | organisation des tests (groupes, `--filter`) |

**Non consultés** : `src/agent.cpp`, SSE, tool-calling, historique (hors périmètre)
et `.env` (secrets).

## 4. MÉTHODE DE DÉFINITION

1. **Gate P0** : état Git, base `f62ca36`, présence du rapport de préparation.
2. **Constat** : la spécification `REPL-01`→`REPL-09` n'existe **nulle part** dans
   le dépôt (recherche `REPL-` → aucune occurrence) — elle est donc **créée
   ici**, sans prétendre à une antériorité.
3. **Dérivation des 9 exigences** à partir des **9 sujets imposés** par la mission
   (déclenchement, saisie simple, accumulation, `:end`, unicité d'envoi, blocs
   vides, `exit`, EOF, retour à l'invite) : **une exigence par sujet**.
4. **Formalisation** de chaque exigence (formulation normative proposée,
   justification, scénario observable, cas limites, statut, dépendances).
5. **Options & recommandations** sur les points non définis (A→G), sans les
   transformer en décisions.
6. **Matrice de traçabilité** exigence → scénario → observable, et tests prévus
   (non exécutés).

## 5. CHEMINS EXACTS DES LIVRABLES

| Livrable | Chemin | Créé ? |
|---|---|---|
| Proposition de spécification | `docs/specifications/repl-multiline-requirements-proposal.md` | **créé** (17 960 o) |
| Rapport de mission | `docs/reports/2026-09-23-repl-multiline-requirements-definition.md` | **créé** (ce fichier) |

**Choix d'emplacement justifié** : la mission **préférait** `docs/specifications/` ;
ce répertoire **n'existait pas**. Il a été créé car il respecte la convention
réelle du dépôt (`docs/<type>/`, minuscules plurielles : `context/`, `history/`,
`questions/`, `roadmap/`, `reports/`) et qu'une spécification d'exigences ne
relève d'aucune des catégories existantes. **Cette nouvelle catégorie reste à
ratifier** (décision D-6 du document de proposition).
Chemins vérifiés **libres** avant création ; **aucun document écrasé**.

## 6. RÉSUMÉ DES EXIGENCES PROPOSÉES

| ID | Titre | Règle proposée (résumé) |
|---|---|---|
| **REPL-01** | Déclenchement du multiline | ligne **exactement** `:multi` ⇒ bascule en multiline ; ligne non accumulée, **0** envoi |
| **REPL-02** | Saisie simple inchangée | ligne non vide ⇒ **1** `agent.send()` ; vide ⇒ 0 envoi ; `exit` ⇒ fin (comportement actuel) |
| **REPL-03** | Conservation exacte | accumuler chaque ligne **sans transformation** (vides, espaces, tabulations) ; contenu = lignes jointes par `\n`, **hors** terminateur |
| **REPL-04** | Terminateur `:end` | ligne **byte-exacte** `:end` termine la collecte, **exclue** du contenu ; en simple ⇒ contenu ordinaire |
| **REPL-05** | Unicité de l'envoi | **1** `agent.send()` par bloc **non vide** terminé par `:end` ; **aucun** envoi anticipé / pour bloc vide ou abandonné |
| **REPL-06** | Blocs vides | aucune ligne accumulée + `:end` ⇒ **0** envoi (message à décider) |
| **REPL-07** | `exit` selon le mode | simple ⇒ fin du REPL ; multiline ⇒ **contenu littéral** |
| **REPL-08** | EOF / Ctrl+D | simple ⇒ fin (inchangé) ; multiline ⇒ **abandon** du bloc **sans** `agent.send()` |
| **REPL-09** | Retour à l'invite | après `:end`, bloc vide ou abandon ⇒ retour à l'invite **primaire** + état de collecte **réinitialisé** |

**Points explicitement imposés par la mission et repris tels quels** : EOF en
multiline ⇒ abandon sans envoi (REPL-08) ; un seul `agent.send()` après
terminaison valide d'un bloc non vide (REPL-05).

## 7. ARBITRAGES ENCORE OUVERTS

| # | Sujet | Recommandation (non décisionnelle) |
|---|---|---|
| D-1 | Mécanisme de **déclenchement** | **A1** : sentinelle `:multi` |
| D-2 | Reconnaissance de **`:end`** | **B1** : byte-exacte |
| D-3 | **Bloc vide** | **C1** : aucun envoi + message informatif |
| D-4 | **`exit`** en multiline | **D1** : contenu littéral |
| D-5 | **Intégration/testabilité** + autorisation **CMake** | **G1** : `PromptReader` isolé dans `agent_core` |
| D-6 | **Emplacement** `docs/specifications/` | ratifier (ou déplacer) |
| D-7 | **Conservation** : `\r` de collage, `\n` final, bloc d'espaces seuls | à trancher (§2.F du document de proposition) |
| D-8 | **Approbation globale** de `REPL-01`→`REPL-09` | approuver / corriger |

## 8. VÉRIFICATIONS EFFECTUÉES

| # | Commande / contrôle | Résultat |
|---|---|---|
| V-1 | `git branch --show-current` / `git log -1 --oneline` / `git rev-parse HEAD` | `main` / `f62ca36 …` / `f62ca3680384b42…` |
| V-2 | `git cat-file -t f62ca36` | `commit` ⇒ base confirmée |
| V-3 | `git status --short` (initial) | `??` rapport de préparation (préexistant) |
| V-4 | `git diff --stat` / `git diff --cached --stat` | **vides** |
| V-5 | `grep -rn "REPL-"` (hors `build/`) | **aucune occurrence** ⇒ spécification absente du dépôt |
| V-6 | `test -e` des 2 chemins cibles | **absents** ⇒ création sans écrasement |
| V-7 | `find docs -maxdepth 1 -type d` | `context history questions reports roadmap` ⇒ `specifications/` **créé par cette mission** |
| V-8 | `git diff --check` (après création) | **silencieux** |
| V-9 | `git status --short` (après création) | **3** `??` : rapport de préparation + 2 livrables créés |
| V-10 | `git status --porcelain -- src include tests CMakeLists.txt` | **vide** ⇒ aucune source/test/CMake modifié |

**Aucun build, aucun test, aucun commit** n'a été lancé ou effectué au cours de
cette mission (conformément aux interdictions). **Aucune validation fonctionnelle
n'est revendiquée** : il s'agit d'une proposition d'exigences.

## 9. CONFIRMATION DE NON-MODIFICATION

* **Aucun** fichier de `src/`, `include/`, `tests/` ni **`CMakeLists.txt`**
  modifié (V-10).
* **Aucun** comportement du REPL, d'`Agent`, du SSE, du tool-calling ou de
  l'historique modifié.
* Le rapport de préparation `2026-09-23-repl-multiline-preparation.md` **n'a pas
  été modifié** ; `docs/README.md` **n'a pas été modifié** (indexation volontairement
  omise, non autorisée pour cette mission).

## 10. VERDICT

### PROPOSITION PRÊTE POUR ARBITRAGE

**Justification** : le gate P0 est validé ; les 9 exigences `REPL-01`→`REPL-09`
sont formulées (règle, justification, scénario observable, cas limites, statut
`PROPOSITION — NON APPROUVÉE`, dépendances) ; les points non définis sont exposés
en options avec recommandation (A→G) ; la matrice de traçabilité et les tests
prévus (non exécutés) sont fournis ; aucun code/test/CMake n'a été touché. La
proposition est **soumise à l'approbation du mainteneur**.

**ARRÊT** : conformément à la consigne, la mission **s'arrête ici**. Aucune
implémentation, même si les choix recommandés semblent évidents.
