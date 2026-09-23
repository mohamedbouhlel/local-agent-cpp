# Revue finale — spécification REPL multiline et plan d'implémentation

> Mission de **revue statique et de planification**. **Aucune** modification de
> `src/`, `include/`, `tests/` ou CMake ; **aucune** implémentation de
> `PromptReader` ; **aucun** build, **aucun** test, **aucun** commit. La
> spécification ratifiée **n'a pas été modifiée** (aucune décision en attente
> contournée). Seul artefact créé : ce rapport.

## 1. BRANCHE, HEAD ET ÉTAT INITIAL DU WORKTREE

| Élément | Valeur observée |
|---|---|
| Branche | `main` |
| HEAD | `f62ca3680384b42ff4cdd8287a44eae4e3b15be2` |
| Référence de ratification déclarée | `f62ca3680384b42ff4cdd8287a44eae4e3b15be2` → **IDENTIQUE** |
| Nombre de commits | **39** |
| Index (staged) | **vide** |
| `git diff --stat` (initial) | `docs/README.md | 8 ++++++++` (indexation issue de la mission de ratification — **préexistante, préservée**) |
| Fichiers non suivis (initiaux) | `docs/specifications/repl-multiline-requirements-proposal.md`, `docs/reports/2026-09-23-repl-multiline-preparation.md`, `docs/reports/2026-09-23-repl-multiline-requirements-definition.md`, `docs/reports/2026-09-23-repl-multiline-requirements-ratification.md`, `docs/reports/revue-proposition-repl-multiline-2026-09-23.md` |

**Aucun changement inattendu** : tous les éléments ci-dessus sont **expliqués**
(artefacts des missions précédentes). Le périmètre est établi ⇒ la mission peut
se poursuivre. **Aucun fichier préexistant n'a été écrasé ni intégré.**

## 2. DOCUMENTS ET CODE EXAMINÉS

