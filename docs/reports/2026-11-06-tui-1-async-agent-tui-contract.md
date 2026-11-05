# TUI-1 — Prototype du contrat asynchrone Agent / TUI

**Date :** 2026-11-06
**Base :** `1593fb11d0232ad8efc42c09f03e2c48e26cfe86` (`origin/main`)
**Branche :** `feat/tui-1-agent-tui-async-contract`
**Worktree :** `/tmp/local-agent-cpp-tui1`
**Statut :** prototype isolé validé; aucune intégration de production

## Objectif

Éprouver le transfert asynchrone d'événements Agent vers une future boucle UI,
la gestion de surcharge et l'arrêt d'un worker sans modifier l'agent, le REPL,
le transport ou le confinement.

## Précontrôle et faits observés

- `origin/main` et le HEAD de ce worktree sont tous deux au SHA indiqué ci-dessus.
- Le checkout principal reste sur `feat/ui-4-observable-metrics`, au SHA
  `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`, huit commits derrière la référence
  `origin/main` observée ici. Il conserve ses modifications UI-4 non commitées.
- Le statut UI-4 a été contrôlé avant et après le prototype; les listes sont
  identiques : modifications `CMakeLists.txt`, `docs/README.md`, contrôleur de
  présentation et `tests/CMakeLists.txt`, plus les nouveaux fichiers métriques
  et le rapport UI-4. Aucun fichier de ce checkout n'a été modifié par TUI-1.
- Dans le HEAD testé, `Agent::send()` est synchrone et publie `AgentEvent`
  synchroniquement sur son thread d'exécution. `PresentationController` met à
  jour son état et appelle le renderer immédiatement; cet état et le renderer
  ne sont donc pas partageables sans protection avec le worker.
- `AgentEvent` est le type canonique possédé (payload `std::variant`, chaînes
  possédées). Le POC l'inclut directement; il ne crée pas de type événementiel
  miroir.
- `MetricCollector` n'existe pas dans l'arbre `origin/main` à ce SHA. Il est
  présent uniquement dans les modifications locales UI-4, non importées dans
  ce worktree. Aucun contrat de métriques n'a été inventé pour le prototype.

## Architecture et contrat éprouvés

Le POC autonome se trouve dans `poc/tui-async/` et n'est pas référencé par le
CMake racine.

- Le thread UI est prévu propriétaire de l'état de présentation et consomme la
  file avec `EventChannel::try_pop()`.
- Un unique thread worker crée, appelle et détruit son `TurnExecutor`. Dans
  l'application future, ce propriétaire devra créer l'`Agent` dans sa factory;
  cette adaptation n'est pas incluse ni testée ici.
- Un seul tour peut être en vol. `submit()` refuse le deuxième tour tant que le
  premier est actif/en attente et refuse tout nouveau tour après fermeture.
- Les `AgentEvent` acceptés sont copiés par valeur dans une FIFO; le consommateur
  reçoit des événements indépendants de la durée de vie du callback.
- La capacité est bornée par nombre d'événements (128 par défaut), somme des
  charges utiles variables (256 KiB) et taille d'une charge utile (8 KiB).
  L'overhead des objets, de l'allocateur et du processus n'est pas une limite
  RSS garantie.
- `on_event()` prend le mutex avec `try_lock`. En cas de contention, de limite
  atteinte, de payload trop grand ou d'échec de copie/allocation, il marque la
  perte et ne bloque pas en attendant de place. Au premier événement perdu d'un
  tour, les détails restants de ce tour sont abandonnés pour éviter de
  présenter une séquence partielle comme complète. Les pertes sont comptées.
- Il n'existe pas de garantie de conservation de tous les événements terminaux
  dans la file. Le résultat autoritatif est hors bande, dans le futur
  `std::future<TurnCompletion>` : retour de `send()` ou `std::exception_ptr`,
  identifiant de tâche, compte de détails perdus et diagnostics d'observateur.
  Un retour signifie seulement que l'appel d'exécution a retourné; il ne prouve
  pas l'accomplissement fonctionnel de la demande utilisateur.
- `request_close()` ferme l'admission et laisse finir le travail accepté.
  `join()`/destruction attendent le retour réel du worker. Aucun mécanisme
  d'annulation n'est simulé; un appel bloqué peut donc retarder indéfiniment la
  fermeture en l'absence de délai ou d'annulation côté moteur.
- Le POC des erreurs d'observateur emploie un publisher synthétique reflétant
  la politique actuelle de l'Agent (désactivation et diagnostic unique). Il ne
  relie pas l'Agent réel. L'exception de l'exécuteur est capturée puis
  réexposée via le futur sans perdre son type dynamique.

