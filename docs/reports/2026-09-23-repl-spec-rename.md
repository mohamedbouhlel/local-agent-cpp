# Application de l'arbitrage A-9 — renommage de la spécification REPL (rapport)

> Itération **exclusivement documentaire**. **Aucun** fichier `.cpp`, `.h`,
> CMake, test ou d'implémentation modifié ; **aucune** implémentation du mode
> multiline ; **aucun** commit, **aucun** staging. Les **modifications
> préexistantes ont été préservées** (aucun écrasement, aucune annulation,
> aucun reformatage).

## 1. OBJECTIF ET PÉRIMÈTRE

Appliquer **uniquement** l'arbitrage **A-9** ratifié le 2026-09-23 :

1. renommer `docs/specifications/repl-multiline-requirements-proposal.md` en
   `docs/specifications/repl-multiline-requirements.md` ;
2. mettre à jour les références documentaires **devenues obsolètes** ;
3. produire ce rapport durable.

**Hors périmètre** : A-1 à A-8 (déjà enregistrés), toute implémentation, tout
contenu normatif. **Aucune décision A-1 à A-9 n'a été réinterprétée ni complétée.**

## 2. HEAD ET ÉTAT INITIAL DU WORKTREE

| Élément | Valeur observée |
|---|---|
| Répertoire | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `f62ca3680384b42ff4cdd8287a44eae4e3b15be2` (`f62ca36 docs: record agent test validation`) |
| Nombre de commits | **39** — inchangé |
| Index (staged) | **vide** |
| Worktree | **non propre** — **modifications préexistantes** (cf. §5), **expliquées**, **préservées** |

### 2.1 Documents de gouvernance consultés

`AGENTS.md` — `docs/context/PROJECT_CONTEXT.md` — `docs/history/PROJECT_HISTORY.md` —
`docs/roadmap/ROADMAP.md` — `docs/questions/OPEN_QUESTIONS.md` —
`docs/specifications/repl-multiline-requirements-proposal.md` —
`docs/reports/2026-09-23-repl-arbitrages-ratification.md` — `docs/README.md`.
*(Les documents racine `PROJECT_CONTEXT.md`, `PROJECT_HISTORY.md`, `ROADMAP.md`,
`OPEN_QUESTIONS.md` n'existent pas à la racine : ils résident sous `docs/` — fait
vérifié en phase 0.)*

### 2.2 Recensement des références au nom actuel (avant modification)

| # | Emplacement | Nature | Action |
|---|---|---|---|
| 1 | `docs/README.md:24` | **lien vivant** (index documentaire) | **MISE À JOUR** |
| 2 | `docs/reports/2026-09-23-repl-multiline-requirements-definition.md:67` | archive (état de cette mission) | **non modifié** |
| 3 | `docs/reports/revue-proposition-repl-multiline-2026-09-23.md:4` | artefact de revue | **non modifié** |
| 4 | `docs/reports/2026-09-23-repl-multiline-requirements-ratification.md:64` | archive | **non modifié** |
| 5 | `docs/reports/2026-09-23-repl-multiline-final-review-and-implementation-plan.md:19,29` | archive | **non modifié** |
| 6 | `docs/reports/2026-09-23-repl-arbitrages-ratification.md:19,35,91,118` | archive (état initial de cette mission) | **non modifié** |
| 7 | `docs/specifications/…-proposal.md` §6 (ligne **A-9** et bloc « Portée de cet enregistrement ») | **référence interne devenue obsolète** | **MISE À JOUR** (1 phrase, cf. §4) |

## 3. FICHIERS RENOMMÉS ET MODIFIÉS

| Fichier | Action | Détail |
|---|---|---|
| `docs/specifications/repl-multiline-requirements-proposal.md` | **renommé →** `docs/specifications/repl-multiline-requirements.md` | fichier **non suivi** par Git ⇒ `mv` simple (aucun historique Git à préserver). **Contenu intégralement conservé** (22 444 o, **260** lignes avant et après) |
| `docs/specifications/repl-multiline-requirements.md` | **modifié** (1 phrase) | §6, dernière puce de « Portée de cet enregistrement » (§4) |
| `docs/README.md` | **modifié** (1 ligne) | lien + libellé de l'index « Spécifications » (§4) |
| `docs/reports/2026-09-23-repl-spec-rename.md` | **créé** | ce rapport |

