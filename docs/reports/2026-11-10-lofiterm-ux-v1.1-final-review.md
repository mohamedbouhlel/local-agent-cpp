# LOFITERM UX v1.1 — revue finale ciblée

**Date :** 2026-11-10<br>
**Dépôt :** `mohamedbouhlel/local-agent-cpp`<br>
**Branche :** `feat/lofiterm-ux-v1.1`<br>
**HEAD / base examinée :** `46d653fe9e788c6410129b7224edd33d870af593`<br>
**Référence distante :** `origin/main` au même SHA<br>
**Type de revue :** lecture seule du code et des tests; aucun test relancé, aucun code corrigé.

## Résumé et verdict

Le périmètre annoncé correspond à **15 fichiers suivis modifiés et 4 nouveaux
fichiers**. Les changements restent dans la présentation TUI, ses tests, CMake
et la documentation. Aucun changement n'est présent dans `Agent::send()`, SSE,
les outils, Landlock, le broker d'approbation, la politique d'écriture, le CLI
classique ou les calculs MetricCollector.

La revue trouve néanmoins deux défauts de comportement souris démontrables par
le code (redraw sur événement de mouvement et coordonnées de hit-test décalées
lorsque le focus menu est actif), une incohérence de clic pour refermer une
rubrique, et une couverture PTY qui ne démontre pas la sélection effective de
l'onglet. Le coût du rendu Markdown à chaque delta est non borné par conception
et n'a pas été mesuré. La version précédente du rapport affirme à tort qu'aucun
plan séparé n'était fourni : un plan distinct existe dans Téléchargements et
n'est pas intégré au dépôt.

**Verdict : CORRECTIONS CIBLÉES REQUISES AVANT LA QUALIFICATION PHYSIQUE.** La
recette Ghostty seule ne devrait pas servir à valider un candidat dont le
comportement souris et le coût du rendu ont ces lacunes connues. Aucun de ces
constats ne met en évidence une régression du moteur ou un contournement de
l'approbation d'écriture.

## État Git et périmètre

`HEAD` et `origin/main` valent `46d653fe9e788c6410129b7224edd33d870af593`;
branche `feat/lofiterm-ux-v1.1`. Le statut observé comprend exactement les
19 chemins annoncés au rapport d'implémentation :

- 15 fichiers suivis modifiés : `CMakeLists.txt`, `README.md`, `USAGE.md`,
  `docs/README.md`, `docs/roadmap/ROADMAP.md`, trois en-têtes UI, quatre sources
  UI, `tests/CMakeLists.txt` et deux fichiers de tests TUI.
- 4 nouveaux fichiers : la spécification copiée, `markdown_presenter.hpp`,
  `markdown_presenter.cpp` et le rapport d'implémentation.

`git diff --check` passe au candidat. La vérification des changements montre
qu'aucun chemin applicatif hors de la couche UI n'est modifié. Le rapport de
revue présent ajoute un nouveau fichier documentaire; il ne fait pas partie
des 19 fichiers du candidat.

Les statuts du checkout principal et des deux worktrees graphiques contrôlés
étaient vides. Aucun worktree n'a été modifié par cette revue. La vérification
du tag technique n'est pas une opération de cette revue; aucun tag n'a été
écrit.

## Plan d'exécution distinct et traçabilité

Le fichier séparé est accessible à :
`/home/oper/Téléchargements/LOFITERM-PLAN-EXECUTION-UX-2026-11-10.md`.
Il indique la base `46d653fe`, décrit des séquences A–E dans **une seule
mission**, et exige notamment :

- la lecture du plan avant modification;
- un rendu Markdown fragmenté sans reparsing non borné de toute la conversation;
- des événements souris limités aux besoins, avec molette et zones cliquables
  vérifiées après redimensionnement;
- une couverture de comportement par PTY et une recette physique distincte;
- l'indexation des références et un rapport de qualification qui distingue les
  essais non exécutés.

