# Consolidation documentaire et qualification finale v1.0

> **Complément du 2026-11-10 :** la recette physique Debian 13/Ghostty au SHA
> `78aab200af4709307dc2b0fffe69306d2a63f1d5` a depuis validé le composeur
> multiligne et les scénarios d'approbation/refus décrits comme non validés
> ci-dessous. Les résultats initiaux restent conservés comme photographie
> datée au SHA `81964c`/`617e9dd`; consulter le [rapport de qualification
> physique](2026-11-10-lofiterm-v1-physical-qualification.md) pour les preuves
> plus récentes et leur attribution au mainteneur. Ce complément ne transforme
> pas les assertions historiques 8 104 (DEV) en résultat de la machine de
> recette, qui a obtenu 8 102.

## Révision examinée

- Dépôt : `mohamedbouhlel/local-agent-cpp`.
- `origin/main` vérifié le 2026-11-09 : `617e9dd56d39d13a145bdb44fcc2b28153da8f87`, merge de la PR #37.
- Correctif TUI #37 : commit `9914585f37b8934f65bdb000a2a27626f9a33243`.
- Worktree documentaire : `/home/oper/projects/worktrees/local-agent-cpp-v1-manual-recipe`, branche `docs/v1-manual-recipe`, HEAD de travail `81964be88279754fae11980c4c46f2a2c8ee37fd`.
- Aucun test ni build n'a été exécuté dans cette intervention documentaire.

Le checkout principal est toujours sur `feat/ui-4-observable-metrics` et son HEAD local `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`; ses fichiers modifiés et non suivis ont été relevés avant/après et préservés. Aucun autre worktree n'a été nettoyé ou modifié.

## Périmètre documentaire

Les modifications de cette branche sont limitées aux guides et rapports : `README.md`, `USAGE.md`, `AGENTS.md`, `docs/README.md`, le rapport de recette documentaire existant et le présent rapport. Aucun fichier de code, CMake ou test n'a été modifié.

Les guides décrivent maintenant l'approbation obligatoire par défi pour chaque création `write_file`, l'inspection du contenu, le refus par défaut, l'absence d'écrasement et les limites de taille de la TUI. La TUI ne refuse pas sur la seule touche `n` : une réponse non conforme doit être soumise par Entrée pour produire un refus; Échap annule et F10 ferme en annulant. La recette manuelle a été actualisée pour ce comportement.

Le README et AGENTS distinguent les résultats de la campagne initiale du correctif PR #37. L'identité graphique LOFITERM est explicitement signalée comme non intégrée; le produit reste le binaire technique `agent`/`agent-cpp` avec CLI classique et TUI ncursesw.

## Résultats automatisés disponibles

Ces nombres proviennent des rapports et résultats fournis antérieurement; ils n'ont pas été réexécutés ici.

| Vérification | Statut | Révision exécutée | Résultat / réserve |
| --- | --- | --- | --- |
| Campagne initiale de convergence | PASS rapporté | `81964be88279754fae11980c4c46f2a2c8ee37fd` | 253 cas, 8 044 assertions; CTest 13/13. La campagne signalait alors des scénarios manuels write-approval non exécutés. |
| Harnais après correctif de saisie TUI #37 | PASS rapporté | `9914585f37b8934f65bdb000a2a27626f9a33243` | 258 cas, 8 065 assertions; zéro échec. |
| Tests TUI ciblés après correctif #37 | PASS rapporté | `9914585f37b8934f65bdb000a2a27626f9a33243` | 25 cas, 353 assertions; zéro échec. |
| CTest après correctif #37 | PASS rapporté | `9914585f37b8934f65bdb000a2a27626f9a33243` | 13/13 groupes. |
| Build/tests sur le merge exact `617e9dd…` | NOT RUN | `617e9dd56d39d13a145bdb44fcc2b28153da8f87` | Aucun rejeu demandé ou effectué dans cette mission. |

Les résultats du commit de branche #37 ne sont pas présentés comme une nouvelle exécution sur le merge `main`.

## Recette manuelle communiquée par le mainteneur

Environnement communiqué : Debian 13, Ghostty, `TERM=xterm-ghostty`, terminal UTF-8 interactif. Les observations ci-dessous sont attribuées au mainteneur; elles n'ont pas été reproduites indépendamment dans cette mission. Le SHA exact du binaire utilisé pour cette dernière session n'est pas fourni ici.

| Scénario | Statut | Observation et portée |
| --- | --- | --- |
| Démarrage de la TUI | PASS confirmé | Démarrage interactif confirmé. |
| Navigation et clavier | PASS confirmé | Utilisation de la navigation confirmée. |
| Saisie de l'approbation caractère par caractère | PASS confirmé | Le correctif de dialogue fonctionne selon l'observation utilisateur. |
| Refus dans un workspace annoncé vierge | PASS confirmé | Le scénario de refus intégral est rapporté réussi. |
| Refus de `refus-final.txt` | PASS confirmé | Le refus de cette cible est rapporté. |
| `refus-clavier/test.txt` | LIMITATION / origine non établie | La présence du chemin a été constatée après le parcours. Les éléments disponibles ne déterminent pas si le modèle a émis une autre opération, si une demande distincte a été approuvée, ni le lien causal avec le refus. Ce constat ne prouve pas un contournement technique de l'autorisation. Il exige une recette instrumentée et isolée avant de conclure sur le parcours. |
| Création approuvée de `accord-final.txt` | NOT RUN / non validée | La dernière sortie terminal ne valide pas la présence ni le contenu exact du fichier après approbation. Le parcours essentiel d'écriture approuvée n'est donc pas démontré par cette recette. |
| Interaction modèle après refus | NON QUALIFIÉ | Les répétitions de demandes, opérations différentes ou hors demande et l'arrêt à la limite d'itérations doivent être consignés séparément. Aucun élément fourni ne permet de conclure à un contournement technique; le refus d'un appel ne garantit pas que le modèle ne proposera pas un autre appel. |
| Composeur multiligne TUI, réserve signalée le 2026-11-09 | NOT RUN / symptôme non caractérisé | Une réserve concernant le composeur multiligne a été signalée pour cette date, mais aucun scénario reproductible ni résultat attendu/obtenu n'est détaillé dans les éléments disponibles. Ne pas la transformer en défaut confirmé. Rejouer avec un texte synthétique à plusieurs lignes, navigation entre vues, édition du brouillon et soumission F9; comparer exactement le contenu soumis et vérifier la conservation du brouillon et du curseur. |

