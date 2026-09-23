# P3.1 — Réconciliation documentaire : `HttpClient::post()` conservé (rapport)

> Itération de **réconciliation documentaire** uniquement. **Aucun changement
> de `src/`, `include/`, `tests/` ou `CMakeLists.txt`** ; aucune méthode ni
> aucun test supprimé ; aucune dépendance ajoutée ; aucun changement de
> comportement applicatif. **Aucun commit.**

## 1. OBJECTIF ET PÉRIMÈTRE

Consigner dans les documents du projet la **décision du mainteneur**
(**option A — conserver `HttpClient::post()`**), retirer les affirmations
devenues obsolètes (« non testée », « dette à arbitrer ») et produire ce rapport
vérifiable.

**Dans le périmètre** : `docs/roadmap/ROADMAP.md`, `docs/questions/OPEN_QUESTIONS.md`,
`docs/context/PROJECT_CONTEXT.md`, `docs/README.md`, et ce rapport.

**Hors périmètre** : tout le code de production et les tests ; les autres items
de la roadmap ; l'arbitrage lui-même (déjà tranché, **non rouvert**).

Documents de gouvernance relus : `AGENTS.md` (règles, §Instruction « ne pas
ajouter de framework externe »), `CLAUDE.md` (renvoi vers `AGENTS.md`).

## 2. DÉCISION DU MAINTENEUR

**OPTION A — CONSERVER `HttpClient::post()`.** Décision explicite et ferme ;
elle **n'est pas rouverte** par cette itération, qui se limite à la consigner.

## 3. ÉTAT GIT INITIAL RÉELLEMENT OBSERVÉ

| Élément | Valeur relevée dans cette itération |
|---|---|
| Répertoire | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `51b7d2c3bd2684c32521e11b59ba833dd85bebae` (`test: add end-to-end SSE mock scenarios`) |
| Nombre de commits | **31** |
| `git diff --check` | **silencieux** |
| Working tree | **non propre** — changements **préexistants** de l'itération P3 (non commités) : ` M docs/README.md` + `?? docs/reports/PHASE_3_HTTPCLIENT_POST_REPORT.md` |

**Point d'attention** : le working tree **n'était pas propre** au démarrage. Les
deux entrées ci-dessus proviennent de **P3**, pas de P3.1 ; elles sont
**préservées** et ne sont pas attribuées à cette itération (voir §9).

## 4. RÉFÉRENCE AU RAPPORT DE PRÉPARATION P3

Référence factuelle : `docs/reports/PHASE_3_HTTPCLIENT_POST_REPORT.md`
(**209 lignes**, sha256 `2ec19418132d8a6acec9e802fec62b07ab79278f2a42f448c2147eb3b64ddd5d`),
lue intégralement.

Ses constats vérifiables ont été **re-confirmés** sur l'état courant :

| Constat du rapport P3 | Re-vérification P3.1 |
|---|---|
| `post()` déclaré `include/http_client.hpp:16`, défini `src/http_client.cpp:138` | **confirmé** |
| `post()` délègue à `perform_request()` partagé | **confirmé** (`src/http_client.cpp:144`) |
| Aucun appelant en production (seul `post_streaming`, `src/agent.cpp:209`) | **confirmé** |
| **4** tests couvrent `post()` (succès, non-2xx, échec curl, tronquée) | **confirmé** (`tests/http_client_test.cpp:23,35,84,96`) |

**Aucune divergence** entre le dépôt et les faits du rapport P3. La divergence
à réconcilier opposait le dépôt aux documents **ROADMAP/OPEN_QUESTIONS**
(prémisse « non testée »), traitée en §6.

## 5. FICHIERS DOCUMENTAIRES MODIFIÉS

| Fichier | Nature | Périmètre |
|---|---|---|
| `docs/roadmap/ROADMAP.md` | modifié | §4-A |
| `docs/questions/OPEN_QUESTIONS.md` | modifié | §4-B |
| `docs/context/PROJECT_CONTEXT.md` | modifié | §4-C |
| `docs/README.md` | modifié | §4-D (index du nouveau rapport) |
| `docs/reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md` | **ajouté** | §4-E (ce rapport) |

