# LOFITERM — Contrat d'échec et historique de `Agent::send()`

Date : 13 novembre 2026
SHA étudié : `912b086e43c20be58f5ddea5f95ebdec53dcdced`
Worktree : `/home/oper/tmp/local-agent-cpp-mission03-agent-failure` (HEAD détaché)

## Verdict

**Aucun défaut de conservation de l'historique ou de faux succès n'a été
reproduit dans les tests Agent déterministes exécutés.** Une réponse assistant
incomplète n'est pas ajoutée à l'historique. Le message utilisateur échoué est
conservé; un appel d'outil déjà réussi et son résultat le sont également.
L'échec ultérieur fait échouer le tour sans annuler les effets déjà produits.

Une contradiction est établie dans le commentaire public de `AgentError` : il
affirme que rien n'a été ajouté à l'historique pour le tour, ce qui est faux même
pour un échec SSE simple (le message utilisateur a déjà été ajouté), et plus
encore après l'exécution réussie d'un outil. Aucun correctif de code ou de
commentaire n'a été appliqué dans cette mission.

Le cas combiné « outil réussi, puis requête de suivi échouée » n'a pas de test
d'intégration dédié dans les tests inspectés. Le comportement est traçable dans
le code, mais son historique exact et sa séquence complète d'événements ne sont
pas démontrés par une assertion de test dédiée. Cela reste une lacune de
couverture, pas un défaut reproduit.

## Portée et méthode

Seuls `include/agent.hpp`, `src/agent.cpp`, les tests Agent et le chemin de test
associé ont été examinés. Les tests exécutés utilisent `MockHttpServer` sur
loopback et des workspaces temporaires; aucun serveur LLM réel n'a été contacté.
Le cas de création de fichier présent dans le groupe Agent s'exécute dans un
`TempDir` de test avec provider d'approbation de test.

Environnement : Debian 13.7, noyau `6.12.115+deb13.1-amd64`, GCC 14.2.0,
CMake 3.31.6, libcurl 8.14.1, ncursesw 6.5.20250216.

## Contrat constaté dans le code

### Échec de transport ou de protocole avant résultat final

Dans `src/agent.cpp:271-274`, `RequestStarted` est publié puis le message
utilisateur est ajouté à `history_` avant la requête. Chaque tentative de
requête prépare un accumulateur assistant local (`:282-293`). Le contenu
assistant n'est ajouté à l'historique qu'après retour normal du transport,
flush SSE et validation de `[DONE]` et de `finish_reason` (`:397-427`). Une
coupure de transport ou un flux sans `[DONE]` interrompt donc le tour sans
ajouter la réponse assistant partielle.

- **Sans `[DONE]` après des fragments texte :** `HttpRequestFinished(completed)`
  signifie que le transport HTTP a fini; le contrôle de complétude lance ensuite
  `AgentError` (`src/agent.cpp:397-417`). Le catch publie
  `RequestFailed(protocol)` puis `TurnFinished(failed)` (`:542-549`). Le
  résultat du mock ne peut pas être confondu avec un succès de tour.
- **Coupure pendant le streaming :** `HttpRequestFinished(failed)` est publié
  lors de l'exception de transport (`:382-395`), puis le catch publie
  `RequestFailed(http)` et `TurnFinished(failed)` (`:536-541`). Le texte déjà
  streamé peut avoir été provisoirement affiché par callback, mais n'est pas
  une réponse finale ni un message assistant historique.
- **Tour suivant :** un nouvel appel à `send()` ajoute son propre message
  utilisateur. Le prochain payload conserve les messages utilisateur
  précédents, sans assistant incomplet. Il ne s'agit pas d'un retry HTTP
  automatique.

### Outils exécutés avant un échec ultérieur

Après validation d'un message assistant complet contenant des tool calls, le
message assistant avec `tool_calls` est ajouté à l'historique avant
l'exécution (`src/agent.cpp:420-440`). Pour chaque outil, l'exécution réelle
précède `ToolSucceeded` ou `ToolFailed`; le résultat est ensuite ajouté sous
`role=tool` avec son `tool_call_id` (`:446-530`). La boucle repart ensuite sur
une nouvelle requête (`:282-286`).

Si cette requête de suivi échoue, l'exception remonte vers les mêmes catches de
fin de tour. Le code ne supprime ni le message assistant de tool call, ni le
résultat d'outil déjà ajouté, ni un effet filesystem déjà produit. L'observateur
a déjà reçu `ToolSucceeded`; le tour global finit `failed`, avec compteurs
`tools_succeeded` conservés dans `TurnFinished` (`:261-268`, `:504-517`). Un
appel `send()` ultérieur inclut cette interaction dans son historique, sous
réserve de la troncature par tours complets (`:564-588`). Le programme ne
prétend donc pas que l'ensemble d'un tour est transactionnel.

Pour une création réussie, le fichier reste créé si la requête LLM suivante
échoue. Cette conclusion découle de l'ordre d'exécution et de l'absence de
rollback dans `Agent::send()`; elle ne signifie pas qu'une écriture a été
effectuée lors de cette mission en dehors des workspaces temporaires des tests.

## Tests existants et résultats observés

### Conservation du message utilisateur et absence d'assistant partiel

- `tests/agent_streaming_test.cpp:423-465`,
  `agent.flux_sans_done_refuse_et_historique_preserve` : le mock envoie deux
  fragments sans `[DONE]`; le test vérifie l'erreur, l'existence de fragments
  provisoires, puis que la requête suivante contient les deux messages
  `user`, aucun message `assistant`, et retourne une réponse valide.
