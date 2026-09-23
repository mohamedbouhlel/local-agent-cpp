# Phase R0 — Réconciliation documentaire des compteurs de tests (rapport)

> Intervention **strictement documentaire**. Aucune modification de code source,
> de test, de configuration de compilation ou d'architecture. **Aucun commit,
> aucun staging.**

## A. MÉTADONNÉES

| Élément | Valeur |
|---|---|
| Date et heure d'exécution | **2026-09-23 12:55 CEST** |
| Dépôt | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `47acd5d70f937f96b6db1f8d03f9e7fb132744eb` (32 commits) — **inchangé** |
| Objet | Aligner les documents de gouvernance sur la **référence courante R0** (**75 cas / 298 assertions**) tout en préservant le **relevé historique P3.3** (**66 cas / 264 assertions**) |

## B. ÉTAT INITIAL

### B.1 État Git observé (`git status --short`)

Modifications **préexistantes** (non attribuées à cette itération) :

```
 M AGENTS.md
 M docs/README.md
 M docs/questions/OPEN_QUESTIONS.md
 M docs/roadmap/ROADMAP.md
 M include/agent.hpp
 M include/sse_parser.hpp
 M src/agent.cpp
 M src/http_client.cpp
 M src/main.cpp
 M src/sse_parser.cpp
 M src/workspace_tools.cpp
 M tests/agent_e2e_test.cpp
 M tests/agent_streaming_test.cpp
 M tests/http_client_test.cpp
 M tests/sse_parser_test.cpp
 M tests/workspace_tools_test.cpp
?? docs/reports/PHASE_3_3_TEST_DOCUMENTATION_RECONCILIATION_REPORT.md
?? docs/reports/PHASE_R0_EXECUTION_LOOP_RELIABILITY_REPORT.md
```

Les corrections de code/test **R0** et le rapport **P3.3** étaient donc déjà
présents et **ont été préservés** (aucun reset, aucun clean, aucun écrasement).
Le rapport **R0** existait bien avant l'intervention.

### B.2 Incohérences documentaires identifiées

| Fichier | Référence observée | Nature | Problème |
|---|---|---|---|
| `AGENTS.md:20` | « **74 cas / 297 assertions** » | présentée comme **courante** | valeur **intermédiaire** d'un état R0 antérieur à l'ajout du test de timeout ; **ne correspond plus** au total réel |
| `docs/roadmap/ROADMAP.md:31` | « 5 groupes CTest, **66 cas / 264 assertions** \| présente, 0 échec *(vérifié)* » | présentée comme **courante** | le mot « vérifié » la présente comme l'état **actuel** alors qu'il s'agit du **relevé P3.3**, antérieur à R0 |
| `docs/questions/OPEN_QUESTIONS.md:19` | « **5** groupes CTest, **66 cas / 264 assertions**, **0 échec** » sous `[CORRECTION — état actuel, vérifié]` | présentée comme **courante** | même confusion : étiquette « état actuel » pour un chiffre **historique P3.3** |

**Aucune** autre référence courante n'a été trouvée. Les occurrences de
**66 cas / 264 assertions** dans les rapports **P3.3**, **P1.2**, **P3.1**,
**P3**, **P3.2** sont **historiques** et **correctes dans leur contexte** ⇒
**conservées telles quelles** (§E).

## C. CORRECTIONS RÉALISÉES

| Fichier | Référence initiale | Référence finale | Justification |
|---|---|---|---|
| `AGENTS.md` (§`## Test`) | `74 cas / 297 assertions` | **`75 cas / 298 assertions`** | total réel après l'ajout du test `http.timeout_distinct_des_autres_echecs_curl` (R0) ; la valeur 74/297 était un état intermédiaire jamais figé |
| `docs/roadmap/ROADMAP.md` §A (ligne « Exécution de la suite ») | `66 cas / 264 assertions` présentés comme courants (« 0 échec (vérifié) ») | **`75 cas / 298 assertions`** marqués « (référence courante, R0) » | la ligne du tableau §A décrit l'**état actuel** ; le chiffre 66/264 est **antérieur à R0** |
| `docs/roadmap/ROADMAP.md` §A (paragraphe) | — | ajout d'un paragraphe **« Compteurs — ne pas confondre les états »** (66/264 = P3.3, 75/298 = R0) | lève l'ambiguïté entre les deux jeux de résultats, comme demandé |
| `docs/roadmap/ROADMAP.md` §A (titre du paragraphe de correction) | « Correction (état actuel, **P1.1/P2**) » | « Correction (état actuel, **P1.1/P2/R0**) » | refléter la dernière itération ayant fait évoluer la suite |
| `docs/questions/OPEN_QUESTIONS.md` Q1 (`[CORRECTION — état actuel, vérifié]`) | `66 cas / 264 assertions` | **`75 cas / 298 assertions`** (référence courante, **R0**), avec mention explicite du relevé **P3.3 = 66 cas / 264 assertions** | même distinction courant/historique ; **Q1 reste close** (le fond — suite versionnée — est résolu) |

