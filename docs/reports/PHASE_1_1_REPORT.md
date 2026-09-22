# PHASE 1.1 — Harness de tests reproductible (rapport)

> Référence de départ : branche `main`, HEAD `4f33b623c17ff6e23e7b38694300128d51a9d418`.
> Audit précédent : PHASE 1 — PASS SOUS RÉSERVE.
> Ce rapport est un livrable d'audit ; il ne corrige aucun défaut et ne modifie
> aucun fichier de production.

## 1. HEAD et état Git

| Moment | Branche | HEAD | Fichiers suivis modifiés |
|---|---|---|---|
| Avant | `main` | `4f33b623c17ff6e23e7b38694300128d51a9d418` | aucun |
| Après | `main` | `4f33b623c17ff6e23e7b38694300128d51a9d418` | `CMakeLists.txt` (8 insertions) |

**HEAD identique avant/après, cohérent avec la référence P0.** Aucun commit créé.

État Git final :

```
 M CMakeLists.txt
?? OPEN_QUESTIONS.md      (document d'audit P0, intact)
?? PROJECT_CONTEXT.md     (document d'audit P0, intact)
?? PROJECT_HISTORY.md     (document d'audit P0, intact)
?? ROADMAP.md             (document d'audit P0, intact)
?? tests/                 (harness)
```

Intégrité vérifiée :
- `git diff --stat -- src include` → **vide** (aucune modification de production).
- `workspace/` du dépôt → **`.gitkeep` uniquement** (jamais utilisé par les tests).
- Répertoire `build/` existant → **non touché** (`build/agent` mtime `2026-09-22 14:07:18`, `build/CMakeCache.txt` mtime `2026-09-21 23:00:27`, inchangés).
- Aucun résidu `agent_cpp_test_*` dans le répertoire temporaire après exécution.
- `.env` et `build/.env` : **jamais lus**.

## 2. Fichiers créés ou modifiés

### Créés (nouveaux, sous `tests/`)

| Fichier | Lignes | Rôle |
|---|---|---|
| `tests/test_support.hpp` | 157 | micro-harnais : registre de cas, `TEST`, assertions, `TempDir` RAII |
| `tests/test_main.cpp` | 49 | exécuteur, `--filter=<groupe>`, résumé, code de retour |
| `tests/mock_http_server.hpp` | 64 | serveur HTTP loopback scriptable (déclaration) |
| `tests/mock_http_server.cpp` | 247 | implémentation POSIX sockets |
| `tests/sse_parser_test.cpp` | 192 | groupe `sse` (16 cas) |
| `tests/workspace_tools_test.cpp` | 473 | groupe `workspace` (25 cas, registre inclus) |
| `tests/http_client_test.cpp` | 124 | groupe `http` (6 cas) |
| `tests/agent_streaming_test.cpp` | 587 | groupe `agent` (12 cas : tool calls + historique) |
| `tests/CMakeLists.txt` | 22 | cible `agent_tests` + enregistrement CTest |
| **Total** | **1915** | |

### Modifié

| Fichier | Modification |
|---|---|
| `CMakeLists.txt` | +8 lignes : `option(AGENT_BUILD_TESTS ... OFF)` + `enable_testing()` + `add_subdirectory(tests)` |

**Aucun fichier de `src/` ou `include/` modifié. Aucun document d'audit P0 modifié.**

## 3. Choix du framework et justification

| Aspect | Choix | Justification | Limite |
|---|---|---|---|
| Framework | **micro-harnais maison** (header-only, zéro dépendance) | `AGENTS.md:103` interdit d'ajouter un framework externe sans demande explicite ; `AGENTS.md:28` « pas de dépendances lourdes » | pas de fixtures/reporters avancés |
| Alternative écartée | Catch2 / GoogleTest | mentionnés par `AGENTS.md:19` **pour plus tard** (« si le projet grossit »), mais l'ajout exigerait un accord explicite | — |
| Activation | `-DAGENT_BUILD_TESTS=ON`, **OFF par défaut** | l'exécutable `agent` et `agent_core` n'ont aucune dépendance de test | — |
| Isolation | `agent_tests` ne fait que **lier** `agent_core` (+ `Threads`) | aucune modification des cibles de production | — |
| Granularité | binaire unique, `--filter=<groupe>`, 4 tests CTest | résultats lisibles par groupe et code de retour par groupe | — |
| Réseau | **mock HTTP loopback** (127.0.0.1, port éphémère) intégré aux tests | atteindre `Agent::send`/`HttpClient` sans LLM réel ni service distant ; aucun téléchargement | dépendance au timing pour le découpage en fragments |
| Fichiers | un `TempDir` RAII par test | ne touche jamais le `workspace/` réel ; nettoyage automatique | — |

