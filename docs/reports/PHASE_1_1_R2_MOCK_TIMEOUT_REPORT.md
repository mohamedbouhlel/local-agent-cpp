# P1.1-R2 — Remédiation du mock HTTP bloquant

> Itération de **correction** ciblée sur le constat **R-01** de
> [`PHASE_1_1_REVIEW_REPORT.md`](PHASE_1_1_REVIEW_REPORT.md).
> **Aucun fichier de production modifié.** Aucun `git add`, aucun commit, aucun
> push. Le présent rapport est un **nouveau** fichier ; aucun rapport existant
> n'a été écrasé.

## 1. Objectif et périmètre

Rendre le mock HTTP de test résilient aux **connexions silencieuses** et aux
**requêtes incomplètes**, qui pouvaient l'immobiliser indéfiniment. Le périmètre
est strictement limité à :

- `tests/mock_http_server.hpp` / `tests/mock_http_server.cpp` (le mock) ;
- un nouveau fichier de tests ciblés ;
- `tests/CMakeLists.txt` (enregistrement du nouveau groupe + garde-fou CTest).

**Hors périmètre, volontairement** : le code de production, les autres tests
existants, les autres constats du rapport de revue (R-02 à R-08), le
`CMakeLists.txt` racine et `docs/`.

**Conflit éventuel avec `AGENTS.md`** : aucun. Les règles applicables
(`AGENTS.md:103` pas de framework externe — aucun ajouté ; `AGENTS.md:104-105`
fichiers courts et responsabilité unique — respecté) sont compatibles avec la
consigne.

## 2. État Git initial

| Élément | Valeur au démarrage |
|---|---|
| Répertoire | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `4f33b623c17ff6e23e7b38694300128d51a9d418` |
| `git status --short --branch` | `## main` · ` M CMakeLists.txt` · `?? docs/` · `?? tests/` |
| Modifications préexistantes | `CMakeLists.txt` : `+8` (livrable P1.1) — **préservée** |
| Non suivis préexistants | `docs/` (7 fichiers), `tests/` (9 fichiers) — **préservés** |

## 3. Constat R-01 (rappel, non re-litigé)

