# Phase R0 — Fiabilisation de la boucle d'exécution (rapport)

> Itération **code + tests + gouvernance**. Aucun nouvel outil, aucun framework,
> aucune fonctionnalité métier ajoutée : uniquement des invariants de fiabilité
> et leur couverture de test. **Aucun commit effectué.**

## 1. OBJECTIF ET PÉRIMÈTRE

Réduire l'écart entre **les actions réellement exécutées et confirmées par le
programme** et **les actions que le modèle prétend avoir réalisées** : complétude
du streaming, gestion des timeouts, validation des tool calls, cohérence de
l'historique après échec, état d'exécution explicite, séparation
résultat/vérification/verdict.

**Dans le périmètre** : `src/agent.cpp`, `src/http_client.cpp`,
`src/sse_parser.cpp`, `src/workspace_tools.cpp`, `src/main.cpp`, les en-têtes
associés, les tests, et `AGENTS.md`/`ROADMAP.md` (invariants + dettes closes).

**Hors périmètre** : nouveaux outils, refonte du protocole métier, `fsync`
(dette P7), limites configurables (P9).

## 2. ÉTAT INITIAL OBSERVÉ

| Élément | Valeur |
|---|---|
| Branche / HEAD | `main` / `47acd5d70f937f96b6db1f8d03f9e7fb132744eb` (32 commits) |
| Suite de tests (avant R0) | **66 cas / 264 assertions**, 5 groupes, **0 échec** |
| Modifications préexistantes | **P3.3 non commité** : `docs/README.md`, `docs/questions/OPEN_QUESTIONS.md`, `docs/roadmap/ROADMAP.md`, `docs/reports/PHASE_3_3_…md` — préservées, non attribuées à R0 |

## 3. CONSTATS (causes démontrées par lecture du code)

| ID | Défaut | Preuve (avant R0) | Effet observé |
|---|---|---|---|
| **D1** | **Flux incomplet accepté comme réponse finale** : `[DONE]` était seulement ignoré, aucun contrôle de complétude avant d'accepter le message assistant | `src/agent.cpp` (callback : `if (data == "[DONE]") continue;`), puis acceptation inconditionnelle du contenu accumulé | une coupure propre était indistinguable d'une réponse complète ⇒ « correction annoncée sans preuve d'application » |
| **D2** | **`SseParser` sans `flush()`** : la dernière trame non terminée par une ligne vide restait dans le buffer et était perdue | `include/sse_parser.hpp` (pas de `flush`), `src/sse_parser.cpp` (boucle `feed`) | contenu final tronqué silencieusement (dette §B.2) |
| **D3** | **`finish_reason` jamais lu** | `src/agent.cpp` (`apply_stream_delta` n'y touche pas) | une réponse coupée par la limite de tokens passait pour complète |
| **D4** | **Pas de `CONNECTTIMEOUT`**, timeouts non distingués | `src/http_client.cpp` (seul `CURLOPT_TIMEOUT`) | « timeouts après volumes importants » non identifiables (dette §B.9) |
| **D5** | **Validation des tool calls absente** : un tool call à `id`/`name` vides était construit puis dispatché | `src/agent.cpp` (`apply_tool_call_delta`, `build_assistant_message`, boucle de dispatch) | message `assistant` + message `tool` non conformes (`tool_call_id: ""`) inscrits dans l'historique |
| **D6** | **Aucune représentation explicite du verdict d'exécution** : `send` ne renvoyait qu'un `std::string` ; la CLI n'affichait rien sur les outils | `include/agent.hpp`, `src/main.cpp` | seul le **texte du modèle** servait de verdict ⇒ « verdicts contradictoires avec les constats » |
| **D7** | **`write_file` ne vérifiait pas l'état final** | `src/workspace_tools.cpp` (return d'un message après `rename`) | le message « wrote N bytes » était une **déclaration**, non une preuve |

## 4. INVARIANTS D'EXÉCUTION DÉFINIS

* **I1 — Flux complet** : un flux n'est accepté que s'il se termine par `[DONE]`.
* **I2 — Message assistant acceptable** : ni flux incomplet, ni
  `finish_reason == "length"`, ni tool call malformé.
* **I3 — Tool call traçable** : `id` ET `function.name` non vides, sinon refus
  avant tout dispatch ; aucun message `assistant`/`tool` incohérent en historique.
