# TUI-3 — Première intégration ncursesw

## Référence

- Date : 2026-11-06.
- Branche : `feat/tui-3-ncursesw-integration`.
- Base vérifiée : `3a75dfc6b453cee07628dad65398da45de1bb1fc` (`origin/main`, merge de TUI-2).
- Environnement : Debian 13, Linux 6.12.113, GCC 14.2.0, CMake 3.31.6, ncursesw 6.5.20250216.
- Checkout principal UI-4 : distinct et préservé. Son état n'a pas été modifié par cette phase.

## Objectif et architecture

Le mode plein écran est sélectionné explicitement par `--ui=tui`. Le mode par
défaut reste `--ui=classic` et conserve le chemin Plain/Rich et le REPL actuel.
Le mode TUI demande stdin et stdout interactifs et refuse `TERM=dumb` ou un
terminal sans capacité ANSI déclarée. Il n'y a pas de repli silencieux.

La cible CMake `agent_tui` contient `TerminalSession`, `TuiApplication`,
`TuiEventLoop` et `TuiView`. Elle dépend de `agent_core` et ncursesw ;
`agent_core` et les contrats de l'agent restent sans dépendance ncurses.
`TuiApplication` utilise l'`AgentWorker` TUI-2 existant : un worker, un seul
tour actif et la FIFO existante. Le thread principal reste l'unique utilisateur
de ncursesw. `PresentationState` valide les événements tant que sa séquence est
continue ; les détails d'un tour perdu sont signalés séparément et le prochain
`RequestStarted` permet de reprendre la projection des événements suivants.

Le premier écran contient un en-tête, un état technique, une conversation
défilante et un éditeur multiligne. `Enter` insère une nouvelle ligne, `F9`
envoie, `F10` ou `Escape` demande la fermeture, `Tab` change le focus et
`PageUp` / `PageDown` contrôlent le scroll. Le scroll suit le bas tant que
l'utilisateur y reste ; après remontée, les lignes reçues n'imposent pas le
retour au bas. Le suivi reprend lorsque le défilement revient à zéro. Les
petites dimensions utilisent une disposition compacte, avec la ligne du curseur
visible en 10×4.

## Résultat et erreurs

- Les deltas `TextDelta` passent une fois par le sanitiseur terminal incrémental
  déjà présent ; les contrôles OSC/CSI et caractères dangereux ne sont pas
  envoyés à ncurses. Les noms d'outils et diagnostics sont également assainis.
- Si les détails d'événements sont complets, seuls les deltas sont affichés ;
  `TurnFinished` est un état technique, jamais une validation de la tâche.
- Si la FIFO perd des événements mais que le worker retourne une réponse, le
  texte final assaini remplace le fragment affiché. Il n'est pas concaténé.
  Si le worker échoue, un fragment éventuel est marqué incomplet et l'exception
  est présentée comme erreur.
- Le résultat HTTP, le résultat final du tour et le succès des outils restent
  distincts. Les événements d'outil affichent le nom et l'état observés, sans
  arguments ni résultats sensibles.
- À la fermeture pendant un tour, l'interface affiche l'attente et continue de
  consommer jusqu'au résultat du worker. Il n'existe pas d'annulation de
  `Agent::send()` ; l'attente peut durer jusqu'au timeout/résultat des opérations
  HTTP ou outil.

## Landlock et durée de vie

ncurses charge la base terminfo pendant `newterm()`. L'accès à `/usr/share`
étant volontairement absent des règles Landlock, initialiser ncurses après le
confinement ferait échouer ce chargement. Le mode TUI initialise donc la session
terminale par RAII en premier, applique ensuite les règles Landlock inchangées,
puis seulement crée le worker et l'agent. Aucun chemin ni droit Landlock n'a
été ajouté. Si l'application du confinement échoue, la session terminale est
détruite et restaurée avant l'erreur CLI. L'agent et le worker sont détruits
avant la session terminale lors de la sortie normale.

Cette séquence laisse l'initialisation terminfo s'exécuter avant Landlock ; elle
ne change pas les permissions du moteur et aucune requête LLM ni opération
d'outil n'a lieu avant le confinement. Les tests ci-dessous valident le chemin
PTY avec Landlock actif.

## Fichiers concernés

- `CMakeLists.txt` : cible `agent_tui`, détection wide ncurses.
- `README.md` : dépendance ncursesw et sélection du mode TUI documentées.
- `src/main.cpp`, `include/ui/ui_options.hpp` : choix explicite classic/TUI ;
  classic reste le défaut.
