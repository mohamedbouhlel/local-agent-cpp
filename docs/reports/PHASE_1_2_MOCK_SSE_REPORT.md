# P1.2 / P2 — Mock SSE versionné et tests de bout en bout (rapport)

> Itération de **tests** uniquement : ajout de scénarios de bout en bout
> s'appuyant sur le mock HTTP/SSE déjà versionné (`tests/mock_http_server.*`).
> **Aucune modification de `src/`, `include/`, du protocole HTTP/SSE ni de la
> configuration CMake racine.** Aucun framework externe ajouté.
>
> **Identifiant** : la demande utilise « P1.2 » ; la roadmap
> (`docs/roadmap/ROADMAP.md`, vague 1) désigne cette itération **P2** —
> « Versionner un mock serveur SSE minimal, sous un répertoire dédié, pour
> rendre la validation de bout en bout reproductible ».

## 1. OBJECTIF ET PÉRIMÈTRE

Rendre la validation de bout en bout de l'agent **déterministe et reproductible
sans LLM réel**, en réutilisant le mock HTTP/SSE local et la suite de tests
existante (`tests/`, micro-harnais maison). Scénarios exigés :

1. **A — streaming** : une réponse textuelle découpée en plusieurs événements
   SSE, livrée progressivement au callback de texte, contenu final exact.
2. **B — tool-calling** : un tool call dont les champs sont fragmentés sur
   plusieurs événements, reconstruit puis exécuté ; le résultat est renvoyé au
   modèle dans la requête suivante avec le `tool_call_id` correspondant.

Contrainte : **aucune dépendance externe supplémentaire**, **aucun changement du
comportement de production**.

### État initial (Phase 0)

| Élément | Valeur |
|---|---|
| Répertoire | `/home/oper/projects/local-agent-cpp` |
| Branche / HEAD | `main` / `db88cdf834bd19f777f49c231d7e01445cf6c46b` (30 commits) |
| Working tree | **propre** (aucune modification préexistante) |
| Mock existant | `tests/mock_http_server.hpp` / `.cpp` (**déjà versionné**, commit `1abe663`) |
| Suite existante | 64 cas / 239 assertions, 5 groupes CTest (`sse`, `workspace`, `http`, `agent`, `mock`) |

**Constat de préparation** : le mock versionné existe déjà et sait servir des
corps SSE arbitraires, les découper (`piece_size`/`piece_delay_us`), simuler une
troncature ou une coupure, et exposer les requêtes reçues (`received_bodies()`).
Il ne nécessitait **aucune** adaptation. Le manque réel portait sur la
**couverture** : au niveau `Agent`, le texte était toujours servi en **un seul**
delta, et aucun test ne vérifiait les paramètres de streaming (`stream: true`)
ni le lien `tool_call_id` de la requête de suivi.

## 2. FICHIERS AJOUTÉS OU MODIFIÉS

| Fichier | Nature | Rôle |
|---|---|---|
| `tests/agent_e2e_test.cpp` | **ajouté** | deux scénarios de bout en bout (Test A, Test B) sur `MockHttpServer` |
| `tests/CMakeLists.txt` | modifié | enregistre le nouveau fichier dans la cible `agent_tests` (**+1 ligne**) |
| `docs/README.md` | modifié | indexe ce rapport (**+1 ligne**) |
| `docs/reports/PHASE_1_2_MOCK_SSE_REPORT.md` | **ajouté** | ce rapport |

**Aucune** modification de `src/`, `include/`, `CMakeLists.txt` racine, ni du
mock. Le groupe CTest `agent` est réutilisé : aucun groupe nouveau, donc aucune
modification de la boucle `foreach(group …)`.

## 3. SCÉNARIOS DE TEST EXÉCUTÉS ET RÉSULTATS

### Test A — `agent.texte_fragmente_livraison_progressive`

* flux SSE = 5 événements `delta.content` : `"Bon"`, `"jour"`, `", "`, `"monde"`, `" !"`, puis `[DONE]` ;
* assertions : la **séquence exacte** des fragments reçus par `on_text_delta`
  vaut `{Bon, jour, ", ", monde, " !"}` (livraison **progressive**, pas un bloc) ;
  `reply` == `streamed` == `"Bonjour, monde !"` ; une seule requête ;
  `model == "local-model"`, `stream == true` (booléen), `messages` = `[user]`.

### Test B — `agent.tool_call_fragmente_bout_en_bout`

* flux 1 = tool call `read_file` fragmenté sur 4 événements (id, puis `name`/
  `arguments` concaténés : `{"path":` + `"a.txt` + `"}`) ;