> Mock mono-thread sans timeout : une connexion silencieuse bloque le serveur
> indéfiniment ; sonde antérieure expirée avec le **code 124**.
> Sévérité **MAJEUR**, impact **latent** (la suite n'était pas affectée).

## 4. Analyse de la cause

Chaîne exacte du blocage, établie par lecture du code **avant** modification :

| Étape | Code (avant) | Effet |
|---|---|---|
| 1 | `run():152-164` — `accept()` puis `handle_connection()` **en ligne** | un seul client traité à la fois : `accept()` n'est rappelé qu'après le retour de `handle_connection` |
| 2 | `handle_connection():166-203` — boucle `recv()` (`:198`) tant que l'en-tête + le corps annoncé ne sont pas complets (`:194`) | **aucun délai** posé : seuls `SO_REUSEADDR` (écoute, `:92`) et `TCP_NODELAY` (connexion, `:169`) l'étaient ⇒ `recv` **bloque indéfiniment** sur un client silencieux |
| 3 | `run():152-164` | le blocage de l'étape 2 empêche tout nouvel `accept()` : les connexions suivantes restent dans le backlog (`listen(listen_fd_, 8)`) ⇒ le client de test expire sur **son** timeout |
| 4 | `~MockHttpServer():115-127` — `stopped_=true` + `shutdown(listen_fd_, SHUT_RDWR)` | ne réveille que `accept()`. Si le thread est dans `recv` sur la **socket acceptée**, ce `shutdown` est sans effet ⇒ `thread_.join():122` **bloque sans fin** ⇒ processus suspendu (code 124) |

**Conséquences sur les tests** : un scénario laissant une connexion incomplète
(bug de test, ou régression de production empêchant l'envoi complet) faisait
**pendre** le binaire de test — et donc un job CI entier — au lieu d'échouer.

## 5. Fichiers modifiés et justification

| Fichier | Nature | Justification |
|---|---|---|
| `tests/mock_http_server.hpp` | modifié (76 l.) | délai de lecture configurable + suivi de la connexion active |
| `tests/mock_http_server.cpp` | modifié (291 l.) | `SO_RCVTIMEO`, réponse `408`, reprise sur `EINTR`, réveil au destructeur |
| `tests/mock_http_server_test.cpp` | **créé** (184 l.) | tests ciblés R-01 (§7) |
| `tests/CMakeLists.txt` | modifié (25 l.) | enregistrement du groupe `mock` + `TIMEOUT` CTest |

**Aucun autre fichier touché.** `src/` et `include/` : inchangés (vérifié,
§10). `CMakeLists.txt` racine : diff `8 0` identique à l'initial (non touché).

## 6. Description de la correction

Correction **minimale**, trois changements ciblés :

1. **Bornage de la lecture** — `SOL_SOCKET/SO_RCVTIMEO` posé sur chaque
   connexion acceptée, à partir d'un délai **configurable** :
   `explicit MockHttpServer(int read_timeout_ms = 2000)` (`mock_http_server.cpp:101-102`,
   `:200-201`). Défaut 2000 ms, très au-dessus des besoins loopback ; les tests
   négatifs passent un délai court (300 ms) pour rester rapides.
2. **État contrôlé sur expiration** — sur `EAGAIN`/`EWOULDBLOCK`, le serveur
   répond `HTTP/1.1 408 Request Timeout` puis ferme
   (`respond_request_timeout()`, `:89-95` ; appel `:241-243`) ; `status_text()`
   connaît 408 (`:77`). La requête incomplète **n'est pas enregistrée** dans
   `received_bodies()` — seules les requêtes complètes le sont désormais.
3. **Arrêt non bloquant** — la connexion en cours est suivie dans
   `std::atomic<int> active_connection_{-1}` (`mock_http_server.hpp:70`), publiée
   autour de `handle_connection` (`:186-188`) et **réveillée par le destructeur**
   (`:142-145`). L'arrêt devient immédiat au lieu d'attendre le délai.
4. **`EINTR` sur `recv`** — reprise de la lecture (`:235-237`) plutôt que perte
   d'une requête valide ; pas d'attente active (on re-bloque sur `recv`).

**Aucune nouvelle dépendance** (POSIX déjà utilisé : `<sys/socket.h>`,
`<sys/time.h>`, `<cerrno>`).

**Garde-fou CTest** : `set_tests_properties(${group} PROPERTIES TIMEOUT 30)`
(`tests/CMakeLists.txt:24`) ⇒ un éventuel blocage futur devient un **échec**
exploitable au lieu d'un job suspendu.

## 7. Tests ajoutés ou adaptés

Nouveau fichier `tests/mock_http_server_test.cpp`, groupe `mock` (5 cas,
14 assertions). Les sockets clients bruts portent leur **propre** `SO_RCVTIMEO`
(3 s) afin qu'une régression fasse **échouer** le test au lieu de le faire pendre.

| Cas | Ce qui est démontré |
|---|---|
| `requete_valide_toujours_servie` | une requête HTTP valide reste servie ; `received_bodies()==1` ; `pending_responses()==0` |
| `connexion_silencieuse_recoit_408_sans_bloquer` | aucune donnée envoyée ⇒ **408 Request Timeout** reçu (borne 3 s), puis la requête suivante est servie ; la requête jamais complétée **n'est pas** enregistrée |
| `requete_incomplete_scenario_r01_ne_bloque_pas` | **reproduction exacte de la sonde R-01** (en-tête complet, `Content-Length: 100`, 1 octet de corps) ⇒ 408, puis service normal |
| `arret_prompt_apres_scenario_de_timeout` | après le 408, la destruction du serveur est prompte (< 1500 ms ; mesuré **0 ms**) |
| `arret_prompt_pendant_une_lecture_bloquee` | délai de lecture 5000 ms + connexion silencieuse : la destruction est prompte (mesuré **0 ms**) ⇒ prouve le réveil de la connexion active |

**Tests existants** : aucun modifié (les 59 cas antérieurs sont inchangés et
repasent — §9). Seul `tests/CMakeLists.txt` a été ajusté (ajout de source,
groupe `mock`, `TIMEOUT`).

## 8. Commandes de validation et résultats observés

| Commande | Code retour | Résultat observé |
|---|---|---|
| `cmake -S . -B /tmp/p11r2b-tests -DAGENT_BUILD_TESTS=ON …FETCHCONTENT_FULLY_DISCONNECTED=ON` | 0 | configuration hors ligne OK |
| `cmake --build /tmp/p11r2b-tests` | 0 | **0 erreur, 0 avertissement** |
| `agent_tests --filter=mock` | 0 | **5 cas / 14 assertions / 0 échec** ; arrêts mesurés **0 ms** |
| `agent_tests` (suite complète) | 0 | **64 cas / 239 assertions / 0 échec** · `ECHEC: 0 · FAIL: 0 · SKIP: 0` |
| `ctest --output-on-failure` | 0 | **5/5 Passed** (`sse` `workspace` `http` `agent` `mock`), 1.59 s |
| 15 × `agent_tests` | 0 | **0 échec / 15** |
| 5 × `agent_tests --filter=mock` | 0 | **0 échec / 5** |
| 3 × `ctest` | 0 | `100% tests passed, 0 tests failed out of 5` (3/3) |
| `cmake … -DAGENT_BUILD_TESTS=OFF` + build | 0 / 0 | cibles `agent`+`agent_core` ; `ctest -N` ⇒ **0 test** |
| **Rejeu de la sonde R-01** (`/tmp`, garde-fou `timeout 30`) | **0** | `C: requete suivante SERVIE, corps=[corps-servi]` puis `D: destruction` ⇒ **contre `124` avant correctif** |
| **Test de mutation A** — mock privé de `SO_RCVTIMEO` | **1** | **6 assertions / 3 cas** en échec (dont le scénario R-01) ⇒ détecté |
| **Test de mutation B** — mock privé du réveil de connexion | **1** | `elapsed < 1500` violé : arrêt mesuré **5017 ms** ⇒ détecté |

Les mutations ont été appliquées à des **copies** dans `/tmp` (aucune source du
dépôt modifiée) ; artefacts supprimés après usage.

## 9. Limites et réserves

| # | Réserve | Portée |
|---|---|---|
| L1 | **Course résiduelle bornée au destructeur** : si une connexion est acceptée entre la lecture de `active_connection_` et sa publication, le réveil la manque ; l'attente est alors bornée par le délai de lecture (≤ 2000 ms par défaut) — **jamais indéfinie** | très faible ; documenté |
| L2 | `received_bodies()` ne contient plus les requêtes **incomplètes** (changement sémantique voulu). Aucun test existant concerné (tous passent par `HttpClient`, qui envoie des requêtes complètes) | documenté |
| L3 | Le cas `arret_prompt_pendant_une_lecture_bloquee` comporte un `sleep_for(200 ms)` pour laisser le serveur accepter la connexion : **synchronisation bornée**, avec marge large (assertion 1500 ms vs 5000 ms de délai) et filet de sécurité `TIMEOUT 30` | acceptable |
| L4 | **Hors périmètre, non traités** : R-02 (500 silencieux), R-03 (`pending_responses()` non utilisé dans les tests antérieurs — la nouvelle suite l'utilise), R-04 (`CHECK_NO_THROW` inutilisé), R-05 (timing du streaming), R-06 (`EINTR` sur `accept`), R-07 (en-têtes non inspectés), R-08 (`what()` vide) | inchangés |
| L5 | La correction rend le blocage **borné** sans rendre le mock **multi-thread** : deux clients lents restent servis séquentiellement (≤ 2 × délai). Acceptable pour un mock de test | choix assumé |
| L6 | Le groupe `mock` ajoute ~1.2 s au temps CTest | mesuré |

**Aucune validation annoncée n'est `NON EXÉCUTÉE`** : toutes les lignes du §8
ont été réellement lancées dans cette itération.

## 10. État Git final

```bash
$ git status --short --branch
## main
 M CMakeLists.txt
?? docs/
?? tests/

$ git diff --stat
 CMakeLists.txt | 8 ++++++++
 1 file changed, 8 insertions(+)

$ git rev-parse HEAD
4f33b623c17ff6e23e7b38694300128d51a9d418
```

**Contrôle de non-régression du périmètre** :

- `src/` + `include/` : **INCHANGÉS** (`git diff --quiet` ⇒ propre) ;
- `CMakeLists.txt` racine : `8 0`, **identique à l'état initial** — modification
  **préexistante** de P1.1, non produite ni modifiée par cette itération ;
- documents existants (`docs/reports/PHASE_1_1_REPORT.md`,
  `docs/reports/PHASE_1_1_REVIEW_REPORT.md`, documents de référence) :
  **non écrasés, non modifiés** ;
- seul ajout de documentation : **ce rapport**.

**Confirmation explicite** : **aucun staging** (`git diff --cached` vide),
**aucun commit**, **aucun push**, **aucun changement de branche**, aucune
commande destructive, aucun fichier de production ou de configuration modifié,
HEAD **inchangé**.

## 11. Verdict

### PASS

Justification : le mécanisme du blocage a été établi avant toute modification
(§4) ; la correction est **minimale**, sans nouvelle dépendance, et porte
exclusivement sur le mock de test ; les **cinq** exigences fonctionnelles de la
consigne sont satisfaites et **démontrées** — requête valide toujours servie,
connexion silencieuse et requête incomplète **bornées** avec `408` contrôlé,
arrêt et nettoyage **prompts** (0 ms mesurés), aucune attente active ; les tests
ciblés ajoutés **détectent** les deux régressions simulées (mutations A et B) et
la **sonde R-01 d'origine passe de `124` à `0`** ; la suite complète
(**64 cas / 239 assertions / 0 échec**), CTest (**5/5**), 15 répétitions et 3
passes CTest sont vertes ; le mode `AGENT_BUILD_TESTS=OFF` compile toujours et
n'expose aucune cible de test ; **aucune modification de production**, rapport
durable créé et vérifié.

Les réserves L1 à L6 sont **documentées et non bloquantes** ; les constats R-02 à
R-08 du rapport de revue restent **ouverts** (hors périmètre de cette itération).