* **I4 — Échec partiel** : une réponse non exploitable lève `AgentError` et
  **n'ajoute rien** à l'historique (le message `user` du tour reste, reprise
  possible).
* **I5 — Reprise** : après un échec, le tour suivant fonctionne et l'historique
  ne contient aucun artefact du tour échoué.
* **I6 — Verdict** : aucune déclaration du modèle ne vaut preuve. Le verdict
  d'exécution provient du programme (`AgentCallbacks::on_tool_result`) ; le
  verdict d'écriture provient d'une **relecture** du fichier.

## 5. CORRECTIONS (minimales, justifiées)

| Fichier | Correction | Défaut |
|---|---|---|
| `include/sse_parser.hpp` / `src/sse_parser.cpp` | ajout de `flush()` (restitue la dernière trame) et `buffered_bytes()` | D2 |
| `include/agent.hpp` | `struct AgentError`, `struct ToolOutcome{name, ok, result}`, `AgentCallbacks::on_tool_result` | D5, D6 |
| `src/agent.cpp` | contrôle de complétude (`saw_done`) + `flush()` en fin de flux ; refus si `finish_reason == "length"` ; validation `id`/`name` avant dispatch ; émission de `on_tool_result` ; `AgentError` au dépassement d'itérations | D1, D3, D5, D6 |
| `src/http_client.cpp` | `CURLOPT_CONNECTTIMEOUT = min(timeout, 10 s)` ; message dédié sur `CURLE_OPERATION_TIMEDOUT` | D4 |
| `src/workspace_tools.cpp` | `verify_written_file()` : relecture + comparaison byte à byte du fichier temporaire **avant** `rename()` | D7 |
| `src/main.cpp` | gestion d'`AgentError` ; affichage par outil (`succes`/`ECHEC`) et récapitulatif `[execution: N outil(s) reussi(s), M en echec]` | D6 |
| `AGENTS.md` | 5 invariants ajoutés (complétude, tool calls, état d'exécution, preuve d'écriture, timeouts) ; compteurs de tests corrigés | — |
| `docs/roadmap/ROADMAP.md` | §B.2 et §B.9 marqués **RÉSOLUS (R0)** ; items **P6**/**P8** marqués **FAIT** | — |

Aucune signature publique **existante** n'est modifiée : seules des **ajouts**
(`flush`, `buffered_bytes`, `AgentCallbacks::on_tool_result`, `AgentError`,
`ToolOutcome`) ; `Agent::send` conserve sa signature.

## 6. TESTS

### 6.1 Ajoutés / modifiés

| Groupe | Test | Défaut couvert |
|---|---|---|
| `sse` | `flush_restitue_l_evenement_non_termine`, `flush_sans_reliquat_ne_rend_rien` | D2 |
| `http` | `timeout_distinct_des_autres_echecs_curl` | D4 |
| `agent` | `flux_sans_done_refuse_et_historique_preserve` | D1, I4, I5 |
| `agent` | `reponse_tronquee_par_limite_de_tokens_refusee` | D3 |
| `agent` | `derniere_trame_sans_ligne_vide_est_traitee` | D2 (garde-fou) |
| `agent` | `callback_tool_result_expose_le_verdict_d_execution` | D6 |
| `agent` | `tool_call_sans_id_ou_nom_est_refuse` *(remplace `tool_call_sans_id_ni_nom_ne_corrompt_pas_le_flux`)* | D5 |
| `agent` (e2e) | `write_file_verifie_l_etat_reel_du_fichier` — lit le fichier **réellement écrit sur le disque** | D7, I6 |
| `workspace` | `ecriture_verifiee_sur_disque` | D7 |

Les catégories exigées sont couvertes : coupures SSE, erreurs HTTP (dont
timeout), arguments invalides, erreurs d'outils, exécutions partielles, limites
d'itérations, reprises après échec.

### 6.2 Résultats réellement obtenus

```
$ cmake --build <build-tests> --clean-first -j
(0 avertissement / 0 erreur)

$ ctest
100% tests passed, 0 tests failed out of 5

$ ./tests/agent_tests
cas executes        : 75 / 75
assertions          : 298
assertions en echec : 0
RESULTAT: OK
```

Avant R0 : **66 cas / 264 assertions**. Après : **75 cas / 298 assertions**
(**+9 cas / +34 assertions**), **0 échec**, aucune régression.

### 6.3 Preuve que les correctifs sont nécessaires (test négatif)

`src/agent.cpp` remis temporairement à son état HEAD (`git stash push --
src/agent.cpp`), puis suite relancée : **5 tests échouent / 9 assertions**
(`tool_call_sans_id_ou_nom_est_refuse`, `flux_sans_done_refuse_et_historique_preserve`,
`reponse_tronquee_par_limite_de_tokens_refusee`,
`callback_tool_result_expose_le_verdict_d_execution`,
`write_file_verifie_l_etat_reel_du_fichier`). Le correctif restauré, la suite
redevient **verte**. Ces tests échouent donc bien **sans** la correction.

## 7. VÉRIFICATIONS EXÉCUTÉES

| # | Contrôle | Résultat |
|---|---|---|
| V-1 | `cmake --build … --clean-first` avec `-Wall -Wextra -Wpedantic` | **0 erreur / 0 avertissement** |
| V-2 | `ctest` | **5/5 groupes**, 0 échec |
| V-3 | `./tests/agent_tests` | **75/75 · 298 assertions · 0 échec** |
| V-4 | Test négatif (agent.cpp HEAD) | **5 tests échouent sans le correctif** (§6.3) |
| V-5 | `git diff --check` | **silencieux** |
| V-6 | État du dépôt | 16 fichiers modifiés + 1 rapport ajouté ; **aucun commit** |

## 8. LIMITES / RESTES À VÉRIFIER

| # | Limite | Portée |
|---|---|---|
| L-1 | `src/main.cpp` (affichage du récapitulatif d'exécution) **n'a pas de test automatisé** (pas de harnais CLI) | mécanismes sous-jacents testés (`AgentError`, `on_tool_result`) ; seul le formatage n'est pas couvert |
| L-2 | `derniere_trame_sans_ligne_vide_est_traitee` **ne discrimine pas** seul (il passait aussi avec l'ancien code) | la discrimination est assurée par les tests `sse.flush_*` (qui ne compilent pas sans `flush()`) |
| L-3 | Les autres valeurs de `finish_reason` (`content_filter`, `tool_calls`) ne déclenchent pas de traitement spécifique | seul `length` est refusé ; comportement à décider ultérieurement |
| L-4 | La preuve d'écriture couvre **taille + contenu** après écriture, **pas** la durabilité après crash (`fsync`) | dette **P7**, hors périmètre R0 |
| L-5 | Le seuil `min(timeout, 10 s)` pour `CONNECTTIMEOUT` est une valeur choisie, non dérivée d'une contrainte mesurée | à ajuster si un besoin réel apparaît |
| L-6 | Les modifications **P3.3** (documentation) restent présentes et **non commitées**, mêlées à R0 dans `docs/README.md` et `docs/roadmap/ROADMAP.md` | séparation à faire au moment du commit |

## 9. VERDICT

### VALIDÉ

**Justification** :

1. Les **7 défauts démontrés** (D1–D7) sont corrigés par des changements
   **minimaux** documentés (§5), sans nouvelle fonctionnalité ni framework.
2. Chaque correction est **couverte par un test** et la suite complète passe :
   **75/75 cas · 298 assertions · 0 échec**, **5/5** groupes CTest, **0
   avertissement**.
3. Un **test négatif** (§6.3) prouve que les nouveaux tests échouent **sans** la
   correction : il ne s'agit pas d'assertions tautologiques.
4. Les invariants du projet sont **écrits dans `AGENTS.md`** et les dettes
   correspondantes de la roadmap (§B.2, §B.9, P6, P8) sont **closes**.

**Limites documentées** (§8) : le formatage CLI (L-1) et la durabilité après
crash (L-4) ne sont pas couverts ; `derniere_trame…` (L-2) est un garde-fou non
discriminant. Aucune ne remet en cause les invariants I1–I6.

## 10. COMMIT / STAGING

**Aucun commit, aucun staging.** Le dépôt contient : les corrections R0
(11 fichiers source/test/en-têtes + `AGENTS.md` + `ROADMAP.md`), ce rapport, et
les modifications **P3.3** préexistantes. La séparation P3.3 / R0 reste à faire
lors d'un commit autorisé.