## 4. RÉFÉRENCES MISES À JOUR (minimales)

1. **`docs/README.md`** — dans la section « Spécifications » (bloc **préexistant**,
   non altéré par ailleurs) :
   * avant : ``[`specifications/repl-multiline-requirements-proposal.md`](specifications/repl-multiline-requirements-proposal.md)``
   * après : ``[`specifications/repl-multiline-requirements.md`](specifications/repl-multiline-requirements.md)``
   * **seul** changement sur cette ligne (libellé et description inchangés).

2. **Spécification renommée**, §6 « Registre des arbitrages ratifiés », bloc
   « Portée de cet enregistrement » — **une seule** puce, devenue **fausse** après
   ce renommage :
   * avant : « l'application de **A-9 (`rename`)** reste **à exécuter** dans une
     itération documentaire ultérieure. »
   * après : « l'application de **A-9 (`rename`)** a été **exécutée le
     2026-09-23** : ce document a été renommé en
     `docs/specifications/repl-multiline-requirements.md` (référence mise à jour
     dans `docs/README.md` ; voir `docs/reports/2026-09-23-repl-spec-rename.md`). »
   * **Justification** : mise à jour d'une **référence interne devenue obsolète**
     (autorisée par la phase 1.2, « uniquement lorsque cela est nécessaire »).
   * **Le registre `A-1` → `A-9` (tableau) et le contenu normatif `REPL-01` →
     `REPL-09` sont préservés à l'identique** — **aucune** décision modifiée.

## 5. MODIFICATIONS PRÉEXISTANTES PRÉSERVÉES

| Élément | État |
|---|---|
| `docs/README.md` — section « Spécifications » (bloc de **8** lignes) | **PRÉEXISTANTE** (mission de ratification). **Conservée** : seule la ligne de lien a été mise à jour (§4.1) ; le reste du bloc est **intact** |
| 6 rapports non suivis (`…preparation.md`, `…definition.md`, `…ratification.md`, `…final-review…md`, `…arbitrages-ratification.md`, `revue-proposition-…md`) | **PRÉEXISTANTS**, **non modifiés** |
| `docs/specifications/` (répertoire non suivi) | **PRÉEXISTANT** ; seul son **contenu** a été renommé |

**Aucun** changement préexistant n'a été écrasé, annulé, reformaté ni incorporé
silencieusement.

## 6. VÉRIFICATIONS EFFECTUÉES ET RÉSULTATS

| # | Contrôle | Résultat |
|---|---|---|
| V-1 | `ls -1 docs/specifications/` | `repl-multiline-requirements.md` **seul** fichier |
| V-2 | `test -e docs/specifications/repl-multiline-requirements-proposal.md` | **ABSENT** ⇒ ancien chemin supprimé (renommé) |
| V-3 | `test -f docs/specifications/repl-multiline-requirements.md` | **PRÉSENT** ; **260** lignes / 22 444 o (identiques avant/après) ⇒ **contenu préservé** |
| V-4 | `grep -rn "repl-multiline-requirements-proposal"` (dépôt, hors `build/`) | occurrences **résiduelles uniquement dans 5 archives/historiques** (§2.2 lignes 2-6) — **aucune** dans un **lien vivant** |
| V-5 | Liens relatifs de `docs/README.md` | **29 OK**, **0 cassé** |
| V-6 | Liens relatifs de la spécification renommée | **1 OK** (`../reports/2026-09-23-repl-multiline-final-review-and-implementation-plan.md`), **0 cassé** |
| V-7 | `git diff --check` | **silencieux** |
| V-8 | `git status --short` | `M docs/README.md` + **7** non suivis (dont ce rapport) — voir §7 |
| V-9 | `git status --porcelain -- src include tests CMakeLists.txt` | **vide** ⇒ **aucune** source/test/CMake modifiée |
| V-10 | `git diff --cached --stat` | **vide** ⇒ **rien de stagé** |
| V-11 | `git rev-parse HEAD` / `git rev-list --count HEAD` | `f62ca3680384b42…` / **39** — **inchangés** (aucun commit) |

