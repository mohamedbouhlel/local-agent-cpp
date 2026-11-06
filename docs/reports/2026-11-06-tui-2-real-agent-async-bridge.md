# TUI-2 — Pont asynchrone vers `Agent::send()`

## Référence et périmètre

- Date : 2026-11-06.
- Branche : `feat/tui-2-real-agent-async-bridge`.
- SHA de base : `71b4c2ce33973aeb0d768cd9313ce1a78788544a` (`origin/main` vérifié).
- Répertoire isolé : `/tmp/local-agent-cpp-tui2`.
- Objectif : exposer les événements du vrai `Agent::send()` à un consommateur asynchrone, sans intégrer ncursesw ni changer le moteur.

Le checkout principal sur `feat/ui-4-observable-metrics` a été préservé. Ses changements locaux UI-4 n'ont pas été copiés ni modifiés. `src/main.cpp`, `src/agent.cpp`, HTTP/SSE, REPL, Landlock, les outils workspace et la politique `write_file` ne font pas partie des fichiers modifiés dans ce worktree.

## Architecture implémentée

`AgentWorker` possède un unique thread de travail et crée, utilise puis détruit l'`Agent` sur ce thread. Il exécute un seul tour à la fois ; une soumission concurrente ou une soumission pendant la fermeture est refusée par `std::logic_error`. La fabrique reçoit un `shared_ptr<EventObserver>` et doit conserver vivantes jusqu'à `join()` les dépendances non possédées de l'Agent, notamment un `ToolRegistry` référencé.

L'Agent publie son `AgentEvent` canonique UI-1 dans `EventChannel`. Le canal copie les événements acceptés dans une FIFO; `try_pop()` transfère la propriété d'une copie au consommateur. Aucun renderer ni état de présentation n'est appelé depuis le worker.

Le canal limite le nombre d'événements, le cumul des tailles des champs variables pris en charge et la taille d'un événement. Le producteur prend le mutex par `try_lock`; si la capacité est dépassée ou le verrou indisponible, il abandonne l'événement et les suivants du même tour. Le nombre d'événements abandonnés est compté. L'issue technique ne dépend pas de cette file : le `future<TurnCompletion>` conserve le retour exact de `Agent::send()` ou son `exception_ptr`, ainsi que les identifiants connus et les compteurs d'abandon/échec d'observateur.

Les détails perdus rendent `event_details_incomplete()` vrai. Si le tour a réussi, `response` contient le texte final exact renvoyé par le moteur. Le consommateur doit remplacer le texte partiel reconstruit depuis les deltas par ce texte final lorsqu'il choisit cette récupération; il ne doit pas le concaténer aux fragments. Le pont ne réalise pas cette projection UI lui-même. Si le tour échoue, `response` est absent : aucun fragment reçu ne devient une réponse finale.

`request_close()` ferme l'admission, sans annuler `send()`. `join()` ferme aussi l'admission puis attend le worker; le destructeur applique le même ordre. Un tour déjà actif est achevé avant la destruction de l'Agent. L'attente peut durer jusqu'au timeout réseau ou jusqu'au retour d'un handler d'outil; aucune durée maximale ni annulation n'est promise. Aucun thread n'est détaché.

## API et fichiers

- `include/ui/agent_worker.hpp` : `EventQueueLimits`, `EventChannelSnapshot`, `EventChannel`, `TurnStatus`, `TurnCompletion`, `AgentFactory`, `AgentWorker`.
- `src/ui/agent_worker.cpp` : FIFO bornée, comptage des pertes, worker séquentiel, capture d'exception et cycle de fermeture.
- `tests/agent_worker_test.cpp` : dix scénarios d'intégration utilisant le vrai `Agent`, `MockHttpServer` et des handlers d'outils contrôlés.
- `CMakeLists.txt` et `tests/CMakeLists.txt` : ajout du composant à `agent_core` et du groupe CTest `tui2`.
- `docs/README.md` : index du présent rapport.

