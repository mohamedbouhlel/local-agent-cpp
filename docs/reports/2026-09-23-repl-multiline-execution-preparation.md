# Préparation de l'implémentation REPL multiline — plan d'exécution

> Itération de **préparation uniquement**. **Aucun** code, test, CMake ou
> configuration modifié ; **aucune** implémentation ; **aucun** staging,
> **aucun** commit. Seul artefact créé : ce rapport.
> Référence normative : `docs/specifications/repl-multiline-requirements.md`
> (**APPROUVÉE**). Les arbitrages `A-1`→`A-9` sont **ratifiés** et **ne sont pas
> rediscutés** ici.

## 1. ÉTAT INITIAL

| Élément | Valeur observée |
|---|---|
| Dépôt | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `f62ca3680384b42ff4cdd8287a44eae4e3b15be2` (`f62ca36 docs: record agent test validation`) |
| Nombre de commits | **39** |
| Index (staged) | **vide** |
| Worktree | **non propre** — **préexistants** : `M docs/README.md` (indexation de la spécification) + **8** documents non suivis (spécification renommée + 7 rapports REPL) |

### 1.1 Documents de référence — présence vérifiée

| Document | Lignes | Rôle |
|---|---|---|
| `AGENTS.md` | 140 | gouvernance (code style, architecture, tests, invariants) |
| `docs/specifications/repl-multiline-requirements.md` | 262 | **normatif** — `REPL-01`→`REPL-09`, registre `D-1`→`D-8` + `A-1`→`A-9` |
| `docs/reports/2026-09-23-repl-multiline-final-review-and-implementation-plan.md` | 295 | revue finale + plan antérieur (base de ce plan) |
| `docs/reports/2026-09-23-repl-spec-rename.md` | 157 | renommage A-9 |
| `docs/reports/2026-09-23-repl-multiline-preparation.md` | 243 | préparation **initiale** (verdict `BLOQUÉE`, antérieure à la spécification) |

### 1.2 Divergences / décisions de nommage

| # | Constat |
|---|---|
| **D-1** | Le rapport cible `docs/reports/2026-09-23-repl-multiline-execution-preparation.md` **n'existe pas** ⇒ création sans écrasement (vérifié). |
| **D-2** | Un rapport de préparation **existe déjà** (`…-repl-multiline-preparation.md`) mais il n'est **pas équivalent** : il précède la spécification ratifiée et concluait `BLOQUÉE` (critères `REPL-01`→`09` alors introuvables). Il est **conservé tel quel** et le présent rapport utilise un **nom distinct**, conformément à la consigne. |
| **D-3** | **Aucune divergence** entre la spécification ratifiée et le code actuel : le REPL actuel ne comporte **ni** `:multi`, **ni** `:end`, **ni** multi-ligne — cohérent avec `REPL-01`→`09` (à implémenter). |
| **D-4** | Le **plan antérieur** (§6 du rapport de revue) reste valide ; **deux points** y étaient laissés ouverts (statut `Exit` dédié ? gestion de la ligne vide dans le lecteur ou dans `main` ?). Ils sont **tranchés par le présent plan** selon l'option la **plus minimale** (§3.4), sans modifier la spécification. |

## 2. ANALYSE TECHNIQUE

### 2.1 Chemin d'exécution actuel du REPL

`src/main.cpp` (138 l.) — séquence réelle :

| Ligne | Rôle |
|---|---|
| `66` | bannière `agent-cpp ready. Type 'exit' to quit.` |
| `68` | `std::string input;` |
| `69` | `while (true)` |
| `70` | invite **primaire** : `std::cout << "> "` |
| **`71`** | **point d'entrée de lecture** : `if (!std::getline(std::cin, input) \|\| input == "exit") break;` |
| `75-77` | ligne vide ⇒ `continue` (aucun envoi) |
| `79-104` | callbacks (`on_text_delta`, `on_tool_call`, `on_tool_result`) + compteurs `tools_ok`/`tools_failed` |
| **`107`** | **envoi** : `agent.send(input, callbacks)` |
| `108-112` | saut de ligne + récapitulatif `[execution: …]` |
| `113-134` | gestion d'erreurs (`HttpError`, `AgentError`, `std::exception`) |

