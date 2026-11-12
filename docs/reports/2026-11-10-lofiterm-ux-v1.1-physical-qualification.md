# LOFITERM UX v1.1 — fiche de qualification physique Debian 13 / Ghostty

**Date de préparation :** 2026-11-10
**Branche :** `feat/lofiterm-ux-v1.1`
**SHA du code candidat :** `46d653fe9e788c6410129b7224edd33d870af593`
**Binaire candidat :** `/tmp/local-agent-cpp-lofiterm-ux-v11-corrections/agent`
**État :** résultats manuels rapportés par le mainteneur; qualification
partielle faute de correspondance suffisamment détaillée avec tous les critères.

## État Git et contenu qualifié

Le worktree est `/home/oper/projects/worktrees/local-agent-cpp-lofiterm-ux-v1.1`,
sur `feat/lofiterm-ux-v1.1`, HEAD `46d653fe9e788c6410129b7224edd33d870af593`.
Il contient les changements locaux de l'implémentation UX v1.1 et les
corrections ciblées, sans commit. Aucun fichier n'a été restauré, remplacé ou
supprimé pendant la préparation de cette recette. Le checkout principal n'a
pas été modifié.

L'exécutable du build propre validé est présent au chemin indiqué plus haut.
Les résultats automatisés déjà exécutés sur ce candidat sont documentés dans
[`2026-11-10-lofiterm-ux-v1.1-targeted-corrections.md`](2026-11-10-lofiterm-ux-v1.1-targeted-corrections.md) :
290 cas / 8 503 assertions et CTest 14/14. Ils ne remplacent pas la recette
physique décrite ici.

Le fichier `.env` est absent du worktree isolé et présent à la racine du
checkout principal. Seuls les noms des variables ont été relevés :
`LLM_BASE_URL`, `LLM_MODEL`, `LLM_API_KEY`, `LLM_TIMEOUT_SECONDS`. Aucune
valeur ni clé n'a été lue dans la sortie ou copiée dans le rapport. Le binaire
lit ces valeurs depuis l'environnement; il ne charge pas `.env` lui-même.

### Inventaire Git candidat

État observé après l'enregistrement des observations manuelles : les fichiers
ci-dessous sont le périmètre local à examiner avant une éventuelle intégration.
Cette liste n'est ni un index Git ni une autorisation de commit.

Modifiés et suivis :

- `CMakeLists.txt`
- `README.md`
- `USAGE.md`
- `docs/README.md`
- `docs/roadmap/ROADMAP.md`
- `include/ui/terminal_session.hpp`
- `include/ui/tui_event_loop.hpp`
- `include/ui/tui_navigation.hpp`
- `include/ui/tui_view.hpp`
- `src/ui/terminal_session.cpp`
- `src/ui/tui_event_loop.cpp`
- `src/ui/tui_navigation.cpp`
- `src/ui/tui_view.cpp`
- `tests/CMakeLists.txt`
- `tests/tui_navigation_test.cpp`
- `tests/tui_test.cpp`

Non suivis :

- `docs/reports/2026-11-10-lofiterm-ux-v1.1-final-review.md`
- `docs/reports/2026-11-10-lofiterm-ux-v1.1-implementation.md`
- `docs/reports/2026-11-10-lofiterm-ux-v1.1-physical-qualification.md`
- `docs/reports/2026-11-10-lofiterm-ux-v1.1-targeted-corrections.md`
- `docs/specifications/SPEC-LOFI-001-v1.1.md`
- `include/ui/markdown_presenter.hpp`
- `src/ui/markdown_presenter.cpp`

## Résultats manuels rapportés par le mainteneur

Le mainteneur indique avoir personnellement exécuté dans Ghostty une checklist
rapide de 11 contrôles : **11 PASS, 0 FAIL**. Les contrôles annoncés sont :

1. Six vues F1–F6.
2. Clavier.
3. Souris.
4. Molette.
5. Identité « LOFITERM Bot ».
6. Markdown et coloration syntaxique.
7. Mode texte brut.
8. Streaming d'une réponse LLM réelle.
9. Redimensionnement.
10. Refus et annulation d'écriture.
11. Restauration du terminal.

Cette liste transcrit les catégories fournies; elle ne précise pas chaque
touche, taille de terminal, langage de coloration, effet filesystem ou détail
de la réponse. Elle est attribuée à cette observation manuelle et n'est pas
présentée comme reproduite par l'agent. La version de Debian, `TERM`, locale et
dimensions exactes ne sont pas consignés dans le résultat transmis.