**Aucun build et aucun test n'ont été exécutés** (aucun code modifié) : aucune
validation de compilation ou de test n'est revendiquée.

## 7. ÉCARTS ET LIMITES

| # | Point | Statut |
|---|---|---|
| **E-1** | Les **5 archives** (rapports historiques) conservent l'ancien nom | **Intentionnel** : interdiction de modifier rétroactivement les rapports historiques ; ces mentions décrivent l'état **à leur date** ⇒ **non mises à jour**, **signalées** (phase 1.7 : signaler plutôt qu'improviser) |
| **E-2** | La ligne **`A-9`** du tableau du registre indique encore « *Application non réalisée dans cette itération* » | **Préservée intégralement** (phase 1.4). Elle reste **exacte** : « cette itération » désigne l'itération d'**enregistrement** des arbitrages (2026-09-23, matin), pas celle-ci. La puce correspondante, elle, a été mise à jour (§4.2). Une consolidation rédactionnelle ultérieure reste possible |
| **E-3** | La description de l'entrée d'index mentionne `D-1 à D-8` mais **pas** `A-1 à A-9` | **Non modifiée** (consigne : mise à jour **minimale** de `docs/README.md`). Amélioration **optionnelle** à autoriser |
| **E-4** | Le renommage **n'est pas enregistré par Git** | Le fichier était **non suivi** ; le nouveau chemin est également non suivi ⇒ le renommage apparaîtra à l'**intégration**. **Aucun staging/commit** effectué (interdit) |

**Aucun écart bloquant** : l'ancien chemin n'existe plus, tous les **liens vivants**
sont valides, le contenu normatif et le registre `A-1`→`A-9` sont **préservés**.

## 8. DISTINCTION DES CHANGEMENTS

| Origine | Changements |
|---|---|
| **Modifications PRÉEXISTANTES** (itérations antérieures) | `docs/README.md` : bloc « Spécifications » (8 lignes) ; 6 rapports non suivis ; répertoire `docs/specifications/` |
| **Cette itération** | renommage du fichier de spécification ; **1** ligne de `docs/README.md` (lien) ; **1** phrase de la spécification (§6, puce A-9) ; **création** de ce rapport |

## 9. VERDICT

### VALIDÉ — RENOMMAGE DOCUMENTAIRE APPLIQUÉ

**Justification** :

1. Le fichier est **renommé** : `…-proposal.md` → **`…-requirements.md`** ;
   l'**ancien chemin n'existe plus** (V-1, V-2) et le **contenu est intégralement
   conservé** (V-3).
2. La **seule référence vivante** (`docs/README.md`) est **mise à jour** ;
   **tous les liens sont valides** (V-4, V-5, V-6).
3. **Contenu normatif et registre `A-1`→`A-9` préservés** ; la seule édition de la
   spécification est la mise à jour d'une **référence interne devenue obsolète**
   (autorisée, justifiée, tracée §4.2).
4. **Aucune** source, test ou configuration CMake modifiée (V-9) ; **aucun**
   staging, **aucun** commit (V-10, V-11) ; `git diff --check` **silencieux** (V-7).
5. **Modifications préexistantes préservées** (§5) ; écarts documentés (§7).

**Limites assumées** : **E-1** (archives conservant l'ancien nom),
**E-2** (ligne `A-9` du registre préservée), **E-3** (description d'index non
étendue), **E-4** (renommage non enregistré par Git — attendu).

**Arrêt** : itération documentaire terminée ; **aucun commit**. La suite
(implémentation du mode multiline) relève d'une **consigne d'exécution séparée**.