| Élément | Nature |
|---|---|
| `docs/specifications/repl-multiline-requirements-proposal.md` | **spécification ratifiée** (17 807 o) — lue intégralement |
| `docs/reports/2026-09-23-repl-multiline-preparation.md` | préparation (point d'intégration) — lue |
| `docs/reports/2026-09-23-repl-multiline-requirements-definition.md` | définition des exigences — lue |
| `docs/reports/2026-09-23-repl-multiline-requirements-ratification.md` | ratification — lue |
| `docs/reports/revue-proposition-repl-multiline-2026-09-23.md` | revue antérieure du mainteneur — lue |
| `AGENTS.md` | conventions (code style, architecture, tests) |
| `src/main.cpp` (138 l.) | REPL réel : `getline` (l.71), `exit`/EOF, ligne vide, `agent.send()` (l.107) |
| `include/agent.hpp` | `Agent::send(const std::string&, const AgentCallbacks& = {})` (signature **inchangée** requise) |
| `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/test_main.cpp` | intégration build/test (groupes CTest, `--filter`) |
| `src/workspace_tools.cpp` (constantes) | `max_read_chars = 50'000`, `max_write_chars = 200'000`, `max_search_results = 50` |
| `src/agent.cpp` (constantes) | `max_history_messages = 20`, `max_tool_iterations = 5` |
| `docs/README.md` | conventions documentaires + index |

**Non examinés** : `src/agent.cpp` hors constantes, SSE, tool-calling (hors périmètre) ; `.env` (**non lu** — secrets).

## 3. CONSTATS PAR EXIGENCE

| Exigence | Cohérence interne | Conformité D-1→D-8 | Matrice | Remarque |
|---|---|---|---|---|
| **REPL-01** | ✔ | ✔ D-1/A1 | ✔ | **Seule déviation** du comportement actuel en mode simple : une ligne exactement `:multi` est **consommée** (non envoyée). Cohérent avec REPL-02 (« en l'absence de déclenchement ») mais à rendre **conspicue** (§4/A-5). |
| **REPL-02** | ✔ | ✔ D-8 | ✔ | `exit`/ligne vide/`"   "` conformes au code actuel (`src/main.cpp:71-77`). Voir A-6 (`exit` = correspondance **exacte**). |
| **REPL-03** | ✔ | ✔ D-7 | ✔ | `\n`, vides/espaces/tabulations conservés, **CRLF → LF**, pas de `\n` final. Voir **A-1** (portée de la normalisation) et **A-2** (`\r` isolé). |
| **REPL-04** | ✔ | ✔ D-2/B1 | ✔ | Limitation `:end` littéral **explicitement ratifiée**. Asymétrie simple/multiline (A-5). |
| **REPL-05** | ✔ | ✔ D-8 | ✔ | Unicité de l'envoi ; cohérent avec la matrice. |
| **REPL-06** | ✔ | ✔ D-3/C1 | ✔ | Distinction **zéro ligne** (0 envoi + message) vs **lignes vides/espaces** (envoyé) **explicite**. Cohérent avec REPL-02 (ligne vide simple ≠ bloc blanc). |
| **REPL-07** | ✔ | ✔ D-4/D1 | ✔ | `exit` littéral en multiline ; `exit` = fin en simple. |
| **REPL-08** | ✔ | ✔ (correction obligatoire) | ✔ | EOF multiline ⇒ **abandon + fin du REPL** ; **jamais** de retour à l'invite. Aucune contradiction résiduelle avec REPL-09 (§5). |
| **REPL-09** | ✔ | ✔ (corrigée) | ✔ | Retour à l'invite **uniquement** après `:end` (bloc vide inclus). Voir A-3 (libellé d'invite non normatif). |

**Absence de contradiction structurelle** : les 9 exigences, la matrice (§2 de la
spécification) et le registre D-1→D-8 sont **mutuellement cohérents**.
**Conformité à l'architecture** : la spécification reste compatible avec
`Agent::send(const std::string&, const AgentCallbacks& = {})` (signature
inchangée) et avec le micro-harnais maison ; aucun framework externe requis
(AGENTS.md).

## 4. ÉCARTS, AMBIGUÏTÉS ET POINTS EN ATTENTE

> **Aucune décision ratifiée n'est remise en cause.** Les points ci-dessous sont
> des **précisions** à confirmer ou des **lacunes** à décider ; ils sont
> susceptibles de provoquer des **implémentations divergentes** s'ils restent
> implicites.

| ID | Gravité | Point | Recommandation |
|---|---|---|---|
| **A-1** | **MOYENNE** | **Portée de la normalisation CRLF.** Le §0 énonce « CRLF → `\n` » **sans préciser le mode**, alors que REPL-02 exige le **comportement actuel en mode simple** (où un `\r` final serait envoyé tel quel). Ambiguité : la normalisation s'applique-t-elle aussi au mode simple ? | **Restreindre la normalisation au contenu de bloc multiline** (REPL-02 prime en simple). **Confirmation requise.** |
| **A-2** | MINEURE | **`\r` isolé** (non suivi de `\n`) : non spécifié. | Ne supprimer **qu'un seul `\r` final** par ligne collectée (traitement de `\r\n`) ; conserver tout autre `\r`. |
| **A-3** | MINEURE | **Libellé de l'invite de continuation** (`... `) donné comme **exemple** (limite L-3) : non normatif. | Les tests **ne doivent pas** dépendre du texte de l'invite : asserter l'**état** (mode multiline actif), pas la chaîne. |
| **A-4** | MINEURE | **EOF en multiline** : le sort du contenu est défini (jeté) mais **aucun message** n'est prévu avant la fin du REPL. | **Aucun message requis** (le REPL se termine) ; optionnel : note sur `stderr`. Laisser en attente. |
| **A-5** | INFO | **Asymétrie des commandes en mode simple** : `:multi` = **commande**, `:end` = **contenu**. | Ratifiée (D-1/D-2) ; à **rendre explicite** dans la spécification (documentation, non décision). |
| **A-6** | MINEURE | **`exit`** : « la ligne `exit` » — correspondance exacte ? | Préciser **« exactement `exit` »** (comme le code actuel `input == "exit"`), afin d'éviter une implémentation par `trim`. |
| **A-7** | INFO | **Formule du contenu** : non écrite littéralement. | Adopter **`content = join(lignes, "\n")`** (0 ligne ⇒ bloc vide ; 1 ligne vide ⇒ `""` ; 2 lignes vides ⇒ `"\n"`). |
| **A-8** | **MOYENNE** | **Limite de taille d'un bloc** : **indéfinie** (§3 de la mission). | Voir §5.1 — **décision utilisateur en attente**. |
| **A-9** | MINEURE | **Nommage du fichier** ratifié en `…-proposal.md`. | Voir §5.2 — **décision utilisateur en attente**. |

## 5. RECOMMANDATIONS SUR LES POINTS OUVERTS

### 5.1 Limite maximale d'un bloc multiline

**Faits observés** : le mode simple n'a **aucune** limite de longueur (une ligne
`getline` est transmise telle quelle). Le projet utilise ailleurs des plafonds
explicites (`max_read_chars = 50'000`, `max_write_chars = 200'000`,
`max_search_results = 50`). Aucune constante n'existe côté prompt.
**Aucune limite n'est inventée ici.**

