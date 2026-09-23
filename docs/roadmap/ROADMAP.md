# ROADMAP.md — trajectoire reconstituée et suite logique

> Ce document distingue **trois registres**, jamais mélangés :
> - **FAIT** : vérifié dans le code/diff à HEAD (référence de fichier ou commit).
> - **DÉDUCTION** : inféré de l'historique et de l'état du code, sans intention explicite dans le dépôt.
> - **PROPOSITION** : suggestion de l'audit, non décidée par le projet.
>
> Aucune refonte du code existant n'est proposée ici : l'état des lieux
> ([`PROJECT_CONTEXT.md`](../context/PROJECT_CONTEXT.md), [`PROJECT_HISTORY.md`](../history/PROJECT_HISTORY.md)) précède toute proposition.

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
| **Suite de tests / CI / validation versionnée** | **absente** | `git ls-files` ; `AGENTS.md:18` |

**Aucune fonctionnalité n'est « validée » au sens reproductible depuis le
dépôt.** Les validations ont eu lieu hors dépôt, dans des harnais temporaires
supprimés (voir [`OPEN_QUESTIONS.md`](../questions/OPEN_QUESTIONS.md) Q1).

## B. FAITS — dettes techniques et travaux inachevés (constatés, non jugés)

Classés par preuve, sans priorisation (la priorisation est en §D).

1. ~~`HttpClient::post()` : code mort applicatif~~ — **RÉSOLU (P3.1, décision A :
   conservé)**. Fait inchangé : aucun appelant en production (`grep` : seul
   `post_streaming` est utilisé par `Agent`). Mais l'API est **conservée**
   (délégation à `perform_request()`, partagé) et **déjà couverte par 4 tests**
   (`tests/http_client_test.cpp`). Ce n'est plus une dette ouverte (voir
   [`PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md`](../reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md)).
2. **`SseParser` n'a pas de `flush()`** : un ultime événement non terminé par
   une ligne vide est silencieusement abandonné. Sans impact avec OpenAI (qui
   termine ses événements et envoie `data: [DONE]\n\n`).
3. **Chemins SSE non exercés** : séparateur CRLF et concaténation de
   plusieurs lignes `data:` sont implémentés (`src/sse_parser.cpp`) mais
   aucun test ne les couvre.
4. **`finish_reason` ignoré** : `choices[0].finish_reason` n'est jamais lu ;
   le seul signal d'arrêt exploité est `"[DONE]"` + fin de réponse HTTP.
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
9. **Pas de `CURLOPT_CONNECTTIMEOUT`** : seul `CURLOPT_TIMEOUT` est posé, donc
   un hôte injoignable peut consommer le timeout complet.
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
16. **La valeur du refactor P5 n'est pas encore réalisée** : `agent_core` a
    été extrait (justification inscrite en `47ccea9`/`8f015bf` : faciliter
    tests/harnais) mais aucun test n'existe. `[FAIT + DÉDUCTION]`.
17. **`.env` local** contient un `LLM_API_KEY` non vide (valeur non
    inspectée) : un serveur réel est configuré localement, sans qu'aucun
    serveur/modèle ne soit documenté dans le dépôt.
18. **Le sandbox dépend du répertoire de lancement** : `workspace` est résolu
    par `std::filesystem::absolute("workspace")` (CWD), pas depuis la racine
    du projet. Documenté dans `USAGE.md`, mais c'est une dépendance implicite.

## C. DÉDUCTIONS — trajectoire et logique du projet

- **D1** — La trajectoire suit quatre axes, dans cet ordre : (1) *REPL
  minimal fonctionnel*, (2) *agent capable d'agir sur des fichiers*,
  (3) *robustesse des mutations de fichiers* (atomicité, permissions),
  (4) *qualité de l'interaction* (streaming, feedback). `[DÉDUCTION]` depuis
  la séquence P1 → P2 → P4 → P6.
- **D2** — Le refactor du build (`47ccea9`) a été fait **en préparation de
  tests** : la justification est explicitement écrite dans le commit et dans
  `AGENTS.md`. L'étape suivante logique (une suite de tests) n'a pas été
  engagée. `[DÉDUCTION]`
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

- **P1 (PROPOSITION)** — Créer une cible CMake de tests liant `agent_core`,
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
- **P2 (PROPOSITION)** — Versionner un **mock serveur SSE** minimal (script
  unique, sans dépendance) sous un répertoire dédié, pour rendre la
  validation de bout en bout reproductible. Résout Q1/Q3.

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
- **P6 (PROPOSITION)** — Ajouter `CURLOPT_CONNECTTIMEOUT` (valeur à décider,
  p. ex. une fraction du timeout total). Dette §B.9.
- **P7 (PROPOSITION)** — Décider explicitement : soit `fsync` fichier +
  répertoire avant/après `rename`, soit documenter que la durabilité après
  crash n'est pas garantie. Dette §B.11.
- **P8 (PROPOSITION)** — Ajouter `SseParser::flush()` **ou** documenter
  formellement que le dernier événement non terminé est ignoré. Dette §B.2.

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
P1 + P2  (tests + mock versionné)      ← débloque la vérifiabilité
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