- `tests/agent_streaming_test.cpp:652-700`,
  `agent.echec_en_plein_flux_najoute_pas_de_message_assistant` : réponse HTTP
  tronquée après du texte partiel; le test vérifie l'erreur curl, la reprise au
  tour suivant, la conservation des deux messages utilisateur et l'absence
  d'assistant partiel.
- `tests/agent_events_test.cpp:162-206` : les scénarios SSE malformés vérifient
  un unique `RequestFailed(protocol)`, un `TurnFinished(failed)`, l'état
  `PresentationState::failed` et la requête suivante sans assistant incomplet.
- `tests/agent_events_test.cpp:418-432`,
  `agent.missing_done_emits_failure_without_false_turn_success` : le transport
  terminé est distingué de l'échec protocolaire et du verdict final du tour.
- `tests/agent_events_test.cpp:465-480`,
  `agent.http_error_emits_one_failed_request_and_turn` : statut 500 du mock,
  résultat HTTP échoué, un `RequestFailed(http)` et une clôture terminale
  unique.

### Appel d'outil réussi et résultat conservé

- `tests/agent_events_test.cpp:237-297` vérifie deux outils réussis, les
  associations d'identifiants, les résultats et la requête de suivi réussie.
- `tests/agent_e2e_test.cpp:207-275` vérifie un `read_file` réussi, puis le
  payload de suivi `user → assistant(tool_calls) → tool` avec le résultat et
  le `tool_call_id` exacts.
- `tests/agent_e2e_test.cpp:281-314` vérifie l'effet filesystem réel d'un
  `write_file` approuvé dans un workspace temporaire.
- `tests/agent_events_test.cpp:299-330` couvre un outil échoué suivi d'une
  réponse finale; ce n'est pas le scénario d'un outil réussi suivi d'un échec
  réseau/protocolaire.

**Scénario non couvert par un test dédié :** un outil réussit, puis la requête
de suivi échoue (HTTP, coupure ou SSE incomplet). Les tests ci-dessus démontrent
séparément la conservation d'un résultat d'outil lorsque le suivi réussit et la
gestion d'échecs de transport/protocole sans outil antérieur; ils ne démontrent
pas la combinaison des deux.

### Exécution déterministe de cette mission

Configuration et compilation neuves :

```sh
cmake -S /home/oper/tmp/local-agent-cpp-mission03-agent-failure \
  -B /tmp/local-agent-cpp-mission03-agent-failure-build \
  -DAGENT_BUILD_TESTS=ON
cmake --build /tmp/local-agent-cpp-mission03-agent-failure-build \
  --target agent_tests --parallel 2
```

Le build a réussi. Avertissements observés : CMP0135 non défini, plusieurs
initialisations agrégées de `AgentConfig` omettent `http_timeouts`, et GNU
make a signalé que `/home/oper/tmp/GMfifo3` existait déjà; aucun n'a empêché la
compilation.

Commande ciblée :

```sh
/tmp/local-agent-cpp-mission03-agent-failure-build/tests/agent_tests \
  --filter=agent
```

- Première exécution sandboxée : **NON ÉVALUABLE**, les tests n'ont pas pu
  ouvrir leur socket mock (`mock: socket() failed`).
- Exécution suivante avec loopback autorisé : **43/43 cas, 900 assertions,
  zéro échec**. Ce résultat couvre le groupe Agent complet dans ce build et
  utilise le mock local, pas un LLM réel.
- CTest complet et autres groupes : **NON EXÉCUTÉS**, hors du périmètre ciblé.

## Classification et décision proposée

| Constat | Classe | Preuve | Décision proposée |
|---|---|---|---|
| Message utilisateur conservé après échec; assistant partiel exclu | Comportement attendu, tests exécutés | Tests streaming cités ci-dessus; groupe Agent PASS | Conserver le contrat. |
| Échec HTTP/SSE distingue transport, protocole et fin de tour | Comportement attendu, tests exécutés | Événements et assertions de `agent_events_test.cpp` | Conserver les événements distincts; aucun correctif indiqué. |
| Outil réussi avant échec ultérieur : résultat et effet demeurent, tour global échoue | Comportement découlant du code; combinaison non testée | Ordre `src/agent.cpp:440-530`, puis boucle et catches `:282-286`, `:536-555` | Ne pas ajouter de rollback implicite. Si une preuve de régression est exigée, ajouter ultérieurement un test déterministe dédié avec un outil non destructif dans un `TempDir` et une seconde réponse mockée en échec. |
| Commentaire `AgentError` promet « rien n'a été ajouté à l'historique pour ce tour » | Contradiction documentaire établie | `include/agent.hpp` commentaire de `AgentError`; `src/agent.cpp:271-274` ajoute le user, `:440-530` ajoute les tool calls/résultats | Corriger ce commentaire dans une modification ultérieure autorisée pour distinguer le user et les résultats d'outils conservés de la réponse assistant incomplète. |

## Limites

- Aucun test n'a été ajouté ou modifié; cette mission était limitée au rapport et
  à son index.
- Le scénario outil-réussi puis requête-de-suivi-échouée n'a pas été exécuté par
  un cas dédié; son analyse est une lecture du flot de contrôle, pas une
  reproduction.
- Aucun appel de serveur LLM réel n'a été effectué.
- Aucun CTest complet, test TUI, test sandbox isolé supplémentaire ou mesure de
  mémoire/contexte n'a été effectué.
- Le `TurnFinished(failed)` n'annule pas les effets déjà réalisés. Le modèle
  pourrait demander une nouvelle action au tour suivant; aucune fréquence ni
  conséquence supplémentaire n'est inférée ici.

## État des fichiers

Seuls ce rapport et son entrée dans `docs/README.md` sont modifiés dans ce
worktree. Le checkout principal, les autres worktrees, les tags et le code de
production sont restés inchangés. Aucun commit, push, PR, merge ou tag n'a été
créé.