* flux 2 = réponse texte `"voici le contenu"` (fragments `"voici"`, `" le contenu"`) ;
* assertions : `reply == "voici le contenu"` ; callback outil = `{read_file}` ;
  **2** requêtes reçues ; requête 1 `stream == true` et `messages=[user]` ;
  requête 2 `stream == true`, `messages=[user, assistant, tool]` ;
  `assistant.tool_calls[0]` = `id="call_e2e"`, `type="function"`,
  `function.name="read_file"`, `function.arguments="{\"path\":\"a.txt\"}"` ;
  message `tool` : **`tool_call_id == "call_e2e"`** et
  `content == "hello world\n"` (résultat réel de l'outil sur le workspace temporaire).

### Commandes réellement exécutées et résultats

Build hors dépôt (ns n'utilise pas le `build/` de production), hors ligne
(`FETCHCONTENT_SOURCE_DIR_JSON` réutilisé) :

```
$ cmake -S . -B /home/oper/tmp/agent-cpp-tests -DAGENT_BUILD_TESTS=ON \
      -DFETCHCONTENT_SOURCE_DIR_JSON=…/build/_deps/json-src -DFETCHCONTENT_FULLY_DISCONNECTED=ON
-- Configuring done / Generating done          # rc=0

$ cmake --build /home/oper/tmp/agent-cpp-tests -j
[100%] Built target agent_tests                # rc=0, 0 erreur / 0 avertissement

$ ctest --output-on-failure                    # (dans /home/oper/tmp/agent-cpp-tests)
    Start 1: sse .......... Passed 0.01
    Start 2: workspace .... Passed 0.07
    Start 3: http ......... Passed 0.08
    Start 4: agent ........ Passed 0.08
    Start 5: mock ......... Passed 1.17
100% tests passed, 0 tests failed out of 5      # rc=0

$ ./tests/agent_tests
cas executes        : 66 / 66
assertions          : 264
assertions en echec : 0
RESULTAT: OK                                  # rc=0
```

| Contrôle | Résultat |
|---|---|
| Configuration / compilation | **rc=0**, **0 erreur / 0 avertissement** (`-Wall -Wextra -Wpedantic`) |
| Découverte CTest | **5** groupes (`sse`, `workspace`, `http`, `agent`, `mock`) |
| Suite complète | **66/66 cas · 264 assertions · 0 échec** — **+2 cas / +25 assertions** vs 64/239 |
| Nouveaux tests | les deux exécutés et **OK** (`--filter=agent`) |
| Dépendance à un LLM réel | **aucune** : tout sur `127.0.0.1` (mock local) |
| Régression | **aucune** : les 64 cas antérieurs passent toujours |

## 4. VÉRIFICATIONS

1. **Repli sur LLM réel** : les tests n'ouvrent que `127.0.0.1` via
   `MockHttpServer` ; aucun accès réseau externe, aucun `.env` requis.
2. **Périmètre** : `git status --short` → **2** fichiers touchés
   (`tests/CMakeLists.txt`, `tests/agent_e2e_test.cpp`) avant ce rapport ;
   **aucune** modification de `src/`, `include/` ou `CMakeLists.txt` racine.
3. **Propreté du diff** : `git diff --check` → **silencieux**.
4. **Fichiers temporaires** : build **hors dépôt** ; `TempDir` écrit sous le
   répertoire temporaire système ; **aucun** résidu `agent_cpp_test_*` observé
   après exécution ; `workspace/` du dépôt **non modifié**.

## 5. LIMITES RESTANTES

| # | Limite | Portée |
|---|---|---|
| L-1 | Le streaming est vérifié au niveau **événement SSE**, pas au niveau du découpage TCP (`piece_size`) | le découpage réseau est déjà couvert par `http.post_streaming_fragmente_et_accumule` ; hors périmètre ici |
| L-2 | Chevauchement partiel avec `agent.tool_call_unique_arguments_fragmentes` (mock, reconstruction, exécution) | Test B **ajoute** les assertions manquantes : `stream: true` et `tool_call_id` de la requête de suivi |
| L-3 | Aucun test d'un flux SSE **sans** `[DONE]` ni d'événement final non terminé (`SseParser::flush()` absente, dette P8) | hors périmètre P2, déjà documentée dans la roadmap |
| L-4 | Le mock reste dans `tests/` (pas de répertoire dédié) | la roadmap mentionne « répertoire dédié » ; le déplacer serait un refactor hors périmètre |

## 6. VERDICT

### VALIDÉ

**Justification** :

1. Les **deux scénarios exigés** (streaming fragmenté, tool-calling fragmenté de
   bout en bout) sont implémentés et **exécutés avec succès** (Phase 3).
2. La suite complète passe : **66/66 cas · 264 assertions · 0 échec**,
   **5/5** groupes CTest, sans régression.
3. **Aucune dépendance externe** ajoutée, **aucun** changement de comportement
   de production, **aucun** fichier temporaire laissé dans le dépôt.
4. Le mock HTTP/SSE était déjà versionné ; l'itération se limite à la couverture
   manquante, conformément au principe « ne pas modifier la production sans
   nécessité démontrée ».
