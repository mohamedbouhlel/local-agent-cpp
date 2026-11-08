# Correction TUI — drainage final des événements à la fermeture

**Date :** 2026-11-08
**Dépôt :** `mohamedbouhlel/local-agent-cpp`
**Base :** `df44cabbfa3e5d4217f496a3ecd9b23ac6ac4e1a`
**Branche isolée :** `fix/tui-close-final-drain`
**Worktree :** `/home/oper/projects/worktrees/local-agent-cpp-tui-close-fix`

## Cause vérifiée

F10 dans le dialogue d'approbation résout la demande courante comme `cancelled`, puis demande la fermeture de la TUI et la fermeture du broker. Le broker réveille le worker. `Agent::send()` reçoit le refus, publie `ToolAuthorizationResolved(cancelled)`, ajoute au transcript outil `write authorization cancelled; no file was created`, ne lance pas le writer et laisse l'agent terminer le tour.

La boucle d'événements drainait le canal au début de l'itération, mais pouvait consommer le futur du worker juste après l'attente clavier. Si le worker terminait entre ces opérations, `active_turn_` était réinitialisé et la boucle de fermeture sortait sans retraiter les événements déjà publiés. Le statut outil et la notice TUI pouvaient donc être omis de l'écran final, bien que l'annulation et l'absence d'effet filesystem soient effectives.

## Correction

Dans `TuiEventLoop::collect_completion()`, une fois le futur prêt et avant sa consommation :

1. vider une dernière fois le canal d'événements;
2. synchroniser le dialogue d'approbation;
3. consommer le résultat du worker et marquer le rendu comme modifié.

La disponibilité du futur signifie que le worker a fini de publier pour ce tour. Le drainage ne bloque pas sur le worker et ne fabrique aucun événement. Il utilise le chemin existant de détection de pertes; les détails évincés ne sont pas présentés comme complets. Le dernier rendu est effectué avant le retour de la boucle, puis `TerminalSession` restaure le terminal par RAII.

Le test PTY `closing_during_write_approval_cancels_without_creating_parent` répète désormais le scénario 30 fois. Chaque répétition vérifie la fermeture normale, la boîte d'approbation, la notification d'annulation dans la sortie PTY, l'absence du répertoire parent, deux requêtes mock et le résultat outil `cancelled` transmis au modèle.

La première exécution CTest après cette correction a exposé une course distincte dans le helper CLI `challenge_from()` : son prédicat attendait le marqueur mais pas la fin des 32 caractères du défi. Le groupe `workspace` a alors lu un défi incomplet. Le test attend maintenant la présence du marqueur et du token complet via sa condition variable. Le scénario d'expiration utilise aussi un délai de 5 s pour laisser le worker atteindre le provider, puis attend jusqu'à l'expiration réelle; aucun comportement de production n'est modifié. Le groupe ciblé `workspace` passe ensuite.

## Comparaison avec la baseline

Sans modifier le checkout baseline, le groupe `tui3` a été répété cinq fois sur le binaire `/tmp/local-agent-cpp-v1-qualification-build/tests/agent_tests` : 3/5 réussites et 2/5 échecs sur la même assertion `run.output.find("no file was created")`.

Le candidat V1-PROTO-001, avant ce correctif TUI, a également donné 3/5 réussites et 2/5 échecs. Dans les exécutions qui échouaient, les assertions d'absence du répertoire parent et du résultat outil `cancelled` passaient. Cela confirme la cause de présentation préexistante et l'absence d'effet d'écriture observé.

## Vérifications après correction

Build propre C++20 :

```text
cmake -S . -B /tmp/local-agent-cpp-tui-close-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-ghostty-qualification-build/_deps/json-src
cmake --build /tmp/local-agent-cpp-tui-close-build -j2
```

Résultats :

| Vérification | Résultat |
|---|---|
| TUI3 ciblé | 19 cas, 323 assertions, 0 échec; les 30 répétitions PTY passent |
| Agent ciblé | 36 cas, 621 assertions, 0 échec |
| SSE ciblé | 18 cas, 46 assertions, 0 échec |
| Workspace/autorisation ciblé | 48 cas, 206 assertions, 0 échec |
| Harnais complet avec loopback | 247 cas, 7 888 assertions, 0 échec |
| CTest complet avec loopback | 13/13 groupes réussis, 28,80 s |
| `git diff --check` | réussi |

Les tests nécessitant des sockets mock ont été exécutés avec accès loopback autorisé. Une exécution CTest antérieure à la synchronisation du helper a échoué uniquement dans `workspace` sur le défi CLI incomplet; elle est consignée ici, puis le helper a été synchronisé et le CTest complet rejoué avec succès. Les essais PTY restent des essais automatisés; aucune qualification Ghostty n'a été effectuée dans cette mission.

## Portée et limites

Fichiers modifiés dans cette branche : `src/ui/tui_event_loop.cpp`, `tests/tui_test.cpp`, `tests/write_authorization_test.cpp` (synchronisation de test uniquement), ce rapport et `docs/README.md`. Le code d'autorisation, `PreparedWrite`, Landlock et le writer ne changent pas. Le correctif SSE reste dans son worktree distinct et n'est ni déplacé ni réécrit.

Le canal d'événements reste borné. Le drainage final récupère les événements encore présents; si des détails ont été perdus, le statut incomplet repose sur le résultat indépendant du worker, sans prétendre reconstruire les événements absents. Trente réussites réduisent le risque de régression observée mais ne constituent pas une preuve mathématique d'absence de toute course.

## Verdict

**PRÊT POUR REVUE.** Le test de fermeture est stable sur les 30 répétitions demandées, les contrôles filesystem et le résultat outil sont vérifiés, et le build/CTest/harness complets passent. Aucun commit, push, PR ou merge n'a été effectué.

## Validation croisée avec V1-PROTO-001

Les deux commits ont été composés dans un worktree temporaire isolé à partir de `df44cabbfa3e5d4217f496a3ecd9b23ac6ac4e1a`. La composition finale, incluant la stabilisation du test du broker, est le commit local `97692037a454eb58e5819245e45157111d3653b1` sur `validation/v1-both-fixes` (branche de validation uniquement).

Le premier CTest combiné a échoué dans `tui2.approval_broker_accepts_only_current_request_and_close_wakes_waiter`: le test sondait `pending_request()` seulement 200 fois avec `yield()`. Le ciblage suivant a passé. Pour retirer cette course de test, les deux branches modifient maintenant ce helper de façon identique : interrogation jusqu'à une échéance bornée d'une seconde avec une pause de 1 ms, et vérification que la demande de fermeture est effectivement en attente. Aucun code du broker n'a changé. Après cette correction, le ciblage TUI-2 a passé (13 cas, 117 assertions), le harnais combiné a passé (253 cas, 8 044 assertions) et CTest a passé 13/13 groupes en 30,18 s.

La composition a eu deux conflits textuels résolus : les lignes distinctes d'index dans `docs/README.md` ont toutes deux été conservées; dans `tests/write_authorization_test.cpp`, les deux branches apportaient le même prédicat attendant le défi complet et les mêmes délais de test, conservés une seule fois. `tests/tui_test.cpp` s'est fusionné automatiquement. Le code de production TUI et SSE touche des fichiers séparés. Cette validation combinée ne constitue pas une fusion et aucun changement n'a été appliqué à `main`.