**Aucune autre correction** n'a été appliquée. Aucun remplacement global : chaque
édition a été faite manuellement, occurrence par occurrence, en vérifiant le
contexte.

### C.1 Extension de périmètre consignée

`docs/README.md` a reçu **une ligne d'index** pour ce rapport (convention du
dépôt : tout rapport de `docs/reports/` est référencé dans l'index ; cf.
`PHASE_3_2_GIT_CLOSURE_AUDIT_REPORT.md` §A-3, qui traite l'absence d'indexation
comme une anomalie). Cette extension est **motivée** et **consignée ici**.

## D. VÉRIFICATIONS

| # | Commande / contrôle | Résultat exact |
|---|---|---|
| D-1 | `git rev-parse --abbrev-ref HEAD` | `main` |
| D-2 | `git rev-parse --short HEAD` / `git rev-list --count HEAD` | `47acd5d` / **32** — inchangés |
| D-3 | `git status --short` (initial) | 16 `M` + 2 `??` (§B.1) — préexistants préservés |
| D-4 | `grep -rn "74 cas\|74/74\|297 assertion\|66 cas\|66/66\|264 assertion\|75 cas\|75/75\|298 assertion" AGENTS.md docs/ --include=*.md` | localise toutes les occurrences ; après correction, **aucune** référence présentant 74/297 comme courante |
| D-5 | `grep -rn "74 cas / 297\|74/74\|297 assertion" AGENTS.md docs/ --include=*.md` (après) | **aucune occurrence** |
| D-6 | Existence de l'exécutable de tests | `/home/oper/tmp/agent-cpp-tests/tests/agent_tests` **présent** (mtime `2026-09-23 11:48:39`) |
| D-7 | `find src include tests CMakeLists.txt -newer <agent_tests>` | **aucune sortie** ⇒ **aucun** fichier source/test plus récent que le binaire ⇒ le binaire correspond à l'état courant du code |
| D-8 | `git diff --check` | **silencieux** |
| D-9 | `git diff` des fichiers modifiés | revu intégralement : uniquement des changements **documentaires** |
| D-10 | `git status --porcelain -- src include tests CMakeLists.txt` | **non vide** ⇒ ⚠ voir §D.2 |

### D.1 Tests relancés ou non — décision et justification

**Tests NON relancés.** Justification :

1. Le code source et les tests **n'ont pas été modifiés** par cette itération
   (documentaire uniquement).
2. Aucun fichier de `src/`, `include/`, `tests/` ou `CMakeLists.txt` n'est
   **plus récent** que le binaire de tests (D-7) : le binaire du 11:48:39
   correspond donc à l'état courant du code.
3. La validation **R0** (75 cas / 298 assertions, 0 échec, 5/5 groupes CTest)
   est documentée dans
   [`PHASE_R0_EXECUTION_LOOP_RELIABILITY_REPORT.md`](PHASE_R0_EXECUTION_LOOP_RELIABILITY_REPORT.md)
   et **reste applicable**.
4. Le prompt d'exécution §8 prescrit de ne pas relancer les tests pour une
   simple correction documentaire lorsque le code est inchangé.

⇒ La référence **75/298** est retenue **sur la base de** (a) l'exécution
documentée en R0, (b) la correspondance binaire/source vérifiée (D-7), et
(c) l'absence de modification de code depuis. **Aucun résultat de test n'est
inventé** : aucun nouveau passage n'est revendiqué dans cette itération.

### D.2 Écart de périmètre — `src/`/`tests/` modifiés (non par cette itération)

