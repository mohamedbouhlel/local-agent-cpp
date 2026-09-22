# P1.1-V2 — Correction E2/E3 (reproductibilité de la suite de tests)

> Itération de **correction ciblée** : deux écarts démontrés en P1.1-V.
> **Aucun `git add`, aucun commit, aucun push.** Aucune modification de `src/`,
> `include/`, du protocole HTTP/SSE, du mock, ni des dettes R-02 → R-08.
> Seuls fichiers touchés : `tests/test_support.hpp` et
> `tests/workspace_tools_test.cpp` ; plus ce rapport (nouveau).

## 1. État Git initial

| Élément | Valeur au démarrage |
|---|---|
| Répertoire | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `1c46372e822c0d982b11628425fdf697cb0653a8` |
| `git status --short --branch` | `## main` · ` M CMakeLists.txt` · `?? docs/...` (8 entrées) · `?? tests/` |
| `git diff --stat` | ` CMakeLists.txt | 8 ++++++++` — **préexistant** (P1.1) |
| `git ls-files tests` | **0 fichier** (suite non versionnée) |
| Rapports présents dans `docs/reports/` | `PHASE_1_1_REPORT.md`, `PHASE_1_1_REVIEW_REPORT.md`, `PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md`, `PHASE_1_1_V_REPRODUCIBILITY_REPORT.md`, `streaming-audit.md` |
| `PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md` | **absent** au démarrage |

## 2. Cause des écarts

### E2 — collision entre exécutions concurrentes (MAJEUR, reproduit 6/6 en P1.1-V)

`tests/test_support.hpp:84` (avant) construisait le répertoire de travail d'un
cas ainsi :

```cpp
path_ = std::filesystem::temp_directory_path() / ("agent_cpp_test_" + tag);
```

