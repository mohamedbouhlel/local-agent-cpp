# Préparation — REPL multiline (`local-agent-cpp`)

> Mission de **préparation uniquement**. Aucune source, aucun test, aucun fichier
> CMake ou de configuration modifié ; aucune branche, aucun commit, aucun
> `reset`/`checkout`/`stash`/`clean`. **Aucun build ni test lancé** (cf. §10).
> Seul artefact créé : ce rapport.

## 1. DATE ET OBJECTIF

| Élément | Valeur |
|---|---|
| Date / heure | **2026-09-23 14:34 CEST** |
| Dépôt | `/home/oper/projects/local-agent-cpp` |
| Objectif | Vérifier l'état réel du dépôt, inspecter le point d'intégration du mode **REPL multiline**, figer le périmètre fonctionnel, proposer une stratégie technique (`PromptReader`) et les validations futures — **sans implémenter**. |

## 2. BRANCHE, HEAD ET ÉTAT DU WORKTREE OBSERVÉS

| Élément | Valeur observée |
|---|---|
| Branche | `main` |
| HEAD | `f62ca3680384b42ff4cdd8287a44eae4e3b15be2` |
| Commit de tête | `f62ca36 docs: record agent test validation` |
| Base attendue (`f62ca36`) | **CONFIRMÉE** (`git cat-file -t f62ca36` → `commit`) |
| `git status --short` | **vide** (worktree propre, index vide) |
| `git diff --stat` / `--cached --stat` | **vides** |
| Fichiers non suivis | **aucun** (hors `.env` et `build/`, **ignorés**, non inspectés) |
| Stash / remote / branche | aucun stash ; aucun remote |

**Gate P0 : VALIDÉ** — base conforme, aucun changement non expliqué, aucune
incertitude sur l'état de départ. *(Le fichier `.env` n'a pas été lu : secrets.)*

## 3. COMMANDES EXÉCUTÉES ET RÉSULTATS UTILES

| Commande | Résultat |
|---|---|
| `git branch --show-current` / `git log -1 --oneline` / `git rev-parse HEAD` | `main` / `f62ca36 docs: record agent test validation` / `f62ca3680384b42…` |
| `git status --short` ; `git diff --stat` ; `git diff --cached --stat` | **vides** |
| `git ls-files --others --exclude-standard` | **aucun** fichier non suivi |
| `git ls-files --others --ignored --exclude-standard` | `.env`, `build/*` (ignorés — **non lus**) |
| `find .. -maxdepth 3 -name AGENTS.md` | 5 occurrences ; **celle du dépôt** : `../local-agent-cpp/AGENTS.md` |
| `find src tests include -maxdepth 3 -type f` | 6 `src/*.cpp`, 6 `include/*.hpp`, 11 fichiers `tests/` |
| `grep -nE 'add_executable\|add_library\|add_test\|agent_tests\|enable_testing' CMakeLists.txt tests/CMakeLists.txt` | `agent_core` (STATIC), `agent` (exe), `agent_tests` (exe), `enable_testing()`, **5** `add_test` |
| `grep -rn "REPL-" .` (hors `build/`) | **AUCUNE occurrence** |
| `grep -rln -i "spécification\|cahier des charges\|périmètre fonctionnel" docs/` | **aucun document** |
| `grep -rn "PromptReader\|prompt_reader" src include tests` | **aucune occurrence** |
| `wc -l src/*.cpp include/*.hpp` | `main.cpp` = **138** lignes ; `agent.cpp` = **361** ; `workspace_tools.cpp` = **426** |

## 4. FICHIERS EXAMINÉS

**[CONSTAT VÉRIFIÉ]**

