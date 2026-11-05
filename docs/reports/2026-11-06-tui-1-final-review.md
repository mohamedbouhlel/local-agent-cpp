# TUI-1 — Revue finale du prototype asynchrone

**Date :** 2026-11-06
**SHA de base :** `1593fb11d0232ad8efc42c09f03e2c48e26cfe86`
**Branche :** `feat/tui-1-agent-tui-async-contract`
**Worktree :** `/tmp/local-agent-cpp-tui1`
**Référence distante vérifiée :** `origin/main` restait au SHA de base après
`git fetch origin main`
**Statut :** revue favorable du prototype isolé; aucune intégration produit

## Objectif et périmètre examiné

La revue a relu `AGENTS.md`, SPEC-UI-001 v2.0, les rapports TUI-0, le protocole
TUI-0, les rapports POC-1 à POC-3, le rapport initial TUI-1, puis l'intégralité
des sources et tests sous `poc/tui-async/`. La base, le statut du worktree et
les références distantes ont été vérifiés. L'API de l'Agent fusionné a été
consultée uniquement pour vérifier le contrat réel; aucun composant produit
n'a été modifié.

Le checkout `/home/oper/projects/local-agent-cpp` demeure sur
`feat/ui-4-observable-metrics`, SHA `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`,
huit commits derrière le `origin/main` observé. Ses modifications UI-4
préalablement présentes sont restées identiques; elles n'ont pas été importées
ni touchées.

## Constats contradictoires

### Conforme — worker et durée de vie

- Un thread persistant construit, utilise et détruit un seul `TurnExecutor`.
- `submit()` limite le worker à un tour en vol; le worker est réutilisable après
  retour ou exception de `send()`.
- L'entrée acceptée est possédée (`std::string` déplacée dans `Task`); les
  événements acceptés sont copiés par valeur avant le retour du callback.
- Le worker n'est jamais détaché. `request_close()` ferme l'admission; le
  travail déjà accepté finit; `join()` attend le thread.
- Le futur est résolu avec `TurnCompletion` pour un retour normal ou une
  exception. `std::exception_ptr` conserve le type dynamique et la valeur
  propagée par l'exécuteur.
- Le destructeur ferme puis joint; un test vérifie que l'exécuteur est détruit
  sur le thread worker avant le retour de `join()`.

### Conforme — file et surcharge

- La file vérifie les maxima d'événements, de charge utile variable cumulée et
  de payload individuel avant la copie; la prise du mutex producteur est
  `try_lock`.
- Les événements acceptés sont dépilés dans l'ordre FIFO.
- Au premier overflow, conflit de verrou, payload trop grand ou exception de
  copie/allocation, le compteur augmente et le producteur abandonne le reste
  des détails portant le même `TurnId`. Un nouveau `RequestStarted` réinitialise
  ce latch pour le tour suivant.
- Le test de surcharge établit le décompte exact : 3 événements acceptés,
  1 000 perdus, dont `RequestFailed` et `TurnFinished`. Après drainage de la
  file, le tour suivant repart avec zéro perte, ce qui vérifie l'absence de
  contamination du compteur et du latch.

### Conforme, garantie limitée — issue technique

Le futur reste disponible indépendamment de la file, même lorsque les deux
événements terminaux sont perdus. Le prototype distingue `returned` et
`exception`; l'exception originale est conservée. Dans l'Agent actuel, les
chemins d'échec de `Agent::send()` publient un `TurnFinished(failed)` puis
relancent l'exception; un retour normal et un échec technique sont donc
distinguables par l'adaptateur qui appelle cette API.

Le futur ne transporte toutefois pas le payload `TurnFinished`, le code
`RequestFailureCategory` ni l'issue de chaque opération perdue. Sous surcharge,
il permet de déclarer les détails incomplets et de distinguer retour/exception,
pas de reconstruire ces détails. `returned` ne constitue jamais une preuve
d'accomplissement fonctionnel de la demande utilisateur.

### Conforme — arrêt et soumissions

- Les soumissions en vol et après fermeture sont refusées.
- La fermeture d'un worker inactif est couverte.
- Pendant un tour actif, `request_close()` ne demande pas d'annulation; le test
  maintient le send bloqué par une promesse jusqu'à sa libération, puis vérifie
  le résultat et la jointure.
- Aucun détachement n'est fait. La destruction libère l'exécuteur sur le worker.

### Défauts confirmés et corrigés

1. Le compteur de pertes était seulement vérifié par une borne inférieure, et
   la remise à zéro après saturation n'était pas éprouvée. Le test vérifie
   maintenant exactement 1 000 pertes, draine, puis vérifie zéro perte au tour
   suivant.
