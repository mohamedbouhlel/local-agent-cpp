# P1.1-R — Revue indépendante du harness de tests

> Itération de **revue** : aucune correction de code, de test ou de configuration.
> **Seul fichier créé/modifié : ce rapport.** Toutes les vérifications ci-dessous
> ont été **réellement exécutées pendant cette itération** ; aucune donnée n'est
> reprise d'une exécution antérieure sans l'avoir été refaite.
> Ce document **remplace** le contenu précédent du même fichier (revue du même
> jalon réalisée plus tôt dans la session) : mêmes constats, ré-établis ici sur
> l'état courant, dans le format exigé.

---

## 1. OBJECTIF

Évaluer indépendamment le harness de tests introduit en P1.1 : conception,
représentativité des assertions, couverture, déterminisme, reproductibilité,
isolation, intégration CMake/CTest, faux positifs. **Aucune correction.**

## 2. ÉTAT INITIAL

| Élément | Valeur observée |
|---|---|
| Répertoire | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `4f33b623c17ff6e23e7b38694300128d51a9d418` |
| `git status --short --branch` | `## main` · ` M CMakeLists.txt` · `?? docs/` · `?? tests/` |
| `CMakeLists.txt` | **modifié non commité** : `+8` (option `AGENT_BUILD_TESTS` + `enable_testing()` + `add_subdirectory`) |
| `tests/` | **non suivi**, 9 fichiers |
| `docs/` | **non suivi**, 7 fichiers (4 documents de référence + `README.md` + 2 rapports) |

**Comparaison à la référence documentée.** [`PROJECT_CONTEXT.md`](../context/PROJECT_CONTEXT.md)
(§2) déclare HEAD `4f33b62` **et** « arbre de travail propre ». Or l'arbre n'est
**pas propre** : `CMakeLists.txt` est modifié et `docs/`/`tests/` sont non suivis.

**Divergence documentée, non bloquante et attendue** : elle correspond exactement
à ce que les itérations P1.1 ([`PHASE_1_1_REPORT.md`](PHASE_1_1_REPORT.md) §1) et
P1.1-DOC ont produit et déclaré. Aucune réinitialisation, suppression ni
écrasement n'a été effectué. La revue reste techniquement fiable : HEAD est
identique à la référence, et les écarts portent sur des ajouts **déclarés**.

**[NON VÉRIFIÉ]** : ce dépôt ne possède ni remote ni CI ; il est impossible de
corroborer l'état par une source externe.

## 3. PÉRIMÈTRE

**Examiné intégralement** : `CMakeLists.txt` ; `tests/` (9 fichiers :
`test_support.hpp`, `test_main.cpp`, `mock_http_server.hpp/.cpp`,
`sse_parser_test.cpp`, `workspace_tools_test.cpp`, `http_client_test.cpp`,
`agent_streaming_test.cpp`, `CMakeLists.txt`).

**Sources de production examinées** (concernées par les tests) :
`src/agent.cpp` (assembly des tool_calls, historique, dispatch),
`src/http_client.cpp` (erreurs, streaming), `src/sse_parser.cpp` (framing),
`src/tool_registry.cpp` (exécution), `src/workspace_tools.cpp` (sandbox, limites),
`src/main.cpp` (config/REPL), `include/*.hpp`.

**Rapports P1.1 comparés** : [`PHASE_1_1_REPORT.md`](PHASE_1_1_REPORT.md) (objectifs,
59 cas annoncés, verdict PASS) et la version précédente du présent fichier.
Rappel de gouvernance appliqué : [`AGENTS.md`](../../AGENTS.md) (106 l., lu
intégralement) — notamment `:103` « Ne pas ajouter de framework externe sans le
demander explicitement » et `:41-43` (un harnais peut lier `agent_core`).

## 4. MÉTHODE

