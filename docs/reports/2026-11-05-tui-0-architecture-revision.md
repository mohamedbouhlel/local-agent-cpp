# TUI-0 — Étude d'architecture et révision SPEC-UI-001

**Date :** 2026-11-05
**Branche documentaire :** `docs/tui-0-architecture-revision`
**SHA de référence :** `ba1ee2e9b3535f3e5a6b79a4d544dd5b3a2ab81f` (`origin/main`, après `git fetch origin main`)
**Checkout UI-4 préservé :** `feat/ui-4-observable-metrics` à `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`
**Verdict :** étude documentaire préparée; choix backend et architecture concurrente à ratifier avant implémentation.

## 1. Objectif et périmètre

Examiner le code réellement présent après les merges UI-1 à UI-3 et proposer une SPEC-UI-001 v2.0 répondant à la décision mainteneur d'intégrer une TUI plein écran, tout en préservant la CLI classique. La mission ne modifie aucun code, CMake, test, moteur, CLI ou sandbox. Elle n'exécute ni build ni tests et ne commit/push/merge rien.

## 2. Vérification Git et faits constatés

Le checkout initial était sur `feat/ui-4-observable-metrics`, HEAD `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`. Après récupération contrôlée, `origin/main` a avancé à `ba1ee2e9b3535f3e5a6b79a4d544dd5b3a2ab81f` (deux commits depuis le checkout initial); cette référence inclut le merge PR #21. Le worktree UI-4 initial est signalé `behind 2` par rapport à `origin/main`.

État UI-4 initial, aucun fichier indexé :

- Modifiés : `CMakeLists.txt`, `docs/README.md`, `include/ui/presentation_controller.hpp`, `src/ui/presentation_controller.cpp`, `tests/CMakeLists.txt`.
- Non suivis : `docs/reports/2026-11-05-ui-4-observable-metrics.md`, `include/ui/metrics.hpp`, `src/ui/metrics.cpp`, `tests/ui_metrics_test.cpp`.

Aucun de ces fichiers n'a été modifié, déplacé, supprimé, indexé ou intégré à la branche documentaire. Une branche locale `docs/tui-0-architecture-revision` et un worktree séparé `/tmp/local-agent-cpp-tui0-docs` ont été créés depuis le `origin/main` actualisé; son `HEAD` est le SHA de référence ci-dessus. Le nom de branche distant a été recherché avec `git ls-remote`; aucune tête correspondante n'a été retournée. Le worktree documentaire est propre avant rédaction. Le checkout UI-4 est resté sur sa branche d'origine.

Les PR #17 et #18 apparaissent dans l'historique de `origin/main` via les merges UI-1/2/3; PR #21 y est aussi fusionnée. UI-4 n'est pas intégrée au SHA de référence. Le rapport UI-4 local indique historique de tests UI-4 `7/7`, CTest `10/10`, `182` cas / `1 154` assertions, mais ces valeurs sont **rapportées dans un fichier local non commité**, pas rejouées dans TUI-0. Elles ne sont pas revendiquées comme résultats TUI-0 ni comme validation de `origin/main`.

La PR #21 a ajouté `docs/reports/2026-11-05-incident-agent-nonconvergence-workspace.md`. Ce rapport, fondé sur une session utilisateur rapportée, décrit des tool calls de mutation non sollicités et des tours non convergents; il identifie des hypothèses et déclare la cause racine non déterminée. C'est un signal pertinent de fiabilité/intégrité, pas une preuve que la TUI corrigera ou empêchera ces comportements.

## 3. Architecture présente, vérifiée dans les sources

- `include/ui/agent_event.hpp` définit le contrat canonique `AgentEvent`, `EventPayload` en `std::variant`, les IDs, `sequence`, timestamp UTC et durée `steady_clock`; `TextDelta` transporte du texte transitoire.
- `Agent::send()` demeure synchrone. `publish_event()` construit puis appelle `EventObserver::on_event()` dans le même thread. Une exception d'observateur désactive la voie et appelle le diagnostic isolé.
- `PresentationState::apply()` consomme et valide l'ordre/corrélation, mémorise phase et compteurs, mais ne stocke pas les fragments.
- `PresentationController` applique l'état puis le `MetricCollector` et le renderer; les erreurs de renderer sont traitées à la frontière de présentation.
- Plain/Rich et `TerminalTextSanitizer` sont les renderers actuels. La sélection repose sur les capacités de `TerminalCapabilities`; ce chemin n'active pas le raw mode.
- `main.cpp` initialise le workspace et Landlock avant le REPL, puis utilise `PromptReader` et `run_console_turn()`. Le contrat multiligne historique reste séparé.
- UI-4 a un collecteur dans le worktree non commité, pas sur `origin/main` de référence.