Contrainte d'interface documentée : `trim_history`, `append_message` (privés de `Agent`) et
`apply_tool_call_delta` / `apply_stream_delta` / `build_assistant_message` / `has_tool_calls` /
`sandbox_path` / `is_hidden_path` (namespace anonyme) **ne sont pas accessibles** depuis un test.
Les rendre testables unitairement exigerait une **évolution de la production** (exposition d'API
ou extraction de fichier). Conformément à la consigne (« proposer une solution séparée, sans
l'implémenter sans nécessité »), **aucune modification de production n'a été faite** : ces
comportements sont couverts **indirectement** via l'API publique (`Agent::send`,
`ToolRegistry::execute`) et le mock loopback.

## 4. Commandes exactes exécutées

```bash
# Précontrôle
git rev-parse HEAD && git branch --show-current && git status --short --branch

# Configuration depuis un répertoire vierge, HORS du dépôt, sans réseau
cmake -S . -B /tmp/p11-build -DAGENT_BUILD_TESTS=ON \
      -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src \
      -DFETCHCONTENT_FULLY_DISCONNECTED=ON

# Compilation
cmake --build /tmp/p11-build

# Exécution
/tmp/p11-build/tests/agent_tests            # tous les cas
/tmp/p11-build/tests/agent_tests --filter=sse
cd /tmp/p11-build && ctest --output-on-failure

# Déterminisme
for run in 1 2 3 4 5; do /tmp/p11-build/tests/agent_tests; done

# Intégrité
git diff --stat -- src include    # doit être vide
git status --short
```

Recommandation permanente (reproduction) :

```bash
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON && cmake --build build-tests
cd build-tests && ctest --output-on-failure
```

## 5. Résultat du build propre

| Contrôle | Résultat |
|---|---|
| `cmake` (répertoire vierge `/tmp/p11-build`) | **rc=0**, configuration réussie |
| Récupération réseau FetchContent | **aucune** (source locale `build/_deps/json-src` via `FETCHCONTENT_SOURCE_DIR_JSON` + `FETCHCONTENT_FULLY_DISCONNECTED=ON`) |
| `cmake --build` | **rc=0**, cible `agent_tests` liée |
| Erreurs de compilation | **0** |
| Avertissements (`-Wall -Wextra -Wpedantic`) | **0** |

Observation préexistante (non introduite par le harness) : `CMakeLists.txt:13`
(`FetchContent_Declare`) émet l'avertissement `CMP0135` (« DOWNLOAD_EXTRACT_TIMESTAMP »).
Il provient du code d'origine.

## 6. Résultats détaillés des tests

Exécution complète : **59 cas, 225 assertions, 0 échec** — code de retour **0**.

| Groupe | Cas | Assertions | Résultat | Durée CTest |
|---|---|---|---|---|
| `sse` | 16 | — | **OK** | 0.01 s |
| `workspace` | 25 | — | **OK** | 0.07 s |
| `http` | 6 | — | **OK** | 0.07 s |
| `agent` | 12 | — | **OK** | 0.06 s |

CTest : `100% tests passed, 0 tests failed out of 4` (rc=0).
Déterminisme : **5 exécutions consécutives**, `0` échec à chaque fois.

### A. Parseur SSE (`sse`, 16 cas)