**Commandes REPL actuelles** : **`exit`** (correspondance **exacte**, `main.cpp:71`) et la **ligne vide** (`:75`). **Rien d'autre** : `:multi` et `:end` **n'existent pas**. Aucune structure de lecture (`PromptReader`) n'existe dans `src/` ni `include/`.

### 2.2 Composants à créer / réutiliser

| Composant | Action | Justification |
|---|---|---|
| `PromptReader` (nouveau) | **créer** dans `agent_core` (`D-5`/`G1`) | isoler la lecture d'une interaction ; **testable sans `Agent`, sans réseau, sans LLM** via flux injectés |
| `Agent` / `AgentCallbacks` | **réutiliser sans modification** | la signature `send(const std::string&, const AgentCallbacks& = {})` reste **inchangée** ; le mode multiline ne change **rien** au protocole LLM |
| Reste de `main.cpp` (l.79-134) | **réutiliser sans modification** | callbacks et gestion d'erreurs R0 préservés |

### 2.3 Tests existants et organisation

* Micro-harnais maison (`tests/test_support.hpp`, `test_main.cpp`) — **aucun framework externe** ; `TEST(groupe, nom)` ; exécution filtrée par `--filter=<groupe>`.
* **5** groupes CTest : `sse`, `workspace`, `http`, `agent`, `mock` (`tests/CMakeLists.txt:23`), `TIMEOUT 30`.
* **75 cas / 298 assertions**, 0 échec (référence R0).
* **Aucun** test CLI/REPL : `main.cpp` **n'est pas** couvert (aucun harnais ne l'invoque) ⇒ la lecture doit être **extraite** dans un composant testable pour couvrir `REPL-01`→`09`.

### 2.4 Risques de régression

| # | Risque | Mitigation |
|---|---|---|
| **R-1** | Altérer le **mode simple** (bannière, invite, ordre `exit`/vide, envoi unique) | `REPL-02` + `A-6` ; **conserver** l'ordre et les libellés ; tests `T-04`→`T-06`, `T-12`, `T-17`, `T-19` |
| **R-2** | Modifier les **callbacks/gestion d'erreurs** (l.79-134) | **ne pas** toucher ces lignes ; revue de diff ciblée |
| **R-3** | `PromptReader` lié à `std::cin` en dur ⇒ **non testable** | constructeur prenant `std::istream&` / `std::ostream&` ; `main.cpp` injecte `std::cin`/`std::cout` |
| **R-4** | Normaliser CRLF **aussi** en mode simple (contredit `A-1`) | normalisation **dans la seule branche multiline** ; test `T-08b` (mode simple : `\r` **conservé**) |
| **R-5** | Terminer un bloc sur une ligne `:end` **indentée** (contredit `D-2`/`B1`) | correspondance **byte-exacte** ; test `T-11` |
| **R-6** | Ajouter un statut `Exit` ou déplacer le test `exit` ⇒ surface inutile | option **minimale** retenue (§3.4) |
| **R-7** | Le nouveau groupe CTest change les compteurs ⇒ `AGENTS.md` périmé | mise à jour de `AGENTS.md` §`Test`/§`Architecture` dans la même itération (§3.3) |
| **R-8** | Tests dépendant du **libellé** de l'invite (non normatif, `A-3`) | asserter les **statuts/contenus**, jamais le texte de l'invite |

### 2.5 Critères de vérification déterministes

