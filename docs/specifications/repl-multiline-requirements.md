# Spécification — REPL multiline (`REPL-01` → `REPL-09`)

> **ÉTAT D'IMPLÉMENTATION VÉRIFIÉ AU 2026-11-05 : IMPLÉMENTÉ** au
> commit `465faae586c8f268d276597df05724431c750085`. Les décisions normatives `REPL-01` à `REPL-09`,
> `D-1` à `D-8` et `A-1` à `A-9` restent inchangées. L'implémentation
> correspondante est observable dans `src/prompt_reader.cpp`,
> `src/main.cpp` et `tests/prompt_reader_test.cpp`.
> Le groupe CTest `repl` est déclaré dans `tests/CMakeLists.txt`.
> **Les tests n'ont pas été rejoués pendant M4** ; les résultats d'exécutions
> antérieures restent des validations **rapportées**.
>
> **Portée temporelle :** les passages ci-dessous annonçant « aucune
> implémentation », « état actuel : une ligne », « tests à créer » ou
> « tests non exécutés » décrivent **l'état lors de la ratification du
> 23 septembre 2026**. Ils sont préservés pour la traçabilité et **ne
> décrivent pas l'état au commit ci-dessus**. Les exigences et arbitrages
> ratifiés ne sont ni réécrits ni remplacés par cette note de statut.
>
> **STATUT : APPROUVÉE** — ratifiée le **2026-09-23** par les décisions **D-1 à
> D-8** (voir §4). Ce document **remplace** la version « proposition » du même
> chemin ; il est **normatif** pour l'implémentation à venir.
>
> **Aucune implémentation** n'accompagne ce document : ni code, ni test, ni
> modification CMake. Les décisions D-5 et D-6 **autorisent** certaines
> modifications pour la future itération d'implémentation, elles ne les réalisent
> pas ici.

## 0. Contexte, portée et sources

**Projet** : `local-agent-cpp` — agent C++20 minimal (REPL → LLM local
OpenAI-compatible → réponse, avec outils sandboxés).

**Besoin** : saisir ou coller un **prompt multi-ligne** et qu'il soit transmis à
l'agent comme **une seule interaction**, **préservé à l'identique**. État actuel :
le REPL ne lit **qu'une ligne** par itération (`src/main.cpp:71`) et n'offre
**aucun** mécanisme multi-ligne ; un prompt collé est découpé en interactions
distinctes.

**Sources consultées**

