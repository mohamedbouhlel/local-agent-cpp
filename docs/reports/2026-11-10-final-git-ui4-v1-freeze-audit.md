# Audit final Git, UI-4 et gel du socle technique

## Périmètre et méthode

- Dépôt : `mohamedbouhlel/local-agent-cpp`.
- SHA distant vérifié : `b1d194288b02b5df094953affd763776991d3e0a` (`origin/main`).
- Worktree documentaire : branche `docs/v1-technical-freeze-audit`, basée sur ce SHA.
- Checkout de développement examiné : `/home/oper/projects/local-agent-cpp`, branche `feat/ui-4-observable-metrics`, HEAD `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`.
- Date de l'audit : 2026-11-10.

Audit en lecture seule du code et de Git, à l'exception de ce rapport et de son entrée dans l'index documentaire. Aucun build ni test n'a été relancé : aucune raison technique ne justifiait de répéter les campagnes historiques, et la mission interdit de relancer les tests déjà validés sans raison. Aucun fichier applicatif, worktree existant, tag, commit ou branche produit n'a été modifié.

La référence initialement présente dans `origin/main` était en retard. Une récupération non destructive de `origin/main` a confirmé le SHA ci-dessus. Le `main` local est distinct et obsolète; les conclusions de cet audit se rapportent à `origin/main`, pas à cette branche locale.

## État Git

### Checkout UI-4

Le checkout principal reste sur `feat/ui-4-observable-metrics`, à `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`. Il est à 47 commits derrière `origin/main`. Ce HEAD est un ancêtre de `origin/main`; les changements UI-4 sont des modifications locales non commitées, pas une branche intégrée.

État constaté avant l'audit :

| État | Chemins |
| --- | --- |
| Modifiés, suivis | `CMakeLists.txt`, `docs/README.md`, `include/ui/presentation_controller.hpp`, `src/ui/presentation_controller.cpp`, `tests/CMakeLists.txt` |
| Non suivis | `docs/reports/2026-11-05-ui-4-observable-metrics.md`, `include/ui/metrics.hpp`, `src/ui/metrics.cpp`, `tests/ui_metrics_test.cpp` |

`git diff --check` sur les modifications locales UI-4 ne signalait pas d'erreur. Leur empreinte de statut n'a pas été altérée durant l'audit. Ces fichiers sont du travail candidat à préserver; ils n'ont pas été ajoutés au worktree documentaire.

### Branches, worktrees et tags

- `git branch --no-merged origin/main` identifie `validation/v1-both-fixes` comme branche locale non fusionnée.
- Cette branche pointe sur `97692037a454eb58e5819245e45157111d3653b1` et contient les commits `40cf181` (drainage final TUI), `5eb565d` (rejet SSE malformé) et `9769203` (synchronisation d'un test d'approbation). Elle provient d'un historique antérieur à leur intégration dans `main`; une comparaison directe fait apparaître des inversions et suppressions de code/documentation par rapport à `origin/main`. Ce n'est donc pas un candidat à fusionner tel quel. La présence des mêmes sujets dans `main` est confirmée par les rapports et le code courant; la branche locale reste intacte.
- Aucune branche distante non fusionnée n'a été rapportée par `git branch -r --no-merged origin/main`.
- `git worktree list --porcelain` a recensé les worktrees existants; aucun n'a été supprimé ni modifié. Six anciens enregistrements sous `/tmp` sont signalés `prunable` car leur chemin n'existe plus. Aucune commande de prune ou de nettoyage n'a été exécutée.
- Aucun tag local ni distant n'a été trouvé. Il n'existe donc pas de tag v1.0 observé.

## État du produit dans `main`

La PR #40 est intégrée au SHA `b1d1942`. La comparaison exacte avec son parent `78aab200af4709307dc2b0fffe69306d2a63f1d5` donne trois fichiers, tous documentaires : `docs/README.md` et les deux rapports de qualification v1.0. Le code produit n'a pas changé dans ce merge. La qualification physique historique est au SHA parent, et ses résultats restent attribués au mainteneur; ils n'ont pas été rejoués pendant cet audit.

