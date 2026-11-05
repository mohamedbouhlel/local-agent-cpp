# UI-3 — Revue finale, validation et publication

**Date :** 2026-11-05
**Branche :** `feat/ui-3-console-renderers`
**SHA de base UI-3 :** `8c7bbf384e76f38918138e4f807aeac3b43cc04c`
**SHA `origin/main` vérifié après fetch :** `8c7bbf384e76f38918138e4f807aeac3b43cc04c`

## Périmètre examiné

Revue du diff complet UI-3 depuis la base : CMake, intégration CLI, renderers, contrôleur de présentation, sanitizer, tests et documentation. Les sources UI-1/UI-2 et les composants protégés ont été lus pour vérifier les contrats consommés. La recherche GitHub n'a trouvé aucune PR dont la branche source est `feat/ui-3-console-renderers` au moment de la revue.

## Constats

- **Conforme :** le chemin de rendu CLI est `AgentEvent → PresentationController → PresentationState → ConsoleRenderer`. Les callbacks historiques ne sont pas fournis par ce chemin; les deltas ne sont rendus qu'une fois.
- **Conforme :** les états et messages de fin différencient fin HTTP, échec d'outil et fin technique du tour. Une erreur HTTP ou un flux incomplet ne produit pas le statut de fin technique.
- **Conforme :** le drapeau de texte partiel est remis à zéro par `begin_turn()`. Le test `failure_on_later_turn_does_not_reuse_previous_turn_partial_text` vérifie qu'une erreur ultérieure ne réutilise pas l'état du tour précédent.
- **Conforme :** la défaillance d'observateur avant tout delta utilise la réponse finale une seule fois; après le début d'un delta, le rendu avertit du risque de sortie partielle sans réimprimer la réponse.
- **Conforme :** le sanitizer maintient son état entre fragments, filtre ESC/CSI/OSC/DCS et autres chaînes de contrôle, échappe les C0/C1 dangereux, les contrôles bidi et l'UTF-8 invalide. Les caractères UTF-8 ordinaires et LF sont conservés. `finish()` abandonne les séquences terminales incomplètes.
- **Couverture ajoutée :** les tests vérifient maintenant explicitement qu'un OSC et un DCS non terminés sont supprimés en fin de flux, en complément du test CSI incomplet déjà présent. Aucun défaut d'implémentation correspondant n'a été reproduit.
- **Conforme :** la sortie conversationnelle passe par stdout; les diagnostics passent par stderr. Plain n'émet pas d'ANSI. La sélection respecte stdout TTY, `TERM=dumb`, `NO_COLOR`, les capacités stderr et le fallback ASCII.
- **Conforme :** `main.cpp` conserve le chemin PromptReader, l'initialisation Landlock et les marqueurs `:multi`, `:end`, `exit` et EOF. Aucun diff n'affecte l'agent, HTTP, SSE, les outils workspace, Landlock ou la politique `overwrite:true`.

## Fichiers modifiés pour la revue

- Test : `tests/ui_renderer_test.cpp` — deux assertions ajoutées pour OSC et DCS incomplets à la fin du flux.
- Documentation : ce rapport et son entrée dans `docs/README.md`.

Les fichiers d'implémentation UI-3 déjà présents restent ceux décrits dans [`2026-11-05-ui-3-console-renderers.md`](2026-11-05-ui-3-console-renderers.md). Aucun correctif C++ de production supplémentaire n'a été nécessaire pendant cette revue.

## Validation

Build propre dans `/tmp/local-agent-cpp-ui3-final-clean`, GCC 14.2.0, libcurl 8.14.1; source FetchContent JSON locale fournie explicitement. CMake affiche son avertissement de développement existant au sujet de CMP0135.

Commandes exécutées :

```text
cmake -S . -B /tmp/local-agent-cpp-ui3-final-clean -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-ui3-final-clean
./tests/agent_tests --filter=ui3
ctest --test-dir /tmp/local-agent-cpp-ui3-final-clean --output-on-failure
./tests/agent_tests
```

Résultats observés sur le build propre final :

- Configuration et compilation complète : réussies.
- UI-3 ciblé : **20/20 cas, 86 assertions, 0 échec**.
- CTest : **9/9 groupes réussis** (`sse`, `workspace`, `http`, `agent`, `ui`, `ui3`, `repl`, `mock`, `sandbox`).
- Harnais complet : **175/175 cas, 1 090 assertions, 0 échec**.
- Les tests loopback ont été exécutés après autorisation hors sandbox.

`git diff --check` et `git diff --cached --check` sont exécutés au contrôle de publication; les fichiers moteur et sécurité sont également vérifiés comme inchangés.

## Limites et risques résiduels

Aucun processus CLI complet n'a été testé dans un terminal physique, et aucun serveur LLM externe n'a été utilisé. Les tests d'intégration utilisent le serveur HTTP mock et le chemin `PromptReader`/`run_console_turn` commun à `main.cpp`. La détection TTY, `TERM` et locale ne garantit pas le comportement de chaque émulateur ou police terminal.

## Verdict

**VALIDÉ** pour UI-3, les composants et scénarios vérifiés ci-dessus. Publication autorisée par la demande : commit atomique, push sans force et PR brouillon vers `main`; aucune fusion et aucun travail UI-4.
