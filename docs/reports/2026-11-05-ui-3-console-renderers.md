# UI-3 — Renderers console et intégration CLI

**Date :** 2026-11-05
**Branche :** `feat/ui-3-console-renderers`
**SHA de base :** `8c7bbf384e76f38918138e4f807aeac3b43cc04c`
**PR #17 :** fusionnée dans `main` au commit `12cdca8d876b0238d65745dfffdf6ed3e1aab3bd`.
**PR #18 :** fusionnée dans `main` au commit `8c7bbf384e76f38918138e4f807aeac3b43cc04c`.
**État :** travail local non commité; aucun push ni merge effectué.

## Objectif et architecture implémentée

Ajouter le rendu simple et enrichi à partir des événements UI-1, de l'état UI-2 et des capacités terminal, sans changer les décisions ni le protocole de l'agent.

Le chemin actif est `AgentEventObserver → PresentationController → PresentationState → ConsoleRenderer`. La composition CLI choisit automatiquement le renderer à partir de `TerminalCapabilities`. `run_console_turn()` est appelé par `main.cpp` et par les tests d'intégration; il n'imprime jamais la valeur de retour si les événements de streaming ont été rendus. Si l'observateur échoue avant tout delta, cette valeur complète n'est rendue qu'une fois en recours. S'il échoue après le début du streaming, l'interface avertit que le texte affiché peut être incomplet et n'imprime pas une seconde copie complète. Les callbacks historiques ne sont pas fournis au tour CLI, ce qui évite un second chemin d'affichage. Le suivi du texte partiel est réinitialisé à chaque tour, même si l'observateur a été désactivé durablement.

## Revue contradictoire et correction

Défaut confirmé pendant la revue : après la désactivation de l'observateur et une fin de tour qui avait déjà reçu du texte, un échec lors d'un tour ultérieur pouvait consulter le compteur `text_delta_count` du snapshot précédent et annoncer à tort que le nouveau tour avait affiché une réponse partielle. La correction minimale utilise `turn_had_text_`, remis à zéro par `begin_turn()`, pour décider si le diagnostic de l'échec courant doit mentionner du texte partiel. Le test `failure_on_later_turn_does_not_reuse_previous_turn_partial_text` reproduit la projection terminale précédente et vérifie l'absence du faux avertissement.

Un test distinct couvre une exception du renderer pendant un `TextDelta`: si le moteur termine ensuite son tour, le contrôleur signale que le rendu à l'écran peut être incomplet et ne répète pas la réponse complète, afin d'éviter les fragments en double. Si l'observateur échoue avant tout delta, la réponse finale est utilisée une fois comme recours.

## Contrats et rendu

- `ConsoleRenderer`, `PlainRenderer`, `RichRenderer` et `make_console_renderer()` : rendu injectant séparément stdout et stderr.
- `PresentationController` : applique chaque événement UI-1 dans `PresentationState`, rend le texte et les événements d'exécution, et classe les réponses comme fin technique sans déclarer la réussite fonctionnelle.
- `run_console_turn()` : présente les erreurs `HttpError`, `AgentError`, exceptions standard et non standard après réception des événements terminaux.
- `TerminalTextSanitizer` : conserve son état entre fragments, filtre les séquences ESC/CSI, OSC, DCS, SOS, PM, APC et C1, rend visibles les contrôles C0/C1, l'UTF-8 invalide et les contrôles bidi. Les caractères UTF-8 ordinaires et les sauts de ligne sont conservés. Les séquences interrompues sont supprimées à la frontière de fin de rendu.

`PlainRenderer` n'émet aucun ANSI; le modèle passe sur stdout et les diagnostics sur stderr. `RichRenderer` garde le texte conversationnel dans la couleur normale et ajoute des indicateurs sémantiques sur stderr. Les couleurs sont émises seulement si stderr est ANSI et que `NO_COLOR` est absent. La sélection Rich exige stdout TTY et ANSI; `TERM=dumb` et stdout redirigé sélectionnent Plain. Les marqueurs d'interface utilisent des glyphes Unicode seulement si la capacité stderr l'indique, sinon ASCII. La largeur étroite utilise un espacement compact; une largeur inconnue reste inconnue.

Aucun spinner, pourcentage, ETA, débit de tokens, logger, commande REPL, nouvelle dépendance ou changement de permission n'est ajouté. Une fin de tour affichée signifie seulement que le protocole agentique a abouti; le texte rappelle que la réussite fonctionnelle n'est pas vérifiée.

## Fichiers

