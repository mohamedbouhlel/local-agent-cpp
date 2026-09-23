# P3.3 — Réconciliation de la documentation de tests (dette E-1) — rapport

> Itération **documentaire uniquement**. **Aucun changement de `src/`,
> `include/`, `tests/` ou `CMakeLists.txt`** ; aucune CI créée ; **aucun commit**,
> **aucun staging**. Seuls documents modifiés : `docs/roadmap/ROADMAP.md` (§A) et
> `docs/questions/OPEN_QUESTIONS.md` (Q1), plus l'index et ce rapport.

## 1. OBJECTIF ET PÉRIMÈTRE

Traiter la dette documentaire **E-1** identifiée par l'audit de clôture P3/P3.1
(`docs/reports/PHASE_3_2_GIT_CLOSURE_AUDIT_REPORT.md` §6) : corriger les
affirmations **périmées** relatives à la suite de tests.

**Dans le périmètre** :

* `docs/roadmap/ROADMAP.md` §A — mentions « suite absente » et « aucune
  fonctionnalité validée » ;
* `docs/questions/OPEN_QUESTIONS.md` Q1 — question et citation obsolète de
  `AGENTS.md:18` ;
* l'index `docs/README.md` (ajout de ce rapport) ;
* ce rapport.

**Hors périmètre** : tout le code, les tests, la configuration, la création d'une
CI, et toute autre dette documentaire (§B, autres questions).

Gouvernance relue : `AGENTS.md` (notamment sa section `## Test`, qui décrit
désormais la suite versionnée et son activation).

## 2. ÉTAT GIT INITIAL OBSERVÉ

| Élément | Valeur |
|---|---|
| Branche | `main` |
| HEAD | `47acd5d70f937f96b6db1f8d03f9e7fb132744eb` (`docs: reconcile HttpClient::post() decision (keep, tested)`) |
| Nombre de commits | **32** |
| Working tree | **propre** au démarrage (`git status --short` vide) |
| Index | **vide** |

Le commit **`47acd5d`** (réconciliation P3/P3.1) est **HEAD** ; il n'a **pas été
touché** (ni amendé, ni réécrit).

## 3. CONSTATS VÉRIFIÉS (état réel de la suite et de la CI)

Aucun chiffre n'est repris de mémoire : tous ont été **exécutés** dans cette
itération.

### 3.1 Suite de tests — présente et versionnée

| Fait vérifié | Preuve |
|---|---|
| Suite suivie par Git | `git ls-files tests/` → **11 fichiers** (`CMakeLists.txt`, `test_support.hpp`, `test_main.cpp`, `mock_http_server.hpp/.cpp`, et 6 fichiers de tests) |
| Groupes CTest | **5** (`sse`, `workspace`, `http`, `agent`, `mock`), `TIMEOUT 30` |
| Cas de test | **66 / 66** exécutés |
| Assertions | **264** |
| Échecs | **0** (`RESULTAT: OK`) |
| CTest | **100 % tests passed, 0 tests failed out of 5** |
| Activation | `-DAGENT_BUILD_TESTS=ON` (option **OFF** par défaut) |

### 3.2 CI — absente

| Fait vérifié | Preuve |
|---|---|
| Aucun workflow de CI suivi | `git ls-files | grep -iE '\.github/|\.gitlab|jenkins|travis|circleci|azure|\.ya?ml$'` → **aucune ligne** |
| Aucun répertoire de CI | `ls -d .github .gitlab-ci.yml .travis.yml` → **absent** |

⇒ Le statut « **CI absente** » est un **fait actuel**, conservé dans la
documentation mise à jour.

### 3.3 Affirmations périmées confirmées

| Emplacement | Affirmation (périmée) | Réalité vérifiée |
|---|---|---|
| `ROADMAP.md` §A (ex-`l.30`) | « Suite de tests / CI / validation versionnée : **absente** » | suite **présente** (11 fichiers, 5 groupes, 66 cas / 264 assertions) ; seule la **CI** reste absente |
| `ROADMAP.md` §A (ex-`l.32`) | « **Aucune fonctionnalité n'est « validée »** au sens reproductible depuis le dépôt » | la suite est **exécutable depuis le dépôt** (`-DAGENT_BUILD_TESTS=ON` + `ctest`) |
| `OPEN_QUESTIONS.md` Q1 | cite `AGENTS.md:18` : « **Pas de suite de tests pour l'instant** » | `AGENTS.md` §Test décrit désormais la suite et son activation — citation **périmée** |

