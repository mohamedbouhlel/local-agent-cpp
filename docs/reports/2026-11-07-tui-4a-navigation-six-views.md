# TUI-4A — Navigation et six vues

**Date :** 2026-11-07
**Branche :** `feat/tui-4a-navigation-views`
**SHA de base :** `738c77792713b5a83e82824b461b3616a9fd550d`
**HEAD de travail :** `738c77792713b5a83e82824b461b3616a9fd550d` (modifications non commitées)

## Objectif

Ajouter à la TUI ncursesw existante six vues sélectionnables, un état explicite de navigation et des projections limitées aux données présentes. Le moteur, le transport et le REPL classique restent hors périmètre.

## Architecture et raccourcis

- `TuiNavigation` détient la page sélectionnée et le focus associé à chaque page.
- `TuiViewData` construit une projection en lecture seule depuis `AgentConfig` et les schémas du `ToolRegistry`.
- `TuiView` rend Conversation, Outils, Métriques, Diagnostics, Configuration et Aide; il conserve transcript, saisie, curseur et offsets de scroll pendant la navigation.
- `TuiEventLoop` traite les touches et continue à consommer les événements du même `AgentWorker`, quelle que soit la vue affichée. Aucun worker supplémentaire n'est créé.

Raccourcis effectivement implémentés : F1 à F6 sélectionnent les vues dans l'ordre indiqué; Échap revient à Conversation; Tab fait cycler le focus; F9 envoie uniquement depuis Conversation; F10 ferme et attend la requête active puisqu'aucune annulation n'est disponible. Flèches haut/bas déplacent la sélection lorsque le menu a le focus et défilent le contenu lorsque la vue a le focus. PageUp/PageDown défilent la vue courante.

Le changement de vue ne soumet aucune requête. Une réponse reçue hors de Conversation met à jour le transcript sans changer la page. Le rendu garde le suivi du bas et les offsets de Conversation. Les zones conversation et saisie sont écrites par ncurses sans limite en octets, afin de ne pas couper un caractère UTF-8; l'affichage métrique dispose d'un libellé ASCII quand Unicode n'est pas annoncé disponible.

## Sources de données et limites

| Vue | Données affichées | Indisponible / limite |
| --- | --- | --- |
| Conversation | Transcript, deltas, saisie multiligne, statut et événements du tour | Le texte partiel reste signalé incomplet à l'échec; aucun verdict fonctionnel n'est déduit |
| Outils | Noms et descriptions des schémas déclarés; nom et statut/durée des exécutions observées | Arguments et résultats omis; aucune exécution n'est inventée |
| Métriques | Message explicite : « Métriques indisponibles — MetricCollector non intégré » | Aucun compteur, durée, pourcentage ou ETA fabriqué |
| Diagnostics | Catégories d'échec et anomalies de séquence/perte observées par l'interface | Pas d'historique d'événements général ni de diagnostic moteur non exposé |
| Configuration | Modèle, origine de l'endpoint expurgée, timeout, présence/absence de clé | Valeur de la clé jamais affichée; chemins, credentials, chemin URL, query et fragment de l'endpoint masqués; workspace déclaré indisponible dans cette vue |
| Aide | Raccourcis ci-dessus | L'aide décrit les interactions réelles de cette version |

Les données externes rendues dans les vues Outils, Diagnostics et Configuration sont neutralisées pour empêcher l'interprétation de contrôles terminal. Le viewport et la saisie restent dans l'instance de `TuiView` au changement de page. Un essai pseudo-terminal couvre la conservation d'un brouillon et du curseur pendant navigation, l'envoi unique après retour à Conversation, la réception de la réponse pendant l'affichage Outils, le redimensionnement et la restauration de l'écran.

## Fichiers concernés

Nouveaux :

- `include/ui/tui_navigation.hpp`, `src/ui/tui_navigation.cpp`
- `include/ui/tui_view_data.hpp`, `src/ui/tui_view_data.cpp`
- `tests/tui_navigation_test.cpp`