| Option | Description | Conséquences |
|---|---|---|
| **O1** | **Aucune limite artificielle** (parité avec le mode simple) | Comportement homogène ; risque : dépassement de la fenêtre de contexte du modèle (erreur remontée par le serveur, non contrôlée localement) |
| **O2** *(recommandée sous réserve)* | Plafond explicite appliqué au **contenu assemblé**, au niveau de `PromptReader` : au-delà ⇒ **erreur explicite, aucun `agent.send()`** | Prévisible, testable, protège le contexte ; **la valeur reste à ratifier** (ordre de grandeur des plafonds existants) |
| O3 | Plafond **configurable** (variable d'environnement, défaut documenté) | Souple ; surface de configuration supplémentaire (cohérent avec la dette P9 « limites non configurables ») |

**Recommandation** : **O2**, valeur **non fixée** — la mission interdit d'inventer
la limite. Si O2 est retenu, le test associé doit vérifier **l'absence d'envoi**
en cas de dépassement. **Décision utilisateur requise** (A-8).

### 5.2 Nom du fichier de spécification

**Faits observés** : le document **ratifié** porte encore le suffixe
`-proposal.md`. Références recensées : **1 lien vivant** (`docs/README.md:24`) et
**3 mentions d'archive** (`…-requirements-definition.md:67`, `…-ratification.md:64`,
`revue-proposition-…md:4`) qui documentent un **état historique** — elles **ne
doivent pas** être réécrites.

| Option | Conséquences |
|---|---|
| **R1** *(recommandée)* | **Renommer** en `repl-multiline-requirements.md` **au moment de l'intégration**, et mettre à jour le **seul** lien vivant (`docs/README.md`). Coût : **1 ligne** + 1 renommage. Le fichier étant **non suivi**, aucun historique Git n'est perdu |
| R2 | **Conserver** `-proposal.md` | Aucun coût ; nom **trompeur** pour un document normatif (risque de confusion durable) |

**Recommandation** : **R1**, à exécuter **dans l'itération d'intégration**, avec
**autorisation explicite** (la mission interdit de renommer sans nécessité
démontrée ou autorisation). Les archives conservent l'ancien nom. **Décision
utilisateur requise** (A-9).

### 5.3 Index documentaire