1. **Statique** : lecture intégrale des 9 fichiers de `tests/` et du diff
   `CMakeLists.txt` ; repérage des lignes ; comptage des cas (`TEST`) et des
   assertions (`CHECK*`) ; recherche d'assertions triviales, de mocks permissifs,
   de timeouts absents, de dépendances à l'ordre/au temps/à l'environnement.
2. **Exécutable** : configuration et compilation dans **deux répertoires de build
   hors dépôt** (`/tmp/p11r2-tests` avec `AGENT_BUILD_TESTS=ON`,
   `/tmp/p11r2-prod` avec `OFF`), source `nlohmann/json` locale
   (`FETCHCONTENT_SOURCE_DIR_JSON` + `FETCHCONTENT_FULLY_DISCONNECTED=ON`) ⇒
   **aucun accès réseau**. Exécution complète, CTest, filtre invalide,
   20 répétitions, CTest répété 3 fois.
3. **Test de mutation** (preuve d'efficacité des assertions) : deux mutations
   appliquées à des **copies** de `src/agent.cpp` dans `/tmp`, suite recompilée
   contre la copie mutée. Les sources suivies n'ont **pas** été touchées.
4. **Sonde de robustesse du mock** (compilée dans `/tmp`, bornée par `timeout`) :
   connexion TCP incomplète puis requête normale.
5. `build/` existant **non touché** ; `.env` et `build/.env` **non lus** ; aucun
   `git add`, aucun commit, aucun changement de branche.

## 5. ANALYSE DU HARNESS

### 5.1 Correspondance comportement / assertions

| Groupe | Ce qui est **réellement vérifié** | Assertions | Démonstration |
|---|---|---|---|
| `sse` (16 cas) | framing : événement complet, payload coupé entre fragments, séparateur coupé, N événements par chunk, événement final non terminé (**perdu** — comportement observé/consigné `tests/sse_parser_test.cpp:80`), lignes vides, charge non-JSON retournée brute, `[DONE]` tel quel, CRLF, multi-`data:` jointes, commentaires ignorés, octet par octet | 38 `CHECK_EQ` | valeurs exactes attendues, pas de simple « ne plante pas » |
| `workspace` (25 cas) | registre (4 outils, ordre `std::map`, doublon rejeté) ; écriture/lecture ; parents créés ; garde `overwrite` ; **atomicité + absence de résidu** (`:179`) ; permissions préservées ; `..`/absolu/`.`/lien symbolique (`:258`) ; arguments manquants/mal typés ; cible répertoire ; limites 200 000 (`:305`) / 50 000 ; binaire rejeté ; `list_files` hors cachés ; `search_files` littéral/casse/plafond 50 (`:394`) ; permissions d'écriture ; **défaut documenté** (`:451`) | 22 `CHECK_THROWS_MSG` + 20 `CHECK_EQ` + 16 `CHECK` | sous-chaînes de messages d'erreur **exactes**, contenus relus, absence de fichier vérifiée |
| `http` (6 cas) | `post` nominal ; non-2xx avec corps ; streaming fragmenté + concaténation + retour ; échec curl ; réponse tronquée ; 500 en streaming | 3 `CHECK_THROWS_MSG` + 5 `CHECK_EQ` + 4 `CHECK` | le streaming vérifie `concaténation == corps` **et** `returned == corps` (`tests/http_client_test.cpp:54-81`) |
| `agent` (12 cas) | reconstruction des tool_calls (fragments, index entrelacés, arguments absents, delta sans index, outil inconnu, arguments invalides, champs vides — `:371`) ; deltas non conformes ignorés ; ordre de l'historique ; **troncature par tours** (`:472`, invariant `messages.size() <= 20` `:501`) ; **invariant anti-partiel** (`:520`) ; **plafond 5 itérations** (`:570`) | 41 `CHECK_EQ` + 28 `CHECK` + 1 `CHECK_THROWS_MSG` | inspection des **requêtes réellement émises** vers le mock (corps JSON parsé), pas seulement du retour |

**Aucun cas ne se contente d'être « exécuté sans crash »** : chaque cas assert au
moins une valeur ou un message précis. Les 22 `CHECK_THROWS_MSG` du groupe
`workspace` vérifient le **libellé** de l'erreur attendue, pas la simple levée.

**Couverture indirecte assumée** (documentée par P1.1 §3) : `trim_history` (privé),
`apply_stream_delta`/`apply_tool_call_delta`/`build_assistant_message`/`has_tool_calls`
et `sandbox_path`/`is_hidden_path` (namespace anonyme) ne sont atteignables que
via `Agent::send` / `ToolRegistry::execute`. Les tests les exercent par ce
chemin, ce qui **démontre le comportement observable** mais ne permet pas
d'isoler une branche interne précise.

### 5.2 Couverture

**Présent** : cas nominaux ; cas limites (200 000 / 200 001, 50 000 / en dessous,
50 résultats + marqueur, plafond 5 itérations, troncature à 12 tours) ; erreurs
(curl, non-2xx, réponse tronquée, fichier absent, répertoire cible) ; entrées
invalides (`..`, absolu, `.`, arguments mal typés, JSON invalide, index manquant) ;
états vides (`choices` vide, `content: null`, événement SSE vide, workspace vide
de `list_files`) ; interactions entre composants (`Agent` ↔ `HttpClient` ↔
`SseParser` ↔ `ToolRegistry` ↔ outils).

**Zones non couvertes (vérifiées par lecture du code de production, non testées)**

| Zone | Référence |
|---|---|
| `list_files` **sans plafond** de résultats | `src/workspace_tools.cpp:112-128` |
| `buffer_` SSE **non borné** (croissance mémoire) | `src/sse_parser.cpp:82,98` |
| TOCTOU du sandbox (validation puis usage) | `src/workspace_tools.cpp:65-70` |
| Concurrence sur `write_file` (aucun verrou) | `src/workspace_tools.cpp:292-325` |
| `finish_reason` **jamais lu** | absent de `src/agent.cpp` |
| En-têtes HTTP émis (`stream: true`, `tools`, `Authorization`) | `src/agent.cpp:45-58,189-194` |
| Comportement contre un **vrai** serveur LLM | hors périmètre (interdit) |
| `HttpClient::post()` : **est** testé (3 cas) mais reste non utilisé par `Agent` | `src/http_client.cpp:138-146` |

**Aucun pourcentage de couverture n'est avancé** : aucune mesure d'instrumentation
(`--coverage`/`gcov`/`llvm-cov`) n'a été exécutée.

### 5.3 Faux positifs

Recherche systématique — **conclusion : aucun faux positif détecté dans le
périmètre vérifié** :

- **assertions triviales** : `grep` sur `CHECK(true|false)`, `(void)`, comptages
  inertes ⇒ **aucune** ;
- **tests qui réussiraient avec une implémentation incorrecte** : éprouvé par
  **mutation** (§7.3) — 2 mutations réellement détectées ;
- **mocks trop permissifs** : le mock est au contraire **trop peu** permissif sur
  un point (§8 R-02 : 500 silencieux au lieu d'une erreur de harness) ; il ne
  valide pas les en-têtes (§8 R-07) ;
- **résultats acceptant plusieurs comportements incompatibles** : chaque
  `CHECK_EQ` compare une valeur unique ; les tolérances se limitent à
  `fragments.size() > 1` (attendu : livraison progressive, non un compte exact) ;
- **erreurs silencieusement ignorées** : les erreurs d'outil sont assertées avec
  leur message (`tool error: unknown tool: …`, `… parse_error …`) ;
- **un faux positif historique a été corrigé pendant P1.1** : un test de
  non-2xx émettait **deux** requêtes pour **une** réponse scriptée (la seconde
  recevait le 500 par défaut) — corrigé en P1.1, signalé comme écart dans
  [`PHASE_1_1_REPORT.md`](PHASE_1_1_REPORT.md) §7 et §9.

### 5.4 Déterminisme

| Dépendance | Constat |
|---|---|
| Ordre d'exécution | **aucune** : pas d'état statique mutable hors registre du harnais ; chaque groupe passe **en processus séparé** (CTest) ; CTest répété **3 ×** ⇒ `100% tests passed, 0 tests failed out of 4` à chaque fois |
| Temps | **un cas** dépend du timing : `tests/http_client_test.cpp:54-81` (morceaux de 3 o, délai 5 ms) pour obtenir >1 callback |
| Hasard | aucun (`<random>` absent des tests) |
| Environnement / variables | **aucune lecture** : `grep getenv\|environ\|\.env` dans `tests/` ⇒ rien |
| État global | registre de cas + compteurs du harnais uniquement |
| Fichiers/répertoires temporaires | `tests::TempDir` RAII (`tests/test_support.hpp:80-97`), un répertoire dédié **par cas**, nommé par *tag* |
| Réseau / services externes | **mock loopback uniquement** (`INADDR_LOOPBACK`, port `0` éphémère) — observés : `33355`, `40957` |
| Ressources système | ports éphémères attribués par le noyau ⇒ pas de collision entre cas |

**Preuve de répétabilité effectuée** : **20 exécutions consécutives** du binaire
⇒ `59 cas / 225 assertions / 0 échec` à chaque fois, **0 exécution en échec** ;
+ 3 passes CTest. Il s'agit d'une **preuve empirique, pas d'une démonstration
formelle** : le cas cité (§8 R-05) reste tributaire de l'ordonnancement TCP.

### 5.5 Isolation

| Contrôle | Constat |
|---|---|
| Fichiers temporaires | `TempDir` : `remove_all` avant création (idempotent) et en destructeur, avec `std::error_code` ⇒ **non lançant** (`tests/test_support.hpp:86,94`) |
| Nettoyage après échec | le runner capture toute exception non gérée (`tests/test_main.cpp:26-31`) et poursuit ⇒ déroulement de pile ⇒ destructeurs exécutés ; `PermissionRestorer` (`tests/workspace_tools_test.cpp:74-92`) restaure les droits **avant** la suppression |
| Réutilisation d'état | aucun état partagé entre cas ; un `ToolRegistry` et un `MockHttpServer` par cas |
| Collisions entre cas | *tags* distincts par cas ; ports éphémères ⇒ pas de collision |
| Dépendances entre groupes | **aucune** (4 processus distincts, tous verts isolément) |
| Ports / processus | 1 thread serveur par mock, port noyau-attribué, socket fermée au destructeur |
| `workspace/` du dépôt | **jamais utilisé** : vérifié — `workspace/` ne contient que `.gitkeep` |
| Résidus après 20 exécutions | **aucun** `agent_cpp_test_*` |

**Limite démontrée** : §8 R-01 — le mock est **mono-thread sans timeout** ; une
connexion acceptée mais jamais complétée bloque `accept()` puis le destructeur.

### 5.6 Diagnostics

| Question d'un échec | Réponse fournie |
|---|---|
| Quel cas ? | `[ RUN   ] <groupe>.<cas>` / `[  ECHEC  ] <groupe>.<cas>` |
| Quelle condition attendue ? | `FAIL <fichier>:<ligne>  <expression>` (l'expression source est stringifiée) |
| Quelle valeur obtenue ? | pour `CHECK_EQ` : `[obtenu=[…] attendu=[…]]` ; pour `CHECK_THROWS_MSG` : `[obtenu=[…] attendu contenant […]]` |
| Quelle étape ? | `fichier:ligne` pointent la ligne fautive ; exception non gérée ⇒ `exception non geree: <what>` |
| Sortie exploitable ? | code retour **0** (OK) / **1** (échecs) / **2** (aucun cas exécuté) + résumé `cas executes / assertions / assertions en echec` |

**Vérifié exécutablement** (sonde dans `/tmp`, sources suivies intactes) : un
`CHECK(1+1==3)` produit `FAIL … 1 + 1 == 3` ; un `CHECK_EQ("obtenu","attendu")`
produit `[obtenu=[obtenu] attendu=[attendu]]` ; une exception non gérée est
capturée et n'interrompt pas les cas suivants ; rc=1.

## 6. ANALYSE CMAKE / CTEST

| Point | Constat |
|---|---|
| Création de l'exécutable de test | `tests/CMakeLists.txt:6` `add_executable(agent_tests …)` — 6 sources |
| Dépendances | `tests/CMakeLists.txt:4` `find_package(Threads REQUIRED)` ; `:15` `target_link_libraries(agent_tests PRIVATE agent_core Threads::Threads)` — **aucune dépendance externe de test** |
| Avertissements | `tests/CMakeLists.txt:16` `-Wall -Wextra -Wpedantic` |
| `enable_testing()` | `CMakeLists.txt:38`, **à l'intérieur** du `if(AGENT_BUILD_TESTS)` (`:36-40`) |
| `add_test()` | `tests/CMakeLists.txt:20-22` — `foreach(group sse workspace http agent)` ⇒ 1 test CTest par groupe, commande `agent_tests --filter=<groupe>` |
| Noms CTest | `sse`, `workspace`, `http`, `agent` — **identiques** aux groupes `TEST(...)` (comptes 16/25/6/12 = 59) |
| Mode production | `-DAGENT_BUILD_TESTS=OFF` → configure **rc=0**, build **rc=0**, cibles `agent`+`agent_core` **seulement**, `ctest -N` ⇒ **Total Tests: 0**, ligne de liaison `libagent_core.a libcurl.so` (ni `Threads`, ni objet de test) |
| Mode tests | `-DAGENT_BUILD_TESTS=ON` → configure **rc=0**, build **rc=0**, **0 erreur / 0 avertissement**, cibles incluant `agent_tests` |
| Cohérence annoncés / compilés / enregistrés / exécutés | **4 annoncés = 4 enregistrés = 4 exécutés (4/4 Passed)** ; **écart : aucun**. 9 fichiers de test compilés dans **1** binaire — attendu (binaire unique + filtre). |

## 7. VALIDATION EXÉCUTABLE

Commandes exactes (§4) et résultats **réellement observés** :

| Commande | Code retour | Résultat |
|---|---|---|
| `cmake -S . -B /tmp/p11r2-tests -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=… -DFETCHCONTENT_FULLY_DISCONNECTED=ON` | 0 | configuration OK, hors ligne |
| `cmake --build /tmp/p11r2-tests` | 0 | **0 erreur, 0 avertissement** |
| `ctest --test-dir /tmp/p11r2-tests -N` | 0 | **4 tests** : `sse`, `workspace`, `http`, `agent` |
| `ctest --output-on-failure` | 0 | **100% tests passed, 0 tests failed out of 4** (0.21 s) |
| `/tmp/p11r2-tests/tests/agent_tests` | 0 | **cas executes : 59 / 59** · **assertions : 225** · **assertions en echec : 0** · `RESULTAT: OK` |
| comptage des échecs/SKIP | — | **ECHEC : 0** · lignes `FAIL` : **0** · **SKIP : 0** |
| `… --filter=nope` (filtre invalide) | **2** | `cas executes : 0 / 59` + `AUCUN CAS EXECUTE (filtre=[nope])` — **pas de faux PASS** |
| 20 × `agent_tests` | 0 | **0 exécution en échec / 20** |
| 3 × `ctest` | 0 | `100% tests passed, 0 tests failed out of 4` (3/3) |
| `cmake … -DAGENT_BUILD_TESTS=OFF` + build | 0 / 0 | cibles `agent`, `agent_core` ; `ctest -N` ⇒ 0 test |
| Sonde de diagnostic (`/tmp`) | 1 | échecs détectés avec fichier:ligne, expression, obtenu/attendu |
| **Mutation M1** (`/tmp`, copie de `src/agent.cpp`) | **1** | 1 cas / 1 assertion en échec : `agent.arguments_absents_normalises_en_objet_vide` |
| **Mutation M3** (idem) | **1** | **7 cas / 10 assertions** en échec, dont `agent.tool_call_sans_id_ni_nom_ne_corrompt_pas_le_flux` ⇒ **l'invariant R1 est bien gardé par un test** |
| Sonde de robustesse du mock (`/tmp`, `timeout 15`) | **124** | requête suivante en **timeout 2 s** puis **blocage au destructeur** (processus tué) |

**Aucune validation n'est déclarée `NON EXÉCUTÉE`** : les 13 commandes ci-dessus
ont toutes été lancées dans cette itération.

## 8. CONSTATS

### R-01 — Mock mono-thread sans timeout : un blocage est possible au lieu d'un échec

- **Sévérité : MAJEUR**
- **Fichier** : `tests/mock_http_server.cpp` — **Lignes** : `157` (`accept`),
  `198` (`recv`), `119` (`shutdown` du destructeur) ; `tests/CMakeLists.txt:20-22`
  (aucun `TIMEOUT` CTest)
- **Preuve** : sonde exécutée pendant cette itération — une connexion brute
  incomplète (1 octet de corps sur `Content-Length: 100`) fait échouer la requête
  suivante (`HTTP POST failed: Operation timed out after 2002 milliseconds`), et
  la **destruction du serveur ne rend pas la main** : processus tué par `timeout`,
  **code 124**.
- **Impact** : **latent**. La suite actuelle n'est pas affectée (toutes les
  requêtes se terminent), mais un futur test qui abandonnerait une requête, ou
  une régression de production qui empêcherait l'envoi complet, **pendrait
  indéfiniment** en CI au lieu d'échouer — mode de défaillance pire qu'un échec.
- **Recommandation** (non appliquée) : `SO_RCVTIMEO` sur la connexion et/ou
  traitement des connexions dans un thread détaché ; rendre le destructeur non
  bloquant (fermer les connexions en cours) ; déclarer
  `set_tests_properties(<groupe> PROPERTIES TIMEOUT 30)`.

### R-02 — 500 silencieux lorsque la file de réponses est vide

- **Sévérité : MINEUR**
- **Fichier** : `tests/mock_http_server.cpp` — **Ligne** : `211-216`
- **Preuve** : lecture ; ce comportement a réellement piégé un test en P1.1
  (2 requêtes pour 1 réponse scriptée ⇒ 500 par défaut au lieu d'une erreur de
  harness), échec rattrapé par une assertion.
- **Impact** : un scénario de test mal calibré produit un 500 silencieux plutôt
  qu'un diagnostic explicite ; peut masquer une erreur de scénario si aucun
  contrôle du nombre de requêtes n'accompagne le cas.
- **Recommandation** (non appliquée) : échouer bruyamment (ou marquer le cas en
  échec) lorsque la file est vide.

### R-03 — `pending_responses()` jamais utilisée

- **Sévérité : MINEUR**
- **Fichier** : `tests/mock_http_server.cpp:146`, `tests/mock_http_server.hpp:50`
  — **aucun** usage dans `tests/*_test.cpp`
- **Preuve** : `grep -rn pending_responses tests/*.cpp` ⇒ 0 résultat.
- **Impact** : aucune assertion ne garantit que **toutes** les réponses scriptées
  ont été consommées ⇒ aggrave R-02.
- **Recommandation** (non appliquée) : asserter `pending_responses() == 0` en fin
  de chaque cas utilisant un mock.

### R-04 — `CHECK_NO_THROW` défini et jamais utilisé

- **Sévérité : MINEUR**
- **Fichier** : `tests/test_support.hpp:149-156`
- **Preuve** : comptage par fichier ⇒ `CHECK_NO_THROW=0` partout.
- **Impact** : aucune couverture perdue (le besoin nominal est déjà couvert par
  `CHECK_EQ`), mais code mort dans le harnais.
- **Recommandation** (non appliquée) : l'utiliser ou le retirer.

### R-05 — Déterminisme du cas de streaming fragmenté tributaire du timing

- **Sévérité : MINEUR**
- **Fichier** : `tests/http_client_test.cpp:54-81` — **Lignes** : `61-62`
  (`piece_size = 3`, `piece_delay_us = 5000`), `81` (`CHECK(fragments.size() > 1)`)
- **Preuve** : 20 exécutions + 3 passes CTest sans échec, mais l'obtention de
  **plusieurs** callbacks repose sur l'ordonnancement des segments TCP.
- **Impact** : risque de flakiness théorique sur une machine très chargée ou si
  l'implémentation réseau agrège les écritures.
- **Recommandation** (non appliquée) : conserver l'assertion de concaténation
  (déterministe) et rendre l'assertion de progressivité non bloquante, ou la
  baser sur un critère indépendant du timing.

### R-06 — `accept()` ne retente pas sur `EINTR`

- **Sévérité : INFO** — **Fichier** : `tests/mock_http_server.cpp:157-160`
- **Preuve** : lecture ; sortie de boucle sur tout retour `< 0`.
- **Impact** : un signal pendant `accept` arrêterait le serveur du cas en cours.
- **Recommandation** (non appliquée) : retenter sur `EINTR`.

### R-07 — Le mock n'inspecte que le corps des requêtes

- **Sévérité : INFO** — **Fichier** : `tests/mock_http_server.cpp:180-220`
- **Preuve** : seuls `Content-Length` et `Expect` sont lus ; les en-têtes ne sont
  ni conservés ni exposés.
- **Impact** : `stream: true`, le tableau `tools` et l'en-tête `Authorization`
  (`src/agent.cpp:50,53-55,192-194`) ne sont **pas** vérifiés par la suite.
- **Recommandation** (non appliquée) : exposer les en-têtes reçus et les asserter.

### R-08 — `capture_error` : `what()` vide indistinguable d'une absence d'exception

- **Sévérité : INFO** — **Fichier** : `tests/test_support.hpp:68-78`
- **Preuve** : la sentinelle d'absence d'exception est une chaîne vide.
- **Impact** : négligeable (aucun cas concerné aujourd'hui) ; limite déjà
  documentée dans le fichier.
- **Recommandation** (non appliquée) : retourner un `std::optional<std::string>`.

### R-09 — Constats positifs établis par vérification (aucune action)

- **INFO** — Les assertions sont **effectives** : les mutations M1 (1 assertion)
  et M3 (10 assertions / 7 cas) sont **détectées**, rc=1. Le test
  `agent.tool_call_sans_id_ni_nom_ne_corrompt_pas_le_flux`
  (`tests/agent_streaming_test.cpp:371`) **garde l'invariant** qui neutralise le
  risque R1 de l'audit PHASE 1 : si `build_assistant_message`
  (`src/agent.cpp:148-175`) cessait de poser la clé `id`, l'exception
  `json.exception.out_of_range.403` sortirait de `Agent::send` et **le test
  échouerait**.
- **INFO** — Aucun `SKIP` : les cas de permissions s'exécutent réellement
  (uid 1000) ; ils sont conçus pour se *skipper* sous `root`
  (`tests/workspace_tools_test.cpp:72,431,457`).
- **INFO** — Le comportement discutable `read_file` → `cannot read binary file`
  sur un fichier **texte illisible** est **documenté par un test explicitement
  nommé** (`tests/workspace_tools_test.cpp:451`), qui encode le comportement
  observé — et non le comportement souhaité. Aucune correction appliquée.

## 9. COUVERTURE ET LIMITES

- **Couverte et démontrée** : framing SSE ; reconstruction des tool_calls
  (fragments, index entrelacés, champs absents/invalides) ; historique (ordre,
  troncature par tours, invariant anti-partiel) ; plafond d'itérations d'outils ;
  client HTTP (succès, non-2xx, streaming, échec curl, réponse tronquée) ;
  sandbox et garde-fous de `write_file` (atomicité, permissions, limites) ;
  `list_files`/`search_files` (cachés, casse, plafond).
- **Couverture indirecte uniquement** (API publique, pas d'isolation de branche) :
  `trim_history`, helpers de reconstruction, `sandbox_path`, `is_hidden_path`.
- **Non couverte** : `list_files` sans plafond ; `buffer_` SSE non borné ; TOCTOU ;
  concurrence `write_file` ; `finish_reason` ; en-têtes HTTP ; comportement contre
  un vrai serveur LLM ; tous les cas de permissions sous `root`.
- **Absence de mesure** : aucun pourcentage de couverture (aucune instrumentation) ;
  l'efficacité des assertions n'a été éprouvée par mutation que sur **`src/agent.cpp`**
  (groupe `agent`) — les groupes `sse`, `workspace` et `http` n'ont pas été mutés.
- **Limite de méthode** : le test de mutation et la sonde de robustesse sont des
  vérifications **hors dépôt** (artefacts dans `/tmp`, supprimés après usage) ;
  elles ne sont **pas reproductibles depuis le dépôt** en l'état.

## 10. ÉTAT GIT FINAL

```bash
$ git status --short --branch
## main
 M CMakeLists.txt
?? docs/
?? tests/

$ git diff --stat
 CMakeLists.txt | 8 ++++++++
 1 file changed, 8 insertions(+)

$ git diff -- docs/reports/PHASE_1_1_REVIEW_REPORT.md
(aucune sortie : le fichier est NON SUIVI, donc hors du diff Git)

$ git rev-parse HEAD
4f33b623c17ff6e23e7b38694300128d51a9d418
```

**Interprétation** : `git diff` ne montre **rien** pour le rapport car `docs/` est
**non suivi** — l'existence et la mise à jour du rapport se constatent par
`test -f` + `wc -l` (§12). HEAD est **identique** à l'état initial. **Aucun
`git add`, aucun commit, aucun changement de branche.**

**Non-régression du périmètre** : `src/`, `include/`, `tests/`, `CMakeLists.txt`,
`README.md`, `AGENTS.md`, `CLAUDE.md`, `USAGE.md` — **aucune modification** par
cette itération (le ` M CMakeLists.txt` est un écart **préexistant**, produit par
P1.1 et documenté ; les `?? docs/` et `?? tests/` sont également préexistants).
Seul changement produit : le présent rapport.

## 11. VERDICT

### PASS AVEC RÉSERVES

**Justification** — les critères de PASS sont satisfaits :

- **Pas de faux positifs détectés** et assertions **prouvées effectives** par
  mutation (2 mutations, toutes deux détectées, rc=1) ;
- **reproductible et isolé** : 59 cas / 225 assertions / 0 échec, 20 exécutions
  consécutives sans échec, 3 passes CTest, aucun `SKIP`, aucun résidu temporaire,
  aucun état partagé, aucune lecture d'environnement, mock confiné au loopback à
  port éphémère ;
- **intégration CMake/CTest conforme** : option OFF par défaut, production liée
  sans dépendance de test, 4 annoncés = 4 enregistrés = 4 exécutés, filtre
  invalide ⇒ rc=2 (pas de faux PASS) ;
- **aucune validation annoncée `NON EXÉCUTÉE`** ;
- **aucune correction appliquée** ; périmètre respecté (seul le rapport écrit).

Le statut **PASS AVEC RÉSERVES** — et non PASS — est retenu parce que la revue
établit un constat **MAJEUR** (R-01 : blocage possible au lieu d'un échec, faute
de timeout côté mock **et** côté CTest), ainsi que des limites de couverture
explicitement circonscrites (§9 : zones non couvertes, mutation limitée au groupe
`agent`, vérifications hors dépôt non reproductibles). Aucun défaut **BLOQUANT**
n'a été identifié : la suite **actuelle** est fiable, reproductible et probante.

Les recommandations de §8 ne sont **pas** appliquées dans cette itération.