| Capacité ou élément | Présence dans `main` | Maturité / preuve consultée | Limitation et conséquence | Classement |
| --- | --- | --- | --- | --- |
| Agent conversationnel, HTTP/SSE et tool-calling | Oui | Code courant et rapports de qualification; les défauts SSE identifiés ont des rapports correctifs intégrés | Dépend du serveur/modèle compatible et de son comportement; les résultats sémantiques du modèle ne sont pas garantis | Déjà intégré |
| CLI classique et REPL multiligne | Oui | `README.md`, `USAGE.md`, code du REPL et qualification historique | Validation physique limitée à l'environnement rapporté | Déjà intégré |
| TUI ncursesw à six vues et `AgentWorker` | Oui | Code courant, rapports TUI et qualification Debian 13/Ghostty rapportée | SSH, autres terminaux et conformité Unicode universelle non qualifiés | Déjà intégré |
| Outils workspace et confinement Landlock | Oui | `AGENTS.md`, implémentations et tests versionnés | Landlock dépend de Linux et ne confine pas les autres processus du même compte | Déjà intégré |
| Création `write_file` avec approbation explicite, défi et refus d'écrasement | Oui | Implémentation courante, tests et recette physique rapportée | La robustesse pratique dépend aussi de la réponse du modèle; l'approbation n'est pas une validation du contenu par le modèle | Déjà intégré |
| Historique borné Outils/Diagnostics et indicateur `PARTIAL` | Oui | PR #32/#33 dans l'ascendance et code correspondant dans `main` | Borne sur les entrées/octets textuels retenus, pas sur la mémoire totale du processus | Déjà intégré |
| MetricCollector | Non | `origin/main` affiche dans la vue Métriques « MetricCollector non intégré »; ses fichiers n'existent pas dans l'arbre `main` | La vue ne fournit pas encore les métriques techniques; pas de données inventées | À préserver pour une version future; intégration courante non vérifiée |
| Rapport local UI-4 | Non | Fichier non suivi uniquement dans le checkout de développement UI-4 | Rapport et code ne sont pas une preuve d'intégration à `main` | À préserver pour une version future |
| Compilation depuis les sources | Oui, procédure documentée | Rapport physique historique du SHA `78aab200…` et documentation d'utilisation | Les résultats cités ne sont pas réexécutés au SHA `b1d1942`; le merge entre ces SHA est documentaire seulement | Déjà intégré, preuve historique |
| Installation système / paquet Debian | Non démontré comme disponible | Les références courantes ne déclarent pas de cible CMake `install` ni de paquet qualifié | Compilation source ne signifie pas installation système reproductible ou paquet distribué | Limitation hors gel du socle |
| Identité graphique LOFITERM | Non | Le rapport physique précise que l'identité graphique n'est pas intégrée | Le produit technique demeure l'agent CLI/TUI existant | Hors périmètre du gel technique |

## Audit UI-4 / MetricCollector

| Modification locale | Objectif établi par le code | Achèvement et tests disponibles | Présence / dépendances | Risque et classement |
| --- | --- | --- | --- | --- |
| `include/ui/metrics.hpp` (non suivi) | Définir `TurnMetrics` et `MetricCollector` consommant les événements UI-1 | Implémentation présente; le rapport local annonce 7 cas dédiés, 64 assertions, CTest 10/10 et harnais 182 cas / 1 154 assertions au SHA `0df28d21…` | Absent de `main`; dépend de `AgentEvent` et de l'API historique du `PresentationController` de ce SHA | Tests rapportés, pas rejoués ici; préserver sans conclure à une compatibilité avec le contrôleur courant |
| `src/ui/metrics.cpp` (non suivi) | Calculer durées, compteurs, octets mesurés et issue technique à partir des événements | Le rapport décrit des tests de tours réussis/échoués, requêtes/outils multiples, delta initial, reset et absence de tokens/s fictifs | Absent de `main`; alimentation par événements consommés par `PresentationController` | Les vecteurs de mesures par requête/outil sont conservés durant un tour; pas de borne explicitement démontrée dans ce candidat. Ne constitue pas une borne mémoire globale. Préserver pour revue ultérieure |
| `include/ui/presentation_controller.hpp`, `src/ui/presentation_controller.cpp` (modifiés) | Posséder le collecteur et afficher des mesures dérivées des événements | Le rapport local indique tests d'intégration CLI Plain/Rich et `NO_COLOR` | Le contrôleur correspondant dans `main` a évolué avec les événements d'approbation TUI; l'adaptation n'est pas faite dans ce checkout local | Portage/cherry-pick direct non justifié sans réconciliation; candidat à réévaluer sur `main` |
| `CMakeLists.txt` (modifié) | Ajouter `src/ui/metrics.cpp` à `agent_core` | Construction historiquement rapportée | Le CMake de `main` a depuis ajouté les sources et dépendances de la TUI | Risque de conflit d'intégration mécanique, pas preuve d'un défaut fonctionnel |
| `tests/ui_metrics_test.cpp` (non suivi), `tests/CMakeLists.txt` (modifié) | Ajouter un groupe CTest `ui4` et tester le collecteur | 7 cas dédiés selon le rapport; campagne historique rapportée, non rejouée | Absent de `main`; le CTest actuel compte davantage de groupes que le CTest UI-4 historique | À préserver; adapter et rejouer sur `main` avant toute intégration |
| `docs/reports/2026-11-05-ui-4-observable-metrics.md` (non suivi) | Décrire l'implémentation locale et sa validation historique | Rapport présent dans le checkout UI-4 seulement | Ni indexé ni présent dans `main` à la référence étudiée | À préserver avec ses attributions et SHA; état futur à arbitrer |

