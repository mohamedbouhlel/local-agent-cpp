# Ratification documentaire — spécification REPL multiline

> Mission **strictement documentaire**. Aucune source, aucun test, aucun fichier
> CMake ou de configuration modifié ; **aucun build, aucun test, aucun commit**.
> Fichiers touchés : la spécification (mise à jour) et `docs/README.md`
> (indexation). Ce rapport est créé.

## 1. BRANCHE, HEAD ET ÉTAT DU WORKTREE OBSERVÉS

| Élément | Valeur |
|---|---|
| Branche | `main` |
| HEAD | `f62ca3680384b42ff4cdd8287a44eae4e3b15be2` (`f62ca36 docs: record agent test validation`) |
| Base attendue (`f62ca36`) | **CONFIRMÉE** |
| `git diff --stat` (initial) | **vide** ; `git diff --cached --stat` **vide** |
| Fichiers non suivis (initiaux) | `docs/reports/2026-09-23-repl-multiline-preparation.md`, `docs/reports/2026-09-23-repl-multiline-requirements-definition.md`, `docs/reports/revue-proposition-repl-multiline-2026-09-23.md`, `docs/specifications/` |

### 1.1 Fichier non suivi **inattendu** et arbitrage

`docs/reports/revue-proposition-repl-multiline-2026-09-23.md` **n'existait pas**
lors de la mission précédente : il est apparu **entre les deux missions**
(artefact de revue du mainteneur). **Il n'a été ni modifié, ni écrasé, ni
intégré.**

**Ce n'est pas un blocage** : ce document **confirme et précise** la présente
mission. Il signale notamment :

* une **contradiction bloquante REPL-08/REPL-09** (EOF : termine vs revient au
  prompt) → **corrigée** ici (§2) ;