Aucun delta, prompt, argument d'outil, résultat d'outil, secret ou corps HTTP n'est recopié dans `TurnCompletion`; les événements `TextDelta` existent déjà dans le contrat moteur et leur contenu est seulement mis en file selon les limites de l'événement.

## Vérifications observées

Environnement : CMake 3.31, GCC 14.2, libcurl 8.14.1, C++20; JSON fourni par le cache FetchContent local déjà présent. La première exécution sandboxée des tests HTTP a échoué à `socket()` parce que le sandbox interdit le loopback. Les mêmes tests ont ensuite été autorisés hors sandbox; les résultats ci-dessous proviennent de ces exécutions autorisées.

| Vérification | Résultat observé |
| --- | --- |
| Configuration CMake fraîche, build normal | réussie |
| Groupe ciblé TUI-2 | 10 cas, 102 assertions, zéro échec |
| CTest complet | 10/10 groupes réussis |
| Harnais complet | 185/185 cas, 1 192 assertions, zéro échec |
| Build complet avec ThreadSanitizer | réussi |
| Groupe TUI-2 sous ThreadSanitizer | 10 cas, 102 assertions, zéro échec |
| CTest `tui2` sous ThreadSanitizer | 1/1 réussi |

Les scénarios TUI-2 vérifient : tour réussi et ordre/IDs, HTTP 500, flux SSE sans `[DONE]`, outil réussi, outil échoué puis continuation du modèle, exception non standard propagée par le futur, désactivation après exception d'observateur, saturation avec perte explicite de `TurnFinished` mais conservation de la réponse dans le futur, tours successifs sans confusion d'identifiants/compteurs, fermeture pendant un handler actif, et fermeture inactive par `join()`.

Le test de saturation fixe la capacité à quatre événements : les deux premiers deltas restent disponibles, trois événements ultérieurs sont comptés comme abandonnés (dont `TurnFinished`). Le futur retourne tout de même le texte final complet et marque les détails incomplets. C'est une démonstration de ce scénario déterministe, pas une garantie de mémoire RSS bornée.

## Limites et risques résiduels

- La borne couvre le nombre d'objets mis en file et les chaînes comptabilisées, pas la capacité interne de l'allocateur, le surcoût des objets, les capacités temporaires du moteur ou la mémoire totale du processus.
- `try_lock` évite d'attendre le mutex de la FIFO dans le producteur; les copies et allocations de l'événement ne constituent pas une garantie temps réel stricte.
- Après la première perte d'un tour, ses événements détaillés restants sont abandonnés. Les terminaux peuvent donc manquer dans la FIFO; le `future` reste l'autorité indépendante pour l'issue du `send()` et son exception originale.
- La réponse finale n'est disponible que si `Agent::send()` retourne. Le pont n'invente aucune reconstruction après erreur et ne valide jamais fonctionnellement la réponse du modèle.
- `ToolRegistry` et autres références non possédées par `Agent` doivent rester vivants jusqu'à l'arrêt complet du worker.
- `join()` et le destructeur peuvent bloquer sans limite prédéfinie pendant une requête ou un handler. Il n'y a pas d'annulation coopérative.
- ThreadSanitizer a exécuté ce groupe sans rapport de course dans cet environnement; cela ne prouve pas l'absence générale de data races.
- Les scénarios ne constituent pas une validation d'une UI, d'un terminal réel, d'une annulation, ni de toutes les formes d'exception d'observateur. Aucun serveur LLM externe n'a été utilisé.

## Invariants et verdict

Le diff ne touche ni la logique agentique ni les protocoles SSE/HTTP et n'ajoute aucune permission. Les tests historiques du moteur, du REPL, des outils et du sandbox passent dans le harnais/CTest complet. `Agent::send()` reste synchrone et inchangé; l'asynchronisme est fourni par le composant périphérique.

**Verdict : VALIDÉ pour le pont asynchrone isolé TUI-2, avec les limites de saturation et de fermeture documentées.** Cela n'autorise ni intégration ncursesw, ni intégration à `main.cpp`, ni lancement de TUI-3.