| Cas | Vérifie | Résultat |
|---|---|---|
| `evenement_valide` | événement complet → 1 payload | OK |
| `evenement_coupe_entre_plusieurs_fragments` | payload réparti sur 3 `feed()` | OK |
| `separateur_coupe_entre_deux_fragments` | `"\n"` final isolé ≠ séparateur, puis `"\n"` complète | OK |
| `plusieurs_evenements_dans_un_meme_chunk` | 3 payloads dans l'ordre | OK |
| `evenement_incomplet_en_fin_de_flux` | **0 payload** (pas de `flush()`) — comportement observé, consigné | OK |
| `ligne_vide_et_evenement_vide` | `\n\n`, `\n`, `""` → 0 payload | OK |
| `donnees_malformees_retournees_telles_quelles` | charge non-JSON retournée brute (validée par l'appelant) | OK |
| `marqueur_done_retourne_tel_quel` | `[DONE]` restitué tel quel | OK |
| `separateur_crlf` | `\r\n\r\n` accepté | OK |
| `plusieurs_lignes_data_concatenees` | jointes par `\n` | OK |
| `commentaires_ignores` / `commentaire_dans_le_meme_evenement` | lignes `:` ignorées | OK |
| `evenement_sans_ligne_data_ignore` | `event:`/`id:`/`retry:` → 0 payload | OK |
| `data_sans_espace_apres_deux_points` | `data:{...}` accepté | OK |
| `flux_fragmentaire_octet_par_octet` | 1 octet par `feed()` → 2 payloads corrects | OK |
| `feed_all_equivalence` | multifragments équivalent | OK |

### B. Tool calls (`agent`, 8 cas)

| Cas | Vérifie | Résultat |
|---|---|---|
| `tool_call_unique_arguments_fragmentes` | id/nom/arguments fragmentés reconstruits ; ordre `user→assistant→tool` ; outil réellement exécuté (`hello world\n`) | OK |
| `tool_calls_multiples_index_entrelaces` | 2 tool_calls (index 0/1) entrelacés : noms et arguments **non mélangés**, ordre des index respecté | OK |
| `arguments_absents_normalises_en_objet_vide` | arguments absents → `"{}"` (exécutable) | OK |
| `delta_tool_call_sans_index_ignore` | delta sans `index` ignoré : 1 seule requête, aucune exécution | OK |
| `outil_inconnu_retourne_tool_error` | `tool error: unknown tool: nope_tool` | OK |
| `arguments_invalides_retourne_tool_error` | `tool error: … parse_error …` (pas de crash) | OK |
| `tool_call_sans_id_ni_nom_ne_corrompt_pas_le_flux` | **correction du constat R1** (voir §7) | OK |
| `deltas_non_conformes_ignores` | `choices` vide/absent, `content: null`, JSON invalide → ignorés | OK |

### C. Historique (`agent`, 4 cas)

| Cas | Vérifie | Résultat |
|---|---|---|
| `historique_ordre_et_tours_complets` | ordre `user→assistant→user`, contenus préservés | OK |
| `historique_tronque_par_tours_complets` | 12 tours : **chaque requête ≤ 20 messages**, commence par un `user`, jamais de tour incomplet ; `tour-1` retiré, dernier tour présent ; aucun message `tool` parasite | OK |
| `echec_en_plein_flux_najoute_pas_de_message_assistant` | flux tronqué → erreur `HTTP POST failed`, texte partiel **bien** streamé avant l'échec, puis tour suivant : historique = `[user, user]`, **aucun `assistant`** | OK |
| `boucle_outils_limitee_a_cinq_iterations` | serveur renvoyant toujours un tool_call → `tool call loop exceeded maximum iterations` après exactement **5 requêtes** | OK |

### D. Workspace tools (`workspace`, 25 cas)

Nominal : `ecriture_puis_lecture`, `creation_repertoires_parents`, `garde_fou_overwrite`
(refus puis acceptation), `ecriture_atomique_sans_residu`, `permissions_preservees_a_lecrasement`.
Chemins : `chemin_inexistant`, `chemin_vide`, `chemin_absolu` (aucun fichier créé hors sandbox),
`chemin_avec_double_point`, `chemin_point_seul_sort_du_sandbox`,
`sortie_par_lien_symbolique_rejetee` (lien → hors sandbox : lecture **et** écriture rejetées,
rien créé dans la cible), `arguments_manquants_ou_mal_typoes`, `cible_est_un_repertoire`.
Limites : `limite_ecriture_200000` (200 000 accepté, 200 001 rejeté sans fichier),
`limite_lecture_50000_et_marqueur` (50 000 octets + marqueur ; pas de marqueur sous la limite),
`fichier_binaire_rejete`.
Parcours : `list_files_exclut_les_chemins_caches`, `search_files_litteral_et_sensible_a_la_casse`,
`search_files_plafond_50_resultats` (50 résultats + marqueur), `search_files_ignore_les_binaires_et_les_caches`.
Registre : `registre_outil_inconnu`, `registre_quatre_outils_et_schemas` (ordre alphabétique
déterministe), `registre_double_enregistrement_rejete`.
Permissions : `ecriture_dans_repertoire_non_inscriptible`,
`fichier_texte_illisible_signale_comme_binaire_DEFECT_DOCUMENTE` (voir §7).

**Tous OK.**

### E. Client HTTP (`http`, 6 cas)

| Cas | Vérifie | Résultat |
|---|---|---|
| `post_succes_retourne_le_corps` | `post()` retourne le corps | OK |
| `post_statut_non_2xx_leve_httperror_avec_le_corps` | `HTTP POST failed with status 400` **et** corps inclus | OK |
| `post_streaming_fragmente_et_accumule` | corps écrit par morceaux de 3 o espacés de 5 ms : **callback appelé plusieurs fois**, concaténation == corps, retour == corps | OK |
| `post_echec_curl_connexion_sans_reponse` | connexion fermée sans réponse → `HTTP POST failed` | OK |
| `post_reponse_tronquee_leve_httperror` | `Content-Length` non satisfait → `HTTP POST failed` | OK |
| `post_streaming_statut_non_2xx` | statut 500 en streaming → `HTTP POST failed with status 500` | OK |

## 7. Défauts et constats découverts (aucun correctif appliqué)

### Constats sur la production

1. **Correction du constat R1 de PHASE 1 — risque NON ATTEIGNABLE.**
   PHASE 1 signalait un risque élevé : exceptions `.at("id")` / `.at("function").at("name")`
   hors `try` dans la boucle de dispatch (`agent.cpp:246-248`) pouvant corrompre durablement
   l'historique. Le test `agent.tool_call_sans_id_ni_nom_ne_corrompt_pas_le_flux` démontre que
   `build_assistant_message` (`agent.cpp:148-175`) crée **inconditionnellement** les clés `id`,
   `type`, `function.name` et `function.arguments` : `.at()`/`get<std::string>()` **ne peuvent
   donc pas lever** sur le chemin streaming. Un delta ne fournissant que `index` produit
   `id=""`, `name=""`, `arguments="{}"` → `tool error: unknown tool: ` puis le tour continue
   normalement. **R1 est reclassé : non atteignable par le chemin de code actuel.**
   *(Aucun changement de code : c'est l'audit qui est corrigé, pas la production.)*
2. **Constat R10 confirmé** : `read_file` sur un fichier **texte** non lisible (permissions
   `000`) échoue avec `cannot read binary file: texte.txt` — `appears_binary()` retourne `true`
   quand le fichier ne peut pas être ouvert (`workspace_tools.cpp:97-102`). Message trompeur.
   Encodé par le test `..._DEFECT_DOCUMENTE`, qui documente le comportement **observé**, pas un
   comportement souhaitable.
3. **Constat R2 confirmé** : sans `flush()`, un événement SSE final non terminé par une ligne
   vide est perdu (`sse.evenement_incomplet_en_fin_de_flux` → 0 payload).
4. **Observation mineure** : un statut non-2xx avec corps vide produit un message se terminant
   par `": "` (ex. `HTTP POST failed with status 500: `). Cosmétique.
5. **`HttpClient::post()` est fonctionnel** (3 cas de test) mais **toujours non appelé** par
   `Agent` : la dette « code mort applicatif » de PHASE 1 reste ouverte, désormais *testée*.

### Défauts du harness lui-même (corrigés pendant cette phase, transparence)

6. Première compilation **en échec** : `TempDir` non qualifié (`tests::TempDir`) dans
   `workspace_tools_test.cpp` (erreurs en cascade sur `schemas`/`schema`/`get<...>`).
7. `#include <fstream>` manquant dans `agent_streaming_test.cpp`.
8. Avertissement `-Wdangling-reference` dans la macro `CHECK_EQ` (référence liée à un
   temporaire) → passage en `const auto` par valeur.
9. `http.post_statut_non_2xx_leve_httperror_avec_le_corps` échouait : le test émettait **deux**
   requêtes pour **une** réponse scriptée. Corrigé en capturant le message une seule fois.
   *(Cet échec était un défaut du test, pas de la production.)*
10. Un helper `message_count` défini deux fois puis supprimé (inutilisé).

**Ces cinq points sont des corrections du harness uniquement : aucune ligne de `src/` ou
`include/` n'a été touchée.**

## 8. Limites du harness

| # | Limite | Nature |
|---|---|---|
| L1 | `trim_history`, `append_message` (privés) et les helpers de reconstruction/`sandbox_path`/`is_hidden_path` (namespace anonyme) sont couverts **indirectement**, pas unitairement | nécessiterait une évolution de production (non faite, conformément à la consigne) |
| L2 | `post_streaming_fragmente_et_accumule` dépend du **timing** (3 o / 5 ms) pour obtenir >1 callback | 5 exécutions consécutives stables, mais non garanti formellement |
| L3 | `CHECK_THROWS_MSG` traite une exception à `what()` vide comme une absence d'exception | limite du micro-harnais, documentée dans `test_support.hpp` |
| L4 | Le test d'arguments invalides s'appuie sur le texte `parse_error` de nlohmann/json **v3.11.3** | couplage à la version de la bibliothèque |
| L5 | Les tests de permissions sont **ignorés (SKIP)** en cas d'exécution en `root` | exécution actuelle : uid 1000, donc réellement exécutés |
| L6 | **Non couverts** : `list_files` sans plafond (R7), buffer SSE non borné (R3), concurrence sur `write_file` (R6), TOCTOU/liens symboliques au-delà du cas testé (R5), `finish_reason` jamais lu (constat C4 de PHASE 1) | trous de couverture identifiés |
| L7 | Aucun sanitizer (ASan/UBSan), aucune mesure de couverture de code | non demandé à cette phase |
| L8 | Aucun échange avec un serveur LLM réel : le format exact des deltas d'un serveur donné n'est pas validé | interdit par le périmètre |
| L9 | Le mock loopback ne valide pas les en-têtes envoyés (`tools`, `stream: true`) | non vérifié : le corps est inspecté, pas les en-têtes |

## 9. Écarts par rapport au plan initial

1. **Groupes CTest** : le plan esquissait un groupe `registry` séparé. Les tests `ToolRegistry`
   ont été placés dans le groupe `workspace` (préfixe `registre_`) afin d'éviter un groupe CTest
   vide (le binaire retourne `2` si aucun cas ne correspond au filtre).
2. **Tests B et C (tool calls, historique)** : l'énoncé les listait comme tests unitaires. Ils
   sont implémentés en **tests d'intégration via l'API publique + mock loopback**, pour éviter
   toute modification de production (voir §3, contrainte d'interface).
3. **Cas ajoutés** non explicitement listés : sortie par **lien symbolique**, `check` du
   **caractère littéral/casse** de `search_files`, **double enregistrement** au registre,
   **permissions préservées**, **absence de résidu** de fichier temporaire, **plafond de la
   boucle d'outils**, **échec curl** et **réponse tronquée** du client HTTP.
4. **Itérations de correction** du harness nécessaires (5 défauts, §7) : la première compilation
   n'était pas verte. Signalé ici plutôt que masqué.

## 10. Synthèse « exécuté / échoué / non exécuté / non testable »

| Catégorie | Détail |
|---|---|
| **Exécutés et réussis** | 59 cas / 225 assertions ; 4 tests CTest ; 5 exécutions consécutives ; build propre rc=0 avec 0 avertissement |
| **Exécutés et échoués** | 1 cas lors d'une exécution intermédiaire (`http.post_statut_non_2xx…`), **défaut du test**, corrigé puis re-vérifié vert |
| **Non exécutés** | les cas de permissions si `root` (non le cas ici) ; aucun autre |
| **Non testables sans évolution de production** | test unitaire direct de `trim_history`, `append_message`, `apply_stream_delta`, `apply_tool_call_delta`, `build_assistant_message`, `has_tool_calls`, `sandbox_path`, `is_hidden_path` (couvert indirectement) |

## 11. Verdict

**PASS.**

Critères de sortie vérifiés :

| Critère | Statut |
|---|---|
| Harness reproductible | ✅ commandes fournies, hors ligne, activable par `-DAGENT_BUILD_TESTS=ON`, répertoire de build vierge accepté |
| Tests isolés et déterministes | ✅ `TempDir` RAII par test, mock loopback à port éphémère, 5 exécutions identiques |
| Build propre validé | ✅ rc=0, 0 erreur, 0 avertissement, aucune récupération réseau |
| Résultats et limites documentés | ✅ §6, §7, §8 |
| Aucune modification fonctionnelle de production | ✅ `src/` et `include/` intacts (`git diff` vide) ; seul ajout : option CMake **OFF par défaut** |

Réserves explicites (ne remettent pas en cause le verdict, mais bornent la portée) :
- la couverture de `trim_history` et des helpers de reconstruction est **indirecte** (L1) ;
- des risques identifiés en PHASE 1 restent **non couverts** (L6 : R3, R5, R6, R7, `finish_reason`) ;
- une réserve de **déterminisme** subsiste sur un cas de streaming (L2).

Ces réserves portent sur l'**étendue de la couverture**, non sur la validité du harness.

**Arrêt à ce stade** : PHASE 1.2 non engagée, aucun défaut corrigé, aucune validation humaine
présumée acquise.