## 4. FICHIERS MODIFIÉS

| Fichier | Nature | Changement |
|---|---|---|
| `docs/roadmap/ROADMAP.md` | modifié | §A réécrite : 1 ligne obsolète → 3 lignes factuelles + paragraphes « Correction » / « État historique » |
| `docs/questions/OPEN_QUESTIONS.md` | modifié | Q1 clôturée : état historique conservé, état actuel ajouté |
| `docs/README.md` | modifié | +1 ligne : index de ce rapport |
| `docs/reports/PHASE_3_3_TEST_DOCUMENTATION_RECONCILIATION_REPORT.md` | **ajouté** | ce rapport |

## 5. RÉSUMÉ DES CHANGEMENTS PAR FICHIER

### A. `docs/roadmap/ROADMAP.md` §A

* La ligne unique « **Suite de tests / CI / validation versionnée : absente** »
  est remplacée par **trois lignes** factuelles :
  1. *Suite de tests versionnée (micro-harnais maison + mock HTTP/SSE)* →
     **présente (P1.1/P2)**, preuve `tests/` (11 fichiers), `tests/CMakeLists.txt`,
     `AGENTS.md` §Test ;
  2. *Exécution : 5 groupes CTest, **66 cas / 264 assertions*** → **présente**,
     0 échec, preuve `-DAGENT_BUILD_TESTS=ON` + `ctest` ;
  3. *CI (intégration continue)* → **absente** (fait actuel).
* Le paragraphe « **Aucune fonctionnalité n'est « validée »…** » est remplacé par
  deux paragraphes qui **distinguent explicitement** :
  * « **Correction (état actuel, P1.1/P2)** » — suite versionnée et exécutable
    depuis le dépôt, couvrant streaming, troncature, sandbox, outils, écriture
    atomique ;
  * « **État historique (audit à `4f33b62`)** » — aucune suite versionnée alors,
    validations hors dépôt supprimées : constat **aujourd'hui périmé** qui fondait
    Q1. La **CI reste absente**.

**Aucun autre élément de la roadmap n'a été modifié.**

### B. `docs/questions/OPEN_QUESTIONS.md` Q1

Q1 passe à **`[RÉSOLU — P1.1/P2 : suite de tests versionnée et exécutable depuis
le dépôt]`**. L'historique utile est **préservé** (tag
`[FAIT — état historique, audit à 4f33b62]`, au passé), et un tag
`[CORRECTION — état actuel, vérifié]` apporte les faits actuels (11 fichiers,
5 groupes, 66 cas / 264 assertions, 0 échec) ainsi que la mention que la citation
`AGENTS.md:18` est **périmée**. La « Résolution » renvoie aux items **P1** et
**P2** de la roadmap. Un « **Reste ouvert (distinct)** » précise que **la CI**
n'existe toujours pas.

## 6. CONFIRMATION — CODE, TESTS ET CMAKE INTACTS

```
$ git status --porcelain -- src include tests CMakeLists.txt
(aucune sortie)
```

⇒ **aucun** fichier de `src/`, `include/`, `tests/` ni `CMakeLists.txt` modifié,
ajouté ou supprimé. Aucune CI créée. Le commit `47acd5d` n'a pas été touché.

## 7. VÉRIFICATIONS EXÉCUTÉES ET RÉSULTATS