Le plan est absent du worktree et de l'index documentaire. Le rapport
d'implémentation dit « aucun document séparé de plan n'était fourni » et traite
le §13 de la spécification comme le plan intégré. C'est contradictoire avec le
fichier séparé maintenant retrouvé. Rien ne démontre une autorisation formelle
de substituer le §13 au plan; cette substitution n'est donc pas retenue dans
cette revue. Le rapport initial doit être corrigé ou complété avant clôture
documentaire. Cela ne change pas rétroactivement le code ni les essais.

## Constats ciblés

| ID | Gravité | Constat et preuve | Correction minimale proposée |
|---|---|---|---|
| R1 | Majeure, comportement à confirmer sur terminal réel | `TerminalSession` demande `ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION` (`src/ui/terminal_session.cpp:37-38`). Le chemin `KEY_MOUSE` retourne `true` même pour les événements non reconnus (`src/ui/tui_event_loop.cpp:337-365`); la boucle marque alors l'écran dirty et le redessine. Les rapports de position souris peuvent donc déclencher des redraws à chaque mouvement. | Ne demander que clics et molette nécessaires; ignorer les événements sans action en retournant « non traité »/sans dirty. Vérifier la restauration du masque. Ajouter un test PTY de mouvement ou prouver que le terminal ne rapporte pas ces événements. |
| R2 | Mineure, défaut démontré par géométrie | En focus menu, le dessin positionne le premier onglet à `width_of("MENU") + 1` (`src/ui/tui_view.cpp:943-951`). Le hit-test initialise déjà à `width_of("MENU") + 1`, puis ajoute encore une colonne (`:798-810`). Les zones sont décalées d'une cellule pour les onglets. | Partager le calcul de géométrie entre rendu et hit-test; couvrir toutes les limites d'onglets en focus menu et contenu, à plusieurs largeurs. |
| R3 | Mineure, incohérence UX démontrée | Un clic de rubrique sélectionne l'index, mais n'active la rubrique que si aucune rubrique n'est déjà ouverte (`src/ui/tui_event_loop.cpp:355-359`). Un second clic sur la rubrique ouverte ne la ferme donc pas, contrairement au contrat « même action que sélection + Entrée » de la spécification §5. | Même résultat qu'Entrée : clic sur la rubrique ouverte la ferme; clic sur une autre la sélectionne et l'ouvre. Tester les deux cas. |
| R4 | Lacune de preuve | `pty_mouse_click_selects_tab...` vérifie que la sortie contient `TOOLS` (`tests/tui_navigation_test.cpp:488`), texte déjà présent dans la barre d'onglets avant le clic. Cette assertion ne prouve pas que F2 est devenue la vue sélectionnée. Elle ne vérifie pas non plus les six onglets. | Observer un oracle distinctif du panneau F2 et l'état sélectionné, ou injecter la séquence souris et tester l'état de navigation par une interface de test. Répéter pour les six onglets et le focus menu. |
| R5 | Important, défaut de conformité/performance non mesuré | Chaque delta appelle `rendered_conversation_rows()` avant ajout (`src/ui/tui_view.cpp:446`), puis le recalcul d'ancre après ajout (`:612-616`). Le rendu redessiné reconstruit `rendered_page_rows()` et `rendered_styled_conversation_rows()` (`:968-970`), qui re-parsent tous les messages avec `present_markdown()` (`:618-635`). Cela répète plusieurs projections de l'historique complet par delta; pour une réponse longue le travail cumulé peut croître quadratiquement. Aucun budget ni mesure n'est fourni. | D'abord supprimer les projections dupliquées par redraw; ensuite mettre en cache la projection ou ne recalculer que le message actif, avec invalidation correcte au resize et au changement Markdown. Mesurer une charge longue avant d'affirmer la fluidité. |
| R6 | Mineure, documentation | La matrice UX-20 du rapport d'implémentation dit encore « diff checks requis après ce rapport » (`...implementation.md:151`), alors que le même rapport annonce leur succès aux lignes 120-123. | Harmoniser cette ligne, sans réécrire les résultats historiques. |
| R7 | Mineure, traçabilité | Le rapport précédent affirme l'absence du plan séparé (`...implementation.md:37-38`); le plan externe existe. Il n'est ni copié ni indexé. | Ajouter une note de provenance et de comparaison du plan, ou déclarer explicitement que la revue a identifié l'écart; ne pas prétendre que le §13 le remplace. |

