# LOFITERM UX v1.1 — corrections ciblées après revue

**Date :** 2026-11-10
**Branche :** `feat/lofiterm-ux-v1.1`
**HEAD de départ et HEAD final :** `46d653fe9e788c6410129b7224edd33d870af593`
**État :** modifications locales non commitées; aucun commit ni publication.

## Périmètre et provenance

Les corrections répondent aux constats C1–C6 du rapport
[`2026-11-10-lofiterm-ux-v1.1-final-review.md`](2026-11-10-lofiterm-ux-v1.1-final-review.md).
Le worktree était déjà modifié avant cette intervention; ces modifications
préexistantes de l'implémentation UX v1.1 ont été conservées. Les changements
de cette intervention restent limités à la souris, au hit-test, aux rubriques,
à la projection Markdown, aux tests et à la traçabilité documentaire. Aucun
fichier moteur, SSE, workspace, Landlock, writer, broker d'approbation ou CLI
classique n'a été modifié.

Le plan séparé a été retrouvé à
`/home/oper/Téléchargements/LOFITERM-PLAN-EXECUTION-UX-2026-11-10.md` et lu.
Le rapport d'implémentation a été rectifié : le document était disponible mais
n'avait pas été consulté lors de la première implémentation. Le §13 de la
spécification ne constitue pas une substitution formellement autorisée; l'écart
de procédure initial reste donc explicite.

## Corrections et preuves

| Point | Correction | Preuve ajoutée ou exécutée |
|---|---|---|
| C1 — souris/redraw | `mousemask` ne demande plus le rapport de position. Les `KEY_MOUSE` non reconnus, clics sans changement, et scrolls aux limites ne demandent pas de redraw. Les événements de souris reçus pendant l'approbation sont consommés sans décision. Le masque antérieur est restauré à la sortie. | PTY compact vérifie l'absence du mode de suivi de mouvement `?1003h`; test existant vérifie qu'un clic ne confirme pas une écriture. Revue des chemins de retour booléens de l'event loop. |
| C2 — onglets | Le dessin et le hit-test utilisent désormais la même fonction de géométrie, en focus menu ou non, avec libellés compacts et clipping à la largeur disponible. | Test de coordonnées sur 60, 80, 100 et 120 colonnes, avec les deux états de focus et plusieurs onglets actifs; PTY clique F2 à 60×20. |
| C3 — F5/F6 | Entrée et clic sélectionnent puis activent la même rubrique; recliquer la rubrique ouverte la ferme, une autre rubrique l'ouvre, Échap ferme l'état ouvert avant le retour. La saisie du composeur reste détenue par l'event loop et n'est pas déplacée par cette navigation. | Test de transitions équivalentes sélection/activation et de fermeture, plus les tests PTY/navigation existants. |
| C4 — Markdown | Les lignes stylées de conversation sont mises en cache par message, largeur et mode Markdown; seules les lignes du message modifié sont invalidées. En mode suivi, les projections d'ancrage inutiles sont évitées. Les deltas SSE contigus d'un même drainage sont regroupés avant l'ajout à la vue; la projection de conversation n'est construite qu'une fois par rendu. | Tests existants de fragmentation, texte brut, source intacte, blocs incomplets et resize; mesure comparative ci-dessous. |
| C5 — oracle PTY F2 | L'assertion `TOOLS` (déjà présent dans l'onglet) est remplacée par `Declared tools:`, contenu de la vue F2. Le test prouve son absence avant clic et sa présence après clic à 60×20. | Le test PTY `ux11.pty_mouse_click_selects_tab_and_keyboard_still_quits` passe. |
| C6 — plan | La formulation erronée du rapport d'implémentation est corrigée avec le chemin et l'écart réel de procédure. | Relecture du document externe et contrôle du diff documentaire. |

Les protections d'approbation restent inchangées : la souris n'écrit pas dans
le champ de confirmation et aucun clic n'autorise une création. La sanitizer
est toujours appliquée aux fragments avant leur mise en tampon; l'ordre est
préservé en vidant le lot avant chaque événement non-texte et à la fin du
drainage. Les tests d'approbation, `PARTIAL`, `NO_COLOR`, outils et sandbox
passent dans la suite complète.

## Mesure de projection Markdown

