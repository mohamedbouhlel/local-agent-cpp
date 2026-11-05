# M3 — Réconciliation de l'historique, de la roadmap et des questions ouvertes

- Date : **2026-11-05** (Europe/Paris)
- Dépôt : `mohamedbouhlel/local-agent-cpp`
- **État observé de `main` :** `add64b893ad447e76fb12b90e266ec8eab2f2956`
- **Base de branche M3 :** `f77fb53b2e398d0e455b4e2d9d21fc7fe64de7b1` (branche M2, PR #6 encore en brouillon/non fusionnée au début de M3)
- Branche M3 : `docs/m3-history-roadmap-questions-20261105`
- Périmètre : **documentation uniquement** : `PROJECT_HISTORY.md`, `ROADMAP.md`, `OPEN_QUESTIONS.md`, ce rapport.

## 1. Objectif

Distinguer les archives historiques (dont `4f33b62` et `31f3e2a`), les faits vérifiés au HEAD `add64b893ad447e76fb12b90e266ec8eab2f2956`, les validations seulement rapportées, les décisions approuvées et les propositions non décidées.

## 2. Constats et preuves

| Constat | Source | Qualification |
|---|---|---|
| 65 commits visibles dans l'historique GitHub distant | API GitHub / commits de `main`, consultés le 2026-11-05 | Fait distant vérifié |
| 27 premiers commits archivés jusqu'à `4f33b62` | `PROJECT_HISTORY.md` ancien | Fait historique, archive préservée |
| Suite de tests versionnée, 118 déclarations de cas et 7 groupes Linux | `tests/*_test.cpp`, `tests/CMakeLists.txt` | Code vérifié |
| 403 assertions et 7/7 groupes réussis | `AGENTS.md`, PR #5 | **Rapporté**, non exécuté en M3 |
| REPL multiligne implémenté `31f3e2a` | Git et `src/prompt_reader.cpp` | Fait vérifié |
| Confinement Landlock, refus des racines symboliques, restrictions supplémentaires | PR #3–#5, `src/filesystem_sandbox.cpp` | Fait vérifié |
| `fsync` absent de l'écriture ; limites de concurrence | `src/workspace_write.cpp`, rapports de sécurité | Code vérifié, risques résiduels |
| Absence de workflow CI et de licence | Inventaire de l'arbre Git | Fait distant vérifié |
| Remotes et `.env` locaux | Audit de septembre | **Non recontrôlés**, non généralisables à GitHub |

## 3. Modifications

1. `PROJECT_HISTORY.md` : continuation datée des événements postérieurs à `4f33b62`, avec conservation **verbatim** de l'audit initial.
2. `ROADMAP.md` : remplacement des sections A/B par l'état courant sourcé ; **préservation du contenu initial A/B en annexe historique** ; conservation des propositions C–F sans ratification implicite.
3. `OPEN_QUESTIONS.md` : mise à jour des questions Q1, Q4, Q5, Q15, Q17 et de leur qualification temporelle ; autres points conservés et non artificiellement résolus.
4. Aucun fichier source, test, CMake, configuration, spécification ou autre document de référence modifié pendant M3.

## 4. Vérifications et limites

- Les déclarations de tests et les groupes CTest sont vérifiés **par lecture**, sans exécution.
- La conservation de l'archive historique et des anciennes sections A/B est contrôlée par comparaison stricte de chaînes lors de la publication.
- Les liens et les fichiers sont vérifiés après écriture sur la branche M3.
- Le diff et les noms des fichiers modifiés sont examinés avant ouverture de la PR.
- Aucun nouveau build, CTest, test réel LLM, inspection du worktree local, mesure de sécurité ou merge sur `main` n'est effectué.
- Les sections C–F de la roadmap gardent leur caractère **historique/propositionnel** ; leur réévaluation détaillée est hors M3. `AGENTS.md`, `USAGE.md`, `README.md` et la spécification REPL restent à réconcilier en M4.

## 5. Verdict

**M3 documentaire prêt pour revue**, sous réserve des vérifications post-publication indiquées. La PR M3 doit rester distincte de M2 et cibler la branche M2 tant que PR #6 n'est pas fusionnée. Aucune fusion implicite et aucune déclaration de réconciliation globale.