## Sécurité et non-régression

- **Écritures :** la garde `if (approval_request_) return
  handle_approval_key(...)` précède le traitement ordinaire (`src/ui/tui_event_loop.cpp:325`). Les événements souris pendant la demande sont consommés par le gestionnaire d'approbation; ils ne produisent pas de défi. Le test d'approbation par clic vérifie l'absence du fichier et du répertoire parent (`tests/tui_test.cpp:1033-1051`). Aucune modification du broker ou de la politique n'est dans le diff.
- **Terminal hostile :** les deltas du modèle traversent `TerminalTextSanitizer` avant `append_assistant_delta` (`src/ui/tui_event_loop.cpp:231`). Les diagnostics dynamiques passent également par la neutralisation existante. Le présentateur Markdown ne traite ni HTML actif ni liens comme des actions. Les tests du sanitizer sont préexistants; cette revue ne les a pas relancés et ne trouve pas de nouveau chemin d'exécution terminal dans le parser.
- **SSE/complétude :** aucun fichier moteur, HTTP ou SSE n'est modifié. Le code de fin de tour garde la distinction échec/complet et le marqueur `[incomplete]`; la présentation ne modifie pas le résultat agent.
- **Landlock/workspace/CLI :** aucun fichier de ces composants n'est modifié. Le mode brut de `M` n'agit que lorsque la vue Conversation a le focus contenu (`src/ui/tui_event_loop.cpp:428-432`). Avec le focus composeur, `M` suit le chemin normal d'insertion de caractères (`:434-440`); aucun appel `submit_input` n'est lié à `M`. Ce raisonnement statique indique qu'il ne détourne pas la saisie, mais aucun test événementiel/PTy spécifique à `M` ne le démontre.
- **Approbation :** F9/F10 et la confirmation sont prioritaires dans leurs chemins préexistants; le gestionnaire de dialogue garde l'exigence du défi courant. Cette revue ne change ni ne rejoue ces tests.

## Timeout TUI-3

Le candidat modifie le timeout CTest de `tui3` de 30 à 60 secondes, sans
modifier les 30 répétitions PTY de fermeture d'approbation (`tests/tui_test.cpp:1054-1057`). Le rapport enregistre un run direct de **31,58 s**, un CTest limité à 30 s qui expire, puis un CTest final de **36,57 s** après l'augmentation. Un run observé dépassant 30 s suffit à expliquer l'échec du plafond; il ne permet pas de distinguer un ralentissement soutenu de la variabilité ordonnanceur/charge système. Aucune série de mesures répétées, distribution ou comparaison de machine n'est fournie. Le passage à 60 s évite un timeout manifestement trop juste, mais sa marge n'est pas statistiquement calibrée. Aucun test n'a été relancé pendant cette revue.

## Matrice UX-01 à UX-20

Les statuts ci-dessous sont ceux de cette revue, distincts des résultats
automatisés rapportés antérieurement. « Non vérifié » signifie que le code/test
consulté ne donne pas une preuve suffisante du scénario demandé.