**Fait** : `docs/README.md` référence la spécification dans une section
**« Spécifications »** ajoutée lors de la ratification (conforme à D-6).
**Constat** : les rapports **datés** (`2026-09-23-*`, `revue-proposition-*`) **ne
sont pas indexés** ; la convention documentaire du dépôt décrit les « rapports de
phase » nommés `PHASE_*`, mais **pas** la série datée ⇒ **écart de convention
préexistant**, **non traité ici** (la présente mission n'autorise que les
documents strictement nécessaires à la revue et au plan).
**Recommandation** : statuer **ultérieurement** sur l'indexation de la série
datée ; **ne pas élargir** le périmètre ici.

## 6. PLAN D'IMPLÉMENTATION (sans implémenter)

### 6.1 Responsabilités de `PromptReader`

* Lire **une** interaction depuis un flux d'entrée, en gérant **simple** et
  **multiline** (déclenchement `:multi`, terminaison `:end`).
* **Ne connaître ni `Agent`, ni le réseau, ni le LLM** ⇒ testable avec des flux
  injectés (`std::istringstream` / `std::ostringstream`).
* **Ne jamais** envoyer : il **retourne** un résultat que le REPL décide d'envoyer.
* Appliquer les règles de conservation (D-7) et, si O2 est retenu, le plafond.

### 6.2 Interface candidate (proposition)

```cpp
// include/prompt_reader.hpp
enum class PromptStatus { Line, Block, EmptyBlock, Exit, Eof, Aborted };

struct PromptResult {
    PromptStatus status;   // Line/Block/EmptyBlock/Exit/Eof/Aborted
    std::string content;   // Line : la ligne ; Block : le contenu assemblé
};

class PromptReader {
public:
    explicit PromptReader(std::istream& in, std::ostream& out);  // injection
    PromptResult read();                                          // une interaction
};
```

| Statut | Sens | Action REPL (proposition) |
|---|---|---|
| `Line` | ligne simple non vide | `agent.send(content)` (sauf `content == "exit"` ⇒ `Exit`) |
| `Block` | bloc non vide terminé par `:end` | **1** `agent.send(content)` |
| `EmptyBlock` | bloc **zéro ligne** terminé par `:end` | **aucun** envoi + **message informatif** |
| `Exit` | `exit` exact en mode simple | fin du REPL |
| `Eof` | EOF en mode simple | fin du REPL |
| `Aborted` | EOF en mode multiline | **aucun** envoi **et** fin du REPL (REPL-08) |

*« Ligne vide » en mode simple* : traitée **dans** `PromptReader` (aucun statut
retourné — le lecteur poursuit) ou retournée puis ignorée ; **à trancher** lors de
l'implémentation (impact nul sur l'observable).
*`exit`* : **recommandé** de le laisser en `Line` et de conserver le test
`content == "exit"` **actuel** dans `main.cpp` (moindre changement, REPL-02).

### 6.3 Injection des flux et intégration

* **Injection** : constructeur `PromptReader(std::istream&, std::ostream&)` ;
  `main.cpp` l'instancie sur `std::cin`/`std::cout`, les tests sur des flux
  mémoire. **Aucune** dépendance à `std::cin` en dur (sinon non testable).