2. Aucune exception non standard n'était testée. Un scénario lance `int(73)`,
   vérifie sa conservation dans l'`exception_ptr`, puis réutilise le worker.
3. La destruction des ressources sur le bon thread n'avait pas de test direct.
   Un probe vérifie le thread de destruction avant le retour de `join()`.
4. Le test de soumission concurrente utilisait une temporisation pour maintenir
   artificiellement le worker actif. Elle a été remplacée par une promesse et
   une attente déterministe.
5. Une première version du test producteur/consommateur ajoutée pendant la
   revue dépendait du scheduling avant le premier événement. Un rejeu a
   reproduit l'échec; une barrière `first_published` / `first_consumed` la rend
   déterministe. Le test final a ensuite passé dix répétitions consécutives
   avant le dernier ajout du test de destruction, puis la suite finale complète.
6. **Défaut de code confirmé :** si `EventObserver::on_event()` échouait, le
   publisher désactivait l'observateur et appelait `on_observer_failure()`.
   Ce compteur augmentait, mais `TurnCompletion::event_details_incomplete()`
   et `EventChannelSnapshot::details_incomplete` ne considéraient que les
   événements rejetés par la file. Reproduction : l'observateur synthétique
   échoue sur `TextDelta`; `observer_failures == 1`, aucun rejet de file, puis
   les événements suivants ne sont plus transmis, alors que le résultat
   indiquait les détails complets. Les deux indicateurs incluent désormais
   `observer_failures != 0`; le test d'observateur vérifie explicitement le
   résultat de tour et le snapshot.

Le défaut corrigé reste limité au prototype. Aucun composant de production n'a
été modifié.

## Garanties démontrées et non démontrées

**Démontré par le code et les scénarios synthétiques :** single-flight,
réutilisation après exception, ordre FIFO des événements acceptés, pertes
comptées, abandon du reste du tour après perte, issue retour/exception hors
bande, rejection après fermeture, et destruction joinée.

**Non démontré :** intégration avec une instance réelle `Agent`, transport
HTTP/SSE ou callbacks historiques en activité; absence universelle de data
race; comportement d'une allocation système bloquée; plafond RSS total; délai
maximal de `join()`; annulation; détails terminal structurés dans le futur;
comportement sous perte de processus ou panne matérielle.

L'usage de `try_lock` évite d'attendre la capacité de la file, mais les
allocations, mutex et atomiques ne donnent aucune garantie lock-free ou temps
réel. Les limites d'événements/payload ne bornent pas l'overhead des objets,
l'allocateur, les piles ou le RSS.

## Fichiers modifiés pour la revue

- `poc/tui-async/tests/worker_tests.cpp` — cas et assertions supplémentaires,
  synchronisation déterministe.
- `docs/reports/2026-11-06-tui-1-final-review.md` — présent rapport.
- `docs/README.md` — index du rapport.

Les sources du prototype et le rapport initial restent conservés. Aucun fichier
du produit, CMake racine, `Agent::send()`, `AgentEvent`, présentation, HTTP/SSE,
REPL, Landlock, outil workspace ou politique `write_file` n'a été modifié.

## Validation exécutée

Build propre standard :

```text
cmake -S poc/tui-async -B /tmp/tui-async-final-clean              PASS
cmake --build /tmp/tui-async-final-clean -j2                      PASS
ctest --test-dir /tmp/tui-async-final-clean --output-on-failure   PASS (1/1)
/tmp/tui-async-final-clean/tui_async_tests                         PASS
9 tests, 65 checks
```

ThreadSanitizer :

```text
cmake -S poc/tui-async -B /tmp/tui-async-final-tsan \
  -DTUI_ASYNC_ENABLE_TSAN=ON                                      PASS
cmake --build /tmp/tui-async-final-tsan -j2                       PASS
ctest --test-dir /tmp/tui-async-final-tsan --output-on-failure    PASS (1/1)
/tmp/tui-async-final-tsan/tui_async_tests                         PASS
9 tests, 65 checks
```

La version finale inclut la correction de l'incomplétude de l'observateur.
TSAN est un signal sur ces scénarios exécutés, pas une preuve générale
d'absence de data races.

## Publication et verdict

Le worktree est basé sur `origin/main` sans changement distant observé. Après
reprise avec accès réseau, `gh pr list --head
feat/tui-1-agent-tui-async-contract` a retourné une liste vide; aucune PR
existante n'a donc été trouvée avant la publication.

**VERDICT : REVUE FAVORABLE — tests réussis.** Ce prototype reste isolé et ne
justifie pas l'intégration ncursesw ni le démarrage de TUI-2. Le commit et la
PR restent réservés à cette étape de publication autorisée.