- `include/ui/terminal_session.hpp`, `src/ui/terminal_session.cpp` : TTY,
  initialisation ncursesw et restauration RAII.
- `include/ui/tui_application.hpp`, `src/ui/tui_application.cpp` : ordre
  terminal, confinement, worker.
- `include/ui/tui_event_loop.hpp`, `src/ui/tui_event_loop.cpp` : événements,
  entrée, streaming, erreurs et fermeture.
- `include/ui/tui_view.hpp`, `src/ui/tui_view.cpp` : écran et viewport.
- `tests/CMakeLists.txt`, `tests/tui_test.cpp`, `tests/tui_pty_driver.cpp` :
  groupe TUI-3 et driver isolé de PTY. Le driver limite volontairement la file
  à un événement et un octet par payload pour rendre la perte de TextDelta
  déterministe ; les limites de production restent celles par défaut du worker.
- `docs/README.md` : index de ce rapport.

Aucun changement n'a été apporté à `Agent::send()`, HTTP/SSE, aux callbacks,
Landlock, aux outils workspace, à `write_file` ou au contrat du REPL.

## Vérifications observées

Build propre dans un répertoire CMake distinct, avec la source FetchContent déjà
téléchargée :

```text
cmake -S . -B build-tui3-clean -DAGENT_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-tui3-build-initial/_deps/json-src
cmake --build build-tui3-clean -j2
```

Les deux commandes ont réussi. CMake a trouvé `/usr/lib/x86_64-linux-gnu/libncursesw.so`.
Le premier essai de configuration sans répertoire FetchContent local était
bloqué sur le réseau du sandbox ; après autorisation réseau le téléchargement
de nlohmann/json a abouti. Une nouvelle configuration propre a réutilisé cette
source locale.

Groupe ciblé `./build-tui3-clean/tests/agent_tests --filter=tui3` : **7 cas,
32 assertions, zéro échec**. Les tests exercent : choix d'interface, refus sans
TTY, restauration ncurses après exception contrôlée, viewport et remplacement
de fragment, vrai `Agent::send()` via PTY et serveur mock, redimensionnement
10×4, OSC hostile, perte forcée d'événements, fermeture pendant le tour, et
erreur HTTP affichée.

```text
ctest --test-dir build-tui3-clean --output-on-failure
100% tests passed, 0 tests failed out of 11
Total Test time: 7.12 sec

./build-tui3-clean/tests/agent_tests
cas executes        : 193 / 193
assertions          : 1228
assertions en echec : 0
RESULTAT: OK
```

Les appels loopback ont nécessité l'autorisation d'exécution hors sandbox.
`git diff --check` a réussi. Les essais de terminal sont des essais PTY ; aucun
nouvel essai sur terminal physique ou SSH n'a été effectué pendant TUI-3.

## Limites et risques

- Cette livraison est un premier écran utilisable, pas le système de six vues,
  menus, métriques ou diagnostics définitifs. Aucune statistique agrégée,
  animation ou persistance n'est ajoutée.
- L'éditeur conserve les retours à la ligne et les caractères saisis, et permet
  le déplacement horizontal et l'effacement ; navigation verticale avancée,
  édition de ligne complète et ergonomie multi-locale restent à approfondir.
- Les PTY couvrent le flux simulé par le serveur mock, les séquences terminal,
  le redimensionnement et le chemin Landlock. Ils ne prouvent pas le confort sur
  tous les émulateurs, SSH, les signaux non interceptables (`SIGKILL`) ni la
  restauration après panne matérielle.
- Un tour actif ne peut pas être annulé ; F10 attend la fin réelle du worker.
- Après une perte de détails, le texte final exact est disponible seulement si
  le tour retourne normalement. Un tour échoué conserve le fragment comme
  incomplet et affiche l'erreur, sans inventer de réponse.
- `TerminalSession` restaure par RAII lors d'une fermeture normale et d'une
  exception C++ contrôlée ; aucune garantie n'est faite pour `SIGKILL` ou une
  coupure d'alimentation.

## Verdict

**VALIDÉ pour le périmètre TUI-3 implémenté et les validations PTY observées.**
Le mode TUI est explicitement opt-in ; le mode classique reste le défaut.
Les six vues définitives, l'ergonomie sur terminaux réels et les phases TUI
suivantes restent hors périmètre.