- Modifiés : `CMakeLists.txt`, `src/main.cpp`, `tests/CMakeLists.txt`, `docs/README.md`.
- Nouveaux headers : `include/ui/console_renderer.hpp`, `console_session.hpp`, `plain_renderer.hpp`, `presentation_controller.hpp`, `rich_renderer.hpp`, `terminal_text_sanitizer.hpp`.
- Nouvelles implémentations : `src/ui/console_renderer.cpp`, `console_session.cpp`, `plain_renderer.cpp`, `presentation_controller.cpp`, `rich_renderer.cpp`, `terminal_text_sanitizer.cpp`.
- Tests : `tests/ui_renderer_test.cpp`, groupe CTest `ui3`.
- Rapport : `docs/reports/2026-11-05-ui-3-console-renderers.md`.

`main.cpp` conserve l'initialisation Landlock, `PromptReader`, `:multi`, `:end`, `exit`, EOF, l'ordre des prompts et l'appel unique `Agent::send()`. Il construit le renderer et le contrôleur, puis appelle le même adaptateur de tour que les tests. Aucun changement dans `src/agent.cpp`, le client HTTP, le parseur SSE, `PromptReader`, les outils workspace, Landlock, la politique `write_file` ou les headers UI-1/UI-2.

## Tests et commandes exécutés

Build propre hors dépôt : `/tmp/local-agent-cpp-ui3-release-build`, GCC 14.2.0, libcurl 8.14.1. La source JSON FetchContent locale existante a été fournie explicitement.

```text
cmake -S . -B /tmp/local-agent-cpp-ui3-release-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-ui3-release-build
./tests/agent_tests --filter=ui3
ctest --test-dir /tmp/local-agent-cpp-ui3-release-build --output-on-failure
./tests/agent_tests
```

Résultats observés sur le code final :

- Configuration et build propres : réussis. Avertissement de développement préexistant CMake CMP0135 seulement.
- Tests ciblés UI-3 : **20/20 cas, 84 assertions, 0 échec**.
- CTest : **9/9 groupes réussis** (`sse`, `workspace`, `http`, `agent`, `ui`, `ui3`, `repl`, `mock`, `sandbox`).
- Harnais complet : **175/175 cas, 1 088 assertions, 0 échec**.
- Les groupes HTTP, agent et mock ont exécuté leurs serveurs loopback. Les tests ont nécessité l'exécution autorisée hors sandbox; l'essai dans le sandbox échouait sur `mock: socket() failed`.

La revue a d'abord fait échouer le nouveau test inter-tours, confirmant le défaut décrit plus haut; après la correction, le test et le harnais complet passent. Les premières itérations de test avaient aussi révélé une fixture C++ où `\x07B` absorbait `B` dans l'échappement hexadécimal, ainsi qu'une assertion rejetant les CSI de couleur internes légitimes et une assertion devenue obsolète après modification du libellé. Fixtures/assertions corrigées, les tests fractionnés CSI/OSC, C1, UTF-8 et de dégradation du renderer passent.

## Couverture, invariants et limites

Les tests ciblés couvrent Plain sans ANSI, Rich/color, `NO_COLOR`, `TERM=dumb`, stdout redirigé, stdout/stderr séparés, Unicode/ASCII, largeur étroite/inconnue, commandes de contrôle fractionnées, contenus hostiles, tool échoué puis récupéré, erreur HTTP, SSE incomplet, non-duplication des deltas, statut technique sans succès fonctionnel, ordre des statuts, chemin `PromptReader` vers l'adaptateur de tour, contenu de prompt conservé et fallback après défaillance d'observateur. La suite complète comprend les tests UI-1/UI-2, REPL et sandbox.

Les heuristiques terminal restent celles de UI-2 : TTY/TERM et locale ne garantissent pas toutes les capacités physiques. Les couleurs Rich sont appliquées aux diagnostics stderr, tandis que le texte modèle reste en couleur normale. Le test d'intégration exerce le `PromptReader` et l'adaptateur réellement utilisé par `main.cpp` avec le serveur mock; il ne lance pas un processus `agent` complet avec un terminal/LLM externe. Aucune vérification manuelle avec un serveur LLM réel n'a été faite.

## Vérification finale et verdict

Le diff final est limité aux renderers, à l'adaptateur de tour utilisé par la CLI, à CMake, aux tests et à cette documentation. Les fichiers protégés et la politique `overwrite:true` restent inchangés. `git diff --check` et `git diff --cached --check` ont réussi après l'ajout du rapport et de son index.

**Verdict : VALIDÉ pour le périmètre UI-3 et les tests exécutés.** Les changements demeurent non commités et non publiés sur `feat/ui-3-console-renderers`. Aucun travail UI-4 n'a commencé.
