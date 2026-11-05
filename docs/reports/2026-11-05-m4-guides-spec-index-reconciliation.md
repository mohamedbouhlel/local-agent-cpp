# M4 — Réconciliation guides, spécification REPL et index documentaire

**Date :** 2026-11-05 (Europe/Paris)  
**Dépôt :** `mohamedbouhlel/local-agent-cpp`  
**HEAD distant `main` observé :** `465faae586c8f268d276597df05724431c750085`  
**Branche :** `docs/m4-guides-spec-index-20261105`  
**Périmètre :** documentation uniquement — 5 fichiers de référence et ce rapport.

## 1. Objectif

Corriger les contradictions documentaires restantes après M2 et M3 sans modifier l'implémentation C++, les tests, les décisions normatives de la spécification REPL ou les rapports historiques.

## 2. Constats vérifiés

| Constat | Preuve vérifiée au SHA de base |
| --- | --- |
| PR #6 et #7 fusionnées ; `main` inclut leur contenu | API GitHub PR #6/#7 et comparaison de commits |
| Refus du démarrage hors Linux / sans Landlock | `src/filesystem_sandbox.cpp`, `src/main.cpp` |
| REPL multiligne implémenté `:multi` / `:end` | `src/prompt_reader.cpp`, `src/main.cpp` |
| Contradiction de troncature dans `AGENTS.md` | `AGENTS.md` §architecture et `src/agent.cpp` |
| Les lectures/recherches refusent les fichiers à liens physiques multiples | `src/workspace_tools.cpp`, `src/workspace_search.cpp` (`st_nlink != 1`) |
| Étude `bwrap` datée d'un état historique | `docs/reports/2026-11-04-isolation-systeme.md` |
| README fournit encore des prérequis macOS | `README.md` au SHA de base |
| Spécification approuvée annonce encore une implémentation future | `docs/specifications/repl-multiline-requirements.md` |
| Tests : 118 déclarations, 7 groupes Linux ; 403 assertions / 7/7 rapportés | fichiers `tests/`, `tests/CMakeLists.txt`, `AGENTS.md` |
| Index incomplet avant M4 | 39 rapports `docs/reports/` au HEAD de base, 22 indexés |

## 3. Actions exécutées sur la branche

1. `README.md` — prérequis Linux/Landlock, installation, configuration et commandes de vérification ; clarification de la portabilité, sans choix technique nouveau.
2. `USAGE.md` — description du REPL multiligne, dépannage Landlock et SSE, correction du passage sur les liens physiques, distinction entre essais historiques et garanties actuelles.
3. `AGENTS.md` — correction de la troncature par tours, cadrage de la section architecture, références aux preuves et inventaire des modules ; gouvernance Git conservée.
4. `docs/specifications/repl-multiline-requirements.md` — note d'implémentation datée en tête ; exigences D-1…D-8, REPL-01…REPL-09 et arbitrages A-1…A-9 **préservés à l'identique**.
5. `docs/README.md` — index complété, lien vers rapport M4 ajouté, règle de maintien à jour et qualification temporelle des documents.
6. Aucun code, test, CMake, choix produit ou architecture modifié.

## 4. Vérifications et limites

- Contrôle postpublication des **6 contenus** par lecture GitHub de la branche.
- Comparaison du diff à `main` : les **6 chemins exacts** ci-dessus et aucun autre.
- Vérification des liens documentaires de l'index et du maintien des exigences/arbitrages normatifs historiques.
- Vérification du nombre de rapports présents/annoncés : **40 après ajout de M4**, dont l'ensemble doit être indexé.
- **Pas de compilation, CTest, essai LLM ou test sécurité exécuté pendant M4** ; l'état local de la machine n'est pas vérifié.
- Les anciennes sections de certaines références peuvent toujours comprendre des éléments historiques ; leur lecture doit respecter les étiquettes de période.

## 5. Verdict

M4 **réconcilié sur la branche et prêt pour revue**, sous réserve de la vérification postpublication et du contrôle du diff. Aucune fusion sur `main` n'est autorisée implicitement.