## 6. RÉSUMÉ PRÉCIS DES CHANGEMENTS PAR FICHIER

### A. `docs/roadmap/ROADMAP.md` (4 éditions ciblées)

1. §A (tableau des faits) — ligne « POST OpenAI non-streamé » : « implémenté,
   **plus utilisé par l'agent** » → « implémenté, **sans appelant en
   production** — conservé (décision P3.1), couvert par 4 tests ».
2. §B.1 — la formulation « **`HttpClient::post()` est du code mort
   applicatif** » est **barrée** et remplacée par « **RÉSOLU (P3.1, décision A :
   conservé)** », en conservant le fait (aucun appelant en production) et en
   ajoutant la couverture de test. La **numérotation §B.1 est conservée** pour
   ne pas casser les renvois « Dette §B.x » des items P4→P15.
3. §D « Vague 2 » — item **`P3 (PROPOSITION)`** → **`P3 (DÉCIDÉ — option A,
   P3.1)`** : `post()` conservé, déjà couvert par 4 tests, dette §B.1
   **close**, avec renvois vers les deux rapports.
4. §F (chemin critique) — la ligne `P3..P8` devient `P4..P8 (… ; P3 traité)`,
   pour ne plus lister un item clos.

**Aucun autre élément de la roadmap n'a été touché.**

### B. `docs/questions/OPEN_QUESTIONS.md` (Q3 clôturée)

Q3 « `HttpClient::post()` doit-il rester ? » passe à **`[RÉSOLU — décision du
mainteneur (P3.1) : CONSERVER]`**. L'historique utile est conservé ; une mention
`[CORRECTION]` acte que « API publique **non testée** » est **périmée** (4 tests).
Les rubriques « Pourquoi ça compte / Ce qui trancherait » sont remplacées par la
décision et son motif, avec renvois vers les rapports P3 et P3.1.

### C. `docs/context/PROJECT_CONTEXT.md` (§7, protocole HTTP)

La phrase « `post()` reste défini et fonctionnel, mais **n'est plus appelé** par
`Agent` (voir dette technique, …) » est remplacée par une formulation qui
distingue explicitement **l'absence d'appelant en production** (fait) de son
statut d'**API disponible conservée** (décision P3.1, couverte par 4 tests,
délégation à `perform_request()`), **sans** suggérer que l'absence d'appelant
justifie une suppression.

### D. `docs/README.md` (index)

Deux entrées sont présentes dans la table « Rapports de phase », dans l'ordre
chronologique existant : le rapport **P3** (ajouté lors de l'itération
précédente) et le rapport **P3.1** (ajouté ici). Format identique aux autres
lignes ; aucune entrée existante modifiée ni cassée.

### E. Ce rapport

Créé sous `docs/reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md`.

## 7. CONFIRMATION — CODE, TESTS ET CMAKE NON MODIFIÉS

```
$ git status --porcelain -- src include tests CMakeLists.txt
(aucune sortie)
```

⇒ **aucun** fichier de `src/`, `include/`, `tests/` ni `CMakeLists.txt` modifié,
ajouté ou supprimé. Aucune méthode ni aucun test supprimé ; aucune dépendance
ajoutée ; comportement applicatif inchangé.

## 8. VÉRIFICATIONS RÉELLEMENT EXÉCUTÉES ET RÉSULTATS

