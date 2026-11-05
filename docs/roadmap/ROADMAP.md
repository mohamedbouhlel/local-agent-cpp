# ROADMAP.md — trajectoire reconstituée et suite logique

> Ce document distingue **trois registres**, jamais mélangés :
> - **FAIT** : vérifié dans le code/diff au **SHA d'observation explicitement identifié** (référence de fichier ou commit).
> - **DÉDUCTION** : inféré de l'historique et de l'état du code, sans intention explicite dans le dépôt.
> - **PROPOSITION** : suggestion de l'audit, non décidée par le projet.
>
> Aucun changement du code existant n'est proposée ici : l'état des lieux
> ([`PROJECT_CONTEXT.md`](../context/PROJECT_CONTEXT.md), [`PROJECT_HISTORY.md`](../history/PROJECT_HISTORY.md)) précède toute proposition.


> **Réconciliation M3 du 2026-11-05 :** sections A/B mises à jour à `add64b893ad447e76fb12b90e266ec8eab2f2956` ; sections C–F conservent les **déductions et propositions historiques** de l'audit. Les anciennes sections A/B sont préservées en annexe et ne définissent plus l'état courant.

## Suivi des écarts documentaires M4/M5 — `2b54563f650e0995603472261ecfa306f53bd654`

- **[RÉSOLU DOCUMENTAIREMENT — M4]** Le `README.md` ne présente plus macOS
  comme plateforme d'exécution prise en charge ; Linux/Landlock est annoncé.
- **[RÉSOLU DOCUMENTAIREMENT — M4]** `AGENTS.md` décrit la troncature par
  **tours complets** et sa section d'architecture ne porte plus le titre
  « Décisions d'architecture (v1) ». Les items historiques **Q6/Q7**
  sont requalifiés dans [OPEN_QUESTIONS.md](../questions/OPEN_QUESTIONS.md).
- **[RÉSOLU DOCUMENTAIREMENT — M4]** La spécification REPL conserve ses
  décisions normatives, mais précise en introduction que le REPL multiligne
  a été implémenté ; ses formulations d'avant implémentation restent des
  archives datées.