Aucun secret, défi, prompt privé ou contenu réel n'est inclus dans ce rapport.

## Tableau de qualification

| Domaine | Statut | Motif |
| --- | --- | --- |
| Compilation et automatisation historique | PASS rapporté | Résultats précis aux SHA ci-dessus; pas rejoués sur le merge actuel. |
| Contrat d'approbation `write_file` | PASS au niveau automatisé rapporté | Le chemin exige une approbation par opération; la création approuvée réelle n'est pas validée dans la dernière recette communiquée. |
| Refus de la cible explicitement testée | PASS confirmé | Le refus de `refus-final.txt` est confirmé par le mainteneur. |
| Effet filesystem inattendu `refus-clavier/test.txt` | NOT RUN / origine inconnue | Présence observée, cause et chronologie non établies. Ne pas qualifier de bypass sans preuve. |
| Création approuvée et vérification contenu/fichier | NOT RUN | `accord-final.txt` non validé par la dernière sortie terminal. |
| Rejeu manuel indépendant par l'agent | NOT RUN | L'environnement courant n'a pas réalisé la recette Ghostty. |
| Composeur multiligne TUI | NOT RUN | Réserve mainteneur du 9 novembre inscrite, mais manque de détails reproductibles; pas de diagnostic établi. |
| Identité graphique LOFITERM | LIMITATION | Marque/identité graphique non intégrée; nom technique et interface livrés documentés. |
| Installation système ou paquet | LIMITATION | Pas de cible `install` ni de paquet Debian déclaré dans la procédure préparée. Compilation depuis les sources uniquement. |

## Recette restante

Rejouer dans un workspace temporaire neuf, avec une demande par scénario et une vérification du système de fichiers après chacune des décisions :

1. Refuser une seule cible nommée et vérifier l'absence du fichier et de ses parents.
2. Capturer les appels d'outils successifs et leurs identifiants; distinguer une répétition de la même opération d'une nouvelle opération avec autre chemin/contenu. Répondre individuellement à chaque dialogue et ne jamais approuver une demande inattendue.
3. Approuver une cible unique, puis vérifier après le tour l'existence et les octets exacts du fichier.
4. Fermer pendant une approbation et vérifier l'absence du fichier et des parents.
5. Reproduire la réserve du composeur multiligne avec un brouillon synthétique; noter le symptôme exact, les touches, la vue active, le texte attendu et le texte réellement soumis.
6. Conserver une trace expurgée des noms de chemins synthétiques, des décisions, du nombre d'appels d'outils, des résultats filesystem et de la restauration terminal; ne pas sauvegarder les défis ou transcriptions privées.

Les commandes de préparation, lancement CLI/TUI, compilation Debian 13 et contrôles filesystem sont détaillées dans `docs/reports/2026-11-08-v1-manual-recipe-and-docs.md`. Cette recette est préparée, pas exécutée par le présent rapport.

## Diff documentaire et contrôles

La branche de travail a reçu le commit documentaire `a1a4b41` puis le merge non destructif de `origin/main` (`617e9dd56d39d13a145bdb44fcc2b28153da8f87`) dans `ef8770b2bd2a1981067cebded8122fda233e6cbd`. Le correctif PR #37 est donc présent dans l'ascendance. Le diff `origin/main...HEAD` contient exclusivement les six documents listés dans le présent rapport; aucun fichier de production, test ou CMake n'y apparaît.

Vérifications exécutées après réconciliation :

- `git diff --check origin/main...HEAD` : réussi.
- `git status --short` : propre après le merge; seuls les commits documentaires sont en avance.
- Liens Markdown relatifs des six documents modifiés/ajoutés : aucun chemin cible manquant.
- Recherche de fins de ligne avec espaces/tabulations dans les six documents : aucune occurrence.
- Checkout principal UI-4 : mêmes cinq fichiers suivis modifiés et quatre fichiers non suivis avant/après; aucun contenu touché par cette mission.
- Build/tests : non exécutés, car seules des modifications documentaires étaient autorisées.

Le merge de `origin/main` a fait entrer dans l'historique du worktree les changements de code déjà fusionnés par la PR #37. Ils ne font pas partie du diff de cette branche par rapport à `origin/main` et ne sont pas des modifications apportées par cette mission.

## Verdict

**NON PRÊT POUR V1.0 à ce stade de preuve.** Ce verdict ne déclare pas un défaut de sécurité démontré. Il résulte de deux inconnues directement pertinentes pour la fonctionnalité obligatoire : la création approuvée `accord-final.txt` n'a pas été validée, et la présence de `refus-clavier/test.txt` n'a pas de provenance établie. Une recette ciblée et traçable doit vérifier l'écriture approuvée, puis expliquer ou reproduire le second chemin avant une décision de release.
