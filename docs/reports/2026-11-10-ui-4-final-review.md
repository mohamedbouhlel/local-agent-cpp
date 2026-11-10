# UI-4 — revue finale de MetricCollector

**Date :** 2026-11-10\
**Branche examinée :** `feat/ui-4-metric-collector-integration`\
**Base et HEAD du candidat :** `4efc75ff71bebbed62f96359691f140086aa76f5`\
**Référence distante vérifiée :** `origin/main` au même SHA\
**Mode de revue :** lecture seule; aucune suite de tests relancée.

## Portée et état Git

Le diff candidat contient dix fichiers suivis modifiés et cinq fichiers non suivis (dont le collecteur, ses tests et deux rapports). Les changements sont limités à CMake, à la présentation CLI/TUI, aux tests et à la documentation.

Le worktree candidat est basé directement sur le SHA de `main` indiqué ci-dessus. Le checkout historique `/home/oper/projects/local-agent-cpp`, branche `feat/ui-4-observable-metrics`, reste à son HEAD historique `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb` avec ses cinq modifications suivies et quatre fichiers non suivis. Aucun fichier de ce checkout n'a été modifié. Le tag `v1.0.0-technical` reste pointé sur `b1d194288b02b5df094953affd763776991d3e0a`.

## Architecture examinée

`MetricCollector` consomme les `AgentEvent` ordonnés, sur le thread de présentation. Il conserve compteurs, durées, issues techniques et détails d'identification bornés; les contenus de `TextDelta`, prompts, réponses, arguments et résultats d'outils ne sont pas stockés (`include/ui/metrics.hpp:71-80`). Les plafonds sont 64 détails HTTP, 64 détails outil, 16 KiB cumulés pour les identifiants d'appels d'outils et 128 octets par nom d'outil (`include/ui/metrics.hpp:16-19`). Ils ne bornent pas la mémoire RSS globale, les copies temporaires, le transcript ni le brouillon.

Le collecteur valide la corrélation de tour et l'ordre de séquence, marque les détails incomplets en cas de trou ou de séquence invalide, limite les requêtes HTTP en attente à la capacité déclarée et ne donne les octets totaux que lorsque les fins de requêtes sont corrélées et les détails complets (`src/ui/metrics.cpp:133-164`, `174-187`, `238-275`, `284-291`). Les événements d'approbation ne sont pas comptés comme des exécutions d'outil (`src/ui/metrics.cpp:278-283`).

Le contrôleur CLI appelle le collecteur après acceptation par `PresentationState`; le chemin d'approbation reste séparé, et le résumé est rendu à la fin technique du tour (`src/ui/presentation_controller.cpp:63-72`, `94-105`, `138-148`). La TUI collecte les événements sur le thread UI. Elle confronte les compteurs de pertes/erreurs observateur de `EventChannel` au début du tour, marque les métriques PARTIAL, puis copie le snapshot à la vue (`src/ui/tui_event_loop.cpp:163-179`, `182-224`). À la disponibilité du futur, elle draine encore les événements avant de lire la complétion et propage le drapeau de détails incomplets (`src/ui/tui_event_loop.cpp:278-320`). F3 affiche l'issue technique, les durées et compteurs observés, signale PARTIAL/détails limités, et précise que les fragments ne sont pas des tokens (`src/ui/tui_view.cpp:426-495`, `587-611`).

## Constats

### Conforme — données et sémantique

- Les durées proviennent des horodatages `steady_clock` des événements; la première latence delta est décrite comme observée côté client, non comme TTFT serveur.
- Un fragment texte n'est jamais assimilé à un token; aucun débit tokens/s, pourcentage ni ETA n'est calculé.
- L'issue technique de `TurnFinished` reste distincte d'une réussite fonctionnelle.
- Les fins HTTP ne terminent pas le tour; requêtes HTTP et outils ont des comptes et issues distincts.
- Les événements d'approbation ne faussent pas le nombre d'outils exécutés.
- Les détails sont bornés, les dépassements signalés, et les compteurs saturent au lieu de déborder silencieusement.
- Les événements d'approbation restent traités par les chemins CLI/TUI; le collecteur n'enregistre pas le contenu de la demande d'écriture.

### Conforme avec réserve de couverture — saturation EventChannel

Le chemin TUI observe les compteurs de pertes du canal et l'état de complétude transmis avec la fin du tour. Le résultat final peut remplacer le rendu partiel lorsque des événements sont perdus, sans concaténer aveuglément le texte final aux deltas partiels (`src/ui/tui_event_loop.cpp:163-179`, `278-320`). Les tests du worker couvrent une saturation et le compte exact d'événements perdus (`tests/agent_worker_test.cpp:290-309`).