* **Intégration** : `src/main.cpp:68-77` (lecture + `exit` + ligne vide) remplacé
  par une boucle appelant `reader.read()` et aiguillant selon le statut ;
  `src/main.cpp:79-104` (callbacks) et `106-134` (gestion d'erreurs) **inchangés**.

### 6.4 Fichiers concernés (à créer/modifier — **non touchés ici**)

| Fichier | Action | Rôle | Risque |
|---|---|---|---|
| `include/prompt_reader.hpp` | créer | interface + `PromptResult`/`PromptStatus` | faible |
| `src/prompt_reader.cpp` | créer | collecte, `:multi`, `:end`, CRLF, bloc vide, EOF | faible |
| `src/main.cpp` | modifier | brancher le lecteur (l.68-77) | **moyen** (ne pas altérer callbacks/erreurs) |
| `tests/prompt_reader_test.cpp` | créer | tests unitaires REPL-01→09 | faible |
| `CMakeLists.txt` (racine) | modifier **(autorisé D-5, non fait)** | ajouter `src/prompt_reader.cpp` à `agent_core` | faible |
| `tests/CMakeLists.txt` | modifier **(autorisé D-5, non fait)** | ajouter le test + groupe CTest `repl` (`foreach(group sse workspace http agent mock **repl**)`) | faible |

### 6.5 Cas de tests prévus (non exécutés)

| ID | Exigence | Cas | Attendu |
|---|---|---|---|
| T-01 | REPL-01 | saisie `:multi` | bascule ; **0** ligne retournée comme `Line` |
| T-02 | REPL-01 | ` :multi` / `:multi ` | **`Line`** = contenu (pas de bascule) |
| T-03 | REPL-01 | `:multi` **dans** un bloc | contenu littéral |
| T-04 | REPL-02 | `bonjour` | `Line("bonjour")` |
| T-05 | REPL-02 | ligne vide | **aucun** `Line` (poursuite) |
| T-06 | REPL-02 | `"   "` | `Line("   ")` (non vide) |
| T-07 | REPL-03 | `l1`, vide, `\tl2` | `Block("l1\n\n\tl2")` |
| T-08 | REPL-03 | `a\r\nb` (CRLF) | `Block("a\nb")` — **portée à confirmer (A-1)** |
| T-09 | REPL-03 | bloc `a` | contenu **`"a"`** (pas de `\n` final) |
| T-10 | REPL-04 | bloc + `:end` | terminé ; `:end` **hors** contenu |
| T-11 | REPL-04 | ` :end` | **contenu** littéral |
| T-12 | REPL-04 | `:end` en mode simple | `Line(":end")` |
| T-13 | REPL-05 | bloc 3 lignes + `:end` | **exactement 1** `Block` ⇒ 1 `send` |
| T-14 | REPL-06 | `:multi` puis `:end` | `EmptyBlock` ; **0** `send` + message |
| T-15 | REPL-06 | 2 lignes vides + `:end` | `Block("\n")` ⇒ **1** `send` |
| T-16 | REPL-07 | `exit` dans un bloc | contenu littéral ; pas d'`Exit` |
| T-17 | REPL-07 | `exit` simple | `Exit` ⇒ fin (ou `Line("exit")` + test inchangé) |
| T-18 | REPL-08 | `:multi`, 2 lignes, EOF | `Aborted` ; **0** `send` ; fin du REPL |
| T-19 | REPL-08 | EOF simple | `Eof` ⇒ fin |
| T-20 | REPL-09 | après `:end`, `bonjour` | `Line("bonjour")` (mode simple) — état réinitialisé |
| T-21 | REPL-09 | après `EmptyBlock`, `bonjour` | `Line("bonjour")` |
| T-22 | A-8 (si O2) | bloc > plafond | erreur explicite ; **0** `send` |

**Non-régression** : la suite existante (**75 cas / 298 assertions, 5 groupes**)
doit rester **verte** ; le REPL simple ne doit pas changer de comportement.

### 6.6 Critères d'acceptation

1. `ctest` : **6/6** groupes (5 existants + `repl`), **0 échec**.
2. `agent_tests` : total inchangé + nouveaux cas `repl`, **0 échec**.
3. **Aucun** `agent.send()` avant terminaison valide (vérifié par compteur d'appels).
4. **Un seul** `agent.send()` par bloc non vide terminé par `:end`.
5. **Aucun** `agent.send()` après abandon EOF (multiline).
6. Contenu du bloc **exact** (espaces, tabulations, lignes vides, CRLF→LF, sans `\n` final).
7. Mode simple **inchangé**.

## 7. FICHIERS MODIFIÉS PAR CETTE MISSION

| Fichier | Nature |
|---|---|
| `docs/reports/2026-09-23-repl-multiline-final-review-and-implementation-plan.md` | **créé** (ce rapport) |

**Aucun autre** fichier créé ou modifié : la spécification ratifiée, les rapports
antérieurs, `docs/README.md`, tout `src/`, `include/`, `tests/` et `CMakeLists.txt`
sont **inchangés** par cette mission.

## 8. COMMANDES RÉELLEMENT EXÉCUTÉES ET RÉSULTATS

| # | Commande | Résultat |
|---|---|---|
| V-1 | `git branch --show-current` / `git log -1 --oneline` / `git rev-parse HEAD` | `main` / `f62ca36 …` / `f62ca3680384b42…` |
| V-2 | comparaison HEAD ↔ référence de ratification | **IDENTIQUE** |
| V-3 | `git status --short` ; `git diff --stat` ; `git diff --cached --stat` | `M docs/README.md` (préexistant) + 5 non suivis ; **rien de stagé** |
| V-4 | `git rev-list --count HEAD` | **39** (inchangé) |
| V-5 | `grep -rn "repl-multiline-requirements"` | **1 lien vivant** (`docs/README.md:24`) + 3 mentions d'archive ⇒ impact renommage **limité** |
| V-6 | `grep -rn "max_read_chars\|max_write_chars\|max_history_messages\|max_tool_iterations"` | plafonds existants relevés : `50 000`, `200 000`, `20`, `5` ⇒ base de comparaison pour A-8 |
| V-7 | `grep -rln "getline\|std::cin\|PromptReader"` (src, tests) | aucun `PromptReader` ; lecture uniquement dans `src/main.cpp` ⇒ lecteur **à créer**, isolable |
| V-8 | `test -e <chemin du rapport>` | **absent** ⇒ création sans écrasement |
| V-9 | `git diff --check` | **silencieux** |
| V-10 | `git status --porcelain -- src include tests CMakeLists.txt` | **vide** ⇒ **aucune** source/test/CMake modifié |

**Aucun build, aucun test lancé, aucun commit** (conformément aux interdictions) :
aucune exigence n'est déclarée « vérifiée par test ».

## 9. VERDICT

### PRÊT SOUS RÉSERVE D'ARBITRAGES

**Justification** :

1. **Cohérence interne validée** : les 9 exigences REPL-01→REPL-09, la matrice de
   traçabilité et le registre D-1→D-8 sont **mutuellement cohérents** (§3) ;
   **aucune contradiction structurelle** ne subsiste, en particulier sur
   **EOF** (abandon + fin du REPL, jamais de retour au prompt), la **distinction
   zéro ligne / lignes vides**, la **conservation**, la **normalisation CRLF**,
   l'**absence de `\n` final**, l'**exclusion de `:end`** et le **traitement
   littéral** de `:multi` et `exit` dans un bloc.
2. **Conformité** : architecture (`Agent::send` inchangé), conventions
   (micro-harnais, pas de framework externe) et décisions ratifiées respectées ;
   la spécification **n'a pas été modifiée**.
3. **Deux décisions utilisateur restent en attente** (§5) : **limite de taille**
   (A-8) et **nommage du fichier** (A-9) — plus six **précisions** à confirmer
   (A-1 à A-7), dont **A-1** (portée de la normalisation CRLF) qui pourrait
   provoquer une **implémentation divergente** si elle reste implicite.
4. **Plan d'implémentation complet et testable** produit (§6) : responsabilités,
   interface candidate, injection des flux, intégration, fichiers, **22 cas de
   tests** couvrant REPL-01→09, critères d'acceptation et non-régression.

**Réserve bloquante pour une exécution sans ambiguïté** : **A-1** (portée CRLF),
**A-6** (`exit` exact), **A-8** (limite) et **A-9** (nommage) doivent être
**confirmés** avant de lancer l'implémentation, afin d'éviter une divergence.

**Arrêt** : aucune implémentation, aucun commit. La mission d'implémentation reste
**subordonnée à une autorisation explicite**.