## 4. Comparaison des backends

Les capacités ci-dessous sont évaluées à partir des interfaces et documentations amont examinées. Aucun des trois choix n'a été intégré ni soumis à un test pty pendant cette mission.

| Dimension | ncurses/ncursesw | FTXUI | Backend personnalisé |
|---|---|---|---|
| Dépendances / acquisition | Bibliothèque système; `FindCurses`, option wide à demander. Installation et versions dépendent de la distro. | Dépendance C++ externe; upstream documente FetchContent et cibles screen/dom/component; pin/tag nécessaire pour reproductibilité. | Aucune bibliothèque TUI externe, mais code terminal propre à maintenir. |
| Intégration CMake | `find_package(Curses)` et liaison; `CURSES_NEED_WIDE` pour caractères wide. | `FetchContent` ou package installé; la doc présente chaque cible et indique des options de tests/exemples/docs. Le CMake minimum du tag retenu doit être vérifié avant adoption; projet à 3.20. | CMake léger mais plusieurs modules POSIX/terminfo/termios et détection de plateforme à gérer. |
| Unicode | API wide disponible avec ncursesw; conversion, largeur d'affichage et locale nécessitent tests. | Screen annonce UTF-8, caractères pleine largeur et combinants; vérifier limitations avec les données réelles. | UTF-8, codepoints combinants et largeur de cellules sont à implémenter et tester. |
| Menu, focus et clavier | `getch`/`keypad`, fenêtres; focus et navigation applicative à écrire. | Component expose Menu, Input, conteneurs, événements clavier, focus et boucle App. | À écrire entièrement : parseur touches, focus, keymap et répétition. |
| Saisie multiligne | À construire (édition, curseur, historique éventuel). | Changelog upstream documente `Input` multilignes; adaptation du contrat exact REPL reste au projet. | À construire entièrement, y compris édition Unicode et navigation. |
| Panneaux et scroll | Fenêtres/panels disponibles dans l'écosystème ncurses; intégration manuelle et dépendances panel peuvent varier. | DOM composable, frame/scroll indicators, composants layout et splits sont prévus par l'API. | Composition, clip, scroll et rendu différentiel à écrire. |
| Souris | Possible, souvent optionnelle et terminal-dépendante. | Événements souris documentés; peut rester désactivée pour version initiale. | Protocole souris xterm à gérer ou exclure. |
| Petites tailles / resize | `KEY_RESIZE`, `resizeterm`/`wresize` sont documentés; application doit réorganiser ses fenêtres. | Dimensionnement/layout réactif; le comportement réel des petites tailles doit être testé. | Politique et calcul layout intégralement spécifiques. |
| Streaming | Écriture incrémentale faisable, mais thread de rendu et redraw à concevoir. | Une boucle événementielle dédiée rend naturel le redraw depuis le thread UI; l'intégration async au moteur reste à concevoir. | File et scheduler/redraw à écrire. |
| Pseudo-terminal / accessibilité | Tests pty possibles; API connue, mais composants accessibles et navigation logique doivent être ajoutés. | Les composants peuvent être exercés par événements, mais tests pty restent nécessaires; accessibilité terminal n'est pas prouvée par le framework. | Harness pty possible, couverture de bas niveau très large; accessibilité entièrement maison. |
| Terminal lifecycle / sécurité | `cbreak`, `noecho`, `endwin`; raw modifie aussi le traitement des caractères de contrôle. Il faut un owner RAII et des essais sur interruption. | Screen/App fournit boucle et gestion de terminal de haut niveau, mais la restauration après exception/signal/crash doit être prouvée sur le tag choisi, jamais supposée. | Risque maximal : termios, signaux, séquences de contrôle et restauration à posséder. |
| Maintenance / réversibilité | Stable mais API C et différences de packaging; backend isolable. | UI C++ plus rapide à construire; dépendance externe réversible si le backend est derrière une interface et la CLI reste séparée. | Réversible en théorie, mais dette propriétaire importante et difficile à valider. |
| Effort estimatif (qualitatif) | Moyen/élevé. | Moyen pour l'UI, moyen/élevé pour queue/concurrence/sécurité d'intégration. | Très élevé. |

### Sources primaires consultées

