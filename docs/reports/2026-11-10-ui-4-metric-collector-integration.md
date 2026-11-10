# UI-4 — Intégration contrôlée de MetricCollector

**Date :** 2026-11-10\
**SHA de base :** `4efc75ff71bebbed62f96359691f140086aa76f5` (`origin/main` vérifié avant la création du worktree)\
**Branche :** `feat/ui-4-metric-collector-integration`\
**Worktree :** `/tmp/local-agent-cpp-ui4-integration`\
**Build propre :** `/tmp/local-agent-cpp-ui4-build`

## Verdict

**GO POUR REVUE.** Le collecteur historique a été récupéré et adapté aux contrats actuels, sans modifier le moteur, le protocole HTTP/SSE, l'autorisation d'écriture, Landlock, le REPL ou le tag technique. Le build propre, le harnais final et les 14 groupes CTest passent. Aucun commit, push, PR, merge ou tag n'a été effectué.

## Provenance et préservation

Le chantier historique `feat/ui-4-observable-metrics` n'a pas été modifié. Son checkout reste au HEAD historique `0df28d21…`, avec les cinq fichiers suivis modifiés et quatre fichiers non suivis inventoriés avant intervention. Les quatre nouveaux éléments récupérables ont été copiés dans le worktree isolé : `metrics.hpp`, `metrics.cpp`, `ui_metrics_test.cpp` et le rapport historique du 2026-11-05. Le rapport historique est conservé comme archive de provenance; ses chiffres concernent son ancien SHA et ne sont pas les résultats de cette intégration.

Le worktree de développement part du SHA `4efc75f…` sur une branche distincte. Les changements historiques préservés comprennent les modifications suivies de `CMakeLists.txt`, `docs/README.md`, `include/ui/presentation_controller.hpp`, `src/ui/presentation_controller.cpp`, `tests/CMakeLists.txt`, et les fichiers non suivis `docs/reports/2026-11-05-ui-4-observable-metrics.md`, `include/ui/metrics.hpp`, `src/ui/metrics.cpp`, `tests/ui_metrics_test.cpp`. Le tag `v1.0.0-technical` n'a pas été modifié.

## Architecture réellement intégrée

`MetricCollector` consomme les événements `AgentEvent` côté présentation et ne dépend ni du moteur ni du renderer. Il est appelé par :

- `PresentationController`, après acceptation de l'événement par `PresentationState`, pour le chemin CLI classique;
- `TuiEventLoop`, sur le thread UI, lors du drainage des événements du worker. Les pertes signalées par `EventChannel`, les incohérences de présentation, les échecs d'observateur du worker et les événements terminaux absents rendent le snapshot incomplet.

`TuiView` reçoit un snapshot et projette les mesures dans F3. Les événements d'approbation restent traités par les branches déjà présentes du contrôleur et de la TUI; ils ne sont pas comptés comme exécution/succès/échec d'outil. La réponse conversationnelle garde son chemin de rendu unique. Le texte `TextDelta` est compté mais jamais conservé par le collecteur.

Les compteurs sont saturants à `UINT64_MAX`; en cas de saturation, l'interface CLI/TUI marque la valeur exacte comme indisponible. Les détails HTTP et outils sont limités à 64 entrées chacun, en FIFO. Les détails d'outils limitent aussi les ID d'appel conservés à 16 KiB cumulés et les noms à 128 octets par entrée; un détail individuel hors plafond est rejeté et signalé. Les compteurs agrégés restent distincts de la rétention des détails.

## Sources, définitions et affichage

| Valeur | Source réelle | Définition et disponibilité |
|---|---|---|
| Durée du tour | `RequestStarted.metadata.elapsed` → `TurnFinished.metadata.elapsed` | différence de durées monotones émises par Agent; absente si une borne manque ou si le temps décroît |
| Latence avant le premier delta | elapsed du premier `TextDelta` moins elapsed de `RequestStarted` | observation côté client/Agent, pas TTFT serveur; absente sans delta |
| Durée HTTP par requête | `HttpRequestFinished.duration` | mesure déjà émise par Agent autour du transport; indisponible si aucune fin corrélée |
| Octets de corps reçus | `HttpRequestFinished.response_bytes` | octets de corps comptés dans le callback HTTP, hors en-têtes; total indisponible après perte/incomplétude, requête non terminée ou overflow |
| Requêtes HTTP | événements `HttpRequestStarted` et `HttpRequestFinished` | compte d'événements observés; un écart de séquence/corrélation signale PARTIAL |
| Outils | `ToolStarted`, `ToolSucceeded`, `ToolFailed` | appels et résultats techniques observés, corrélés par `RequestId` + `ToolCallId`; l'autorisation n'est pas une exécution |
| Fragments de texte | événements `TextDelta` | nombre de fragments, explicitement pas un nombre de tokens |
| Résultat du tour | `TurnFinished.outcome` | état technique `completed`/`failed`, ne vérifie pas l'accomplissement utilisateur |