| Critère | Statut | Preuve et réserve |
|---|---|---|
| UX-01 — identité Chat | CONFORME | `src/ui/tui_view.cpp:624-628` affiche `LOFITERM Bot:` et réduit à `Bot:` quand la largeur l'exige; tests de lignes/rôle adaptés dans `tests/tui_test.cpp`. |
| UX-02 — À propos/version | PARTIEL | À propos indique version/commit indisponibles et les limites; le plan demande aussi modèle/endpoint non sensible ou renvoi à F5. L'écran À propos ne renvoie pas explicitement à F5 (`src/ui/tui_view.cpp`, rubrique About). |
| UX-03 — rubriques/retour | PARTIEL | État par page, Enter et Échap présents; le clic ne referme pas une rubrique déjà ouverte (`tui_event_loop.cpp:355-359`). Pas de preuve PTY complète brouillon/focus. |
| UX-04 — Premiers pas | CONFORME | Trois actions de démarrage sont construites dans la rubrique Getting started; aucun faux raccourci observé (`tui_view.cpp:759-773`). |
| UX-05 — raccourcis | CONFORME | F1–F6/F9/F10, Tab, Enter, Échap, flèches et PgUp/PgDn ont des chemins explicites; aide F6 et tests TUI préexistants. Le test physique reste à faire. |
| UX-06 — clic six onglets | NON CONFORME | Décalage d'une cellule du hit-test en focus menu (`tui_view.cpp:943-951`, `:798-810`). Le test PTY ne prouve pas le changement de page (`tui_navigation_test.cpp:488`). |
| UX-07 — molette/focus | NON VÉRIFIÉ | Le code route bouton 4/5 vers le scroll de la page (`tui_event_loop.cpp:340-344`), mais aucun test PTY de molette ni essai physique; rapports de position peuvent aussi déclencher des redraws. |
| UX-08 — clavier sans souris | PARTIEL | Le clavier reste séparé du support souris et tests TUI/PTY préexistants; pas de recette de terminal sans souris pendant cette branche. |
| UX-09 — souris/approbation | CONFORME au niveau code/PTy rapporté | L'approbation est dispatchée avant les actions souris; un test vérifie absence de création après clic seul (`tui_event_loop.cpp:325`, `tui_test.cpp:1033-1051`). Les tests ne sont pas rejoués dans cette revue. |
| UX-10 — Markdown simple/source intacte | PARTIEL | Le renderer couvre plusieurs formes et test vérifie `TuiMessage` inchangé (`tui_navigation_test.cpp:131-154`); l'emphase et le code inline stylent toute la ligne et non des plages, et tableaux non spécialement formatés. |
| UX-11 — fragments SSE/fences | PARTIEL | Les deltas s'accumulent puis la projection complète est recalculée; fence non terminée reste littérale et des tests fragmentés existent (`tui_navigation_test.cpp:157-179`). Pas de test d'intégration SSE dédié à des frontières arbitraires Markdown/fin incomplète au candidat. |
| UX-12 — contenu hostile | PARTIEL | Le sanitizer streaming existant neutralise CSI/OSC/C0 avant le renderer; aucun HTML/link/image n'est exécuté. Aucun test spécifique combinant réponse Markdown hostile et terminal réel n'a été exécuté/revu comme preuve nouvelle. |
| UX-13 — six langages | PARTIEL | Tags cpp/python/bash/json/yaml/diff reconnus (`tui_view.cpp:116-119`); lexer lexical simplifié et pas de vérification visuelle. Le fallback d'un langage inconnu est code neutre par ce chemin. |
| UX-14 — NO_COLOR/resize | PARTIEL | `NO_COLOR` désactive les couleurs et le code de resize existe; tests précédents/PTY rapportés, mais séquence 120×40→80×24→60×20 et 16 couleurs non qualifiée physiquement sur ce candidat. |
| UX-15 — longues réponses/performance | NON CONFORME | Projection/reparse complet à chaque delta et plusieurs fois par dessin (`tui_view.cpp:440-448, 612-635, 968-970`); aucune borne ni mesure. Conflit direct avec MD-06 du plan. |
| UX-16 — texte brut | CONFORME au niveau projection | `M` passe le contenu assistant à `present_markdown(..., false)` sans changer `TuiMessage`; tests vérifient l'égalité source (`tui_navigation_test.cpp:131-148`). L'action clavier elle-même n'a pas de test PTY. |
| UX-17 — visionneuse .md | PARTIEL / REPORTÉ | Non implémentée. Le motif invoqué (API de lecture sécurisée non réutilisable hors helpers internes) est plausible, mais cette revue ne l'a pas vérifié indépendamment dans le code workspace. |
| UX-18 — chemins de visionneuse | NON APPLICABLE | Aucun chemin de lecture `.md` ajouté; aucune nouvelle surface d'accès workspace. |
| UX-19 — erreur SSE après Markdown | CONFORME au niveau code existant | La finalisation d'échec marque l'assistant incomplet, ajoute un diagnostic et un notice; la vue dessine `[incomplete]`. Le test SSE/PTy concerné est préexistant et n'a pas été relancé ici. |
| UX-20 — documentation/SHA | NON CONFORME | Le candidat indexe les documents, mais le rapport contient une affirmation erronée sur l'absence du plan; sa matrice UX-20 annonce encore des diff checks « requis après ce rapport » alors que les lignes suivantes les déclarent PASS. Aucun SHA de commit final puisqu'il n'y a pas de commit. |