Il manque toutefois un test d'intégration unique qui force la saturation réelle d'`EventChannel`, laisse `TuiEventLoop` consommer la complétion puis vérifie PARTIAL dans F3. Les tests UI-4 valident séparément le collecteur incomplet/F3 et le test worker valide la saturation. La jonction est cohérente à la lecture, mais cette couverture n'est pas démontrée de bout en bout.

### Amélioration future — coût des snapshots et redraw

`TuiEventLoop` transmet un snapshot à chaque événement; `TuiView::set_metrics` reconstruit les lignes F3 et préserve l'ancre (`src/ui/tui_event_loop.cpp:213-224`, `src/ui/tui_view.cpp:262-289`). Cela copie les vecteurs de détails et reprojette les lignes à la cadence des événements. Le coût par snapshot est limité par les plafonds, mais aucune mesure de latence/RSS sur flux soutenu n'a été produite pendant cette revue. C'est un coût à mesurer, pas un défaut de réactivité reproduit.

### Cas critiques absents

- Saturation du canal → complétion → indicateur PARTIAL rendu dans F3, dans un test de bout en bout.
- Pertes/discontinuités répétées confirmant que le tour concerné ne redevient pas complet.
- Profil de charge quantifiant copies et projections sous flux dense.

Ce sont des lacunes de preuve; aucun défaut correspondant n'a été reproduit par la revue statique.

## Tests et preuves

Les tests ne sont **pas relancés** pendant cette revue. Résultats précédemment observés sur le candidat :

- UI-4 ciblé : 13 cas, 91 assertions, zéro échec.
- Harnais complet : 275 cas, 8 196 assertions, zéro échec.
- CTest : 14/14 groupes réussis.
- Un premier lancement sandboxé des mocks loopback avait échoué sur `socket() failed`; les exécutions suivantes avec accès loopback autorisé ont réussi.

`tests/ui_metrics_test.cpp` couvre les durées/issues, requêtes et outils multiples, indisponibilité, reset inter-tour, corrélation HTTP, plafonds FIFO, absence de comptage des approbations, F3 PARTIAL, absence d'arguments/résultats sensibles et séparation des sorties CLI. Le test PTY de navigation consulte F3 durant le streaming et vérifie les compteurs de fragments (`tests/tui_navigation_test.cpp:297-300`). Les chiffres ci-dessus sont les résultats antérieurs rapportés, pas une nouvelle exécution.

## Fichiers du candidat examinés

`CMakeLists.txt`; `include/ui/metrics.hpp`; `src/ui/metrics.cpp`; `include/ui/presentation_controller.hpp`; `src/ui/presentation_controller.cpp`; `include/ui/tui_event_loop.hpp`; `src/ui/tui_event_loop.cpp`; `include/ui/tui_view.hpp`; `src/ui/tui_view.cpp`; `tests/CMakeLists.txt`; `tests/ui_metrics_test.cpp`; `tests/tui_navigation_test.cpp`; `docs/README.md`; `docs/reports/2026-11-05-ui-4-observable-metrics.md`; `docs/reports/2026-11-10-ui-4-metric-collector-integration.md`.

## Limites et risques

- La complétude dépend de la livraison/cohérence des `AgentEvent`; la perte déclarée rend les détails partiels, tandis qu'une panne totale d'observateur peut empêcher le CLI de rendre un résumé final.
- Les plafonds portent sur les enregistrements et certaines chaînes, pas sur la mémoire totale du processus.
- Les copies de snapshots peuvent augmenter le coût CPU/allocation; aucune mesure comparative n'est disponible.
- Les tests PTY ne qualifient pas un terminal physique réel.
- Les métriques décrivent l'observation cliente/transport, pas la performance interne du serveur LLM ni la réussite de la tâche utilisateur.

## Verdict

**GO PR avec réserves.** La revue statique ne révèle pas de défaut bloquant confirmé : les sources sont réelles, les limites et états partiels sont explicites, et l'intégration conserve les événements d'approbation. Avant ou dans la revue PR, ajouter idéalement un test d'intégration de saturation EventChannel jusqu'à PARTIAL en F3, ou documenter pourquoi le scénario ne peut pas être exercé par le harness. Mesurer le coût des snapshots sous flux dense avant toute optimisation; les preuves présentes ne justifient pas une refactorisation.

Cette revue n'a modifié ni testé le code de production et n'a effectué aucun commit, push, PR ou merge.