Aucun token/s, ETA, pourcentage ou progression estimée n'est produit. Le contrôleur CLI écrit la réponse sur stdout et les états/métriques sur le canal diagnostics existant (stderr selon renderer). F3 affiche les mesures réelles, leurs indisponibilités et l'avertissement `PARTIAL`; avant tout `RequestStarted`, les données sont explicitement indisponibles.

## Revue et adaptations

L'intégration ne pouvait pas recopier directement le contrôleur historique : le contrôleur courant traite déjà `ToolAuthorizationRequested` et `ToolAuthorizationResolved`. Ces branches ont été préservées et le collecteur exclut ces événements des compteurs d'exécution.

La TUI n'utilise pas `PresentationController`; son intégration est donc effectuée dans `TuiEventLoop`, sans second worker ni second canal de rendu. Une revue a renforcé le rapprochement de `HttpRequestFinished` par l'ID de requête, au lieu de décrémenter uniquement un nombre de requêtes en attente. Une fin orpheline rend maintenant les détails incomplets et le total d'octets indisponible. Le statut PARTIAL reste visible sur F3 même si l'événement `RequestStarted` lui-même n'a pas été reçu. Le nom d'outil retenu dans les détails est borné et neutralisé au rendu; aucun argument ni résultat d'outil n'est stocké/affiché.

## Fichiers du changement

- Modifiés : `CMakeLists.txt`, `tests/CMakeLists.txt`, `include/ui/presentation_controller.hpp`, `src/ui/presentation_controller.cpp`, `include/ui/tui_event_loop.hpp`, `src/ui/tui_event_loop.cpp`, `include/ui/tui_view.hpp`, `src/ui/tui_view.cpp`, `tests/tui_navigation_test.cpp`, `docs/README.md`.
- Ajoutés : `include/ui/metrics.hpp`, `src/ui/metrics.cpp`, `tests/ui_metrics_test.cpp`, le présent rapport et le rapport historique copié comme archive.
- Non modifiés : `src/agent.cpp`, client HTTP, parseur SSE, `AgentEvent`, worker, approbation d'écriture, workspace, Landlock, `src/main.cpp` et comportement du REPL.

## Validation exécutée

Configuration propre, dépendance JSON récupérée par le FetchContent déjà défini dans le projet :

```text
cmake -S . -B /tmp/local-agent-cpp-ui4-build -DAGENT_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/local-agent-cpp-ui4-build -j2
./tests/agent_tests
ctest --test-dir /tmp/local-agent-cpp-ui4-build --output-on-failure
```

- Build C++20 propre : réussi (GCC 14.2.0, libcurl 8.14.1, ncursesw détecté). CMake a émis son avertissement de développement CMP0135 lors de FetchContent; configuration et compilation ont terminé avec succès.
- Groupe ciblé `ui4` après dernière modification : **13 cas / 91 assertions / 0 échec**.
- Groupes de non-régression ciblés `ui`, `tui3`, `tui4a`, `tui4b1` : passés; `tui3` inclut les scénarios PTY existants. Le PTY TUI-4A a aussi vérifié que F3 expose les fragments déjà reçus pendant le streaming hors vue Conversation.
- Harnais complet final : **275 cas / 8 196 assertions / 0 échec**.
- CTest complet final : **14/14 groupes réussis** (`sse`, `workspace`, `http`, `agent`, `ui`, `ui3`, `ui4`, `tui2`, `tui3`, `tui4a`, `tui4b1`, `repl`, `mock`, `sandbox`).
- `git diff --check` : réussi.

La première exécution sandboxée des tests loopback a échoué à créer les sockets du mock (`socket() failed`). Les tests concernés ont été rejoués avec accès loopback autorisé; les suites ciblées, le harnais et CTest complets ont alors réussi. Aucun serveur LLM externe ni terminal physique n'a été testé dans cette mission.

## Limites et risques résiduels

- Les plafonds bornent le nombre d'entrées et les octets textuels explicites des identifiants/noms conservés par le collecteur, pas le RSS global, les capacités d'allocation, les copies temporaires, le transcript, le brouillon ou les buffers du renderer.
- Les compteurs d'événements après perte ne sont que des observations partielles. L'indicateur PARTIAL interdit de les interpréter comme un total exact; le total d'octets est supprimé en cas de détail incomplet.
- Les événements d'Agent sont séquentiels dans l'implémentation actuelle. Le collecteur garde néanmoins un tableau fixe de 64 requêtes en attente et vérifie les ID; si cette hypothèse d'exécution évolue, les limites doivent être réévaluées.
- La complétion technique du tour ne constitue pas un succès fonctionnel. Aucun débit de tokens n'est calculé.
- L'essai PTY et les mocks ne remplacent pas une recette terminal physique ou une requête LLM réelle.

## État Git final

Branche `feat/ui-4-metric-collector-integration`, basée sur `4efc75ff71bebbed62f96359691f140086aa76f5`. Les modifications sont non commitées dans ce worktree isolé. Aucun fichier généré n'est ajouté au diff. Le checkout historique UI-4 et le tag technique restent préservés.