Le nom ne dépendait **que du `tag`** — ni PID, ni identifiant de build — et
`TempDir` commence par `remove_all(path_)` puis `create_directories(path_)`.
Deux processus exécutant le **même cas** (deux builds, deux checkouts) visaient
donc **le même chemin** : chacun détruisait le répertoire de l'autre, produisant
des erreurs de type `file already exists: a.txt (pass overwrite: true to
replace it)`, `file does not exist`, `failed to open file for reading`, et
faisait échouer le cas `sortie_par_lien_symbolique_rejetee` (répertoire du lien
supprimé par l'autre processus). Mesure P1.1-V : **6 itérations sur 6 en échec
dans les deux processus**.

### E3 — chemin absolu partagé dans un test de rejet (MINEUR)

`tests/workspace_tools_test.cpp:235,237` (avant) utilisaient en dur
`/tmp/agent_cpp_escape.txt` comme cible d'un test de **rejet** de chemin absolu,
avec l'assertion `!std::filesystem::exists("/tmp/agent_cpp_escape.txt")`.
`/tmp` étant un répertoire **partagé et global**, la présence préalable de ce
fichier (autre processus, exécution antérieure, résidu) aurait fait échouer
l'assertion. Non reproduit en P1.1-V (le fichier était absent), mais
structurellement fragile.

## 3. Modifications exactes

### 3.1 `tests/test_support.hpp` — correctif E2

| Ligne | Avant | Après |
|---|---|---|
| `:12` | — | ajout `#include <unistd.h>` (pour `::getpid()`) |
| `:85-86` | `path_ = temp_directory_path() / ("agent_cpp_test_" + tag);` | `path_ = temp_directory_path() / ("agent_cpp_test_" + std::to_string(::getpid()) + "_" + tag);` |
| `:83-91` | — | commentaire expliquant le suffixe PID et renvoyant au constat E2 |
| reste de la classe | `remove_all` avant, `create_directories`, `canonical` ; destructeur `remove_all` (RAII) | **inchangé** |

Le comportement RAII est **préservé** : un répertoire par instance, nettoyé à la
destruction, avec `std::error_code` (non lançant). Le nom reste **lisible et
déterministe dans sa forme** : `agent_cpp_test_<PID>_<tag>` (préfixe conservé,
`tag` du test conservé) ; **aucun test ne dépend du nom exact** (vérifié :
`agent_cpp_test` n'apparaît que dans `test_support.hpp:92`).

### 3.2 `tests/workspace_tools_test.cpp` — correctif E3 (test `chemin_absolu`)

Le test conserve **exactement son intention** — vérifier le rejet d'un chemin
**absolu** — mais les cibles sont désormais prises **dans l'espace temporaire
propre au test** :

- `read_target = (workspace.path() / "lecture_absolue.txt").string()`
- `write_target = (workspace.path() / "ecriture_absolue.txt").string()`
- assertions inchangées : message `absolute workspace paths are not allowed`
  pour `read_file` **et** `write_file`, puis `!exists(write_target)`.

**Extension dans le même test (à signaler)** : la ligne `:233` visait
`/etc/hostname`. Elle a **aussi** été remplacée par une cible temporaire du
test, dans l'unique but de fermer complètement le critère « aucun chemin absolu
dépendant de la machine » (P1.1-V §9). Le rejet restant effectué **avant tout
accès disque** par `sandbox_path`, ce chemin n'était pas une source
d'instabilité — ce remplacement est donc **cosmétique** et peut être annulé si
vous préférez la version d'origine.

Vérifications : `grep -rn "agent_cpp_escape" tests/` → **aucun** ;
`grep -rn "/etc/" tests/` → **aucun**.

## 4. Validation — avant / après

| Point (§Validation du plan) | Commande | Résultat observé |
|---|---|---|
| 1. build propre | 2 × `cmake -S . -B /tmp/v2-{a,b} -DAGENT_BUILD_TESTS=ON …FETCHCONTENT_FULLY_DISCONNECTED=ON` + build | **configure=0 / build=0** (×2), **0 erreur / 0 avertissement** |
| 2. suite complète | `agent_tests` | **64/64 cas · 239 assertions · 0 échec**, code 0 ; `ECHEC=0 FAIL=0 SKIP=0` |
| 3. CTest | `ctest` | `100% tests passed, 0 tests failed out of 5` |
| 4. CTest parallèle 2 | `ctest --parallel 2` | `100% tests passed, 0 tests failed out of 5` |
| 5. CTest parallèle 4 | `ctest --parallel 4` | `100% tests passed, 0 tests failed out of 5` |
| 8. cas E3 | `agent_tests --filter=workspace` | `workspace.chemin_absolu` → **[ OK ]** ; groupe = **25/64 cas, 0 échec** |
| 9. mode production | `cmake … -DAGENT_BUILD_TESTS=OFF` + build | **0 / 0**, **0 avertissement** ; cibles `agent` + `agent_core` seulement (**`agent_tests` absent**) ; `ctest -N` → **Total Tests: 0** |
| 10. contrôle git | `git diff` / `git status` | `src/` + `include/` **INCHANGÉS** ; `CMakeLists.txt` = `8 0` **préexistant** ; index **vide** |

### 5. Test concurrent (critère essentiel)

Scénario **identique** à celui de P1.1-V (deux builds distincts, `--filter=workspace`
simultané), puis trois variantes supplémentaires :

| Variante concurrente (builds `a` + `b`) | Itérations | Échecs |
|---|---|---|
| `--filter=workspace` (scénario P1.1-V) | 6 | **0 / 6** *(P1.1-V : 6 / 6)* |
| suite **complète** | 3 | **0 / 3** |
| `--filter=agent` | 3 | **0 / 3** |
| **TOTAL** | **12** | **0 / 12** |

Chaque itération a été vérifiée **dans les deux processus** (code retour **0** et
`assertions en echec : 0`).

**Preuve directe du mécanisme corrigé** — observation des répertoires créés
pendant deux exécutions simultanées :

```
/home/oper/tmp/agent_cpp_test_301297_write_limit
/home/oper/tmp/agent_cpp_test_301300_write_limit
```

Même `tag` (`write_limit`), **PIDs distincts** : les deux processus ne partagent
plus de répertoire. Résidus après exécution : **0** (nettoyage RAII toujours
effectif).

## 6. Réserves

| # | Réserve | Portée |
|---|---|---|
| R1 | Le suffixe PID ne protège pas d'une **réutilisation de PID dans le temps** ; c'est sans effet ici, `remove_all` précédant `create_directories`, et deux processus au même PID ne pouvant coexister | négligeable |
| R2 | `TempDir` dépend désormais de **`::getpid()` (POSIX)** — cohérent avec le reste du harnais (sockets POSIX dans `mock_http_server.cpp`, `<unistd.h>` déjà utilisé par 3 fichiers de tests) et avec la cible Linux ; cela reste un frein à un portage Windows éventuel | documenté |
| R3 | **E1 (P1.1-V) reste ouverte** : `tests/` n'est **toujours pas versionnée** (0 fichier suivi). Cette itération ne pouvait pas la traiter (`git add` interdit) | **hors périmètre** |
| R4 | **E4 (P1.1-V) reste ouverte** : sensibilité temporelle bornée (`piece_delay_us`, `sleep_for(200 ms)`) — aucune instabilité observée | **hors périmètre** |
| R5 | **E5 → E9 (P1.1-V) restent ouverts** : `TMPDIR`, prérequis réseau FetchContent hérité, `CMAKE_BUILD_TYPE` vide, dettes R-02 → R-08 du mock | **hors périmètre** |
| R6 | La correction du chemin `/etc/hostname` (§3.2) dépasse la demande littérale (qui ne visait que `/tmp/agent_cpp_escape.txt`) ; elle est documentée et **réversible** | à arbitrer |
| R7 | Le nom du répertoire temporaire n'est plus **déterministe en valeur** (il contient le PID) ; il le reste **en forme**, et aucun test n'en dépend | assumé |

## 7. État Git final

```bash
$ git status --short --branch
## main
 M CMakeLists.txt
?? docs/README.md
?? docs/context/
?? docs/history/
?? docs/questions/
?? docs/reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md
?? docs/reports/PHASE_1_1_REPORT.md
?? docs/reports/PHASE_1_1_REVIEW_REPORT.md
?? docs/reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md
?? docs/reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md
?? docs/roadmap/
?? tests/

$ git diff --stat
 CMakeLists.txt | 8 ++++++++
 1 file changed, 8 insertions(+)

$ git rev-parse HEAD
1c46372e822c0d982b11628425fdf697cb0653a8
```

- **Fichiers modifiés par cette itération** : `tests/test_support.hpp` (165 l.),
  `tests/workspace_tools_test.cpp` (481 l.) — tous deux **non suivis par Git**.
- **Fichier créé** : ce rapport.
- `src/`, `include/`, `CMakeLists.txt` : **non modifiés** (` M CMakeLists.txt` et
  les `??` sont **préexistants**).
- **Aucun `git add`** (index vide), **aucun commit**, **aucun push** ;
  HEAD **identique** à l'état initial (`1c46372`).
- Tous les artefacts de validation (`/tmp/v2-*`, journaux) ont été **supprimés** ;
  **0 résidu** `agent_cpp_test_*` dans le répertoire temporaire.

## 8. Verdict

### PASS

**Justification** — les deux écarts visés sont corrigés et **démontrés par
mesure** :

- **E2** : le scénario exact qui échouait **6/6** en P1.1-V passe désormais
  **12/12** (6 × `workspace` + 3 × suite complète + 3 × `agent`, deux builds
  concurrents), avec preuve directe du mécanisme (`agent_cpp_test_<PID>_<tag>`,
  mêmes tags, PIDs distincts, 0 résidu).
- **E3** : plus aucun `agent_cpp_escape` ni `/etc/` dans `tests/` ; le test
  `chemin_absolu` conserve son intention et **passe**.

Aucune régression : build propre **0 avertissement**, suite **64/64 · 239
assertions · 0 échec**, CTest **5/5**, parallélisation **2 et 4** vertes, mode
`AGENT_BUILD_TESTS=OFF` intact (`agent_tests` absent, 0 test CTest), `src/` et
`include/` **inchangés**, protocole HTTP/SSE **non touché**, dettes R-02 → R-08
**non traitées**, aucun staging/commit/push. Les réserves R1 → R7 sont
documentées et **non bloquantes** ; les écarts E1 et E4 → E9 de P1.1-V restent
**ouverts et hors périmètre**.