Le callback ne fait pas d'attente applicative de capacité, mais l'utilisation
d'un mutex `try_lock`, d'atomiques et d'allocations ne constitue ni une preuve
de lock-free ni une garantie temps réel.

## Fichiers

- `poc/tui-async/CMakeLists.txt` — projet CMake autonome C++20, option TSAN.
- `poc/tui-async/include/tui_async/agent_worker.hpp` — contrats du canal et du
  worker.
- `poc/tui-async/src/agent_worker.cpp` — FIFO bornée, factory et cycle de vie.
- `poc/tui-async/tests/worker_tests.cpp` — scénarios déterministes/synthétiques.
- `poc/tui-async/README.md` — contrat, limites et commandes de reproduction.
- `docs/README.md` — index du rapport.

Aucun fichier C++ de production, CMake produit, moteur, HTTP/SSE, REPL, outil,
Landlock ou politique `write_file` n'a été modifié.

## Tests exécutés

Build propre et tests normaux :

```text
cmake -S poc/tui-async -B /tmp/tui-async-clean                         PASS
cmake --build /tmp/tui-async-clean -j2                                 PASS
ctest --test-dir /tmp/tui-async-clean --output-on-failure               PASS (1/1)
/tmp/tui-async-clean/tui_async_tests                                    PASS
7 tests, 50 checks
```

Les sept scénarios couvrent l'ordre et la réutilisation, la surcharge et
l'exception terminale, le rejet rapide d'un payload oversized, l'exception
d'observateur, le producteur/consommateur concurrent, la fermeture en activité
et le refus d'une deuxième tâche en vol.

ThreadSanitizer :

```text
cmake -S poc/tui-async -B /tmp/tui-async-tsan -DTUI_ASYNC_ENABLE_TSAN=ON     PASS
cmake --build /tmp/tui-async-tsan -j2                                        PASS
ctest --test-dir /tmp/tui-async-tsan --output-on-failure                     PASS (1/1)
```

Lors du premier lancement TSAN, le test concurrent supposait à tort qu'au moins
un delta serait accepté avant saturation. L'exécution a réfuté cette hypothèse
non déterministe. Le test a été corrigé pour synchroniser le premier événement
accepté avant de lancer le producteur soutenu; TSAN a ensuite passé. Ce résultat
ne prouve pas l'absence de toute data race dans le futur produit ni dans
l'intégration à l'Agent réel.

Le premier build avait également révélé des erreurs de test : comparaison d'un
payload sans opérateur `==`, puis linkage involontaire de `AgentEvent::type()`
(défini dans `agent.cpp`). Le test utilise désormais `std::variant` pour
reconnaître les payloads, évitant de lier le moteur. Ces échecs ont été corrigés
avant la validation finale.

## Limites et risques résiduels

- Seul un `TurnExecutor` synthétique est exercé. La création effective d'un
  `Agent`, le comportement réel des callbacks historiques et la transmission
  des événements par `Agent::send()` ne sont pas testés ici.
- Les scénarios couvrent la perte d'événements, y compris la perte d'un
  `RequestFailed` et d'un `TurnFinished`, et démontrent que le futur garde
  séparément le retour/exception et le nombre de pertes. Ils ne garantissent
  pas la livraison de ces payloads dans la FIFO.
- L'arrêt n'annule pas un send. Une requête réseau sans borne effective peut
  empêcher `join()` et la destruction de se terminer rapidement.
- La file est bornée en nombre et taille comptabilisée des charges utiles, pas
  en RSS totale. Un flux de détails peut être abandonné dès la première perte
  du tour; aucune stratégie de coalescence de `TextDelta` n'est évaluée.
- Le callback d'observateur de l'Agent réel absorbe ses propres erreurs selon
  son contrat; le test d'observateur en échec est un modèle synthétique, pas une
  intégration.
- `MetricCollector` n'a pas été étudié expérimentalement car absent du HEAD
  testé; ses changements locaux UI-4 restent hors de ce worktree.
- Aucun test de terminal, ncursesw, SSH, LLM ou Landlock n'est applicable au
  POC autonome.

## Verdict

**VALIDÉ — prototype isolé uniquement.** Les builds propres et le test TSAN
passent pour le contrat synthétique implémenté (7 tests, 50 checks). Les preuves
soutiennent un modèle à un worker, FIFO bornée à pertes explicites et issue
technique hors bande. Elles ne suffisent pas à lancer l'intégration de
production. Avant cette intégration, il reste à définir la façon de remonter le
résultat futur à la boucle UI, à garantir la durée de vie de l'Agent/registre,
et à arbitrer le comportement UX de fermeture pendant une requête longue.

**Aucun commit, push, merge ni lancement d'une phase ultérieure n'a été
effectué.**