### Recette complémentaire rapportée après la checklist rapide

Le mainteneur a ensuite rapporté T01, T03, T04, T05 et T06 `PASS`, et T02
`FAIL`. Les identifiants T01–T06 sont conservés tels quels : aucun mapping vers
les 16 scénarios officiels n'a été fourni, donc aucune correspondance n'est
inférée.

Le seul motif donné pour T02 est ergonomique : sélectionner le texte du défi
`yes …` dans Ghostty nécessitait Maj pendant que ncurses recevait les
événements souris. Les captures montrent que le défi a pu être sélectionné et
saisi dans `Input:`. Cela atteste le fonctionnement du collage dans ce cas,
mais pas une sélection native sans Maj. Aucun échec de validation du défi ni
aucune écriture non autorisée n'a été démontré. Le défaut observé est la
concurrence entre le reporting souris de ncurses et la sélection native du
terminal.

La correction locale proposée suspend le reporting souris uniquement pendant
le dialogue d'approbation, puis le réactive à sa fermeture. Cette nouvelle
transition doit encore être vérifiée dans Ghostty; le résultat antérieur ne
peut pas servir de preuve de la sélection native sans Maj après correction.
Les tests PTY vérifient séparément les séquences de bascule et les effets
filesystem dans leurs répertoires temporaires dédiés. Aucun chemin du workspace
physique de la recette complémentaire n'a été fourni : son contenu après refus
ou annulation n'est donc pas attesté par ces observations manuelles.

## Préparer le lancement dans Ghostty

Ouvrir un nouvel onglet Ghostty Debian 13. Vérifier visuellement la taille du
terminal, puis exécuter les commandes suivantes. Elles sourcent le `.env` connu
du checkout principal sans activer le traçage shell et créent un répertoire
temporaire dédié. Le workspace relatif `workspace/` sera donc créé sous ce
répertoire, et non dans le dépôt ou dans un projet de travail.

```sh
set +x
set -a
source /home/oper/projects/local-agent-cpp/.env
set +a
test_workspace="$(mktemp -d "${TMPDIR:-/tmp}/lofiterm-ux-v11-XXXXXX")"
cd "$test_workspace"
printf 'Workspace de recette : %s\n' "$PWD"
/tmp/local-agent-cpp-lofiterm-ux-v11-corrections/agent --ui=tui
```

Ne pas copier ni afficher le contenu du `.env`. Le chemin temporaire imprimé
ne contient aucun secret. Après fermeture, vérifier les effets de la recette :

```sh
find "$test_workspace/workspace" -maxdepth 4 -type f -print 2>/dev/null
```

Garder le répertoire jusqu'à la fin de l'inspection. Ne pas y placer de données
réelles. Les essais de refus et d'annulation doivent laisser leurs fichiers
sondes absents. Aucun nettoyage automatique n'est effectué par cette fiche.

Pour tester `NO_COLOR`, dans le même shell et depuis le même workspace, lancer
une seconde instance :

```sh
NO_COLOR=1 /tmp/local-agent-cpp-lofiterm-ux-v11-corrections/agent --ui=tui
```

Essai facultatif d'erreur de connexion, sans changer le `.env` :

```sh
LLM_BASE_URL=http://127.0.0.1:1/v1/chat/completions \
  /tmp/local-agent-cpp-lofiterm-ux-v11-corrections/agent --ui=tui
```

Cette dernière commande ne teste qu'un échec de transport local. Elle ne
provoque pas une saturation `EventChannel` et ne valide pas `PARTIAL`.

## Scénarios à exécuter et consigner

Pour chaque ligne, noter `PASS`, `FAIL` ou `NOT RUN`, la taille du terminal,
les touches/clics effectués et le résultat visible. Ne pas inclure les prompts
privés, réponses complètes, valeurs de configuration ou clés dans le rapport.

