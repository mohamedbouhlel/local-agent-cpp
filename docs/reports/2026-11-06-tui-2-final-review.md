# TUI-2 — Revue finale du pont asynchrone Agent

## Référence

- Date : 2026-11-06.
- Base de la branche : `71b4c2ce33973aeb0d768cd9313ce1a78788544a`.
- `origin/main` après `git fetch origin main` : `71b4c2ce33973aeb0d768cd9313ce1a78788544a`.
- Branche : `feat/tui-2-real-agent-async-bridge`.
- Worktree : `/tmp/local-agent-cpp-tui2`.
- Rapport d'implémentation conservé : [TUI-2 — pont asynchrone vers `Agent::send()`](2026-11-06-tui-2-real-agent-async-bridge.md).

## Diff examiné

Relecture du contrat, de l'implémentation et des tests du worker, de l'API réelle `Agent::send()`/`AgentEvent`, des deux fichiers CMake modifiés et des rapports TUI-1/TUI-2. Les seuls fichiers du changement sont :

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `include/ui/agent_worker.hpp`
- `src/ui/agent_worker.cpp`
- `tests/agent_worker_test.cpp`
- `docs/reports/2026-11-06-tui-2-real-agent-async-bridge.md`
- `docs/reports/2026-11-06-tui-2-final-review.md`
- `docs/README.md`

Aucun changement à `Agent::send()`, HTTP/SSE, REPL, Landlock, workspace tools, `write_file`, `src/main.cpp` ou ncursesw. Le checkout UI-4 `/home/oper/projects/local-agent-cpp` est resté inchangé durant cette revue.

## Constats contradictoires

### Conforme — worker, résultats et durée de vie

- Un seul thread possède l'Agent, appelle `send()` et détruit l'Agent; le mutex protège la soumission, l'état actif et la fermeture. Le worker ne traite pas deux appels `send()` simultanément.
- Les événements admis sont copiés en valeur et dépilés en FIFO. Le callback n'attend pas la capacité et tente le mutex avec `try_lock`.
- Le futur est indépendant de la FIFO et contient le retour exact de `send()` ou son `exception_ptr`, conservant le type dynamique d'une exception standard ou non standard.
- Un retour normal signifie uniquement que `Agent::send()` a retourné. Il ne prouve pas l'accomplissement fonctionnel de la demande.
- Une réponse finale doit remplacer les fragments incomplets, jamais leur être concaténée. En cas d'exception, aucun texte final n'est fourni. Le pont ne rend pas directement la réponse et ne transforme pas un résultat d'outil en réponse finale.
- Les dépendances non possédées par Agent, telles que le `ToolRegistry` référencé, doivent rester vivantes jusqu'à l'arrêt/join; cette obligation figure dans l'API.
- La fermeture n'annule pas le tour actif et `join()`/destructeur peut attendre le retour d'un HTTP ou d'un handler sans borne de durée globale. Aucun worker n'est détaché.

### Conforme — saturation, ordre et indication de perte

- La FIFO borne le nombre d'événements, la taille comptabilisée des champs variables cumulés et le payload individuel. Le compteur d'événements abandonnés est indépendant de la file.
- Après la première perte d'un tour, les événements suivants de ce tour sont abandonnés; le prochain `RequestStarted` réarme la politique pour le tour suivant. L'ordre des événements admis reste FIFO.
- `TurnCompletion` calcule les deltas des compteurs avant/après le `send()` et expose `event_details_incomplete()` si des événements ou l'observateur ont échoué. La FIFO peut perdre `RequestFailed`/`TurnFinished`; l'issue technique reste dans le futur.
- Le test de surcharge fixe la capacité à quatre événements, vérifie exactement trois pertes (dont `TurnFinished`), conserve le texte final intégral dans le futur et signale la vue de détails incomplète.
- La limite ne borne pas la mémoire RSS totale : allocateur, en-têtes, objets et mémoire temporaire ne sont pas inclus. `try_lock` ne constitue pas une garantie temps réel contre le coût d'allocation/copie.

### Défaut confirmé — appel de `join()` depuis le worker

La revue a identifié qu'un handler d'outil pouvait appeler `AgentWorker::join()` depuis le thread qui exécute ce handler. L'ancienne méthode transmettait cette auto-jointure à `std::thread::join()`, dont l'erreur dépend de l'implémentation standard, sans message contractuel explicite.

Correction minimale : `join()` détecte le thread worker et lève `std::logic_error("agent worker cannot join itself")` avant de changer l'état de fermeture. Le test `self_join_from_tool_handler_is_rejected_without_stopping_worker` exerce le vrai agent et le mock HTTP : l'appel d'outil échoue avec cette exception, le modèle poursuit, le tour retourne normalement et le thread est joint depuis son propriétaire.

### Limite de cycle de vie — destruction depuis le worker

La destruction de l'objet propriétaire depuis son propre worker n'est pas prise en charge. L'objet possède le `std::thread` et `run()` accède à `this`; il ne peut ni joindre son thread courant ni se détruire sans laisser la boucle utiliser un objet détruit. Le contrat public précise désormais que `AgentWorker` doit être détruit depuis un thread propriétaire distinct. Le self-join est explicitement rejeté; la destruction depuis le worker reste un usage interdit, non une opération récupérable.

### Risques résiduels

- Un handler ou une requête qui ne revient jamais peut rendre `join()` et le destructeur bloquants indéfiniment; il n'existe pas d'annulation coopérative ni de timeout global de tour.
- Les méthodes de contrôle de cycle de vie ne sont pas destinées à être jointes concurremment par plusieurs propriétaires. L'architecture TUI prévue leur assigne un propriétaire UI unique.
- Le consommateur doit traiter `event_details_incomplete()` et ne jamais afficher des détails partiels comme complets.
- TSAN exécuté sans diagnostic de course dans ces scénarios ne prouve pas l'absence générale de data races.

## Tests et commandes exécutés

Configuration propre et compilation standard sur GCC 14.2/CMake 3.31 :

```text
cmake -S . -B /tmp/local-agent-cpp-tui2-review-clean -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-tui2-review-clean -j2
```

Build réussi. Le JSON a été réutilisé depuis le cache FetchContent existant; aucun changement de dépendance.

Résultats finaux sur les sources corrigées :

| Validation | Résultat observé |
| --- | --- |
| Groupe TUI-2 ciblé | 11 cas, 106 assertions, 0 échec |
| CTest complet | 10/10 groupes réussis |
| Harnais complet | 186/186 cas, 1 196 assertions, 0 échec |
| Build complet sous ThreadSanitizer | réussi |
| Groupe TUI-2 sous ThreadSanitizer | 11 cas, 106 assertions, 0 échec |
| CTest `tui2` sous ThreadSanitizer | 1/1 réussi |
| `git diff --check` | réussi |

Les tests loopback ont échoué dans l'environnement sandboxé sur `socket()`. Ils ont été relancés après autorisation d'exécution hors sandbox et ont réussi. Aucun serveur LLM externe ni terminal réel n'a été utilisé.

## Verdict

**VALIDÉ pour publication de TUI-2**, avec le défaut de self-join corrigé et testé, et les limites de destruction depuis le worker, de surcharge et d'arrêt documentées. La validation porte sur le pont isolé, pas sur la future TUI ou son comportement utilisateur. Aucune fusion ni intégration TUI-3 n'est incluse dans cette revue.