- **[FAIT NOUVEAU — PR #10]** La règle Landlock du workspace interdit
  désormais l'accès `LANDLOCK_ACCESS_FS_EXECUTE` au SHA
  `2b54563f650e0995603472261ecfa306f53bd654`. Les autres limites de sécurité, notamment la concurrence
  entre processus, restent distinctes et **non levées** par ce changement.
- **[VALIDATION NON REJOUÉE]** Le code déclare encore 118 cas CTest répartis
  en 7 groupes sous Linux. L'extension d'un test par la PR #10 ne prouve
  pas un nouvel état **7/7 passés** ; le relevé **403 assertions** reste
  rapporté antérieurement.
- **[PROPOSITIONS HISTORIQUES]** Les sections C–F et l'ancienne annexe A/B
  demeurent des documents de proposition ou d'archive ; aucun choix produit
  nouveau n'est approuvé par M5.

## A. FAITS — photographie bornée au SHA `add64b893ad447e76fb12b90e266ec8eab2f2956`

> **Contrôle M3 :** lecture du dépôt GitHub au SHA ci-dessus. Les réussites de tests décrites par les rapports ou PR ne sont **pas** des exécutions M3. Le contexte technique détaillé est dans [PROJECT_CONTEXT.md](../context/PROJECT_CONTEXT.md) (branche M2, non encore fusionnée dans `main` lors de M3).

| Sujet | État et statut de preuve | Sources |
|---|---|---|
| Application CLI | Implémentée : C++20, `agent_core`, `agent`, REPL simple et multiligne | `CMakeLists.txt`, `src/main.cpp`, `src/prompt_reader.cpp` |
| API LLM | POST streaming SSE, reconstruction deltas et appels d'outils, `[DONE]` obligatoire, `finish_reason=length` refusé | `src/agent.cpp`, `src/http_client.cpp`, `src/sse_parser.cpp` |
| Outils | `list_files`, `read_file`, `search_files`, `write_file` ; limites intégrées au code | `src/workspace_tools.cpp`, `src/workspace_search.cpp`, `src/workspace_write.cpp` |
| Confinement | Linux Landlock imposé avant le REPL ; workspace réel (racine non symbolique), opérations nofollow à descripteurs et vérifications de publication | `src/filesystem_sandbox.cpp`, `src/workspace_tools.cpp`, `src/workspace_write.cpp` |
| Tests versionnés | **118 déclarations `TEST(...)`**, **7 groupes CTest sous Linux**, suite activable avec `-DAGENT_BUILD_TESTS=ON` : **FAITS DE CODE VÉRIFIÉS** | `tests/*_test.cpp`, `tests/CMakeLists.txt` |
| Exécution des tests | **403 assertions, 7/7 groupes sans échec : VALIDATION RAPPORTÉE** par `AGENTS.md` et PR #5, **non rejouée en M3** | `AGENTS.md`, PR #5 |
| CI | Aucun workflow GitHub Actions suivi dans le HEAD examiné | Arbre Git distant |
| `HttpClient::post()` | API conservée selon la décision P3.1, tests versionnés ; pas d'appelant dans la production examinée | `src/http_client.cpp`, `tests/http_client_test.cpp`, rapport P3.1 |

**États précédents à ne pas confondre :** à `4f33b62` aucun test versionné ; relevés historiques **66/264**, **75/298**, **106/372**, puis **107/376**. Ils restent exploitables comme jalons historiques mais ne sont plus des compteurs courants. Voir [PROJECT_HISTORY.md](../history/PROJECT_HISTORY.md).

## B. FAITS ET LIMITES — dettes à requalifier sur la base courante

- **[FAIT VÉRIFIÉ]** `src/workspace_search.cpp` limite la recherche à **50 résultats** mais ne borne pas la taille d'une ligne ni le volume total parcouru ; `list_files` n'impose pas de limite explicite au nombre de chemins affichés. Les propositions **P4/P5** restent des propositions, non décidées.
- **[FAIT VÉRIFIÉ]** `src/agent.cpp` conserve des limites de compilation **20 messages** et **5 itérations d'outils**. Leur calibration n'est pas démontrée ; proposition **P9/P16**, non décidée.
- **[FAIT VÉRIFIÉ — après décision Q20]** `write_file` refuse maintenant tout écrasement; son schéma ne présente plus `overwrite`. Les créations sous Linux partent d'un inode `O_TMPFILE` publié par `linkat(AT_EMPTY_PATH)`. Aucun `fsync` n'est effectué : la durabilité après crash n'est pas garantie. La course antérieure d'écrasement est reproduite dans Q20; elle ne s'applique plus au chemin runtime, puisque celui-ci refuse le remplacement.
- **[FAIT VÉRIFIÉ]** Les opérations de fichiers incluent des garde-fous contre les liens symboliques et liens physiques multiples ; **[LIMITATION]** les accès concurrents par d'autres processus du même UID et la fenêtre entre publication et vérification ne sont pas éliminés. La revue sécurité complète est **non vérifiée**.
- **[FAIT VÉRIFIÉ]** Le programme refuse de démarrer hors Linux dans `src/filesystem_sandbox.cpp` ; la mention de macOS dans `README.md` est une dette documentaire M4.
- **[FAIT VÉRIFIÉ]** `workspace/` dépend du répertoire de lancement (`src/main.cpp`).
- **[FAIT VÉRIFIÉ]** La section « Décisions d'architecture (v1) » de `AGENTS.md` comporte encore des formulations historiques incompatibles avec la troncature par tours ; correction documentaire M4.
- **[FAIT VÉRIFIÉ]** Aucun workflow CI ni fichier de licence présent dans l'arbre Git consulté.
- **[FAIT VÉRIFIÉ]** La fonctionnalité REPL multiligne est implémentée ; sa spécification approuvée contient encore une phrase d'avant implémentation, à réconcilier en M4.
- **[NON VÉRIFIÉ]** Modèle/serveur LLM de référence, calibrage des limites, comportement sur environnement réel et résultats nouveaux de compilation/CTest.
- **[RÉSOLU HISTORIQUEMENT]** `SseParser::flush()`, `CURLOPT_CONNECTTIMEOUT`, validation de `[DONE]`, traitement de `finish_reason=length`, maintien testé de `HttpClient::post()` ; ne pas les présenter comme dettes ouvertes.

## C. DÉDUCTIONS — trajectoire et logique du projet

- **D1** — La trajectoire suit quatre axes, dans cet ordre : (1) *REPL
  minimal fonctionnel*, (2) *agent capable d'agir sur des fichiers*,
  (3) *robustesse des mutations de fichiers* (atomicité, permissions),
  (4) *qualité de l'interaction* (streaming, feedback). `[DÉDUCTION]` depuis
  la séquence P1 → P2 → P4 → P6.
- **D2** — Le refactor du build (`47ccea9`) a été fait **en préparation de
  tests** ; une suite versionnée utilise désormais `agent_core`.
- **D3** — La documentation est traitée comme un livrable de première classe :
  presque chaque itération a son commit `docs`. Un prochain cycle serait
  incomplet sans mise à jour de `AGENTS.md`/`USAGE.md`. `[DÉDUCTION]`
- **D4** — Le projet privilégie l'outillage fait main (harnais `/tmp`, mock
  serveur) plutôt que des dépendances de test. Cela découle de
  « Pas de dépendances lourdes » et « Ne pas ajouter de framework externe
  sans le demander explicitement » (`AGENTS.md`). `[DÉDUCTION]` — conséquence
  importante : proposer Catch2 exige un accord explicite du mainteneur.