* des points à trancher (CRLF, `\n` final, bloc vide vs lignes vides, bloc
  d'espaces, limitation `:end`, autorisation CMake, emplacement documentaire)
  → **traités par les décisions D-1 à D-7** de la présente mission ;
* une **approbation globale à différer jusqu'aux corrections** → la mission
  **D-8** acte précisément l'approbation **après** correction ⇒ séquences
  compatibles.

## 2. DÉCISIONS INTÉGRÉES

| ID | Décision | Intégration dans la spécification |
|---|---|---|
| **D-1 / A1** | `:multi` **exact** déclenche le multiline | **REPL-01** (formulation normative + cas limites : ` :multi` = contenu ; `:multi` dans un bloc = littéral) |
| **D-2 / B1** | seule la ligne **exacte `:end`** termine le bloc ; limitation ratifiée | **REPL-04** (+ limitation explicite : ligne littérale `:end` impossible en multiline) |
| **D-3 / C1** | bloc vide **zéro ligne** : ignoré + **message informatif**, **aucun envoi** | **REPL-06** (distinction explicite **zéro ligne** vs **lignes vides/espaces**) |
| **D-4 / D1** | `exit` **littéral** en multiline ; `exit` **termine** le REPL en simple | **REPL-07** |
| **D-5 / G1** | `PromptReader` **isolé**, testable par **flux injectés** ; **modification CMake autorisée** pour cette architecture | §0 (portée), **§5/L**, registre §4 ; mention que la modification CMake est **autorisée pour l'implémentation**, **non réalisée ici** |
| **D-6** | `docs/specifications/` **ratifié** ; indexation documentaire | **§4 (D-6)** + `docs/README.md` (section « Spécifications ») |
| **D-7** | **Conservation** : séparateur `\n` ; vides/espaces/tabulations conservés ; **CRLF → `\n`** ; **pas de `\n` final artificiel** ; bloc de lignes vides/espaces **structurellement non vide ⇒ envoyé** ; `:end` exclu ; `:multi` dans un bloc **littéral** | **REPL-03** (normalisation CRLF, absence de `\n` final), **REPL-06** (bloc blanc envoyé), **REPL-01** (`:multi` littéral), §0 (défauts d'entrée normalisés) |
| **D-8** | **REPL-01 → REPL-09 approuvées** après correction des contradictions | **en-tête** (« STATUT : APPROUVÉE »), **statut de chaque exigence** passé de « PROPOSITION — NON APPROUVÉE » à **APPROUVÉE**, §4 registre, §3 options écartées |

### 2.1 Corrections de contradictions (exigence §3.3 de la mission)

| Point | Avant | Après (ratifié) |
|---|---|---|
| **EOF en multiline** | « abandon du bloc » sans préciser la suite (ambigu) | **abandon sans `agent.send()` PUIS fin du REPL** ; **jamais** de retour à l'invite |
| **REPL-09** | retour à l'invite **après abandon par EOF** | retour à l'invite **si et seulement si** terminaison par `:end` (**y compris bloc vide**) |
| **Bloc vide** | ambigu (« vide » non défini) | **zéro ligne** ⇒ ignoré + message · **lignes vides/espaces** ⇒ **envoyé** |
| **Conservation CRLF** | non défini | **CRLF → `\n`** ; **pas de `\n` final artificiel** |
| **Statuts** | « PROPOSITION — NON APPROUVÉE » (9×) | **APPROUVÉE** (9×) |

## 3. FICHIERS MODIFIÉS

| Fichier | Nature | Changement |
|---|---|---|
| `docs/specifications/repl-multiline-requirements-proposal.md` | **mis à jour** | réécriture en version **ratifiée** : en-tête APPROUVÉE, 9 exigences approuvées et corrigées, matrice de traçabilité actualisée, options écartées, **registre D-1→D-8**, limites |
| `docs/README.md` | **modifié** | ajout d'une section **« Spécifications »** référençant la spécification ratifiée |
| `docs/reports/2026-09-23-repl-multiline-requirements-ratification.md` | **créé** | ce rapport |

**Non modifiés** (préservés) : `docs/reports/2026-09-23-repl-multiline-preparation.md`,
`docs/reports/2026-09-23-repl-multiline-requirements-definition.md` (archives de
leurs missions), `docs/reports/revue-proposition-repl-multiline-2026-09-23.md`
(artefact de revue), tous les fichiers `src/`, `include/`, `tests/`, `CMakeLists.txt`.

## 4. VÉRIFICATIONS RÉELLEMENT EXÉCUTÉES ET RÉSULTATS

| # | Commande / contrôle | Résultat |
|---|---|---|
| V-1 | `git branch --show-current` / `git log -1 --oneline` / `git rev-parse HEAD` | `main` / `f62ca36 …` / `f62ca3680384b42…` |
| V-2 | `git log --oneline -1 f62ca36` | base **confirmée** |
| V-3 | `git status --short` (initial) | 3 `??` + `?? docs/specifications/` (§1) |
| V-4 | `test -e` du rapport cible | **absent** ⇒ création sans écrasement |
| V-5 | `grep -n "EOF"` sur la spécification | 10 occurrences, **toutes cohérentes** : EOF multiline ⇒ abandon **+ fin du REPL**, **jamais** de retour à l'invite (l.125, 127, 135, 138, 168, 169, 172) |
| V-6 | `grep -n "invite primaire"` | retour **uniquement** après `:end` (l.135) ; « pas de retour après EOF » (l.138, 172) |
| V-7 | `grep -c "^### REPL-0"` | **9** exigences présentes |
| V-8 | `git diff --check` | **silencieux** |
| V-9 | `git diff --stat` | `docs/README.md | 8 +` — **seul** fichier suivi modifié |
| V-10 | `git status --porcelain -- src include tests CMakeLists.txt` | **vide** ⇒ **aucune** source/test/CMake modifié |
| V-11 | `git diff --cached --stat` | **vide** ⇒ rien de stagé ; **aucun commit** |
| V-12 | `git rev-parse HEAD` / `git rev-list --count HEAD` (final) | `f62ca36…` / **inchangé** |

**Aucun build, aucun test** n'a été lancé (aucun code modifié) — conformément à
la consigne §4.

## 5. LIMITES ET POINTS BLOQUANTS

| # | Point |
|---|---|
| L-1 | **Limite de taille d'un bloc** : **non définie** par la spécification (point ouvert, §5/L-1 de la spécification). |
| L-2 | Le **nom de fichier** de la spécification contient encore `…-proposal.md` alors que le document est **ratifié** ⇒ **incohérence de nommage signalée**, non corrigée (un renommage serait une opération non demandée ; décision requise). |
| L-3 | Les rapports datés `2026-09-23-*` (dont ce rapport et l'artefact de revue) **ne sont pas indexés** dans `docs/README.md` : la mission n'autorisait à référencer **que la spécification**. Écart **signalé**, non traité. |
| L-4 | La **modification de `CMakeLists.txt`** (D-5) est **autorisée** mais **non réalisée** : elle relève de l'itération d'implémentation. |
| L-5 | **Aucun test exécuté** : la matrice de traçabilité est un **plan**, pas un résultat. |
| L-6 | `docs/specifications/` est **ratifié** (D-6) mais reste **non suivi** par Git (aucun commit dans cette mission). |

**Aucun point bloquant.**

## 6. VERDICT

### RATIFICATION DOCUMENTAIRE VALIDÉE

**Justification** :

1. Les **décisions D-1 à D-8** sont **intégrées** dans la spécification : en-tête
   **APPROUVÉE**, 9 exigences **approuvées**, **registre des décisions** (§4) et
   **options écartées** (§3).
2. La **contradiction bloquante REPL-08/REPL-09** est **corrigée** : EOF abandonne
   le bloc **puis termine le REPL**, **sans jamais** revenir à l'invite ; REPL-09
   ne prévoit le retour à l'invite primaire **qu'après `:end`** (bloc vide inclus).
   Vérifié par `grep` (V-5, V-6).
3. Les **règles de conservation (D-7)** sont explicites : `\n`, vides/espaces/
   tabulations conservés, **CRLF → `\n`**, **pas de `\n` final artificiel**, bloc
   blanc **envoyé**, `:end` exclu, `:multi` littéral dans un bloc.
4. **Statuts obsolètes supprimés** : plus aucune mention « PROPOSITION — NON
   APPROUVÉE » ; les 9 exigences portent **APPROUVÉE**.
5. **Matrice de traçabilité** actualisée (CRLF, bloc blanc, EOF, retour invite).
6. **Périmètre respecté** : `docs/README.md` (indexation de la spécification) et
   la spécification sont les **seuls** fichiers documentaires touchés ; **aucune**
   source/test/CMake modifié (V-10) ; `git diff --check` **silencieux** (V-8) ;
   **aucun commit** (V-11, V-12) ; aucun fichier préexistant écrasé.

**Arrêt** : aucune implémentation de `PromptReader` ; aucun commit. La suite
(implémentation + tests) reste **subordonnée à une autorisation explicite**.
