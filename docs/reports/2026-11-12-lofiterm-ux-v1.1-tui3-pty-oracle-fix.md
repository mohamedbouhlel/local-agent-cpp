# LOFITERM UX v1.1 — correction du dernier échec automatisé TUI-3

**Date :** 2026-11-12
**Worktree :** `/home/oper/projects/worktrees/local-agent-cpp-lofiterm-ux-v1.1-recovered`
**Branche :** `feat/lofiterm-ux-v1.1`
**HEAD vérifié :** `46d653fe9e788c6410129b7224edd33d870af593`
**Build utilisé :** `/tmp/lofiterm-ux-v11-qualif-NeQwy1`
**État Git :** changements UX locaux préexistants conservés; cette correction ajoute uniquement un oracle PTY, un test associé, ce rapport et son index. Aucun changement de production, commit ou publication.

## Diagnostic et niveau de preuve

Le test concerné est `tui3.typed_no_is_retained_until_enter_without_leaking_to_composer`.
Le helper PTY saisit `n`, puis `o`, attend un rendu et calculait
`dialog_kept_typed_no` avec une recherche de la sous-chaîne brute
`Input: no` dans les octets de sortie du terminal.

Le rendu d'approbation est produit par `TuiView::draw_approval()` : le champ
`Input:` et sa valeur sont écrits dans la même ligne ncurses, mais les
redraws successifs peuvent ne transmettre que les cellules modifiées, avec
des commandes CSI de positionnement du curseur entre le libellé déjà affiché
et les nouvelles lettres. Une recherche brute contiguë ne modélise donc pas
l'état visible du terminal et peut produire un faux négatif. Un test
synthétique ajouté reproduit cette disposition d'octets :
`Input: `, commande CSI de déplacement, puis `no`; la projection textuelle
visible reconstitue correctement `Input: no`.

La cause exacte des deux échecs historiques n'a pas pu être reconstruite
octet par octet : le journal CTest antérieur ne conservait que le nom du groupe
`tui3` comme échec, et la sortie détaillée avait été remplacée par les
exécutions suivantes. L'assertion rapportée par le mainteneur est bien
`run.dialog_kept_typed_no`. Une exécution du test non modifié dans ce worktree
a réussi. Une instrumentation temporaire comparant texte brut et projection
visible n'a pas observé de divergence lors de la répétition. Ainsi, la
fragilité de l'oracle est démontrée comme possibilité de faux négatif, mais le
chemin exact ayant produit les deux anciens échecs reste non vérifié; une
latence PTY lors de ces exécutions ne peut pas être exclue absolument.

Aucun défaut fonctionnel de l'application n'a été reproduit. Le code de
production et les délais d'attente applicatifs/tests n'ont pas été modifiés.

## Correction minimale

Le test construit maintenant une projection textuelle de la sortie PTY qui
ignore les commandes CSI/OSC et caractères de contrôle de terminal, puis
recherche `Input: no` dans le texte rendu. Il conserve la boucle
d'attente bornée existante de deux secondes; aucun délai n'a été allongé.
Un test unitaire vérifie spécifiquement que le libellé et la valeur restent
reconnaissables lorsqu'une séquence CSI de déplacement se trouve entre leurs
écritures.

Les contrôles fonctionnels et de sécurité restent actifs : avant Entrée,
une seule requête HTTP existe et aucun fichier cible n'est présent; après le
refus, aucun fichier ni répertoire parent n'existe; le suivi de conversation
arrive, puis le test envoie F9 pour vérifier qu'aucun `no` résiduel n'a fui
dans le composeur (aucune troisième requête); les assertions de restauration
du reporting souris restent présentes. Les tests distincts conservent aussi
la couverture Échap/F10, expiration, approbation exacte et restauration
terminale.

## Fichiers modifiés pour cette correction

- `tests/tui_test.cpp`
- `docs/README.md`
- `docs/reports/2026-11-12-lofiterm-ux-v1.1-tui3-pty-oracle-fix.md`

Le worktree contient également les modifications UX v1.1 non commitées
inventoriées par `git status`; elles sont antérieures et n'ont été ni
remplacées ni intégrées dans cette correction ciblée.

## Vérifications exécutées

Le build existant a été réutilisé; ce n'est pas une configuration propre.
Les tests nécessitant le serveur mock loopback ont été exécutés avec accès
loopback autorisé après un échec sandboxé initial de `socket()`.

| Commande | Résultat |
|---|---|
| `cmake --build /tmp/lofiterm-ux-v11-qualif-NeQwy1 --parallel 2` | PASS, targets compilés |
| `.../tests/agent_tests --filter=tui3` | PASS, 30 cas, 486 assertions |
| `.../tests/agent_tests --filter=ux11` | PASS, 14 cas, 179 assertions |
| `.../tests/agent_tests` | PASS, 295/295 cas, 8 599 assertions, zéro échec |
| `ctest --test-dir /tmp/lofiterm-ux-v11-qualif-NeQwy1 --output-on-failure` | PASS, 14/14 groupes |
| `git diff --check` | PASS après mise à jour du rapport et de l'index |

Les contrôles automatisés d'approbation et leurs vérifications filesystem
sont inclus dans les suites exécutées. Aucun essai Ghostty physique complet
n'a été relancé, conformément à la demande.

## Limites restantes

- Les octets exacts des deux anciens échecs ne sont pas archivés; leur cause
  individuelle ne peut donc pas être établie avec certitude.
- Le test PTY observe le texte projeté depuis une sortie ncurses, pas une
  capture de pixels ou un terminal matériel.
- Le build utilisé est celui fourni; aucune reconstruction propre n'a été
  faite.
- Aucune modification d'approbation, de Landlock, de SSE, d'outil ou de
  comportement du moteur n'a été effectuée.

## Verdict

**PRÊT POUR INTÉGRATION GIT.** L'application a conservé `no` dans le dialogue
et n'a produit aucun effet d'écriture dans les tests; la suite complète et
CTest passent après remplacement de l'oracle brut par une vérification du
texte rendu indépendante du découpage des commandes ncurses. Le défaut de
fonctionnement de l'application n'est pas reproduit. La réserve est limitée
à l'impossibilité de confirmer rétrospectivement le détail des octets des deux
échecs précédents.