`git status --porcelain -- src include tests CMakeLists.txt` **n'est pas vide** :
les fichiers de **code et de test R0** (11 fichiers) sont modifiés dans le
répertoire de travail, **antérieurement** à cette intervention. Cette itération
**n'y a touché en aucune façon** (aucune édition de ces fichiers). Le contrôle
`git diff --check` (D-8) et la revue du diff (D-9) confirment que les
modifications introduites ici sont **exclusivement documentaires**.

### D.3 Diff de cette itération (documentaire)

`AGENTS.md`, `docs/roadmap/ROADMAP.md`, `docs/questions/OPEN_QUESTIONS.md`
(3 corrections ciblées §C) ; `docs/README.md` (+1 ligne d'index, §C.1) ;
ce rapport (nouveau).

## E. PRÉSERVATION HISTORIQUE

* Le rapport **P3.3**
  ([`PHASE_3_3_TEST_DOCUMENTATION_RECONCILIATION_REPORT.md`](PHASE_3_3_TEST_DOCUMENTATION_RECONCILIATION_REPORT.md))
  **n'a pas été modifié** : son compteur **66 cas / 264 assertions** (lignes 73,
  95, 115, 139, 167) **reste une référence historique** de l'état P3.3, non
  présentée comme le résultat courant R0.
* Les rapports **P1.2**, **P3**, **P3.1**, **P3.2** conservent également leurs
  compteurs d'origine (66 cas), **corrects dans leur contexte**.
* Le rapport **R0** n'a **pas** été modifié : il documente déjà les deux états
  (66/264 **avant**, 75/298 **après**).
* **Aucune donnée historique n'a été écrasée** par une donnée courante.

## F. ÉCARTS RÉSIDUELS

| # | Point | Statut |
|---|---|---|
| F-1 | L'entrée d'index de **P3.3** dans `docs/README.md` (ligne 39) cite « 66 cas / 264 assertions » | **Laissé tel quel** : il décrit le **contenu historique** du rapport P3.3 (comme `PHASE_1_2_MOCK_SSE_REPORT.md` décrit « 66/66 »). Non modifié pour ne pas étendre le périmètre sans justification directe. |
| F-2 | `src/`/`tests/` restent **modifiés** (travail R0 non commité) | **Hors périmètre** de cette itération documentaire ; **préservé**. Aucune action. |
| F-3 | Aucun test n'a été relancé dans cette itération | **Volontaire** et justifié (§D.1) ; la référence R0 provient de la validation R0 documentée. |
| F-4 | L'état R0 (code + tests) n'est **pas commité** | Attendu : aucun commit n'est autorisé dans cette exécution. |

Aucun **blocage** : tous les fichiers attendus étaient présents, le rapport a pu
être créé et vérifié.

## G. VERDICT

### VALIDÉ

**Justification** :

1. Les **trois incohérences** documentaires (74/297 courant dans `AGENTS.md` ;
   66/264 présenté comme courant dans `ROADMAP.md` §A et `OPEN_QUESTIONS.md` Q1)
   sont **corrigées** : la référence **courante** est désormais **75 cas / 298
   assertions** pour R0, et le **contexte** distingue explicitement l'état
   **P3.3** (66/264).
2. **Aucun résultat de test inventé** : la référence 75/298 s'appuie sur la
   validation R0 documentée et sur la correspondance binaire/source vérifiée
   (D-6, D-7) ; aucun nouveau passage n'est revendiqué.
3. **Historique préservé** : le rapport P3.3 et les autres rapports gardent leurs
   compteurs d'origine (§E).
4. **Périmètre respecté** : **aucune** modification de code, de test ou de
   configuration **par cette itération** (§D.2) ; `git diff --check` silencieux
   (D-8) ; les modifications préexistantes sont **intactes** (§B.1).
5. **Rapport durable créé et vérifié** (§A–§G), et **indexé** (§C.1).

**Réserves** : F-1 à F-4 (§F) — réserves informatives, sans incidence sur les
corrections.

## H. COMMIT / STAGING

**Aucun commit, aucun staging.** L'état Git du répertoire de travail combine le
travail **R0** (code + tests, préexistant), la documentation **P3.3**
(préexistante) et les présentes corrections **documentaires R0**. Le staging et
la séparation des lots restent à faire sur **autorisation explicite**.