## Vérifications exécutées pour cette revue

- `git status --short`, `git rev-parse HEAD`, `git rev-parse origin/main`,
  `git branch --show-current`, `git diff --name-only`, `git diff --stat`,
  `git diff --check` : exécutés; état et périmètre consignés ci-dessus.
- Lecture du diff UI/navigation/terminal/tests, du présentateur Markdown, de la
  spécification intégrée, du plan séparé externe, du rapport d'implémentation,
  de `USAGE.md` et de `docs/README.md`.
- Contrôle des états Git des checkouts principal, UI-4 historique et worktree
  graphique : pas de modifications détectées.
- **Aucun build, CTest, harnais, test PTY ou essai Ghostty exécuté pendant cette
  revue.** Les chiffres 289 cas / 8 429 assertions et CTest 14/14 sont ceux du
  rapport précédent, pas une reproduction indépendante.

## Recette physique courte — Debian 13 / Ghostty

À faire après les corrections R1–R4, sur le binaire du candidat reconstruit.
Cette procédure utilise un workspace temporaire dédié et ne montre pas le
contenu de `.env` :

```sh
set -eu
APP=/tmp/local-agent-cpp-lofiterm-ux-v11-final/agent
TMP_WORK=$(mktemp -d)
mkdir -p "$TMP_WORK/workspace"
cd "$TMP_WORK"
set -a
source /home/oper/projects/local-agent-cpp/.env
set +a
before=$(stty -g)
"$APP" --ui=tui
after=$(stty -g)
test "$before" = "$after"
```

Dans la TUI :

1. Essayer F1–F6, puis Tab et flèches; vérifier que les six panneaux et les
   rubriques F5/F6 changent réellement, et qu'un clic sur chacun des onglets
   choisit la même vue. Cliquer une rubrique ouverte doit la refermer.
2. Cliquer le composeur, saisir `M` et confirmer qu'il est du texte; focaliser
   ensuite le contenu et vérifier que `M` change uniquement la projection.
3. Tester molette haut/bas dans Conversation, Outils, Métriques et Diagnostics.
   Bouger la souris sans cliquer et vérifier qu'il n'y a ni redraw visible
   continu ni hausse de CPU soutenue.
4. Demander au modèle réel une réponse avec titre, liste, emphase, lien textuel
   et fence C++; observer le rendu pendant le streaming, après fermeture de la
   fence, puis après une erreur/interruption si un scénario sûr est disponible.
   Vérifier que rien n'ouvre d'URL ni n'exécute HTML/ANSI.
5. Répéter un lancement avec couleurs, puis lancer depuis le même répertoire
   avec `NO_COLOR=1` placé devant la commande; les libellés et états doivent
   rester compréhensibles.
6. Redimensionner successivement à 120×40, 80×24 puis 60×20; vérifier le
   composeur, le brouillon, le scroll, les onglets et l'absence de débordement.
7. Quitter par F10 et confirmer que la comparaison `stty -g` réussit après
   chaque lancement. Noter Debian/noyau, Ghostty/TERM, locale, dimensions et
   PASS/FAIL; ne pas copier `.env` ni son contenu dans le rapport.

## Conclusion

Les protections de sécurité examinées restent intactes et aucune régression
moteur n'est trouvée. Cependant, R1, R2, R3 et R5 ainsi que la preuve faible du
test R4 empêchent de qualifier proprement les interactions et la fluidité. La
recette physique doit suivre les corrections ciblées, pas les remplacer. Le
plan distinct doit être intégré comme référence ou explicitement arbitrée; son
§13 dans la spécification ne vaut pas approbation de substitution.

**Décision : corrections ciblées nécessaires avant qualification physique.**
Pas de commit, push, PR, merge ou tag effectué.
