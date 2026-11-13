# LOFITERM — Contrat `AgentError` et régression outil/suivi

Date : 13 novembre 2026
SHA de base : `9202b1c8e77dfa65e7830db47b321219c1ba4a0a`
Branche : `fix/agent-error-history-contract-test`
Worktree : `/home/oper/tmp/local-agent-cpp-mission03-error-contract`

## Verdict

**PASS.** Le commentaire de `AgentError` décrit maintenant correctement la
conservation du message utilisateur et la possibilité que des appels d'outils,
leurs résultats et effets précèdent l'échec, sans promettre de rollback. Le
nouveau test déterministe vérifie qu'un outil non destructif réussit une seule
fois, que la requête HTTP de suivi échoue, que l'historique reste cohérent au
tour utilisateur suivant et que les événements restent corrélés. Le test ciblé
et le groupe Agent complet ont réussi.

Aucun comportement de production n'a été modifié. Aucun serveur LLM réel n'a
été contacté. Le CTest complet n'a pas été exécuté, conformément au périmètre.

## Changements

Fichiers modifiés :

- `include/agent.hpp` — commentaire de `AgentError` uniquement.
- `tests/agent_events_test.cpp` — ajout de
  `agent.successful_tool_is_preserved_when_followup_http_fails_and_next_turn_runs`.
- `docs/reports/2026-11-13-agent-error-contract-and-tool-followup-regression.md` —
  présent rapport.
- `docs/README.md` — index du rapport.

`src/agent.cpp`, `HttpClient`, le parseur SSE, les outils workspace et la
configuration n'ont pas été modifiés.

### Commentaire corrigé

Le commentaire précise qu'une réponse assistant incomplète n'est pas validée
dans l'historique, alors que le message utilisateur y reste. Il indique aussi
qu'un outil peut déjà avoir été exécuté et que ni ses résultats ni ses effets
ne sont implicitement annulés par l'échec ultérieur.

### Scénario du nouveau test

Le mock sert successivement :

1. un tool call valide `call-once` avec `[DONE]`;
2. une réponse HTTP 500 pour la requête LLM de suivi;
3. une réponse SSE complète au nouveau message utilisateur.

Le handler `once` n'a aucun effet externe : il incrémente un compteur en
mémoire et renvoie une chaîne synthétique. Le test vérifie :

- une seule exécution, un seul callback `ToolOutcome` réussi, un seul
  `ToolStarted` et un seul `ToolSucceeded`;
- la correspondance du `tool_call_id` et du `request_id` entre démarrage,
  succès et requête initiale;
- `HttpRequestFinished(failed)` pour la deuxième requête, puis
  `RequestFailed(http)` corrélé à cette requête;
- exactement un `TurnFinished(failed)` pour le premier tour, avec deux requêtes
  HTTP et un outil réussi, puis un `TurnFinished(completed)` pour le tour
  suivant;
- le payload de suivi échoué et celui du nouveau tour contiennent
  `user → assistant(tool_calls) → tool`; le payload du nouveau tour ajoute le
  nouveau message `user`;
- l'identifiant d'appel et le résultat d'outil sont conservés, aucun message
  assistant final du tour échoué n'est présent, et le handler n'est pas relancé
  au tour suivant.

## Validation exécutée

Configuration CMake neuve :

```sh
cmake -S /home/oper/tmp/local-agent-cpp-mission03-error-contract \
  -B /tmp/local-agent-cpp-mission03-error-contract-build \
  -DAGENT_BUILD_TESTS=ON
```

Compilation des cibles concernées :

```sh
cmake --build /tmp/local-agent-cpp-mission03-error-contract-build \
  --target agent_tests --parallel 2
```

La première compilation a révélé une erreur du nouveau test : le champ réel de
`TurnFinished` est `http_requests`, pas `request_count`. L'assertion du test a
été corrigée selon `include/ui/agent_event.hpp`; aucune source de production
n'a changé. La compilation finale de `agent_tests` a réussi. Les avertissements
de compilation existants concernant CMP0135, des initialisations omettant
`AgentConfig::http_timeouts` et le FIFO `/home/oper/tmp/GMfifo3` ont été
observés; ils n'ont pas empêché le build.

Le harnais versionné ne filtre que par groupe, pas par nom de cas. Pour isoler
le nouveau cas sans modifier d'autre fichier du dépôt, un lanceur temporaire
hors worktree a invoqué le cas enregistré depuis les objets compilés du test.
Commande d'exécution du lanceur :

```sh
/tmp/local-agent-cpp-mission03-error-contract-build/targeted_agent_followup_test
```

Résultat : **1 cas, 110 assertions, zéro échec**.

Groupe complet exécuté ensuite, avec le loopback autorisé uniquement pour
`MockHttpServer` :

```sh
/tmp/local-agent-cpp-mission03-error-contract-build/tests/agent_tests \
  --filter=agent
```

Résultat : **44 cas sur 305 enregistrés, 1 010 assertions, zéro échec**.
La première tentative sandboxée du groupe avait échoué à ouvrir le socket
mock (`mock: socket() failed`); elle n'est pas comptée comme réussite. La
relance autorisée a réussi. Les tests, y compris celui de `write_file`, utilisent
les espaces temporaires fournis par `tests::TempDir`; aucun workspace utilisateur
ou serveur LLM n'a été utilisé.

Enfin, `git diff --check` a réussi. Le CTest complet et tout test LLM réel sont
**NON EXÉCUTÉS**.

## Réserves

- Le comportement observé concerne un outil non destructif à effet mémoire. Le
  test ne prétend pas rendre un tour transactionnel ni annuler un effet d'outil
  réel après une panne de suivi.
- Les résultats sont ceux du SHA de base indiqué, compilé avec le commentaire
  et le test de cette branche; ils ne constituent pas une qualification d'un
  autre SHA.
- Aucun commit, push, PR, merge ou tag n'a été effectué.

## État final attendu

Le diff du worktree est limité aux deux fichiers de code/test autorisés, au
présent rapport et à son index. Aucun changement n'a été apporté aux autres
worktrees ni au tag technique.