| Source | Usage |
|---|---|
| `src/main.cpp` (138 l.) | comportement réel : `std::getline` (l.71), `exit`/EOF, ligne vide, `agent.send()` (l.107) |
| `AGENTS.md` | conventions (code style, architecture, tests) |
| `docs/reports/2026-09-23-reanalyse-boucle-agentique.md` | constat d'absence du multi-ligne |
| `docs/reports/2026-09-23-repl-multiline-preparation.md` | préparation (point d'intégration) |
| `docs/reports/2026-09-23-repl-multiline-requirements-definition.md` | rapport de définition (proposition initiale) |
| `docs/reports/revue-proposition-repl-multiline-2026-09-23.md` | revue : contradiction **REPL-08/REPL-09** + points à trancher (traités par D-1→D-8) |

**Terminologie**

* **Mode simple** : comportement actuel — une ligne ⇒ une interaction.
* **Mode multiline** : collecte de lignes jusqu'au terminateur `:end`.
* **Bloc** : lignes accumulées en mode multiline, **hors** terminateur.
* **Envoi** : unique point d'entrée vers le LLM — `Agent::send(...)`.

**Défauts d'entrée normalisés (D-7)** : `\r\n` (CRLF) est **normalisé en `\n`** ;
les autres contenus sont **conservés tels quels**.

---

## 1. Exigences approuvées

> Chaque exigence est **APPROUVÉE** (D-8, 2026-09-23). Les formulations « le REPL
> **doit**… » sont **normatives**.

### REPL-01 — Déclenchement du mode multiline  *(APPROUVÉE — D-1/A1)*

1. **Identifiant et titre** : `REPL-01` — Déclenchement du mode multiline.
2. **Formulation normative** : *En mode simple, le REPL **doit** basculer en mode multiline lorsque la ligne saisie est **exactement** `:multi` (aucun caractère avant ni après, espaces et tabulations compris). Cette ligne **n'est pas** accumulée et **ne déclenche aucun** `agent.send()`. Le REPL **doit** alors afficher une invite de continuation distincte (ex. `... `).*
3. **Justification** : déclenchement **explicite, déterministe et testable** ; préserve intégralement la saisie simple ; aucune ambiguïté avec du contenu littéral.
4. **Scénario d'acceptation** : saisir `:multi` ⇒ invite de continuation affichée, **0** `agent.send()`.
5. **Cas limites** : ` :multi` / `:multi ` ⇒ **contenu** (non reconnu) ; `:multi` **dans** un bloc ⇒ **contenu littéral** (D-7) ; `:multi` répété en début de collecte ⇒ une seule bascule.
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : REPL-02, REPL-04.

### REPL-02 — Saisie simple et compatibilité  *(APPROUVÉE — D-8)*

1. **Identifiant et titre** : `REPL-02` — Saisie simple inchangée.
2. **Formulation normative** : *En l'absence de déclenchement (REPL-01), le REPL **doit** conserver le comportement actuel : une ligne non vide ⇒ **exactement un** `agent.send()` avec cette ligne ; une ligne vide ⇒ **aucun** envoi ; la ligne `exit` (en mode simple) ⇒ fin du REPL.*
3. **Justification** : **non-régression** stricte (`src/main.cpp:71-77`, `:107`).
4. **Scénario d'acceptation** : saisir `bonjour` ⇒ **1** envoi, contenu exactement `bonjour` ; ligne vide ⇒ **0** envoi.
5. **Cas limites** : ligne d'**espaces** uniquement : **non vide** ⇒ **envoyée** (comportement actuel préservé) ; `exit` avec espaces ⇒ **contenu**, pas commande.
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : aucune (référence de non-régression).

### REPL-03 — Accumulation et conservation exacte du contenu  *(APPROUVÉE — D-7)*

1. **Identifiant et titre** : `REPL-03` — Conservation exacte des lignes.
2. **Formulation normative** : *En mode multiline, le REPL **doit** accumuler chaque ligne **sans transformation** — lignes vides, espaces et tabulations compris. Les fins de ligne **CRLF (`\r\n`) sont normalisées en `\n`** ; aucun `\n` final **artificiel** n'est ajouté. Le contenu du bloc **doit** être les lignes accumulées concaténées par `\n`, **hors** ligne terminateur.*
3. **Justification** : un prompt multi-ligne doit parvenir au modèle **à l'identique** (indentations comprises), avec une règle de reconstruction explicite.
4. **Scénario d'acceptation** : bloc `ligne1`, ligne vide, `\tindenté`, ligne vide, `ligne2` ⇒ contenu exact `"ligne1\n\n\tindenté\n\nligne2"`.
5. **Cas limites** : espaces en fin de ligne ⇒ **conservés** ; collage CRLF ⇒ normalisé ; `\n` final ⇒ **non ajouté** ; limite de taille ⇒ **non définie** par cette spécification (point ouvert, §5).
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : REPL-04, REPL-05.

### REPL-04 — Terminateur `:end`  *(APPROUVÉE — D-2/B1)*

1. **Identifiant et titre** : `REPL-04` — Terminateur `:end`.
2. **Formulation normative** : *En mode multiline, une ligne **exactement** égale à `:end` (byte-exacte, aucun caractère avant ni après) **doit** terminer la collecte, **sans** être incluse dans le contenu du bloc. En mode simple, `:end` **doit** être traité comme du contenu ordinaire.*
3. **Justification** : terminateur **non ambigu**, symétrique de `:multi`, sans terminaison accidentelle.
4. **Scénario d'acceptation** : bloc `analyse ceci` puis `:end` ⇒ collecte terminée, contenu = `analyse ceci`.
5. **Cas limites** : `:end` en **première** ligne ⇒ bloc vide (REPL-06) ; ` :end` / `:end ` ⇒ **contenu** ; `:end` au milieu d'une phrase ⇒ contenu ; **`:end` seul en mode simple** ⇒ contenu ordinaire, **pas** un terminateur.
   **Limitation ratifiée (D-2)** : en mode multiline, il est **impossible** de saisir une ligne littérale valant exactement `:end` — elle termine toujours le bloc. Limitation **acceptée** (arbitrage D-2/B1).
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : REPL-03, REPL-05, REPL-06.

### REPL-05 — Envoi du bloc complet et unicité de l'appel  *(APPROUVÉE — D-8)*

1. **Identifiant et titre** : `REPL-05` — Un seul envoi par bloc valide.
2. **Formulation normative** : *À la terminaison d'un bloc **non vide** par `:end`, le REPL **doit** appeler `agent.send()` **exactement une fois**, avec le contenu exact du bloc. **Aucun** `agent.send()` ne **doit** être appelé : avant la terminaison ; pour un bloc vide (zéro ligne, REPL-06) ; après un abandon par EOF (REPL-08).*
3. **Justification** : corrige le défaut constaté (prompt multi-ligne découpé en plusieurs interactions).
4. **Scénario d'acceptation** : bloc de 3 lignes + `:end` ⇒ **1** `agent.send()`, contenu = bloc exact.
5. **Cas limites** : `:end` immédiat ⇒ **0** envoi (REPL-06) ; EOF avant `:end` ⇒ **0** envoi (REPL-08).
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : REPL-04, REPL-06, REPL-08.

### REPL-06 — Traitement des blocs vides  *(APPROUVÉE — D-3/C1)*

1. **Identifiant et titre** : `REPL-06` — Bloc vide : aucun envoi, message informatif.
2. **Formulation normative** : *Si la collecte se termine par `:end` **sans aucune ligne accumulée** (zéro ligne), le REPL **ne doit pas** appeler `agent.send()` et **doit** afficher un **message informatif** (ex. « bloc vide ignoré »). **Un bloc composé uniquement de lignes vides ou d'espaces est, structurellement, NON vide : il est envoyé** (D-7).*
3. **Justification** : éviter une requête LLM sans contenu ; distinguer explicitement **zéro ligne** d'un **contenu blanc** ; aucun envoi implicite.
4. **Scénario d'acceptation** : `:multi` puis `:end` ⇒ **0** `agent.send()` + message ; `:multi`, trois lignes vides, `:end` ⇒ **1** `agent.send()` avec `"\n\n"`.
5. **Cas limites** : espaces seuls ⇒ **envoyé** ; tabulations seules ⇒ **envoyé**.
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : REPL-04, REPL-05.

### REPL-07 — `exit` selon le mode  *(APPROUVÉE — D-4/D1)*

1. **Identifiant et titre** : `REPL-07` — `exit` en simple vs multiline.
2. **Formulation normative** : *En mode simple, la ligne `exit` **doit** terminer le REPL (comportement actuel). En mode multiline, la ligne `exit` **doit** être traitée comme du **contenu littéral** : ni commande de sortie, ni terminateur, ni abandon.*
3. **Justification** : préserver la sortie explicite en simple ; **aucune perte de contenu** en multiline ; l'abandon dispose d'EOF (REPL-08).
4. **Scénario d'acceptation** : en multiline, `exit` puis `:end` ⇒ le contenu transmis contient `exit` ; le REPL **ne se termine pas**.
5. **Cas limites** : `exit` en première ligne d'un bloc ⇒ contenu ; `exit` en simple ⇒ fin du REPL.
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : REPL-02, REPL-03.

### REPL-08 — EOF / Ctrl+D  *(APPROUVÉE — correction obligatoire)*

1. **Identifiant et titre** : `REPL-08` — Fin de flux (EOF/Ctrl+D).
2. **Formulation normative** : *En mode simple, EOF/Ctrl+D **doit** terminer le REPL (comportement actuel). En mode multiline, EOF/Ctrl+D **doit** **abandonner** le bloc en cours **sans** appeler `agent.send()` (contenu accumulé jeté), **puis terminer le REPL**. **EOF ne revient jamais à l'invite primaire** ; il n'y a **aucune** reprise après abandon.*
3. **Justification** : **correction obligatoire** de la contradiction relevée en revue (REPL-08 ↔ REPL-09) : un flux terminé ne peut pas à la fois finir le processus et rendre la main au prompt.
4. **Scénario d'acceptation** : `:multi`, 2 lignes, puis EOF ⇒ **0** `agent.send()` et **fin du REPL** (aucune invite primaire affichée).
5. **Cas limites** : EOF immédiat après `:multi` ⇒ **0** envoi + fin du REPL ; EOF en simple ⇒ fin du REPL (inchangé).
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : REPL-02, REPL-05, REPL-09.

### REPL-09 — Retour à l'invite primaire  *(APPROUVÉE — corrigée)*

1. **Identifiant et titre** : `REPL-09` — Retour à l'invite **uniquement après `:end`**.
2. **Formulation normative** : *Le REPL **doit** revenir à l'invite **primaire** (`> `) et **réinitialiser** l'état de collecte **si et seulement si** la collecte s'est terminée par `:end` — **y compris un bloc vide terminé par `:end`** (REPL-06). Tant qu'un bloc est en cours (pas de `:end`), le REPL **doit** rester en mode multiline. **EOF n'entraîne jamais de retour à l'invite primaire** : il termine le REPL (REPL-08).*
3. **Justification** : lever la contradiction **REPL-08/REPL-09** signalée en revue ; rendre le retour au prompt **univoquement** lié à `:end` ; aucune reprise implicite après abandon.
4. **Scénario d'acceptation** : après un `:end` (bloc non vide **ou** vide), saisir `bonjour` ⇒ **mode simple**, **1** envoi immédiat, sans continuation ; état de collecte vierge.
5. **Cas limites** : deux blocs successifs terminés par `:end` ⇒ pas de résidu ; bloc vide terminé par `:end` ⇒ retour à l'invite primaire ; EOF ⇒ **pas** de retour à l'invite (fin du REPL).
6. **Statut** : **APPROUVÉE**.
7. **Dépendances** : REPL-01, REPL-06, REPL-08.

---

## 2. Matrice de traçabilité (exigence → règle → scénario → observable)

> **Aucun** de ces tests n'a été exécuté : ils sont **prévus** pour l'itération
> d'implémentation.

| REPL-ID | Règle approuvée | Scénario | Résultat observable |
|---|---|---|---|
| REPL-01 | `:multi` exact déclenche | saisir `:multi` | invite de continuation ; **0** envoi |
| REPL-01 | `:multi` dans un bloc = littéral | bloc `a`, `:multi`, `b` | contenu `"a\n:multi\nb"` ; **1** envoi |
| REPL-02 | saisie simple inchangée | saisir `bonjour` | **1** envoi, contenu `bonjour` |
| REPL-02 | ligne vide ignorée | saisir ligne vide | **0** envoi |
| REPL-02 | ligne d'espaces envoyée | saisir `"   "` | **1** envoi, contenu `"   "` |
| REPL-03 | conservation exacte | `l1`, vide, `\tl2` | contenu `"l1\n\n\tl2"` |
| REPL-03 | CRLF normalisé | bloc avec `a\r\nb` | contenu `"a\nb"` |
| REPL-03 | pas de `\n` final ajouté | bloc `a` + `:end` | contenu exactement `"a"` |
| REPL-04 | `:end` exact termine | bloc puis `:end` | fin de collecte ; `:end` hors contenu |
| REPL-04 | ` :end` non terminateur | ligne ` :end` | **contenu** littéral |
| REPL-04 | `:end` en simple = contenu | saisir `:end` (mode simple) | **1** envoi, contenu `:end` |
| REPL-05 | un seul envoi | bloc 3 lignes + `:end` | **1** `agent.send()` |
| REPL-05 | aucun envoi avant terminaison | collecte en cours | **0** envoi jusqu'à `:end` |
| REPL-06 | bloc **zéro ligne** non envoyé | `:multi` puis `:end` | **0** envoi + message informatif |
| REPL-06 | bloc de lignes vides **envoyé** | 2 lignes vides + `:end` | **1** envoi, contenu `"\n"` |
| REPL-07 | `exit` littéral en multiline | `exit` puis `:end` | contenu contient `exit` ; REPL actif |
| REPL-07 | `exit` en simple | saisir `exit` | fin du REPL |
| REPL-08 | EOF abandonne **et termine** | `:multi`, 2 lignes, EOF | **0** envoi ; **fin du REPL** |
| REPL-08 | EOF en simple | EOF | fin du REPL (inchangé) |
| REPL-09 | retour invite après `:end` | `:end`, puis `bonjour` | **1** envoi simple, pas de continuation |
| REPL-09 | retour invite après bloc vide `:end` | `:multi`, `:end`, puis `bonjour` | **1** envoi simple |
| REPL-09 | **pas** de retour après EOF | EOF en multiline | fin du REPL ; **aucune** invite primaire |

**Tests prévus (non exécutés)** : groupe CTest `repl` (à créer) exécuté via
`agent_tests --filter=repl`, plus la non-régression de la suite existante
(75 cas / 298 assertions, 5 groupes).

---

## 3. Éléments écartés (contexte des décisions)

| Décision | Option **retenue** | Options écartées (et pourquoi) |
|---|---|---|
| D-1 | **A1** — sentinelle `:multi` | A2 continuation `\` (ambigu avec le contenu) ; A3 bascule `on/off` (état persistant) ; A4 détection du collage (non déterministe, dépend du terminal) |
| D-2 | **B1** — `:end` byte-exact | B2 comparaison après `trim` (terminerait un bloc contenant `:end` indenté) |
| D-3 | **C1** — aucun envoi + message informatif | C2 silence total (confusion) ; C3 erreur bloquante (sémantique discutable) |
| D-4 | **D1** — `exit` littéral en multiline | D2 `exit` abandonne le bloc (perte silencieuse de contenu) |
| D-5 | **G1** — `PromptReader` isolé dans `agent_core`, flux injectés | G2 compilation ad hoc dans `agent_tests` ; G3 logique dans `main.cpp` (non testable) |

---

## 4. Registre des décisions ratifiées (D-1 → D-8)

| ID | Décision ratifiée | Justification |
|---|---|---|
| **D-1** | **A1** : `:multi` exact déclenche le mode multiline | déterminisme, compatibilité totale avec la saisie simple, testabilité |
| **D-2** | **B1** : seule la ligne exacte `:end` termine le bloc ; la limitation (impossibilité d'une ligne littérale `:end`) est **acceptée** | reconnaissance non ambiguë, symétrique de `:multi` |
| **D-3** | **C1** : bloc vide (**zéro ligne**) ignoré, **message informatif**, **aucun envoi** | évite une requête LLM sans contenu, sans envoi implicite |
| **D-4** | **D1** : `exit` **littéral** en multiline ; `exit` **termine** le REPL en mode simple | aucune perte de contenu ; préserve la sortie explicite |
| **D-5** | **G1** : `PromptReader` **isolé**, testable via **flux injectés** ; la **modification de CMake est autorisée** pour cette architecture (à réaliser lors de l'implémentation) | testabilité sans `Agent`/réseau/LLM ; séparation des responsabilités |
| **D-6** | `docs/specifications/` est **ratifié** comme catégorie documentaire ; indexation dans `docs/README.md` | conventions du dépôt (`docs/<type>/`, minuscules plurielles) |
| **D-7** | **Conservation** : séparateur `\n` ; lignes vides, espaces et tabulations **conservés** ; **CRLF → `\n`** ; **pas de `\n` final artificiel** ; bloc de lignes vides/espaces **structurellement non vide ⇒ envoyé** ; `:end` exact **exclu** du contenu ; `:multi` dans un bloc **littéral** | règle de reconstruction explicite, préservation fidèle du prompt |
| **D-8** | **REPL-01 → REPL-09 approuvées** après correction de toutes les contradictions (notamment EOF/REPL-09 et conservation) | lève le point bloquant de la revue |

---

## 5. Limites, points ouverts et non-objectifs

| # | Point |
|---|---|
| L-1 | **Limite de taille** d'un bloc : **non définie** par cette spécification (aucun envoi implicite ; comportement à définir si un besoin apparaît). |
| L-2 | **Limitation ratifiée (D-2)** : une ligne littérale valant exactement `:end` est **impossible** en mode multiline. |
| L-3 | **Invite de continuation** : `... ` est un **exemple** ; le libellé exact n'est pas normé. |
| L-4 | **Aucun test exécuté** : la matrice §2 est un **plan de vérification**, pas un résultat. |
| L-5 | **Non-objectifs** : coloration syntaxique, historique de commandes, édition interactive ligne à ligne, complétion. |
| L-6 | Les documents `2026-09-23-repl-multiline-preparation.md` et `2026-09-23-repl-multiline-requirements-definition.md` restent des **archives** (verdicts de leurs missions) : **non modifiés**, non normatifs. |

---

## 6. Registre des arbitrages ratifiés (A-1 → A-9)

> **Décisions ratifiées par le mainteneur le 2026-09-23.**
> **Enregistrement documentaire effectué le 2026-09-23** (itération de
> consignation, sans implémentation).
> Ces arbitrages **lèvent les points laissés en attente** au §4 du rapport
> [`2026-09-23-repl-multiline-final-review-and-implementation-plan.md`](../reports/2026-09-23-repl-multiline-final-review-and-implementation-plan.md),
> où figurent leur **contexte** et leur **justification technique**.

**Règle de préséance** : en cas d'écart textuel entre les exigences `REPL-01`→
`REPL-09` (ou les limites `L-1`→`L-6`) et le présent registre, **les arbitrages
A-1 à A-9 prévalent** et précisent la lecture de l'exigence concernée. Le corps
des exigences n'est **pas** réécrit ici.

| ID | Décision ratifiée | Exigence / limite concernée | Portée normative (lecture à appliquer) | Contexte (justification) |
|---|---|---|---|---|
| **A-1** | **`multiline`** | `REPL-03` ; §0 « Défauts d'entrée normalisés » | La normalisation **CRLF → `\n`** s'applique **uniquement au contenu d'un bloc multiline**. En **mode simple**, `REPL-02` prime : la ligne est transmise **telle quelle** (un `\r` final éventuel est conservé) | Le §0 ne précisait pas le mode ; le restreindre évite une régression du mode simple |
| **A-2** | **Confirmée** (recommandation précédente) | `REPL-03` | Ne supprimer **qu'un seul `\r` final** par ligne collectée (traitement de `\r\n`) ; **tout autre `\r` est conservé** | Le `\r` isolé (non suivi de `\n`) n'était pas spécifié |
| **A-3** | **Confirmée** (recommandation précédente) | `REPL-01`, `L-3` ; plan de tests | Les tests **ne doivent pas** dépendre du **libellé** de l'invite de continuation : asserter l'**état** (mode multiline actif), **pas** la chaîne affichée | `... ` n'est qu'un **exemple** (limite `L-3`) ; éviter des tests fragiles |
| **A-4** | **Confirmée** (recommandation précédente) | `REPL-08` | **Aucun message n'est requis** avant la fin du REPL provoquée par EOF en multiline ; une note sur `stderr` est **optionnelle** et non normative | Le REPL se termine immédiatement : un message serait inutile |
| **A-5** | **Confirmée** (recommandation précédente) | `REPL-01`, `REPL-04` | L'**asymétrie** en mode simple est **explicite** : `:multi` (exact) est une **commande** ; `:end` (exact) est du **contenu**. Ratifiée par `D-1`/`D-2` | Documentation d'un choix déjà ratifié, sans nouvelle décision |
| **A-6** | **`exact`** | `REPL-07` (et `REPL-02`) | En mode simple, `exit` est reconnu par **correspondance exacte** de la ligne (`"exit"`, sans `trim`, sans espaces) ; en multiline, `exit` reste **littéral** | Aligner la spécification sur le code actuel (`input == "exit"`) et éviter une implémentation par `trim` |
| **A-7** | **Confirmée** (recommandation précédente) | `REPL-03`, `REPL-06` | Formule du contenu : **`content = join(lignes, "\n")`** — 0 ligne ⇒ bloc vide (`REPL-06`) ; 1 ligne vide ⇒ `""` ; 2 lignes vides ⇒ `"\n"` | Formule non écrite littéralement ; la fixer évite toute divergence |
| **A-8** | **`none`** | `L-1` | **Aucune limite de taille** n'est imposée à un bloc multiline : **parité avec le mode simple**. `L-1` doit se lire « **aucune limite imposée** » | Le mode simple n'a aucune limite ; aucune valeur n'est inventée |
| **A-9** | **`rename`** | Document (nommage) | Le document doit être **renommé** de `…-proposal.md` vers **`repl-multiline-requirements.md`** (document **ratifié**, plus une « proposition »). **Application non réalisée dans cette itération** (opération documentaire distincte) ; impact identifié : **1 lien vivant** (`docs/README.md`) | Le suffixe `-proposal` est devenu trompeur |

**Contexte et justification** : reportés, sans réinterprétation, au §4 et §5 du
rapport de revue cité en tête de section (A-1 à A-9 y sont formulés avec leurs
options et leurs conséquences).

**Portée de cet enregistrement** :

* il **consigne** des décisions ; il **n'implémente rien** ;
* aucune **proposition future** n'est transformée en **implémentation réalisée** :
  `PromptReader`, le mode multiline, les tests et les modifications CMake restent
  **à réaliser** ;
* les arbitrages `A-2` à `A-5` et `A-7` sont **confirmés conformément aux
  recommandations précédentes** (rapport de revue §4) ; leur formulation n'est
  **ni complétée ni reformulée** au-delà de ces sources ;
* l'application de **A-9 (`rename`)** a été **exécutée le 2026-09-23** : ce
  document a été renommé en `docs/specifications/repl-multiline-requirements.md`
  (référence mise à jour dans `docs/README.md` ; voir
  `docs/reports/2026-09-23-repl-spec-rename.md`).