| # | Scénario officiel | Correspondance avec l'observation manuelle | État détaillé |
|---:|---|---|---|
| 1 | Ouverture plein écran, fermeture F10, restauration du terminal | Restauration déclarée PASS; ouverture et F10 non détaillés séparément. | PARTIAL |
| 2 | F1–F6, Tab, Échap, F9, F10 et raccourcis selon le focus | Six vues et clavier déclarés PASS; touches individuelles non énumérées. | PARTIAL |
| 3 | Clic sur onglets, changement de page et focus | Souris déclarée PASS; clics sur onglets et focus non détaillés séparément. | PARTIAL |
| 4 | Molette et scroll sur contenu long; absence de redraw continu au mouvement du pointeur | Molette déclarée PASS; contenu long et redraw non détaillés. | PARTIAL |
| 5 | Rubriques F5/F6 par Entrée, Échap et clic | Non mentionné dans les 11 contrôles. | NOT RUN |
| 6 | Brouillon multiligne et position de lecture conservés pendant la navigation | Non mentionné. | NOT RUN |
| 7 | Réponse LLM réelle avec éléments Markdown précis | Markdown et streaming réel déclarés PASS; titres, listes, citations et autres éléments non détaillés. | PARTIAL |
| 8 | Coloration des fences C++, Python, Bash, JSON, YAML et diff | Coloration déclarée PASS; langages individuels non précisés. | PARTIAL |
| 9 | `M` bascule le rendu sans détourner la saisie du composeur | Mode texte brut déclaré PASS; touche `M` et absence d'interférence non précisées. | PARTIAL |
| 10 | Streaming long, réactivité et intégrité finale | Streaming LLM réel déclaré PASS; durée, fluidité et absence de duplication non détaillées. | PARTIAL |
| 11 | Tailles 120×40, 100×30, 80×24, 60×20 et retour | Redimensionnement déclaré PASS; dimensions non consignées. | PARTIAL |
| 12 | `NO_COLOR=1`, contraste et restauration | Restauration déclarée PASS; `NO_COLOR` non mentionné. | PARTIAL |
| 13 | Refus `write_file` sans création de fichier | Refus d'écriture déclaré PASS; vérification filesystem non précisée. | PARTIAL |
| 14 | F10 pendant approbation, annulation sans création | Annulation déclarée PASS; F10 pendant le dialogue et vérification filesystem non précisés. | PARTIAL |
| 15 | Message d'erreur d'endpoint inaccessible | Non mentionné. | NOT RUN |
| 16 | État `PARTIAL` obtenu par scénario réel contrôlé | Non mentionné; aucune saturation EventChannel n'est inférée. | NOT RUN |

Pour les scénarios 13 et 14, envoyer au modèle une consigne limitée au workspace
temporaire et à un nom de sonde inédit, par exemple
`ux-v11-refusal-probe.txt`. À l'écran d'approbation, vérifier le chemin et le
contenu, puis saisir un refus et valider; dans l'autre essai, fermer avec F10
pendant l'attente. Inspecter ensuite le filesystem avec `find`. Ne jamais
approuver une écriture pendant cette recette de refus/annulation.

Pour le scénario Markdown, demander une réponse de démonstration textuelle,
sans outil, comportant les constructions listées et un exemple par langage.
Le modèle peut omettre ou modifier des éléments : noter uniquement ce qui a
été effectivement rendu. Une réponse de génération ne permet pas de conclure
sur la saturation EventChannel.

## Environnement physique à relever

À compléter dans Ghostty : Debian/version, noyau, version Ghostty, `TERM`,
locale UTF-8, largeur×hauteur initiale et tailles testées. Ne pas joindre de
capture contenant des secrets, contenu privé ou données de configuration.
Les contrôles PTY automatisés restent distincts de ces observations.

## Verdict

**QUALIFICATION PARTIELLE AVEC RÉSERVES.** Le mainteneur rapporte 11 contrôles
rapides réussis et aucun échec. Plusieurs observations couvrent les grandes
fonctions visées, mais les détails fournis ne démontrent pas tous les critères
physiques officiels. Les scénarios 5, 6 et 15 restent `NOT RUN`; le scénario
16 (`EventChannel` saturé jusqu'à F3 `PARTIAL`) reste également `NOT RUN`.
`NO_COLOR` n'a pas été rapporté. Les scénarios 1–4 et 7–14 sont `PARTIAL`
lorsque l'observation générale ne suffit pas à attester toutes les conditions
du critère.

Cette recette ne démontre donc pas une qualification physique complète ni un
état « prêt pour intégration Git ». Les vérifications résiduelles prioritaires
sont les sous-rubriques F5/F6, la conservation explicite du brouillon et du
viewport, `NO_COLOR`, le refus/annulation avec vérification filesystem, ainsi
que les dimensions et raccourcis exacts. Le scénario de saturation `PARTIAL`
peut rester non exécuté si aucun déclenchement sûr n'est disponible, mais doit
rester déclaré comme tel.