1. **Aucun** accès à `std::cin`, **aucun** réseau : entrées via `std::istringstream`, sorties via `std::ostringstream`.
2. Assertions sur `PromptResult::status` et `content` (jamais sur l'invite — `A-3`).
3. **Unicité de l'envoi** vérifiée par la **séquence de résultats** : un bloc valide ⇒ **exactement un** `Block` (et `main` envoie une fois par `Line`/`Block`).
4. **Non-régression** : suite existante (**75 / 298**) intégralement verte.

### 2.6 Matrice `REPL-01` → `REPL-09`

| Exigence | Comportement attendu (spécification + arbitrages) | Composant | Test(s) prévu(s) |
|---|---|---|---|
| **REPL-01** | ligne **exactement** `:multi` (mode simple) ⇒ bascule multiline ; ligne **non** retournée comme contenu ; **0** envoi (`A-5` : asymétrie assumée) | `PromptReader::read` (état simple) | `T-01`, `T-02`, `T-03` |
| **REPL-02** | mode simple **inchangé** : ligne non vide ⇒ **1** envoi identique ; vide ⇒ 0 ; `exit` (**exact**, `A-6`) ⇒ fin | `PromptReader` + `main` | `T-04`, `T-05`, `T-06`, `T-12`, `T-17`, `T-19` |
| **REPL-03** | conservation **exacte** (vides, espaces, tabulations) ; **CRLF→`\n` en multiline seulement** (`A-1`) ; `\r` final unique retiré par ligne (`A-2`) ; **pas de `\n` final ajouté** ; `content = join(lignes, "\n")` (`A-7`) | `PromptReader::read` (état multiline) | `T-07`, `T-08`, `T-08b`, `T-09` |
| **REPL-04** | ligne **byte-exacte** `:end` ⇒ fin de collecte, **exclue** du contenu ; ` :end`/`:end ` ⇒ contenu ; en simple ⇒ contenu ordinaire (limitation `D-2` assumée) | `PromptReader::read` | `T-10`, `T-11`, `T-12` |
| **REPL-05** | terminaison d'un bloc **non vide** par `:end` ⇒ **1** envoi, contenu exact ; **aucun** envoi anticipé/abandonné/vide | `main` (aiguillage) + `PromptReader` | `T-13`, `T-14`, `T-18` |
| **REPL-06** | **zéro ligne** + `:end` ⇒ `EmptyBlock`, **0** envoi + message informatif ; bloc de **lignes vides/espaces** ⇒ **envoyé** (`A-7`) | `PromptReader` + `main` | `T-14`, `T-15` |
| **REPL-07** | simple : `exit` (**exact**) ⇒ fin ; multiline : `exit` = **contenu littéral** (ni fin, ni abandon) | `PromptReader` + `main` | `T-16`, `T-17` |
| **REPL-08** | simple : EOF ⇒ fin ; multiline : EOF ⇒ **abandon** (contenu jeté) **puis fin du REPL**, **sans** envoi, **jamais** de retour à l'invite | `PromptReader` + `main` | `T-18`, `T-19`, `T-21b` |
| **REPL-09** | retour à l'invite **primaire** + état **réinitialisé** **ssi** terminaison par `:end` (**bloc vide inclus**) ; **pas** de retour après EOF | `PromptReader` (état) + `main` | `T-20`, `T-21`, `T-21b` |
| `A-8` (`none`) | **aucune limite** de taille ⇒ **aucun** chemin d'erreur de taille ; parité avec le mode simple | — | (pas de test de limite ; option : bloc volumineux accepté) |

## 3. PLAN D'EXÉCUTION

### 3.1 Fichiers à modifier / créer (justifiés)

| # | Fichier | Action | Justification |
|---|---|---|---|
| 1 | `include/prompt_reader.hpp` | **créer** | interface du composant isolé (`D-5`/`G1`) |
| 2 | `src/prompt_reader.cpp` | **créer** | logique de collecte (`REPL-01`, `03`, `04`, `06`, `08`, `09`) |
| 3 | `tests/prompt_reader_test.cpp` | **créer** | tests unitaires `REPL-01`→`09` (§3.3) |
| 4 | `src/main.cpp` | **modifier** (l.68-77 **seulement**) | brancher `PromptReader` ; **l.79-134 intactes** |
| 5 | `CMakeLists.txt` | **modifier** (**+1 ligne**) | ajouter `src/prompt_reader.cpp` à `agent_core` (autorisé par `D-5`) |
| 6 | `tests/CMakeLists.txt` | **modifier** (**2 lignes**) | ajouter `prompt_reader_test.cpp` aux sources + `repl` à `foreach(group …)` |
| 7 | `AGENTS.md` | **modifier** (ciblé) | §`Test` : **6** groupes + compteurs ; §`Architecture` : ajouter `prompt_reader` ; §`Current state` |

**Interdits** : `Agent`, SSE, tool-calling, historique, `http_client`, `workspace_tools`, `tool_registry` — **aucune** modification.

### 3.2 Interface candidate (proposition, non encore écrite)

```cpp
// include/prompt_reader.hpp
enum class PromptStatus { Line, Block, EmptyBlock, Eof, Aborted };

struct PromptResult {
    PromptStatus status;
    std::string content;   // Line : la ligne ; Block : contenu assemblé
};

class PromptReader {
public:
    explicit PromptReader(std::istream& in, std::ostream& out);  // injection (R-3)
    PromptResult read();                                          // une interaction
};
```

* `Line` **couvre** la ligne simple **y compris vide** et `"exit"` ⇒ `main` conserve ses tests actuels (`exit` exact, vide) sans nouveau statut (**R-6**, option minimale).
* `EmptyBlock` = **zéro ligne** + `:end` (`REPL-06`) ; `Aborted` = EOF en multiline (`REPL-08`) ; `Eof` = EOF en simple.
* Le **libellé des invites** (primaire `> `, continuation `... `) reste **non normatif** (`L-3`, `A-3`).

### 3.3 Tests à ajouter / adapter

| Test | Exigence | Entrée (flux) | Attendu |
|---|---|---|---|
| `T-01` | REPL-01 | `:multi\n:end\n` | 1ᵉʳ `read()` = `EmptyBlock` ; **aucune** `Line` pour `:multi` |
| `T-02` | REPL-01 | ` :multi\n` puis `:multi \n` | `Line(" :multi")`, `Line(":multi ")` (pas de bascule) |
| `T-03` | REPL-01 | `:multi\na\n:multi\nb\n:end\n` | `Block("a\n:multi\nb")` (`:multi` littéral) |
| `T-04` | REPL-02 | `bonjour\n` | `Line("bonjour")` |
| `T-05` | REPL-02 | `\n` | `Line("")` (⇒ `main` : **0** envoi) |
| `T-06` | REPL-02 | `   \n` | `Line("   ")` (non vide ⇒ **1** envoi) |
| `T-07` | REPL-03 | `:multi\nl1\n\n\tl2\n:end\n` | `Block("l1\n\n\tl2")` |
| `T-08` | REPL-03/A-1 | `:multi\na\r\nb\n:end\n` | `Block("a\nb")` (**multiline** : CRLF normalisé) |
| **`T-08b`** | **REPL-03/A-1** | `a\r\n` | **`Line("a\r")`** ⇒ en **mode simple**, CRLF **non** normalisé |
| `T-09` | REPL-03/A-7 | `:multi\na\n:end\n` | `Block("a")` (pas de `\n` final) |
| `T-10` | REPL-04 | `:multi\na\n:end\n` | `:end` **absent** du contenu |
| `T-11` | REPL-04 | `:multi\n :end\n` | `Block(" :end")` (non terminateur) |
| `T-12` | REPL-04/02 | `:end\n` (mode simple) | `Line(":end")` |
| `T-13` | REPL-05 | `:multi\na\nb\nc\n:end\n` | **exactement 1** `Block` (⇒ 1 envoi) |
| `T-14` | REPL-06 | `:multi\n:end\n` | `EmptyBlock` ⇒ **0** envoi + message |
| `T-15` | REPL-06/A-7 | `:multi\n\n\n:end\n` | `Block("\n")` (blanc ⇒ **envoyé**) |
| `T-16` | REPL-07 | `:multi\nexit\n:end\n` | `Block("exit")` (littéral) |
| `T-17` | REPL-07/A-6 | `exit\n` | `Line("exit")` ⇒ `main` : fin |
| `T-18` | REPL-08 | `:multi\na\nb\n` (EOF) | `Aborted` ⇒ **0** envoi |
| `T-19` | REPL-08 | `` (EOF immédiat) | `Eof` ⇒ fin du REPL |
| `T-20` | REPL-09 | `:multi\na\n:end\nbonjour\n` | `Block("a")` **puis** `Line("bonjour")` (état réinitialisé) |
| `T-21` | REPL-09 | `:multi\n:end\nbonjour\n` | `EmptyBlock` **puis** `Line("bonjour")` |
| `T-21b` | REPL-08/09 | `:multi\na\n` (EOF) | `Aborted` ; **aucun** retour à l'invite primaire (fin) |
| `T-22` | A-8 (option) | bloc volumineux (> 100 000 caractères) | accepté, **aucun** envoi d'erreur de taille |

**Adaptations** : **aucun** test existant à modifier (le mode simple est préservé) — si un test devait échouer, **arrêter** et escalader (§3.6).

### 3.4 Précisions tranchées par le plan (sans changer la spécification)

| Point laissé ouvert (rapport de revue §6.2) | Décision du plan | Motif |
|---|---|---|
| Statut `Exit` dédié ? | **Non** : `Line("exit")`, test `exit` conservé dans `main` | surface minimale (`R-6`), `REPL-02`/`A-6` respectés |
| Ligne vide gérée dans le lecteur ou `main` ? | **`main`** (test `Line("")` conservé) | préserve le code actuel (l.75-77) |
| Libellé de l'invite de continuation | **non normatif** : `... ` par défaut | `L-3` ; tests `A-3` |

### 3.5 Séquençage (minimal, vérifiable)

| Étape | Contenu | Vérification |
|---|---|---|
| **S1** | créer `include/prompt_reader.hpp` + `src/prompt_reader.cpp` ; enregistrer dans `agent_core` (`CMakeLists.txt`) | compilation **seule** ; comportement du REPL **inchangé** (aucune intégration) |
| **S2** | créer `tests/prompt_reader_test.cpp` ; l'ajouter à `agent_tests` ; ajouter le groupe `repl` | `ctest -R repl` / `agent_tests --filter=repl` ⇒ tous verts |
| **S3** | intégrer dans `src/main.cpp` (l.68-77) | suite **complète** ; contrôle manuel du mode simple |
| **S4** | mettre à jour `AGENTS.md` (§Test, §Architecture, §Current state) | relecture ; `git diff --check` |
| **S5** | rapport d'exécution durable | relecture |

### 3.6 Commandes de vérification (à exécuter lors de l'exécution)

```
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON
cmake --build build-tests                                   # 0 erreur / 0 avertissement
cd build-tests && ctest --output-on-failure                 # 6/6 groupes
./tests/agent_tests                                         # 75 + N cas, 0 échec
./tests/agent_tests --filter=repl                            # groupe REPL seul
git diff --check                                             # propre
```
*(Le build de test peut se faire hors dépôt avec `-DFETCHCONTENT_SOURCE_DIR_JSON=<build/_deps/json-src> -DFETCHCONTENT_FULLY_DISCONNECTED=ON`, comme en Phase R0.)*

### 3.7 Critères d'acceptation

1. **`ctest` 6/6** groupes, **0** échec.
2. `agent_tests` : **75 + N** cas, **0** échec (aucune régression).
3. `--filter=repl` : **tous** les cas `T-01`→`T-22` verts.
4. **Mode simple identique** (`T-04`, `T-05`, `T-06`, `T-12`, `T-17`, `T-19`, `T-08b`).
5. **1 seul** envoi par bloc non vide (`T-13`) ; **0** envoi pour bloc vide (`T-14`) et abandon EOF (`T-18`).
6. Contenu **exact** préservé (espaces, tabulations, vides, CRLF multiline, sans `\n` final).
7. Compilation `-Wall -Wextra -Wpedantic` : **0** avertissement.
8. `git diff --check` **silencieux** ; **aucun** fichier hors §3.1 modifié.

### 3.8 Conditions d'arrêt et retour arrière

**Arrêt immédiat (escalade, sans forcer)** si :

* le **mode simple** ne peut pas être préservé à l'identique (`REPL-02`) ;
* un test existant échoue et la correction exigerait de **modifier la spécification** ;
* la modification de `CMakeLists.txt` est **refusée** (D-5 l'autorise, à confirmer dans l'itération d'exécution) ;
* un cas `REPL-01`→`09` s'avère **ambigu** après lecture de la spécification.

**Retour arrière** (planifié ; `git checkout`/`reset`/`clean` sont **interdits dans la présente itération**) : les fichiers modifiés étant **suivis** (`src/main.cpp`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `AGENTS.md`), une restauration depuis HEAD (`git restore <fichiers>`) + **suppression** des 3 fichiers créés (`prompt_reader.hpp/.cpp`, `prompt_reader_test.cpp`) suffit. **Aucun commit** ne sera effectué avant validation complète ⇒ retour arrière **sans perte**.

## 4. VÉRIFICATIONS EFFECTUÉES (cette itération)

| # | Commande | Résultat |
|---|---|---|
| V-1 | `git branch --show-current` / `git log -1 --oneline` / `git rev-parse HEAD` | `main` / `f62ca36 …` / `f62ca3680384b42…` |
| V-2 | `git diff --cached --stat` | **vide** (rien de stagé) |
| V-3 | `git status --short` | `M docs/README.md` + 8 non suivis **(préexistants)** |
| V-4 | présence des 5 documents de référence | **tous présents** (§1.1) |
| V-5 | `test -e` du rapport cible | **absent** ⇒ création sans écrasement (D-1) |
| V-6 | lecture de `src/main.cpp` (138 l.) | point d'entrée **l.71** ; envoi **l.107** (§2.1) |
| V-7 | lecture de `AGENTS.md` (140 l.) | 5 groupes, 75 cas / 298 assertions (`§Test`) ; `agent_core` sans `prompt_reader` (`§Architecture`) |
| V-8 | lecture de `tests/CMakeLists.txt` | sources `agent_tests` + `foreach(group sse workspace http agent mock)` ⇒ groupe `repl` **à ajouter** |
| V-9 | `grep` des exigences dans la spécification | **9** exigences `REPL-01`→`REPL-09` présentes, statuts **APPROUVÉE** |
| V-10 | `git diff --check` | **silencieux** |

**Aucun build, aucun test lancé** ; **aucune** validation fonctionnelle revendiquée.

## 5. RISQUES RÉSIDUELS ET POINTS À CONFIRMER

| # | Point |
|---|---|
| **C-1** | **Autorisation CMake** : l'exécution **doit** pouvoir modifier `CMakeLists.txt` et `tests/CMakeLists.txt` (D-5 l'autorise ; à confirmer explicitement avant S1). |
| **C-2** | Choix de l'implémentation pour **l'option minimale** (§3.4 : pas de statut `Exit`, ligne vide gérée par `main`) — **proposition** du plan, non normative. |
| **C-3** | Aucune **limite de taille** (`A-8` = `none`) ⇒ le test `T-22` est **optionnel**. |
| **C-4** | Le refus du `:multi` en mode simple (ligne **consommée**) est une **déviation assumée** du comportement actuel (`A-5`), à mentionner dans `AGENTS.md` §`Current state`. |

## 6. VERDICT

### PRÊT POUR EXÉCUTION

**Justification** :

1. **Spécification normative disponible et cohérente** : `REPL-01`→`REPL-09`
   approuvées, arbitrages `A-1`→`A-9` ratifiés ⇒ **aucun point de spécification
   bloquant** (les 4 points qui bloquaient au rapport de revue — A-1, A-6, A-8,
   A-9 — sont **tranchés**).
2. **Chemin d'exécution identifié avec preuves** (§2.1) : lecture `main.cpp:71`,
   envoi `main.cpp:107`, commandes actuelles `exit`/ligne vide uniquement.
3. **Plan séquencé, minimal et vérifiable** (§3) : 3 fichiers créés, **1**
   modification ciblée de `src/main.cpp` (l.68-77), **2** lignes CMake, tests
   `T-01`→`T-22` couvrant **les 9 exigences**, critères d'acceptation, conditions
   d'arrêt et retour arrière.
4. **Aucun blocage** : les seuls points ouverts sont des **confirmations**
   opérationnelles (`C-1` autorisation CMake, `C-2`/`C-3`/`C-4` options du plan),
   **sans incidence** sur la faisabilité.

**Décision demandée avant l'exécution** : **C-1** (autorisation de modifier
`CMakeLists.txt` et `tests/CMakeLists.txt`) et validation des options **C-2** à
**C-4**.

**Arrêt** : aucun code, aucun test, aucun staging, aucun commit.