- **D5** — Le périmètre volontairement **hors** trajectoire, faute de toute
  trace : persistance d'historique entre sessions, RAG/embeddings,
  multi-utilisateur, mode serveur, interface graphique, streaming des
  résultats d'outils au fil de l'eau. Rien dans le dépôt ne les évoque.
  `[DÉDUCTION d'absence]`

## D. PROPOSITIONS — roadmap candidat (aucune n'est décidée)

> Chaque item est une **proposition de l'audit**. Les items de la vague 1 sont
> des prérequis aux suivants. Les dépendances externes nécessitant un accord
> explicite (`AGENTS.md`) sont signalées.

### Vague 1 — Rendre l'existant vérifiable (dépend de D2, D3, D4)

- **P1 (RÉALISÉ)** — Cible CMake de tests liant `agent_core`,
  sans framework externe d'abord (`assert` + un `main` par famille), ou avec
  Catch2/GoogleTest **si et seulement si** le mainteneur l'approuve
  (contrainte `AGENTS.md`).
  Couverture prioritaire, dans l'ordre du risque :
  1. `SseParser` : événement coupé entre deux chunks, plusieurs événements
     par chunk, CRLF, commentaires, multi-`data:`, événement final sans ligne
     vide (documenter le comportement attendu).
  2. `trim_history()` : tours complets, tour surdimensionné non tronqué.
  3. Reconstruction des deltas : `tool_calls` entrelacés par `index`,
     `arguments` fragmentés, fragment JSON invalide ignoré, `arguments` vide
     → `"{}"`.
  4. Invariant anti-partiel : échec mid-stream ⇒ historique sans message
     assistant.
  5. `sandbox_path` / `write_file` : `..`, absolu, répertoire cible, refus
     d'écrasement, atomicité, préservation des permissions.
- **P2 (RÉALISÉ)** — Mock serveur HTTP/SSE versionné dans `tests/`,
  sans dépendance externe, pour rendre la validation de bout en bout
  reproductible. Résout Q1/Q3.

### Vague 2 — Boucher les trous de robustesse (indépendants entre eux)

- **P3 (DÉCIDÉ — option A, P3.1)** — `HttpClient::post()` est **conservé** :
  API publique disponible, **déjà couverte par 4 tests**
  (`tests/http_client_test.cpp`), déléguant à `perform_request()` (partagé avec
  `post_streaming()`). L'absence d'appelant en production est un fait, non un
  motif de suppression. Dette §B.1 **close** ; préparation
  [`PHASE_3_HTTPCLIENT_POST_REPORT.md`](../reports/PHASE_3_HTTPCLIENT_POST_REPORT.md),
  réconciliation
  [`PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md`](../reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md).