| # | Commande / contrôle | Résultat |
|---|---|---|
| V-1 | `git ls-files tests/` | **11** fichiers suivis |
| V-2 | `git ls-files | grep -iE '\.github/\|\.gitlab\|…\|\.ya?ml$'` ; `ls -d .github …` | **aucune** config CI |
| V-3 | `cmake -S . -B /home/oper/tmp/agent-cpp-tests -DAGENT_BUILD_TESTS=ON …` | **rc=0** (hors dépôt, hors ligne) |
| V-4 | `cmake --build … -j` | **rc=0**, 0 erreur |
| V-5 | `ctest --output-on-failure` | **100 % — 5/5 groupes**, 0 échec |
| V-6 | `./tests/agent_tests` | **66/66 cas · 264 assertions · 0 échec** (`RESULTAT: OK`) |
| V-7 | `git diff --check` | **silencieux** |
| V-8 | Liens relatifs de `ROADMAP.md` et `OPEN_QUESTIONS.md` | **0 cassé** (8 cibles vérifiées) |
| V-9 | Complétude de l'index | **16 / 16** fichiers de `docs/reports/`, **0** orphelin |
| V-10 | `git status --porcelain -- src include tests CMakeLists.txt` | **vide** (§6) |
| V-11 | `git status --short --branch` / `git diff --stat` | Avant l'entrée d'index : `## main` + **2 `M`** (`ROADMAP.md`, `OPEN_QUESTIONS.md`) + **1 `??`** (ce rapport) ; `2 files changed, 30 insertions(+), 14 deletions(-)`. **État final** (§9) : **3 `M`** (+ `docs/README.md`) + **1 `??`** |

## 8. RESTES OUVERTS ET RÉSERVES

| # | Constat | Portée |
|---|---|---|
| R-1 | **`ROADMAP.md` §B.3** affirme encore : « séparateur CRLF et concaténation de plusieurs lignes `data:` sont implémentés mais **aucun test ne les couvre** » | **PÉRIMÉ** : `tests/sse_parser_test.cpp` contient `TEST(sse, separateur_crlf)` (l.116) et `TEST(sse, plusieurs_lignes_data_concatenees)` (l.125). **Hors du périmètre énuméré** (§A et Q1) ⇒ **signalé, non corrigé**. |
| R-2 | **`AGENTS.md` §Test** indique « **64 cas / 239 assertions** » | **PÉRIMÉ** : l'état réel est **66 cas / 264 assertions** (V-6). Fichier **hors du périmètre** (non listé) ⇒ **signalé, non corrigé**. |
| R-3 | **CI absente** | Fait actuel conservé ; décision de la créer ou non hors de cette dette. |
| R-4 | Les autres items de `ROADMAP.md` §B et les autres questions d'`OPEN_QUESTIONS.md` n'ont **pas** été re-vérifiés | Hors dette E-1. |

**Recommandation** : une réconciliation ultérieure dédiée pourrait corriger R-1
et R-2 (mêmes types d'écarts, portée plus large).

## 9. VERDICT

### RÉCONCILIÉ (dette E-1 traitée)

**Justification** :

1. Les trois affirmations **périmées** de E-1 sont corrigées : `ROADMAP.md` §A
   (suite « absente », « aucune fonctionnalité validée ») et
   `OPEN_QUESTIONS.md` Q1 (citation `AGENTS.md:18`).
2. Les chiffres publiés (11 fichiers, 5 groupes, **66 cas / 264 assertions**,
   0 échec) sont **vérifiés par exécution** (V-1, V-3 → V-6), non repris de
   mémoire.
3. **État historique** et **état actuel** sont explicitement distingués dans les
   deux documents (tags `[FAIT — état historique…]` / `[CORRECTION — état actuel…]`).
4. **Périmètre respecté** : 2 documents corrigés + index + ce rapport ; **aucun**
   fichier de code/test/CMake touché (V-10) ; `git diff --check` silencieux (V-7) ;
   liens intacts (V-8).
5. Le rapport durable est **créé** et **indexé** (V-9).

### Réserves

**R-1** (`ROADMAP.md` §B.3) et **R-2** (`AGENTS.md` §Test) sont des
affirmations **également périmées** mais **hors du périmètre énuméré** ; elles
sont **signalées avec preuve** et **non corrigées** (contrainte « ne modifier
rien d'étranger à cette dette »).

## 10. COMMIT / STAGING

**Aucun commit, aucun staging.** Les deux fichiers corrigés, ce rapport et sa
ligne d'index restent **non commités**, en attente d'autorisation. Le commit
`47acd5d` n'a pas été modifié.