**Conclusion UI-4 :** ce travail est une implémentation locale cohérente et testée selon un rapport historique, mais pas intégrée au `main` actuel. Il n'est pas démontré comme abandonné et ne doit pas être supprimé. Son intégration n'est pas un prérequis établi pour figer le socle technique v1.0 : `main` indique explicitement l'indisponibilité des métriques et le périmètre physique qualifié n'en dépend pas. Pour une intégration future, il faudra porter/revoir le collecteur contre le `PresentationController` actuel, conserver les événements d'approbation, réconcilier CMake/CTest et exécuter une campagne propre sur le commit candidat. Aucune de ces opérations n'a été effectuée ici.

## Pré-release et incohérences

- Les rapports de qualification v1.0 sont présents dans `main` et référencés dans `docs/README.md`.
- Le rapport physique du 10 novembre distingue bien les résultats historiques rapportés : 262 cas, 8 102 assertions sur `versutus-ia`, CTest 13/13; il conserve séparément 8 104 assertions DEV et ne les prétend pas réconciliées.
- La preuve manuelle est attribuée au mainteneur, au SHA `78aab200…`, sur Debian 13/Ghostty. PR #40 n'a changé que des documents; le code produit au SHA `b1d1942` est donc identique à celui de `78aab200…`. Cela établit la correspondance du code, pas une nouvelle exécution de recette au SHA `b1d1942`.
- `README.md` et le rapport de qualification distinguent l'agent technique du produit LOFITERM complet. L'identité graphique n'est pas revendiquée comme livrée.
- Aucun tag `v1.0` ou équivalent n'a été trouvé. Aucun tag n'a été créé.
- Pas de preuve d'un paquet ou d'une installation système. La compilation depuis les sources est le mode établi; ne pas présenter cela comme un paquet installable.
- Aucune incohérence documentaire bloquante n'a été constatée pour le périmètre technique existant. Le caractère non intégré du `MetricCollector` est indiqué explicitement dans la vue Métriques.

## Risques et décisions

### Bloquants pour le gel technique

Aucun défaut technique bloquant nouveau n'a été démontré par cet audit en lecture seule. Aucun test n'a été exécuté pendant cette mission; le verdict s'appuie sur le code au SHA exact et les rapports de qualification disponibles, avec leurs limites d'attribution.

### Non bloquants / à conserver

1. **UI-4 local non commité : risque de préservation.** Quatre nouveaux fichiers et cinq modifications suivies existent dans le checkout UI-4. Ils restent intacts et ne sont pas requis pour le gel du `main`. Ne pas nettoyer ce checkout; organiser séparément sa sauvegarde/publication avant toute opération future qui pourrait le remplacer.
2. **Branche `validation/v1-both-fixes` divergente.** Ne pas la fusionner telle quelle. Les correctifs TUI/SSE utiles sont déjà dans `main`; cette branche comporte des différences inverses et documentaires. La conserver comme preuve historique jusqu'à décision explicite de son propriétaire.
3. **Qualification délimitée.** Debian 13/Ghostty est le seul terminal physique rapporté; SSH et autres terminaux ne sont pas qualifiés. Le packaging, l'installation système et l'identité graphique LOFITERM restent hors du périmètre technique qualifié.
4. **MetricCollector absent.** Les métriques restent indisponibles dans `main`; cette limitation est déclarée et n'empêche pas les parcours de base qualifiés.

## Verdict

**GO — le socle technique existant de `main` peut être figé comme candidat technique v1.0, sans perdre UI-4, à condition de figer le SHA `b1d194288b02b5df094953affd763776991d3e0a` et de conserver séparément, sans nettoyage, le checkout UI-4 non commité.** Cet audit n'a créé aucun tag ni commit.

Ce GO ne qualifie ni un paquet distribué, ni une installation système, ni l'identité graphique LOFITERM, ni la compatibilité avec tous les terminaux. Il ne certifie pas que les tests historiques ont été réexécutés au SHA b1; le merge PR #40 est documentaire seulement. `MetricCollector` peut être reporté sans perte de travaux si son checkout reste préservé.