| # | Commande / contrôle | Résultat observé |
|---|---|---|
| V-1 | `git status --short --branch` (initial) | `## main` · ` M docs/README.md` · `?? …PHASE_3_HTTPCLIENT_POST_REPORT.md` (préexistants P3) |
| V-2 | `git log -1 --oneline` | `51b7d2c test: add end-to-end SSE mock scenarios` |
| V-3 | `git diff --check` | **silencieux** |
| V-4 | Lecture intégrale du rapport P3 + re-vérification de ses faits | **aucune divergence** (voir §4) |
| V-5 | `grep` des mentions obsolètes (`code mort applicatif`, `API publique non testée`, `plus utilisé par l'agent`, `Dette §B.1`) | **aucune** occurrence non traitée : seuls subsistent la formulation barrée « RÉSOLU », la note `[CORRECTION]` (citant l'ancien libellé pour le corriger) et les renvois `Dette §B.10→B.17` **sans rapport** |
| V-6 | `git diff` des 4 documents | conforme à §6 ; **4** fichiers, modifications **strictement ciblées** |
| V-7 | `git status --porcelain -- src include tests CMakeLists.txt` | **vide** (§7) |
| V-8 | Complétude de l'index documentaire (`ls docs/reports/*.md` vs liens de `docs/README.md`) | voir §9 (rapport inclus ⇒ **14/14**, 0 orphelin) |
| V-9 | Validité des liens relatifs de `docs/README.md` | cibles existantes ; aucune entrée cassée |

**Aucun test applicatif n'a été exécuté** : aucun fichier de code ou de test n'a
été modifié, et les consignes du dépôt n'imposent pas de campagne de tests pour
une itération purement documentaire. Les valeurs de test citées (4 cas couvrant
`post()`, 66 cas au total) proviennent de la suite **déjà versionnée et validée
en P2** ; elles **ne sont pas re-mesurées** ici et ne sont pas présentées comme
telles.

## 9. ÉTAT GIT FINAL ET FICHIERS MODIFIÉS

| Élément | Valeur |
|---|---|
| Branche | `main` |
| HEAD | `51b7d2c3bd2684c32521e11b59ba833dd85bebae` — **inchangé** |
| Nombre de commits | **31** — **inchangé** |
| Index (staging) | **vide** |
| `git diff --check` | **silencieux** |

**Fichiers modifiés/ajoutés à l'issue de l'itération :**

| Fichier | État | Origine |
|---|---|---|
| `docs/roadmap/ROADMAP.md` | `M` | **P3.1** |
| `docs/questions/OPEN_QUESTIONS.md` | `M` | **P3.1** |
| `docs/context/PROJECT_CONTEXT.md` | `M` | **P3.1** |
| `docs/README.md` | `M` | **P3** (1 ligne) + **P3.1** (1 ligne) |
| `docs/reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md` | `??` | **P3.1** |
| `docs/reports/PHASE_3_HTTPCLIENT_POST_REPORT.md` | `??` | **P3** (préexistant, préservé) |

Index documentaire : **14 fichiers** dans `docs/reports/`, **14** indexés,
**0** orphelin.

## 10. VERDICT

### RÉCONCILIATION DOCUMENTAIRE VALIDÉE

**Justification** :

1. La **décision A** est actée de façon explicite dans `ROADMAP.md` (faits,
   dette `§B.1`, item P3), `OPEN_QUESTIONS.md` (Q3 clôturée) et
   `PROJECT_CONTEXT.md` (§7).
2. Les affirmations **obsolètes** (« non testée », « dette à arbitrer ») ont été
   **retirées ou reformulées**, en conservant l'historique utile (V-5).
3. `post()` est partout décrit comme **conservé**, **sans appelant en
   production** mais **couvert par 4 tests**, sans formulation suggérant une
   suppression.
4. **Périmètre respecté** : seuls les 4 documents visés + ce rapport ont changé ;
   `src/`, `include/`, `tests/`, `CMakeLists.txt` **intacts** (V-7) ;
   `git diff --check` **silencieux** (V-3).
5. Le nouveau rapport est **créé** et **indexé** (§9).

### Réserves

* **P3.1 n'a pas été rejoué sur les autres constats périmés de la roadmap** :
  `ROADMAP.md` §A affirme encore « Suite de tests / CI / validation versionnée :
  **absente** » et `OPEN_QUESTIONS.md` Q1 cite `AGENTS.md:18` (« Pas de suite de
  tests ») — **faux depuis P1.1**. Ces mentions **ne concernent pas `post()`** et
  sortent du périmètre demandé (§4-A « ne pas modifier les autres éléments sans
  nécessité directe ») ; elles sont **signalées**, non corrigées.
* Les valeurs de tests (4 cas / 66 cas) sont **citées** depuis P2, non
  re-mesurées dans cette itération documentaire (cf. §8).

## 11. COMMIT

**Aucun commit n'a été effectué.** Les modifications restent **non commitées**,
en attente d'une autorisation explicite.