- **P4 (PROPOSITION)** — Plafonner `list_files` (parité avec
  `max_search_results`) et supprimer la branche morte. Dettes §B.6, §B.7.
- **P5 (PROPOSITION)** — Borner `search_files` (taille de ligne, taille de
  fichier lue) et documenter la complexité du parcours. Dette §B.8.
- **P6 (FAIT — Phase R0)** — `CURLOPT_CONNECTTIMEOUT` ajouté (borne
  `min(timeout, 10 s)`) ; timeout distingué des autres échecs curl. Dette §B.9.
- **P7 (RÉSOLU DOCUMENTAIREMENT)** — `AGENTS.md` et `USAGE.md` indiquent que
  l'écriture n'est pas garantie durable après crash faute de `fsync`. La
  synchronisation disque n'est pas implémentée.
- **P8 (FAIT — Phase R0)** — `SseParser::flush()` ajouté ; le dernier événement
  non terminé est restitué (l'agent exige en outre `[DONE]`). Dette §B.2.

### Vague 3 — Configuration et cohérence

- **P9 (PROPOSITION)** — Rendre les limites configurables par variables
  d'environnement (`LLM_MAX_TOOL_ITERATIONS`, `LLM_MAX_HISTORY_MESSAGES`,
  `LLM_MAX_READ_CHARS`, …) **ou** documenter pourquoi elles restent des
  constantes. Dette §B.10.
- **P10 (PROPOSITION)** — Trancher la langue des messages d'outil (anglais vs
  français) et l'appliquer uniformément dans `workspace_tools.cpp`. Dette
  §B.13.
- **P11 (PROPOSITION)** — Nettoyer `AGENTS.md` : renommer la section
  « Décisions d'architecture (v1) », réconcilier les deux puces sur la
  troncature, retirer l'espace ligne 92. Dette §B.14.
- **P12 (PROPOSITION)** — Ajouter la licence du projet et un `.clang-format`
  cohérent avec le style déclaré. Dette §B.15.
- **P13 (PROPOSITION)** — Documenter le serveur/modèle réellement ciblé
  (README ne cite que des exemples) et la politique de gestion de
  `LLM_API_KEY`. Dette §B.17, Q4.

### Vague 4 — Extensions (propositions pures, sans indice d'intention)

> Ces items n'ont **aucune** trace dans l'historique. Ils sont listés pour
> être explicitement acceptés ou rejetés, pas parce qu'ils seraient attendus.

- **P14 (PROPOSITION)** — Nouvelles capacités d'outils : `delete_file`,
  `move_file`, `apply_patch`/éditions ciblées, ou mode ajout dans
  `write_file`. Dette §B.12.
- **P15 (PROPOSITION)** — Retour de progression plus riche pendant un cycle
  d'outils (nom + arguments, ou durée), au-delà de
  `[appel de <nom>...]`.
- **P16 (PROPOSITION)** — Configurer/ajuster la fenêtre d'historique selon le
  modèle réel (aujourd'hui 20 messages en dur).

## E. Non-objectifs (à ne pas proposer sans demande explicite)

Faute de toute trace dans le dépôt : persistance d'historique entre sessions,
RAG/embeddings, multi-utilisateur, mode serveur/API, interface graphique,
streaming des résultats d'outils. `[DÉDUCTION d'absence — cf. D5]`

## F. Chemin critique recommandé (proposition)

```
P1 + P2  (tests + mock versionné)      — réalisés
   ↓
P4..P8   (dettes de robustesse, indépendantes ; P3 traité)
   ↓
P9..P13  (config, cohérence, doc)
   ↓
