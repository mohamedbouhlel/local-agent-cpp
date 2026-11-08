# Correctif V1-PROTO-001 — rejet du JSON SSE malformé

**Date :** 2026-11-08
**Dépôt :** `mohamedbouhlel/local-agent-cpp`
**SHA de base vérifié :** `df44cabbfa3e5d4217f496a3ecd9b23ac6ac4e1a` (`origin/main`)
**Branche :** `fix/v1-proto-malformed-sse`
**Build :** `/tmp/local-agent-cpp-v1-proto-fix-build`

## Statut de publication après le correctif TUI séparé

Le constat de blocage ci-dessous correspond aux exécutions de cette branche avant correction de la course TUI. Le correctif TUI est maintenant disponible dans le commit séparé `1a1f2fc` sur `fix/tui-close-final-drain`; aucun code TUI n'est inclus dans cette branche SSE. La validation de la combinaison des deux changements est requise avant de conclure sur la publication SSE. Les résultats antérieurs restent conservés comme historique de la campagne.

## Cause et reproduction avant correction

Dans `src/agent.cpp`, l'exception `nlohmann::json::exception` levée en décodant une trame SSE était attrapée puis ignorée. Pour le corps `data: {not-json}

data: [DONE]

`, le flux atteignait donc la construction du message assistant vide, son insertion dans l'historique et la fin technique réussie.

Avant toute modification, le probe lié au build de qualification du SHA de base a été relancé. Sortie observée :

```text
first_return_empty=1
second_return=ok
empty_assistant_persisted=1
turns_finished=2
turns_reported_completed=2
requests=2
```

Cela reproduit le faux succès et la contamination du payload du tour suivant.

## Modification appliquée

Lorsqu'une trame `data` ne se parse pas comme JSON :

1. un indicateur local de flux malformé est posé;
2. aucun delta de cette trame ni des trames ultérieures n'est émis ou ajouté à l'accumulateur;
3. le callback libcurl ne laisse pas traverser d'exception et le transport peut finir;
4. après retour du transport et traitement de la dernière trame, `Agent::send()` lève `AgentError("malformed JSON in model stream event")` avant d'accepter le tour.

Le diagnostic est générique et ne reprend ni le payload brut ni l'exception du parseur. `HttpRequestFinished(completed)` conserve son sens transport — le transport HTTP s'est terminé — tandis que `RequestFailed(protocol)` et `TurnFinished(failed)` signalent l'échec du protocole. Un fragment textuel déjà rendu reste provisoire; les renderers CLI et TUI affichent explicitement l'échec et aucun statut de succès n'est émis. L'historique assistant/tool incomplet n'est pas validé.

Les objets JSON syntaxiquement valides mais sans delta exploitable (par exemple `choices` absent/vide ou `content: null`) restent ignorés comme auparavant. Le test correspondant a été ajusté pour ne plus assimiler syntaxe JSON invalide et forme valide mais non exploitable.

## Fichiers concernés

- `src/agent.cpp` — rejet différé et générique de la trame JSON invalide.
- `tests/agent_events_test.cpp` — trois cas : invalide avant `[DONE]`, après delta, et avant un delta ultérieur; assertions sur exception, callback, événements, état de présentation et historique du tour suivant.
- `tests/agent_streaming_test.cpp` — conserve le test des formes JSON valides non conformes sans y mêler le cas désormais rejeté.
- `tests/ui_renderer_test.cpp` — vérifie l'échec CLI/plain après fragment provisoire, l'absence de succès et l'absence du contenu JSON malformé dans le diagnostic.
- `tests/tui_test.cpp` — vérifie sous PTY que la TUI montre le fragment éventuel et une erreur protocolaire, sans succès technique.
- `docs/README.md` — index de ce rapport.

Aucun changement n'a été apporté à `HttpClient`, au parseur SSE, aux tool calls, à Landlock, au REPL ou au checkout UI-4. Les tests existants d'appairage des `tool_call_id` ont passé.

## Vérifications réellement exécutées

Build propre dans un répertoire neuf, configuré en C++20. La source JSON n'a pas été téléchargée : CMake a réutilisé le cache local nlohmann/json v3.11.3.

```text
cmake -S . -B /tmp/local-agent-cpp-v1-proto-fix-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-ghostty-qualification-build/_deps/json-src
cmake --build /tmp/local-agent-cpp-v1-proto-fix-build -j2
```

La configuration et le build ont réussi.

Exécutions ciblées finales :

| Groupe | Cas | Assertions | Résultat |
|---|---:|---:|---|
| Agent | 39 | 734 | 0 échec |
| HTTP | 8 | 16 | 0 échec |
| SSE | 18 | 46 | 0 échec |
| Renderer CLI `ui3` | 21 | 93 | 0 échec |
| TUI PTY `tui3` | 20 | 100 | 0 échec |

Commandes ciblées : `agent_tests --filter=agent`, `--filter=http`, `--filter=sse`, `--filter=ui3`, `--filter=tui3`, avec l'exécutable du build ci-dessus.

Validation complète finale :

- `ctest --test-dir /tmp/local-agent-cpp-v1-proto-fix-build --output-on-failure` : **13/13 groupes réussis**, 12,76 s.
- `agent_tests` complet : **252/252 cas, 7 785 assertions, zéro échec**.
- `git diff --check` : réussi.

Une exécution complète intermédiaire a échoué sur l'assertion du test `workspace.cli_late_confirmation_after_expiration_creates_nothing`, qui attend l'apparition du défi alors que son expiration est fixée à 40 ms. Le groupe `workspace` a ensuite passé isolément (48 cas, 206 assertions), et la relance finale complète a passé. C'est une sensibilité temporelle du test existant observée, pas un défaut V1-PROTO-001; aucun changement hors périmètre n'a été fait.

## Couverture et limites

Les trois variantes invalides vérifient le résultat de `Agent::send()`, la catégorie d'échec, le statut terminal, l'état `PresentationState::failed`, le texte provisoire livré au callback le cas échéant, et l'absence d'assistant/tool dans la requête du nouveau tour. Le test renderer valide le diagnostic CLI/plain; le test PTY valide le diagnostic TUI. Les tests existants couvrent le flux valide `[DONE]`, le flux incomplet sans `[DONE]`, `finish_reason="length"`, les tool calls et leurs identifiants.

Pas de nouveau test réel avec LiteLLM, Ghostty ou SSH; les erreurs sont provoquées par le serveur mock local. Le test de fin de flux conserve le comportement du transport complet même si le protocole applicatif échoue. Aucun nouveau protocole SSE ni aucune récupération automatique après trame invalide n'est introduit.

## État Git et préservation

- Base `origin/main` : `df44cabbfa3e5d4217f496a3ecd9b23ac6ac4e1a`.
- Worktree isolé : `/home/oper/projects/worktrees/local-agent-cpp-v1-proto-fix`.
- Modifications limitées au correctif, à ses tests et au rapport/index.
- Le checkout principal UI-4 conserve son HEAD et ses fichiers locaux inchangés.
- Aucun commit, push, PR ou merge.


## Revue finale conditionnelle — 2026-11-08

### Références et périmètre

- `origin/main` et base de branche : `df44cabbfa3e5d4217f496a3ecd9b23ac6ac4e1a`.
- Branche : `fix/v1-proto-malformed-sse`; aucun commit de publication créé.
- Le diff reste limité à `src/agent.cpp`, tests Agent/SSE/renderers/TUI/autorisation et ce rapport avec son index. Aucun changement moteur hors du correctif de parsing SSE; le changement au test d'expiration ne modifie pas le comportement de production.
- Le checkout principal UI-4 a été vérifié inchangé après annulation d'une insertion accidentelle d'include; ses modifications locales préexistantes sont préservées.

### Cause de l'intermittence d'expiration

L'ancien test fixait l'expiration à 40 ms alors que l'appel au provider s'effectue dans une tâche asynchrone. Selon l'ordonnancement, le délai pouvait expirer avant l'entrée effective dans le provider et l'émission du défi; le test attendait pourtant ce défi. Une seconde course existait dans `challenge_from`: le buffer réveillait le test dès le marqueur textuel, avant que les 32 caractères du défi, écrits ensuite, soient présents. Le test attend désormais le marqueur et les 32 caractères complets; le scénario d'expiration laisse 5 s au worker pour démarrer, puis vérifie l'issue après 6 s. Cela conserve l'assertion d'expiration tardive sans rendre l'échec acceptable. Le groupe `workspace` passe après cette synchronisation.

### Flux malformé restant ouvert

Le test `agent.malformed_stream_still_obeys_configured_http_timeout` envoie une trame invalide puis un flux lent et ouvert (1 octet toutes les 30 ms), avec un timeout HTTP total configuré à 1 s. Il vérifie que le timeout transport est remonté comme `HttpError`, que les événements terminaux sont HTTP/échec, qu'aucun succès de tour n'est publié et qu'un tour ultérieur est propre. Ce test ciblé passe. Le parsing malformé ne désactive donc pas la limite de transport configurée.

### Validation finale observée

Build après les changements de tests : `cmake --build /tmp/local-agent-cpp-v1-proto-fix-build -j2` — réussi.

| Vérification | Résultat observé |
|---|---|
| Agent ciblé | 40 cas, 760 assertions, 0 échec |
| Workspace ciblé après synchronisation | 48 cas, 206 assertions, 0 échec |
| TUI3 ciblé | 20 cas, 100 assertions, 1 échec |
| CTest complet | 12/13 groupes; échec `tui3` uniquement |
| Harnais complet avec loopback autorisé | 253 cas, 7 811 assertions, 1 échec |
| `git diff --check` | réussi avant cette mise à jour du rapport; à recontrôler après édition |

L'échec restant est `tui3.closing_during_write_approval_cancels_without_creating_parent`, à `tests/tui_test.cpp:525`: la capture PTY ne contient pas le texte `no file was created`. Les vérifications antérieures sur le binaire baseline au SHA de référence reproduisaient déjà la même assertion; elle ne résulte donc pas de V1-PROTO-001. La PR reste cependant non publiable selon le critère de suite complète réussie. L'exécution directe initiale du harnais sous sandbox a aussi échoué à ouvrir les sockets mock (`socket() failed`); le résultat complet cité ci-dessus provient de la relance avec autorisation loopback.

Les nouveaux tests malformed SSE couvrent l'erreur avant/après delta, l'absence de validation de l'assistant, les événements et le rendu CLI/TUI. Leur groupe Agent passe. Aucune régression du timeout HTTP n'a été observée.

### Décision de publication

Publication conditionnelle non déclenchée : malgré la réussite des tests du correctif, le contrôle complet exigé n'est pas vert. Aucun commit, push ou PR n'a été créé. La limitation bloquante est l'échec PTY préexistant ci-dessus; sa correction ne relève pas du périmètre autorisé V1-PROTO-001 et doit être arbitrée séparément.


## Diagnostic complémentaire — fermeture pendant une approbation TUI

### Reproduction et comparaison

Le groupe `tui3` a été exécuté cinq fois sur le binaire baseline `/tmp/local-agent-cpp-v1-qualification-build/tests/agent_tests` et cinq fois sur le build de la branche corrective. Dans les deux cas, 2/5 exécutions échouent sur la même assertion de présence de `no file was created` dans la sortie PTY; les autres exécutions passent. Le constat est donc intermittent et préexistant à V1-PROTO-001.

Dans les exécutions en échec, les autres assertions du scénario s'exécutent et passent : le processus sort normalement, la boîte d'approbation a été affichée, le répertoire `workspace/nested` n'existe pas, deux requêtes HTTP ont été reçues et le message outil de la seconde requête contient `cancelled`. Cela exclut une approbation accidentelle ou un effet d'écriture observé. Le provider/broker renvoie bien l'annulation; `Agent::send()` transmet un résultat d'outil explicite au modèle.

### Chemin et cause

1. F10 dans le dialogue résout la demande courante comme `cancelled`, puis appelle `request_exit()`; celui-ci ferme le broker et signale au worker de s'arrêter après l'opération active.
2. Le worker réveillé reçoit `cancelled`. `src/agent.cpp` publie `ToolAuthorizationResolved(cancelled)` et ajoute au transcript outil le résultat `write authorization cancelled; no file was created`; il ne passe pas à `invocation->execute()`. Les répertoires parents ne sont donc pas créés.
3. Le gestionnaire TUI de `ToolAuthorizationResolved` ajoute une notice visible (`write_file approval cancelled; no file was created.`) et un diagnostic.
4. `TuiEventLoop::run()` draine les événements au début d'une itération, mais appelle également `collect_completion()` après l'attente clavier. Si le worker termine entre le dernier `drain_events()` et cet appel, `collect_completion()` efface `active_turn_`; comme `closing_` est déjà vrai, la boucle se termine sans dernier drainage/rendu. Les événements finaux restés dans le canal, dont la notice d'annulation, ne sont alors pas présentés. Cette fenêtre correspond aux échecs intermittents observés.

### Classification

- **A — défaut fonctionnel d'annulation/effet de bord : non reproduit.** Les assertions de résultat outil `cancelled` et d'absence du répertoire passent dans les exécutions en échec.
- **B — défaut de présentation : confirmé par le code et la sortie du test.** La notice d'annulation prévue peut être perdue à la fermeture lorsque la complétion est collectée avant le drainage final du canal. Le statut affiché juste après F10 indique que l'application attend l'opération active; le résultat d'annulation n'est pas garanti visible avant restauration du terminal.
- **C — assertion trop stricte : non retenu comme explication suffisante.** L'assertion cherche une formulation équivalente à un événement de notice explicitement construit par l'interface. L'échec correspond à une fenêtre de traitement démontrable, pas à une simple variation de wording.

La même séquence de source TUI est présente sur la baseline; les fréquences observées sont identiques (2/5 et 2/5). La baseline n'a pas été modifiée.

### Verdict et portée

Le test PTY vérifie correctement l'absence d'écriture et le résultat outil, mais révèle en plus un défaut distinct de drainage/rendu lors de la fermeture. La correction minimale serait de garantir le traitement des événements déjà publiés avant la sortie de la boucle ou de présenter explicitement l'annulation avant restauration du terminal, avec un test déterministe. Cela modifierait la TUI de production hors du périmètre V1-PROTO-001; aucune modification de production n'a été appliquée.

**Verdict courant : NON PRÊT POUR PUBLICATION** tant que ce défaut de présentation distinct n'est pas arbitré et corrigé. Le correctif SSE lui-même reste inchangé et ses tests ciblés passent. Aucun commit, push, PR ou merge n'a été effectué.

Le défaut de drainage/rendu TUI a été traité séparément, sans modifier le correctif SSE. Son rapport est `docs/reports/2026-11-08-tui-final-event-drain-on-close.md` sur la branche `fix/tui-close-final-drain`; ses résultats ne sont pas ceux du worktree SSE et les deux changements n'ont pas été fusionnés.

La validation combinée des commits SSE et TUI est à consigner après exécution dans un environnement temporaire; les résultats des branches séparées ne constituent pas cette preuve.
