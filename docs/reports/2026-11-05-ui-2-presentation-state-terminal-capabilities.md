# UI-2 — État de présentation et capacités terminal

**Date :** 2026-11-05
**Branche :** `feat/ui-2-presentation-state`
**SHA de base :** `12cdca8d876b0238d65745dfffdf6ed3e1aab3bd` (merge PR #17)
**État :** changements locaux non commités ; aucun push ni merge effectué.

## Objectif

Ajouter une projection d'état pure à partir du contrat UI-1, ainsi qu'une détection des capacités terminal indépendante des renderers. La CLI existante n'est pas modifiée visuellement ; UI-3, les commandes, le logger et les threads d'animation restent hors périmètre.

## Architecture et API créées

- `include/ui/presentation_state.hpp`, `src/ui/presentation_state.cpp` : `PresentationState::apply(const AgentEvent&)` met à jour `PresentationSnapshot` et retourne `EventApplyResult`. États : `idle`, `submitted`, `waiting`, `streaming`, `tool_running`, `completed`, `failed`.
- `include/ui/terminal_capabilities.hpp`, `src/ui/terminal_capabilities.cpp` : `detect_terminal_capabilities()` observe l'environnement et les descripteurs réels ; `terminal_capabilities_from_probe(const TerminalProbe&)` fournit le point d'injection déterministe des tests.
- `CMakeLists.txt` et `tests/CMakeLists.txt` : compilation des deux sources et ajout des tests au groupe CTest `ui`.
- `tests/ui_test.cpp` : tests de transitions, d'erreurs de séquence/corrélation et d'observations terminal injectées.
- `docs/README.md` : index de ce rapport.

`PresentationState` n'appelle aucun composant moteur. Son snapshot conserve les IDs de session/tour/requête et l'outil actif, les compteurs, l'issue HTTP, le statut de requête et l'issue technique du tour. Il ne stocke aucun texte `TextDelta` : seul le nombre de fragments est compté. Aucun verdict de réussite fonctionnelle n'est produit.

## Transitions et validation des événements

- Un flux neuf commence par `RequestStarted` en séquence 1. Un nouveau tour n'est accepté qu'après `completed` ou `failed`, dans la même session, avec un `TurnId` croissant.
- Une requête HTTP doit porter un ID non nul inédit dans la session, et son numéro d'itération doit être consécutif dans le tour. `TextDelta` exige une requête active et non vide ; il place l'état en `streaming` sans retenir son contenu.
- `HttpRequestFinished` doit correspondre à la requête active. Il clôt le transport et ramène l'état à `waiting`; il ne complète pas le tour.
- `ToolStarted` exige une réponse HTTP terminée avec succès, ainsi que des IDs de requête et d'appel cohérents. Un seul handler est actif à la fois. `ToolSucceeded`/`ToolFailed` doivent correspondre à ce handler. Un échec outil incrémente son compteur et retourne à `waiting`; il n'impose pas l'échec du tour.
- `RequestFailed` doit précéder `TurnFinished(failed)` et correspondre à l'ID de la requête courante si une requête a été observée. Une fin `completed` n'est admise qu'après au moins une requête HTTP terminée, sans `RequestFailed`, et avec compteurs conformes aux événements.
- Les événements exigent le schéma 1, des IDs de session/tour non nuls et des séquences cohérentes. Les valeurs d'enum hors contrat et les événements terminalement incohérents sont refusés.

Politique exacte de séquence : séquence égale à la dernière acceptée → `duplicate`; séquence inférieure → `out_of_order`; première séquence différente de 1 ou séquence supérieure à la suivante attendue → `sequence_gap`; transition/corrélation invalide → `inconsistent`. Tous les refus laissent snapshot et curseur de séquence inchangés. Une lacune non corrigée rend les événements suivants hors séquence ; `reset()` efface la projection et attend un nouveau flux de session commençant à 1. L'appelant reçoit toujours un statut et un détail, aucune incohérence n'est ignorée silencieusement.

## Capacités terminal

Les observations stdout et stderr sont indépendantes. Le détecteur lit `isatty`, `TERM`, la présence de `NO_COLOR`, la locale active (`LC_ALL`, puis `LC_CTYPE`, puis `LANG`) et la largeur `TIOCGWINSZ` seulement pour un descripteur TTY. Il ne modifie pas l'environnement et ne lance aucune sonde réseau.

- `stdout_ansi`/`stderr_ansi` sont vrais si le flux correspondant est un TTY et si `TERM` est défini, non vide et différent de `dumb`.
- `no_color_present` est une observation distincte : le futur renderer doit la combiner avec la capacité ANSI pour sa politique de couleur. Aucun choix de renderer ou sortie ANSI n'est réalisé ici.
- Les champs Unicode sont une compatibilité détectée par TTY + TERM exploitable + locale dont le nom indique UTF-8 ; ce n'est pas une preuve physique que l'émulateur affiche tous les glyphes. Le fallback ASCII est demandé lorsque cette condition manque.
- La largeur reste `std::nullopt` si elle n'est pas mesurable ou si la valeur observée vaut zéro. « Étroit » est défini pour cette version par une largeur mesurée inférieure à 80 colonnes.
- SSH n'est pas détecté comme catégorie particulière : les descripteurs et capacités du terminal attaché gouvernent les observations, sans supposer un terminal local.

Cette détection ANSI repose sur les conventions TTY/TERM, sans interrogation terminfo ni nouvelle dépendance ; les valeurs observées ne sont pas une garantie exhaustive des séquences prises en charge par chaque émulateur.

## Fichiers modifiés

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- Nouveaux : `include/ui/presentation_state.hpp`, `src/ui/presentation_state.cpp`, `include/ui/terminal_capabilities.hpp`, `src/ui/terminal_capabilities.cpp`, `tests/ui_test.cpp`
- `docs/README.md`
- `docs/reports/2026-11-05-ui-2-presentation-state-terminal-capabilities.md`

Pas de modification de `Agent::send()`, `HttpClient`, `SseParser`, callbacks, `main.cpp`, `PromptReader`, outils workspace, Landlock ou politique `write_file`.

## Vérifications

Construction propre hors dépôt : `/tmp/local-agent-cpp-ui2-build`, GCC 14.2, libcurl 8.14.1. La source JSON FetchContent locale déjà présente a été sélectionnée explicitement pour éviter un téléchargement supplémentaire.

Commandes :

```text
cmake -S . -B /tmp/local-agent-cpp-ui2-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-ui2-build
./agent_tests --filter=ui
ctest --test-dir /tmp/local-agent-cpp-ui2-build --output-on-failure
./agent_tests
```

Résultats finaux observés :

- Configuration et build complets : réussis. Avertissement de développement CMake CMP0135 ; pas d'échec de configuration ou de compilation.
- Tests UI ciblés : **20/20 cas, 153 assertions, 0 échec**.
- CTest : **8/8 groupes réussis**, dont `ui` et les sept groupes historiques.
- Harnais complet : **155/155 cas, 1004 assertions, 0 échec**. Les tests HTTP loopback existants ont été exécutés.
- Les premières exécutions ciblées ont échoué à cause de fixtures qui attribuaient des séquences suivantes malgré le rejet attendu d'un événement, puis d'un événement HTTP terminal dupliqué dans le scénario. Les fixtures ont été corrigées pour réutiliser la prochaine séquence attendue ; le dernier build, les 20 tests UI et l'ensemble des validations ci-dessus sont réussis. Aucun de ces échecs de test n'est présenté comme un succès.

## Non-régression, écarts et risques

Le diff est limité à l'état de présentation, aux capacités terminal, à leur intégration CMake, aux tests et à ce rapport/index. L'exécutable conserve son rendu actuel. Aucun protocole SSE, historique, REPL, tool call, accès filesystem, Landlock ou permission n'a été modifié. Les capacités sont injectées dans les tests ; le test de détection live ne suppose pas que la machine dispose d'un TTY. Les tests vérifient aussi plusieurs tours dans une session, la réinitialisation des compteurs par tour, et l'absence de fin de tour avec un handler sans événement terminal.

Risques/limites : après un trou ou une transition rejetée, la projection ne se resynchronise pas automatiquement ; l'appelant doit traiter le statut (et réinitialiser pour un nouveau flux complet). L'heuristique UTF-8 ne garantit pas l'affichage réel des glyphes. La capacité ANSI fondée sur TERM ne consulte pas terminfo. Aucune politique de couleur du renderer n'est encore implémentée ; `NO_COLOR` est exposé séparément pour UI-3.

## Verdict

**VALIDÉ pour le périmètre UI-2 et les vérifications effectivement exécutées.** Les changements restent sur `feat/ui-2-presentation-state`, sans commit, push ni merge. UI-3 n'a pas commencé.