* `src/main.cpp` (138 l.) — intégralement.
* `AGENTS.md` (58 l. au moins) — consignes, §Setup/Build/Run/**Test**, §Code style, §Architecture.
* `tests/CMakeLists.txt` (26 l.) — intégralement.
* `tests/test_main.cpp` (49 l.) — intégralement.
* `docs/reports/2026-09-23-reanalyse-boucle-agentique.md` (63 l.) — intégralement.
* Inventaire `src/`, `include/`, `tests/`, `docs/`, `docs/reports/`.

**Non examinés** (hors besoin immédiat) : `src/agent.cpp`, `src/http_client.cpp`,
`src/sse_parser.cpp`, `src/tool_registry.cpp`, `src/workspace_tools.cpp` et leurs
en-têtes — **aucune modification de ces fichiers n'est envisagée** (§8).

## 5. POINT D'INTÉGRATION RETENU

**[CONSTAT VÉRIFIÉ]** Le REPL tient entièrement dans `src/main.cpp` :

```
68:    std::string input;
69:    while (true) {
70:        std::cout << "> ";
71:        if (!std::getline(std::cin, input) || input == "exit") {
72:            break;
73:        }
74:
75:        if (input.empty()) {
76:            continue;
77:        }
...
106:        try {
107:            agent.send(input, callbacks);
```

* Lecture : **une ligne** par itération via `std::getline(std::cin, input)`
  (`main.cpp:71`) ; `exit` ou EOF (`getline` faux) termine la boucle ;
  ligne vide ⇒ `continue` (aucun envoi).
* Envoi : **un** `agent.send(input, callbacks)` par ligne non vide (`main.cpp:107`).
* **Aucune structure `PromptReader`**, aucun état de collecte, **aucun mode
  multiline** — confirmé indépendamment par le rapport
  `2026-09-23-reanalyse-boucle-agentique.md` (« Il n'existe pas de mécanisme de
  saisie multi-ligne », l.42 ; « Le REPL ne préserve pas un prompt multi-ligne »,
  l.58).

**[PROPOSITION — point d'intégration]** Insérer le mode multiline **entre la
lecture et l'appel** : `main.cpp:68-77` devient un appel à un composant dédié
(`PromptReader`) qui renvoie un **résultat de lecture** ; `main.cpp:107` n'est
appelé que lorsque ce résultat est un **bloc complet et non vide**. Les lignes
`79-104` (callbacks) et `106-134` (gestion d'erreurs) restent **inchangées**.

*Justification* : ce point est le **seul** endroit où l'entrée est produite ;
il est **isolable** sans toucher à `Agent`, SSE, tool-calling ou historique.

## 6. PÉRIMÈTRE FONCTIONNEL REPL-01 → REPL-09

### 6.1 [BLOQUANT] La spécification des critères REPL-01 → REPL-09 est INTROUVABLE

* `grep -rn "REPL-"` sur **tout le dépôt** (hors `build/`) → **aucune occurrence**.
* Aucun document de spécification dans `docs/` (recherche
  « spécification / cahier des charges / périmètre fonctionnel » → **rien**).
* L'inventaire complet de `docs/reports/` (25 fichiers) **ne contient aucune
  spécification REPL** ; le seul document proche est une **analyse** constatant
  l'**absence** du mode multiline (`2026-09-23-reanalyse-boucle-agentique.md`).

⇒ Conformément à la règle P2 (« si les critères REPL-01 à REPL-09 ne sont pas
disponibles… déclarer ce point bloquant et demander la spécification manquante »),
**le périmètre numéroté REPL-01 → REPL-09 est déclaré NON DISPONIBLE**. Sa
reconstitution par déduction est **refusée** (interdiction de réinventer les
critères).

### 6.2 Éléments utilisables — issus de l'énoncé de mission (hors dépôt)

**[NON VÉRIFIÉ dans le dépôt — fournis par l'énoncé, à confirmer]**

| # | Élément | Source |
|---|---|---|
| a | Terminateur de bloc : **`:end`** seul | énoncé §6 et §P3 |
| b | **Un seul** `agent.send()` après terminaison valide d'un bloc **non vide** par `:end` | énoncé §P3 |
| c | **EOF / Ctrl+D en multiline ⇒ abandonner le bloc incomplet sans appeler `agent.send()`** | énoncé §P3 (impératif) |
| d | Conservation **exacte** des lignes, indentations et lignes vides | énoncé §P2 |
| e | **Bloc vide** : traitement à préciser (non envoyé ? erreur ?) | énoncé §P2 (question ouverte) |
| f | **Compatibilité** avec la saisie simple (une ligne ⇒ comportement actuel) | énoncé §P2 |
| g | Déclenchement du mode multiline : **non précisé** | énoncé §P2 (question ouverte) |

**Incertitudes à lever** (cf. §10) : critères REPL-01→09 exacts ; **condition de
déclenchement** du mode multiline ; sort du **bloc vide** ; interaction avec le
mot-clé `exit` existant ; traitement d'une ligne `:end` en **mode simple**.

## 7. COMPORTEMENT EOF / CTRL+D RETENU

**Impératif énoncé (§P3), retenu tel quel :**

* **Mode simple** : EOF/Ctrl+D **termine la boucle** (comportement actuel
  `main.cpp:71` : `getline` faux ⇒ `break`). À préserver (REPL-08/f si applicable).
* **Mode multiline** : EOF/Ctrl+D **abandonne le bloc incomplet** et **n'appelle
  PAS** `agent.send()` ; le REPL reprend (ou se termine) **sans envoyer** de
  message partiel.

**[PROPOSITION]** Représenter ce cas par un état de résultat distinct
(ex. `Aborted`) pour rendre l'abandon **testable** et **explicite**, plutôt qu'une
chaîne vide ambiguë.

## 8. FICHIERS PRÉVUS POUR L'IMPLÉMENTATION ET LES TESTS

**[PROPOSITION — sous réserve de la spécification §6.1]** Le périmètre reste
limité au lecteur de prompt, au REPL et aux tests nécessaires.

| Fichier | Action | Rôle | Tests associés | Risque |
|---|---|---|---|---|
| `include/prompt_reader.hpp` | **créer** | interface `PromptReader` + type de résultat | — (déclaration) | faible |
| `src/prompt_reader.cpp` | **créer** | logique de collecte (simple/multiline), `:end`, EOF, blocs vides | `tests/prompt_reader_test.cpp` | faible |
| `src/main.cpp` | modifier | remplacer la lecture l.68-77 par `PromptReader` ; appeler `agent.send()` uniquement sur bloc valide | (couvert indirectement) | **moyen** — ne pas altérer les callbacks/erreurs (l.79-134) |
| `tests/prompt_reader_test.cpp` | **créer** | tests du lecteur sur `std::istringstream` injecté | lui-même | faible |
| `CMakeLists.txt` | modifier **(à arbitrer)** | ajouter `src/prompt_reader.cpp` à `agent_core` | — | faible |
| `tests/CMakeLists.txt` | modifier **(à arbitrer)** | ajouter `prompt_reader_test.cpp` à `agent_tests` + éventuel groupe CTest `repl` | — | faible |

**Interdits** : `Agent`, SSE, tool-calling, historique — **aucune** modification.

**[PROPOSITION — isolation]** `PromptReader` doit dépendre **uniquement** de
`std::istream`/`std::ostream` (injectables) et **jamais** d'`Agent`, afin d'être
**testable sans réseau ni LLM** via `std::istringstream`, conformément au
micro-harnais existant.

**[À ARBITRER]** `PromptReader` dans `agent_core` (testable via le lien existant)
**ou** compilé directement dans `agent_tests` — les deux exigent une modification
CMake **non autorisée pendant cette préparation**.

## 9. COMMANDES DE VALIDATION PRÉVUES (NON EXÉCUTÉES)

**[PROPOSITION]** À exécuter lors de l'itération d'implémentation :

```
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON
cmake --build build-tests
cd build-tests && ctest --output-on-failure
./tests/agent_tests            # compteurs globaux
./tests/agent_tests --filter=repl   # si un groupe `repl` est ajouté
```

**Couverture de tests prévue** (à dériver de REPL-01→09 dès réception) :

| # | Cas |
|---|---|
| T-1 | **Saisie simple inchangée** : une ligne ⇒ un envoi, contenu identique |
| T-2 | **Bloc multiligne** terminé par `:end` ⇒ **un seul** `agent.send()` |
| T-3 | **Conservation exacte** : lignes vides internes, retours à la ligne, indentations (tabulations/espaces) |
| T-4 | **`:end` seul** est le terminateur ; `:end` en début de ligne d'un contenu ne le remplace pas |
| T-5 | **Bloc vide** ⇒ **aucun** envoi (comportement à confirmer, cf. §6.2(e)) |
| T-6 | **Un seul `agent.send()`** par bloc valide (compteur d'appels) |
| T-7 | **EOF/Ctrl+D en multiline** ⇒ bloc **abandonné**, `agent.send()` **non appelé** |
| T-8 | **EOF en mode simple** ⇒ fin de boucle (comportement actuel préservé) |
| T-9 | **Non-régression** : suite existante (75 cas / 298 assertions, 5 groupes) toujours verte |

## 10. RISQUES, DÉPENDANCES ET POINTS À ARBITRER

| # | Type | Point |
|---|---|---|
| **B-1** | **BLOQUANT** | **Spécification REPL-01 → REPL-09 introuvable dans le dépôt** (§6.1) ⇒ périmètre fonctionnel **non figé**. |
| B-2 | Dépendance | **Condition de déclenchement** du mode multiline (caractère/sentinelle/commande) **non précisée**. |
| B-3 | Dépendance | Sort du **bloc vide** et interaction avec `exit`/`:end` **non précisés**. |
| B-4 | Risque | Modification de **`CMakeLists.txt`** (racine) et **`tests/CMakeLists.txt`** : nécessaires pour la testabilité (§8) ; **hors autorisation actuelle**. |
| B-5 | Risque | Conflit de nom de groupe CTest : la boucle `foreach(group sse workspace http agent mock)` (`tests/CMakeLists.txt:23`) devra accueillir (ou non) un groupe `repl`. |
| B-6 | Convention | **Indexation du rapport** dans `docs/README.md` **non effectuée** : la mission n'autorise que la **création** du rapport. À faire lors de l'intégration Git (écart assumé). |
| B-7 | Non vérifié | Aucun **build ni test** exécuté ici (imposé par la mission) ⇒ l'état fonctionnel actuel n'est **pas** revalidé dans cette préparation. |
| B-8 | Non vérifié | Les éléments de §6.2 proviennent de l'**énoncé**, pas du dépôt : à confirmer par la spécification. |

## 11. VERDICT

### BLOQUÉE

**Justification (fondée sur les preuves recueillies)** :

1. **Gate P0 : VALIDÉ** — base `f62ca36` confirmée, worktree **propre**, aucun
   changement non expliqué (§2, §3). La préparation **pouvait** commencer.
2. **P1 : constats établis** — le REPL et son point d'intégration sont
   **identifiés avec preuve** (§4, §5) ; aucun `PromptReader` n'existe.
3. **P2 : point bloquant avéré** — la **spécification REPL-01 → REPL-09 est
   introuvable** dans le dépôt (§6.1). La règle P2 impose de **déclarer ce point
   bloquant et de demander la spécification manquante**, sans réinventer les
   critères.
4. Les §7 à §9 ne sont donc que des **propositions marquées comme telles**,
   subordonnées à la réception de la spécification.

**Ce qui est prêt** : point d'intégration, stratégie candidate, liste de fichiers,
plan de tests et commandes de validation.
**Ce qui manque** : la **spécification validée REPL-01 → REPL-09** (et les points
B-2/B-3).

### Divergences / arbitrages demandés

1. Fournir la **spécification REPL-01 → REPL-09** (indispensable).
2. Autoriser, pour l'implémentation, les modifications de **`CMakeLists.txt`** et
   **`tests/CMakeLists.txt`** (nécessaires à la testabilité) — ou indiquer la
   stratégie de liaison retenue.
3. Statuer sur l'**indexation** du présent rapport (B-6).