P14..P16 (extensions, sur décision explicite)
```

Justification : tout le reste est plus sûr une fois qu'une suite de tests
versionnée existe, et cette suite est le prolongement direct du refactor
`agent_core` (§D2) — c'est l'étape dont le projet s'est donné les moyens sans
encore la franchir.

---

## Annexe historique — anciennes sections A et B antérieures à M3

> **ARCHIVE UNIQUEMENT.** Les occurrences d'« actuel », « vérifié », et les compteurs **107/376**, **106/372**, **75/298**, **66/264** ci-dessous se rapportent aux états antérieurs des documents, et non au SHA `add64b893ad447e76fb12b90e266ec8eab2f2956`. Ce passage est conservé pour ne pas perdre le contexte des réconciliations précédentes.

## A. FAITS — ce qui est fait et où

| Fonctionnalité | Statut | Preuve |
|---|---|---|
| REPL (bannière, `exit`/EOF, ligne vide ignorée) | implémenté | `src/main.cpp` |
| Config 4 variables env, tous défauts, aucune obligatoire | implémenté | `src/main.cpp` (`load_config`, `timeout_from_env`) |
| POST OpenAI non-streamé | implémenté, **sans appelant en production** — conservé (décision P3.1), couvert par 4 tests | `http_client.cpp:post()` |
| POST streaming + callback par fragment | implémenté | `http_client.cpp:post_streaming()`, `perform_request()` |
| Parsing SSE générique (frame `\n\n`/`\r\n\r\n`, commentaires, multi-`data:`) | implémenté | `src/sse_parser.cpp` |
| Reconstruction du message depuis les deltas (content + tool_calls par `index`) | implémenté | `src/agent.cpp` (`StreamingMessageAccumulator`) |
| Invariant anti-message-partiel en historique | implémenté | `src/agent.cpp` (`append_message` après `post_streaming`) |
| Affichage progressif + indicateur `[appel de <outil>...]` | implémenté | `src/main.cpp` (callbacks) |
| Historique borné, troncature par tours complets | implémenté | `src/agent.cpp:trim_history()` |
| Boucle de dispatch d'outils, plafond 5 itérations | implémenté | `src/agent.cpp:send()` |
| 4 outils sandboxés (`list_files`, `read_file`, `search_files`, `write_file`) | implémenté | `src/workspace_tools.cpp` |
| Sandbox strict (absolu/`..`/évasion rejetés) | implémenté | `sandbox_path()` |
| `write_file` : garde-fou `overwrite`, taille max, écriture atomique, permissions préservées | implémenté | `87eee25`, `116f61f` |
| `agent_core` (lib) + `agent` (exe fin) | implémenté | `CMakeLists.txt`, `47ccea9` |
| Docs `README.md`, `USAGE.md`, `AGENTS.md`, `CLAUDE.md` | implémenté | HEAD |
| Suite de tests versionnée (micro-harnais maison + mock HTTP/SSE) | **réalisée** (P1.1/P2) | `tests/` (11 fichiers) ; `tests/CMakeLists.txt` ; `AGENTS.md` §Test |
| Exécution de la suite : 6 groupes CTest, **107 cas / 376 assertions** après ajout du test de lien sortant | **vérifiée : 0 échec** | `-DAGENT_BUILD_TESTS=ON` + `agent_tests` |
| CI (intégration continue) | **absente** | `git ls-files` (aucun workflow) |

**Correction (état actuel, P1.1/P2/R0).** La suite de tests est désormais
**versionnée** et **exécutable depuis le dépôt** (`-DAGENT_BUILD_TESTS=ON` puis
`ctest`) ; la validation ne dépend plus de harnais temporaires. Elle couvre
notamment le streaming, la troncature d'historique, le sandbox, les outils et
l'écriture atomique.

**Compteurs — ne pas confondre les états.** Le total a évolué : **66 cas /
264 assertions** au relevé **P3.3** (avant R0), porté à **75 cas / 298
assertions** par **R0** (qui ajoute 9 cas). Le chiffre **66/264** ne vaut donc
que pour l'état **P3.3** ; la référence R0 **75/298** est historique ; après `31f3e2a`, `AGENTS.md` documente **106/372 sur 6 groupes**.

**État historique (audit à `4f33b62`).** Aucune suite de tests n'était alors
versionnée et les validations reposaient sur des harnais **hors dépôt**,
supprimés après usage : c'est ce constat, **aujourd'hui périmé**, qui fondait
[`OPEN_QUESTIONS.md`](../questions/OPEN_QUESTIONS.md) Q1. La **CI** reste
**absente** (fait actuel).

## B. FAITS — dettes techniques et travaux inachevés (constatés, non jugés)

Classés par preuve, sans priorisation (la priorisation est en §D).

1. ~~`HttpClient::post()` : code mort applicatif~~ — **RÉSOLU (P3.1, décision A :
   conservé)**. Fait inchangé : aucun appelant en production (`grep` : seul
   `post_streaming` est utilisé par `Agent`). Mais l'API est **conservée**
   (délégation à `perform_request()`, partagé) et **déjà couverte par 4 tests**
   (`tests/http_client_test.cpp`). Ce n'est plus une dette ouverte (voir
   [`PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md`](../reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md)).
2. ~~**`SseParser` n'a pas de `flush()`**~~ — **RÉSOLU (Phase R0)** :
   `SseParser::flush()` restitue désormais la dernière trame non terminée, et
   l'agent exige le marqueur `[DONE]` pour accepter un flux comme complet
   (`AgentError` sinon).
3. **Chemins SSE exercés** : séparateur CRLF et concaténation de lignes
   `data:` sont couverts dans `tests/sse_parser_test.cpp`.
4. **Complétude du flux** : `finish_reason == "length"` est refusé et
   le marqueur `"[DONE]"` est requis (`src/agent.cpp`).
5. **Commentaire obsolète** : `src/agent.cpp:147` mentionne
   `extract_assistant_message()`, fonction supprimée par `a462373`.
6. **`list_files` sans plafond** de nombre de résultats (à comparer aux 50 de
   `search_files`) : la sortie peut être arbitrairement grande et part dans
   le contexte du modèle.
7. **Branche morte dans `list_files`** : `if (entry.is_directory()) {
   continue; } continue;` — les deux branches font la même chose.
8. **`search_files` non borné par fichier/ligne** : lit des lignes entières
   sans limite de longueur, sans garde de taille de fichier, et reparcourt
   tout le workspace à chaque appel.
9. ~~**Pas de `CURLOPT_CONNECTTIMEOUT`**~~ — **RÉSOLU (Phase R0)** :
   `CURLOPT_CONNECTTIMEOUT` est posé (`min(timeout, 10 s)`) et un timeout produit
   un message distinct.
10. **Limites non configurables** : `max_history_messages = 20`,
    `max_tool_iterations = 5`, `max_read_chars`, `max_write_chars`,
    `max_search_results` sont des `constexpr` de compilation.
11. **`write_file` sans `fsync`** avant `rename` : l'atomicité *de visibilité*
    est garantie (rename), la **durabilité après crash** ne l'est pas. `[FAIT
    de code]` ; l'interprétation est en §C.
12. **Pas de mode ajout** dans `write_file` (remplacement complet uniquement).
13. **Mélange de langues** : l'UI est en français (`main.cpp`), les messages
    d'erreur d'outil renvoyés au modèle sont en anglais
    (`src/workspace_tools.cpp`), et l'historique peut donc contenir les deux.
14. **`AGENTS.md` interne incohérent localement** : la section s'intitule
    toujours « Décisions d'architecture (v1) » alors que des révisions ont eu
    lieu ; la puce « Historique … troncature simple (drop des plus anciens) »
    contredit partiellement la puce suivante (tours complets). Ligne 92 :
    espace en fin de ligne.
15. **Aucun outillage de style** : `AGENTS.md` impose un style C++ mais il
    n'existe ni `.clang-format` ni licence.
16. **Le refactor `agent_core` est désormais exercé par la suite de tests** : les tests agent et REPL sont présents dans `tests/`. L'affirmation « aucun test n'existe » est obsolète depuis `31f3e2a`.
17. **`.env` local** contient un `LLM_API_KEY` non vide (valeur non
    inspectée) : un serveur réel est configuré localement, sans qu'aucun
    serveur/modèle ne soit documenté dans le dépôt.
18. **Le sandbox dépend du répertoire de lancement** : `workspace` est résolu
    par `std::filesystem::absolute("workspace")` (CWD), pas depuis la racine
    du projet. Documenté dans `USAGE.md`, mais c'est une dépendance implicite.

