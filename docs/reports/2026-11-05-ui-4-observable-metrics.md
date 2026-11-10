# UI-4 — Métriques réelles et progression observable

**Date :** 2026-11-05
**Branche :** `feat/ui-4-observable-metrics`
**SHA de base :** `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`
**Référence vérifiée :** `origin/main` au même SHA après `git fetch origin main`; PR #19 UI-3 fusionnée dans ce commit.
**État Git :** changements locaux non commités et non poussés.

## Objectif et architecture

Ajouter un `MetricCollector` sans dépendance au renderer ou au moteur. `PresentationController` lui transmet les événements après acceptation par `PresentationState`, puis affiche les mesures sur le canal diagnostic existant. Les fragments de réponse ne sont jamais conservés par le collecteur; aucun second chemin de rendu n'est créé.

Le collecteur est réinitialisé à chaque `RequestStarted`. Il reçoit l'ordre synchrone canonique des événements UI-1; il ne mesure pas directement l'horloge système et ne modifie ni Agent, ni HTTP, ni SSE.

## Sources, unités et disponibilité

| Mesure | Source effective | Unité / définition | Disponibilité |
|---|---|---|---|
| Durée totale du tour | `RequestStarted.metadata.elapsed` et `TurnFinished.metadata.elapsed` | différence de durées `steady_clock` produites par Agent | indisponible si une borne manque ou si la durée décroît |
| Durée HTTP par requête | `HttpRequestFinished.duration` | `steady_clock::duration` mesurée autour de `HttpClient::post_streaming()` | indisponible si la requête n'a pas d'événement terminal |
| Durée par outil | `ToolSucceeded.duration` ou `ToolFailed.duration` | `steady_clock::duration` autour de l'exécution du handler | indisponible si aucun résultat terminal n'est observé |
| Latence avant premier delta | différence entre l'elapsed du premier `TextDelta` et `RequestStarted` | durée côté client jusqu'à l'observation par l'agent | absente si aucun delta n'arrive; ce n'est pas le TTFT serveur |
| Requêtes HTTP | nombre d'événements `HttpRequestStarted` du tour | requêtes démarrées observées | compte exact, y compris une requête sans fin observée |
| Appels d'outils | nombre de `ToolStarted` corrélés; succès/échecs issus de `ToolSucceeded`/`ToolFailed` | appels démarrés et résultats de handlers observés | les résultats encore sans événement terminal ne sont pas comptés comme succès ou échec |
| Fragments de texte | nombre d'événements `TextDelta` | fragments, pas tokens | zéro est rapporté seulement après un `RequestStarted` observé |
| Octets de corps HTTP reçus | `HttpRequestFinished.response_bytes`, somme des requêtes terminées | octets reçus par le callback du corps HTTP; ne comprend pas les en-têtes | total indisponible si une requête démarrée n'a pas de mesure terminale ou en cas de débordement |
| État technique du tour | `TurnFinished.outcome` | `completed` ou `failed` selon Agent | indisponible avant `TurnFinished`; ne valide pas la tâche utilisateur |

Les durées restent stockées en `std::chrono::steady_clock::duration`. L'affichage les convertit en millisecondes avec trois décimales. La résolution effective dépend de l'implémentation de `steady_clock`; le format ne prétend pas à une précision matérielle particulière.

Les tokens d'entrée/sortie, tokens par seconde, TTFT serveur, ETA et pourcentage d'avancement sont indisponibles et ne sont pas calculés. Aucun compteur d'itérations n'est utilisé comme progression.

## Présentation

- Au premier `TextDelta`, le contrôleur indique la réception du flux et la durée client mesurée depuis `RequestStarted`, explicitement distinguée du TTFT serveur.
- À chaque `HttpRequestFinished`, il affiche l'ID, la durée, le nombre d'octets de corps reçus et l'issue de transport. Une fin transportale n'est pas présentée comme preuve de complétude SSE/LLM.
- À chaque résultat d'outil, il affiche l'issue technique et la durée observée, sans imprimer le résultat de l'outil.
- À `TurnFinished`, il affiche les compteurs du tour, les durées disponibles et l'issue technique. Le statut completed rappelle que la réussite fonctionnelle n'est pas vérifiée.
- Les informations passent par `render_status` sur stderr. La réponse de l'Agent reste sur stdout; son contenu n'est pas modifié dans le moteur et le renderer applique le sanitizer terminal UI-3. Aucun spinner, ANSI forcé, pourcentage ou débit de tokens n'est ajouté; `NO_COLOR` reste appliqué par le renderer existant.

## Fichiers

- Ajoutés : `include/ui/metrics.hpp`, `src/ui/metrics.cpp`, `tests/ui_metrics_test.cpp`, ce rapport.
- Modifiés : `include/ui/presentation_controller.hpp`, `src/ui/presentation_controller.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `docs/README.md`.
- Aucun changement à `src/agent.cpp`, `HttpClient`, parser SSE, `main.cpp`, REPL, workspace, Landlock ou `write_file`.
- Aucune dépendance externe ajoutée.

## Tests et commandes exécutés

Build propre hors dépôt : `/tmp/local-agent-cpp-ui4-build`, GCC 14.2.0, libcurl 8.14.1; la source JSON FetchContent existante a été fournie explicitement.

```text
cmake -S . -B /tmp/local-agent-cpp-ui4-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-ui4-build
./tests/agent_tests --filter=ui4
ctest --test-dir /tmp/local-agent-cpp-ui4-build --output-on-failure
./tests/agent_tests
```

Résultats observés sur le code final :

- Configuration et build propres : réussis; avertissement de développement CMake CMP0135 uniquement.
- UI-4 : **7/7 cas, 64 assertions, 0 échec**.
- CTest : **10/10 groupes réussis** (`sse`, `workspace`, `http`, `agent`, `ui`, `ui3`, `ui4`, `repl`, `mock`, `sandbox`).
- Harnais complet : **182/182 cas, 1 154 assertions, 0 échec**.
- Les groupes utilisant le mock HTTP loopback ont été exécutés hors sandbox après autorisation.

Les tests couvrent durées de tours réussis/échoués, plusieurs requêtes et outils, outil échoué puis poursuite, delta présent/absent, requête non terminée, corrélations, octets reçus, réinitialisation au tour suivant, métriques indisponibles, absence de tokens/s et pourcentage, rendu une seule fois, sortie Plain redirigée, Rich avec `NO_COLOR` et intégration CLI par le chemin partagé `run_console_turn`.

## Limites et verdict

Les tests déterministes du collecteur utilisent des métadonnées d'événements synthétiques; l'intégration Agent/CLI utilise le serveur mock. Aucun terminal physique ni serveur LLM externe n'a été testé. Les octets rapportés sont les octets de corps vus par le callback libcurl, pas une taille de contenu décodé ni des tokens.

**Verdict : VALIDÉ pour UI-4 et les scénarios testés.** Les résultats sont issus d'événements mesurés; aucun accomplissement fonctionnel n'est déduit. Aucun commit, push, merge ou travail UI-5 n'a été effectué.