- [FTXUI Getting Started](https://github.com/ArthurSonzogni/FTXUI/blob/main/doc/getting-started.md) décrit les modules Screen (terminal/grid), DOM (layout) et Component (clavier/souris/main loop) et l'intégration FetchContent.
- [Guide CMake FTXUI](https://github.com/ArthurSonzogni/FTXUI/blob/main/doc/installation_cmake.md) documente FetchContent, les cibles `ftxui::screen`, `ftxui::dom`, `ftxui::component` et options d'exemples/docs/tests désactivées par défaut.
- [Changelog FTXUI](https://github.com/ArthurSonzogni/FTXUI/blob/main/CHANGELOG.md) documente le support multi-ligne de l'input et les éléments Screen/DOM/Component concernés.
- [Releases FTXUI](https://github.com/ArthurSonzogni/FTXUI/releases) indiquait v7.0.3 comme release la plus récente visible au moment de la consultation du 2026-11-05. L'API et compatibilité du tag restent à valider avant pin.
- [NCURSES Programming HOWTO](https://invisible-island.net/ncurses/NCURSES-Programming-HOWTO.html) distingue `raw()` et `cbreak()` et décrit `noecho`, `keypad`, entrées et fenêtres.
- [NCURSES intro](https://invisible-island.net/ncurses/ncurses-intro.html) documente le traitement resize `KEY_RESIZE`, `resizeterm()` et `wresize()`.
- [CMake FindCurses](https://cmake.org/cmake/help/latest/module/FindCurses.html) documente `CURSES_NEED_WIDE` pour la fonctionnalité wide.

Ces pages établissent ce que les projets exposent/documentent; elles ne démontrent ni l'absence de défauts ni le comportement sur les terminaux supportés par local-agent-cpp.

## 5. Architecture recommandée

**Recommandation ouverte : FTXUI comme candidat de POC**, pas comme dépendance approuvée. Sa séparation Screen/DOM/Component, ses composants de menu/input, son layout et l'input multiligne correspondent mieux au besoin que les primitives de ncursesw. Le coût d'une dépendance externe est réel et doit être accepté explicitement; choisir un tag fixe après inspection du CMake minimal et établir un build reproductible. ncursesw reste une option si la priorité mainteneur devient une bibliothèque système et que l'équipe accepte plus de code d'interface bas niveau. Ne pas écrire un backend personnalisé sauf contrainte technique documentée.

### Frontières et cycle

`TuiApplication` possède session terminal, backend, boucle de rendu, modèle de focus et interaction. L'`Agent` ne sait rien de la TUI. Une seule thread UI possède `PresentationState`, métriques, composants et terminal. Un worker unique exécute un `Agent::send()` à la fois et publie vers un `EventDispatcher` à file bornée. Les événements sont des copies possédées de `AgentEvent` (aucune référence ne survit au callback). Les IDs et `sequence` restent ceux du moteur.

### File et réactivité

La première rédaction proposait mutex/condition variable, fusion de deltas contigus et conservation générale des événements terminaux. Ces éléments ont été affinés après revue et ne sont pas des garanties retenues : voir le complément ci-dessous et le protocole POC. La recommandation actuelle n'agrège pas les deltas; elle réserve seulement `RequestFailed`/`TurnFinished` et le résultat worker, et signale explicitement toute perte de détails HTTP/tool au dépassement.

### UX proposée

Écrans : Conversation, Outils, Métriques, Diagnostics, Configuration et Aide. Menu latéral sur grand écran, vue unique en compact, puis repli CLI classique sous le minimum d'affichage. Tab/Shift+Tab pour focus, flèches selon focus, Enter pour activation, Escape pour retour (jamais annulation implicite). `Enter` insère une ligne; soumission proposée par `Ctrl+Enter`, à valider. L'éditeur conserve les octets/retours explicitement contractés et transmet un seul `std::string` à `Agent::send()`. La TUI n'interprète pas `:multi`/`:end`; le REPL classique conserve exactement son contrat.

### Terminal, erreurs, métriques

Initialisation uniquement après TTY/backend validation; encapsuler les changements d'écran et de mode input dans RAII et restaurer lors du teardown normal/exception/setup partiel. Utiliser la boucle backend pour les événements de resize. Les handlers signal ne font aucun rendu ni allocation; relayer les signaux récupérables à la boucle. SIGKILL/crash ne peut pas être garanti récupérable. Une erreur renderer coupe/désactive le rendu, restaure l'écran et utilise stderr simple si possible, sans transformer l'issue moteur.

Réutiliser `PresentationState` et `MetricCollector` après leur intégration validée. Aucun token/s, ETA ou pourcentage non mesuré. Les métadonnées expurgées ne sont affichées que si disponibles. Les logs restent optionnels et off par défaut; aucun élargissement Landlock. La configuration masque `LLM_API_KEY` et expurge les credentials d'URL.

## 6. Impacts sur UI-4 à UI-8

- **UI-4** : son rapport et les fichiers métriques existent uniquement dans le checkout UI-4 local non commité observé. Il faut d'abord terminer sa revue/intégration indépendante. TUI ne les présume pas fusionnés; la v2 dit de les réutiliser uniquement après intégration réelle.
- **UI-5** : diagnostic/errors doit être repositionné après l'EventDispatcher et les vues, avec codes et source factuelle. Les erreurs de queue et backend deviennent des cas nouveaux.
- **UI-6** : JSONL doit rester hors thread renderer, optionnel, allowlist et sans permission Landlock ajoutée. Un fichier dans workspace peut être visible par les outils; ce n'est pas un canal d'audit privé.
- **UI-7** : dispatcher de commandes CLI classique est à distinguer du menu TUI; aucun raccourci UI ne devient commande moteur. Le contrat `PromptReader` ne change pas.
- **UI-8** : ajouter validation pty sur setup/teardown, resize, petites tailles, signaux, surcharge et arrêt worker, en plus du CTest courant et de la régression moteur/sandbox. Aucun résultat de test TUI n'existe encore.

## 7. Invariants sécurité et fiabilité

- Landlock demeure activé et inchangé; aucun accès de terminal n'est utilisé pour ouvrir un chemin arbitraire.
- `write_file` refuse toujours `overwrite:true`; la TUI n'est pas un écran d'autorisation de contournement.
- Tout résultat d'outil est seulement le résultat observé du handler; succès technique du tour n'est pas réussite de tâche.
- Les événements sont ordonnés jusqu'à une saturation déclarée; issue finale conservée par canal dédié, détails d'opération possiblement incomplets et explicitement signalés.
- Aucune exception C++ traverse le callback C libcurl; callback historique conservé.
- Les données modèle/outils sont assainies à chaque surface de rendu et jamais interprétées comme contrôle terminal.
- Clés API, prompts, tool arguments, résultats et corps HTTP restent absents des logs par défaut.
- Aucune requête ne modifie le payload, l'historique, l'appairage assistant/tool ou le protocole SSE.
- Pas d'annulation fictive; l'arrêt UI attend le résultat/timeout moteur tant qu'aucun contrat d'annulation séparé n'est ratifié.

## 8. Plan exécutable proposé

| Phase | Modifications futures indicatives | Validation de sortie |
|---|---|---|
| TUI-1 — POC backend | cible CMake optionnelle, `TerminalSession` RAII, sélection mode et exemple minimal | dépendance/version approuvée; build propre; pty normal/exception/resize; CLI classique inchangée |
| TUI-2 — Async bridge | `EventDispatcher`, queue, worker owner et résultat de tour | tests déterministes ordre/corrélation, stress borné, overflow explicite, stop/join sans deadlock |
| TUI-3 — navigation | `TuiApplication`, menus, focus, layout compact, vues Outils/Aide/Config | injection des événements clavier, resize, rendu adaptatif, aucune commande moteur ajoutée |
| TUI-4 — conversation | transcript borné, scroll, compositeur multiligne, stream events | tests exactitude prompt, TextDelta une seule fois, échec partiel honnête, conservation saisie |
| TUI-5 — observabilité | brancher MetricCollector aux vues | sources/unités visibles, absences explicites, pas de métrique inventée |
| TUI-6 — terminal | sanitizer unifié, signaux et teardown complet | pseudo-TTY, contrôles fractionnés, exception/setup partiel, tests restore |
| TUI-7 — JSONL | logger sérialisant allowlist | privacy sentinels, taille/rotation/panne arbitrées, Landlock identique |
| TUI-8 — clôture | documentation, matrice complète, régression | CTest + harnais + pty; rapport final; verdict séparant pass/fail/non vérifié |

### Stratégie de retour arrière

Garder l'interface actuelle dans son adaptateur classique et garder le backend TUI derrière une construction/sélection isolée. Un échec du backend choisit la CLI classique si les flux et l'usage le permettent, sinon échoue proprement après restauration du terminal. Ne pas rendre une dépendance obligatoire avant le choix ratifié. Aucun retour arrière n'altère Landlock ou le moteur.

## 9. Risques et points ouverts

1. Approbation de dépendance/version, éventuellement préférence pour ncursesw.
2. Politique sélection `auto|classic|tui`; définir stdin/stdout/stderr exigés et le comportement en SSH.
3. Capacité et taille maximale de la file; coalescence, priorité et livraison terminale/out-of-band.
4. Worker unique : comportement quand l'utilisateur veut quitter durant un appel sans annulation réelle.
5. Touche de soumission, `Ctrl+C`, Escape, souris et conflits avec saisie du texte.
6. Limite et politique de transcript écran (indépendant de l'historique moteur).
7. Minimum dimensions et critères de fallback; lecteurs d'écran/accessibilité non validés.
8. Restauration terminal après SIGTERM/SIGINT et crash dur; les garanties du backend doivent être testées.
9. L'incident PR #21 reste une question d'autorisation/convergence moteur; l'UI ne remplace pas son traitement.
10. UI-4 est non commité et sa base a divergé de `origin/main`; intégration/rebase est une opération séparée sous gouvernance, non faite ici.

## 10. Modifications documentaires et vérification

Fichiers préparés dans le worktree documentaire isolé :

- `docs/specifications/console-ui-architecture-requirements.md` — v2.0, décision de TUI ratifiée et propositions explicitement étiquetées.
- `docs/roadmap/ROADMAP.md` — nouvelle section G datée, les sections historiques restent intactes.
- `docs/README.md` — index SPEC v2.0 et rapport TUI-0.
- ce rapport.

Commandes exécutées pour cette mission : `git fetch origin main`, inspection `git status`, `git rev-parse`, `git log`, `git worktree list`, `git branch`, `git ls-remote`, lectures `sed/cat/rg`, sources officielles web et création d'un worktree/branche documentaire. Le `git fetch` a déplacé la référence distante locale à `ba1ee2e`; la consultation distante de la branche TUI-0 n'a retourné aucun résultat. Les deux opérations d'écriture Git ont été limitées à la création de la branche/worktree documentaire. Aucun build ni test n'a été exécuté, conformément au périmètre d'étude.

Résultats observés : worktree initial UI-4 demeuré sur sa branche avec les 9 fichiers listés plus haut; worktree documentaire basé sur `ba1ee2e`, les fichiers UI-4 ne sont pas présents dans son diff. Les liens internes doivent être contrôlés avant publication; aucun contrôle build/test n'est revendiqué. Le contrôle de cohérence documentaire détaillé sera rejoué sur le diff final. Aucun commit, push ou merge n'a été effectué.

## 11. Critères avant lancement d'une phase suivante

- Mainteneur ratifie ou remplace le candidat backend recommandé et autorise explicitement toute dépendance externe.
- Politique de sélection CLI/TUI et touches essentielles arbitrées.
- Contrat de queue (capacité, coalescence, voie terminale, fermeture) accepté avant worker.
- La branche UI-4 locale a une clôture indépendante; le SHA de base TUI suivant est revérifié.
- SPEC-UI-001 v2.0 est ratifiée; aucune proposition n'est traitée comme décision par défaut.
- Chaque phase reçoit une autorisation distincte et un plan de tests.

**Verdict TUI-0 : PRÉPARATION DOCUMENTAIRE — PROPOSITIONS À RATIFIER.** La décision d'introduire une TUI est prise; le choix FTXUI, l'architecture worker/file et les interactions restent ouverts. Aucun code ni test n'a été modifié ou exécuté. Aucune implémentation TUI n'a commencé.

## Complément de revue TUI-0

La revue du 2026-11-05 a précisé des propositions de concurrence et de surcharge qui étaient trop générales dans cette première version. La suggestion initiale de fusionner des deltas et la formulation générale selon laquelle aucun statut terminal ne serait perdu ne constituent pas une garantie ratifiée. Seul `RequestFailed`/`TurnFinished` et le résultat du worker sont proposés pour des slots de fin réservés par tour; les terminaux individuels HTTP/tool peuvent manquer après saturation, avec un état d'observabilité explicitement incomplet. Aucun regroupement `TextDelta` n'est proposé au premier cycle.

Le protocole détaillé, les seuils de test candidat et les critères éliminatoires du POC sont dans [`2026-11-05-tui-0-poc-protocol.md`](2026-11-05-tui-0-poc-protocol.md). Il ne s'agit ni d'une exécution du POC, ni d'une autorisation d'ajouter FTXUI au produit.