Mesure de micro-benchmark, une exécution par charge, même programme et mêmes
corpus que la mesure avant correction rapportée pendant cette intervention.
GCC `-O2`, `steady_clock`, projection `TuiView` à 100 colonnes; le programme
ajoute des fragments de 256 octets et reconstruit les lignes stylées après
chaque fragment. Ce n'est ni un profil CPU isolé ni une mesure d'expérience
Ghostty; le temps est sensible à la charge de la machine.

| Texte | Avant | Après | Variation observée |
|---:|---:|---:|---:|
| 8 192 octets / 32 fragments | 85,8137 ms | 45,8901–68,5852 ms (2 runs) | −20,1 % à −46,5 % |
| 65 536 octets / 256 fragments | 4 993,3 ms | 2 329,68–2 582,82 ms (2 runs) | −48,3 % à −53,4 % |

Les deux exécutions après correction montrent une dispersion mesurable; le
résultat avant correction disponible est un seul run. Ces chiffres montrent
une baisse sur ce micro-benchmark, pas une garantie de latence générale. La
ligne active doit encore être reprojetée après son
invalidation; un streaming très fragmenté et un viewport suspendu peuvent
entraîner davantage de travail que le scénario mesuré. Aucun seuil de
performance contractuel n'est revendiqué.

## Timeout du groupe TUI-3

Observations consignées : l'ancien run direct a pris 31,58 s; un ancien CTest
limité à 30 s a expiré; un autre CTest avait pris 36,57 s. Sur le candidat
corrigé, CTest a mesuré 23,61 s pour `tui3`, et un run ciblé supplémentaire
23,487 s (`user` 1,50 s, `system` 1,37 s). Le harnais complet a également
terminé sans échec. Ces mesures varient suffisamment pour établir qu'un
plafond de 30 s a déjà été dépassé, mais ne démontrent pas un ralentissement
réel et soutenu du produit. Le plafond de 60 s conserve les 30 répétitions et
évite le faux échec observé; sa marge reste empirique, non statistiquement
calibrée.

## Vérifications de cette intervention

Configuration et compilation propres :

```text
cmake -S /home/oper/projects/worktrees/local-agent-cpp-lofiterm-ux-v1.1 \
  -B /tmp/local-agent-cpp-lofiterm-ux-v11-corrections \
  -DAGENT_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-lofiterm-ux-v11-baseline/_deps/json-src
cmake --build /tmp/local-agent-cpp-lofiterm-ux-v11-corrections --parallel 2
```

Résultats exécutés :

- Build complet : PASS.
- `agent_tests --filter=ux11` : PASS, 10 cas / 154 assertions. Le premier
  essai sans accès loopback a échoué à `socket()` dans les mocks; il a été
  relancé avec l'accès loopback requis et a réussi.
- Harnais complet final : PASS, 290 cas / 8 503 assertions, zéro échec.
- CTest complet final : PASS, 14/14 groupes, total 36,28 s; `tui3` 23,62 s.
- Run ciblé supplémentaire TUI-3 : PASS, 29 cas / 415 assertions, 23,487 s.
- Mesures Markdown : réalisées comme décrit ci-dessus.
- `git diff --check` : PASS après la mise à jour finale du rapport.
- Liens relatifs de `docs/README.md` et des deux rapports concernés : PASS.
- Qualification physique Debian 13/Ghostty de ce SHA : NON EXÉCUTÉE.

La suite comprend les tests existants d'approbation, de refus, d'expiration,
de fermeture, de Landlock, de CLI classique, de Markdown fragmenté, de
`NO_COLOR`, de `PARTIAL`, et de composeur multiligne. Aucun changement des
contrats moteur ou des mesures MetricCollector n'a été fait.

Lors de l'ajout du clic PTY F6, le premier essai du nouveau test a visé la ligne
précédente : les coordonnées SGR sont 1-based alors que ncurses expose des
coordonnées 0-based. La coordonnée du test a été corrigée, puis le ciblé UX,
CTest et le harnais complet ont tous été rejoués avec succès sur le code final.

## Limites et verdict

La qualification physique reste à faire : en particulier, la molette,
comportement sur Ghostty lors d'un flux réel, et vérification visuelle des
redraws ignorés. Le PTY confirme le clic compact F2 et l'ouverture d'une
rubrique F6; il ne teste pas toutes les combinaisons souris. La mesure Markdown
est un micro-benchmark ponctuel.

**Verdict : PRÊT POUR QUALIFICATION PHYSIQUE**, sous réserve de cette recette
manuelle; aucune intégration/publication Git n'a été effectuée.