Modifiés :

- `CMakeLists.txt`
- `include/ui/tui_event_loop.hpp`, `src/ui/tui_event_loop.cpp`
- `include/ui/tui_view.hpp`, `src/ui/tui_view.cpp`
- `src/ui/tui_application.cpp`
- `tests/CMakeLists.txt`, `tests/tui_pty_driver.cpp`
- `docs/README.md` et le présent rapport

Aucun changement n'a été apporté à `Agent::send()`, HTTP/SSE, Landlock, workspace tools, politique `overwrite:true` ou REPL classique. `MetricCollector` n'a pas été importé.

## Validation exécutée

Environnement : GNU C++ 14.2, CMake, libcurl 8.14.1, ncursesw détecté; build propre final dans `/tmp/local-agent-cpp-tui4a-final-build`.

Configuration propre :

```text
cmake -S . -B /tmp/local-agent-cpp-tui4a-final-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-tui4a-final-build -j2
ctest --test-dir /tmp/local-agent-cpp-tui4a-final-build --output-on-failure
/tmp/local-agent-cpp-tui4a-final-build/tests/agent_tests
```

La configuration et la compilation complètes depuis un répertoire neuf ont réussi. CMake a émis son avertissement développeur existant CMP0135 (option `DOWNLOAD_EXTRACT_TIMESTAMP` non fixée); il n'a pas empêché la configuration ni le build.

La première exécution CTest dans le sandbox a échoué pour les cas nécessitant le mock loopback (`mock: socket() failed`). Après autorisation d'accès loopback, les mêmes validations ont été rejouées :

| Vérification | Résultat observé |
| --- | --- |
| `ctest --test-dir /tmp/local-agent-cpp-tui4a-final-build --output-on-failure` | 12/12 groupes réussis |
| Harnais `/tmp/local-agent-cpp-tui4a-final-build/tests/agent_tests` | 214 cas, 1 345 assertions, zéro échec après compléments de revue |
| `agent_tests --filter=tui4a` | 15 cas, 99 assertions, zéro échec après compléments de revue |
| `agent_tests --filter=tui3` | 13 cas, 50 assertions, zéro échec |
| `git diff --check` | Réussi avant publication; la revue finale est consignée dans le rapport complémentaire |

Le groupe TUI-4A comprend un test PTY et mock HTTP. Après revue, ce scénario vérifie aussi l'envoi exact d'un brouillon multiligne contenant une ligne d'espaces, le maintien du curseur à travers la navigation et le rendu d'une réponse avec accents et caractères larges. Ces résultats sont des tests automatisés sous pseudo-terminal, pas une nouvelle qualification manuelle de Ghostty. Aucun serveur LLM réel ni terminal physique/SSH n'a été testé pendant cette itération.

## Limites et risques résiduels

- Les raccourcis et le rendu n'ont pas été requalifiés manuellement sous Ghostty dans cette phase.
- La disponibilité des touches de fonction dépend de la description terminfo du terminal; le test PTY couvre le mode utilisé par le driver.
- Les listes de diagnostics et d'activités outil restent conservées en mémoire durant la session; leur bornage n'est pas traité ici.
- La vue Métriques reste explicitement indisponible tant que `MetricCollector` n'est pas intégré et validé sur la branche du produit.
- Une fermeture pendant une requête peut attendre la fin de `Agent::send()`; aucune annulation n'est ajoutée.

## Gouvernance et verdict

Le checkout principal UI-4 conserve ses modifications locales préexistantes; son statut avant/après cette intervention a été vérifié identique. Les changements TUI-4A sont isolés dans ce worktree persistant. Aucun commit, push, merge ou changement du moteur n'a été effectué.

**Verdict : VALIDÉ pour la phase TUI-4A automatisée (build, CTest, harnais et PTY).** La validation manuelle Ghostty et la suite TUI-4B/4C restent hors périmètre.
