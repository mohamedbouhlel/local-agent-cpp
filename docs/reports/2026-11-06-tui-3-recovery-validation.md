# TUI-3 — restauration contrôlée et revalidation

Date : 2026-11-06
SHA de base : 3a75dfc6b453cee07628dad65398da45de1bb1fc
Branche de référence : feat/tui-3-ncursesw-integration
Worktree utilisé : /home/oper/projects/worktrees/local-agent-cpp-tui3 (HEAD détaché au SHA de base)

## Provenance et stratégie Git

La source est /home/oper/tmp/tui3-recovery-2026-11-06, reconstruite depuis 42 événements fileChange (61 opérations) conservés dans l’historique local de session. Le rapport initial est /home/oper/tmp/tui3-recovery-report-2026-11-06.md.

La branche TUI-3 et origin/main local pointent sur la base. Une première commande git ls-remote origin refs/heads/main a retourné ce SHA ; le contrôle final a échoué sur la résolution DNS de github.com. La référence distante en temps réel n’a donc pas pu être confirmée à la clôture. Git refuse d’attacher une seconde fois la branche car .git/worktrees/local-agent-cpp-tui3 la déclare encore utilisée par l’ancien chemin absent /tmp/local-agent-cpp-tui3. Aucune métadonnée n’a été forcée, réparée ou supprimée. Le nouveau worktree est donc détaché. La branche n’a pas été déplacée.

## Restauration et empreintes

Avant copie, le nouveau worktree était propre au SHA de base ; les cinq fichiers suivis à mettre à jour étaient identiques à git show HEAD:<path> et les douze chemins à ajouter étaient absents. Les 17 sources ont été validées avant copie : 12 ajouts, 5 mises à jour, toutes les empreintes concordantes.

| Fichier | SHA-256 récupéré avant restauration | Résultat après restauration |
|---|---|---|
| CMakeLists.txt | fd2ba24d86c76d5c2967214600395c9f97d1e516626d322e7af067a504fe9427 | identique |
| README.md | 04281e3a30212e204cccabdfed90f96f7104b2a57557b372f058be0e42400293 | identique |
| docs/README.md | e333f385939d60dc1ccc5e49237537e12987fac7254fed4e5dfce8c9eeb4e937 | index ensuite complété avec les deux rapports |
| docs/reports/2026-11-06-tui-3-ncursesw-integration.md | 9ccfbfbe68abd6087d611c861b85abcc8d35dd73cfbd820d35e9c5b4a345f502 | identique |
| include/ui/terminal_session.hpp | cc243063b30d2e1611fb0b34ff1c78d146c83712e6b366043a79d1f9869b6537 | identique |
| include/ui/tui_application.hpp | d74e91ba150df6007d2a83ebf79ee5ce18dbb4467b3af7bf290101e9029c4f59 | identique |
| include/ui/tui_event_loop.hpp | 9b1299fe7130edd896be52ed11b4acba42c7e3f4caa344112ab4877670e602e2 | identique |
| include/ui/tui_view.hpp | ed25de32556240eac5150c4ff74298e97eedc349ef1bc4850b038075eccb4b3a | identique |
| include/ui/ui_options.hpp | 23d75b66928fbb1470dd12778981c09c3f4eb669a528a9e610b7fecc5549d1dd | identique |
| src/main.cpp | d79d1476507c24a84147f82f9a22a8f01a05ff47c6c0552a921357ec40ef7fbc | identique |
| src/ui/terminal_session.cpp | f0b6d8214719ca83161c0e1328531b7e44552ae4dd027aebf93bcaab6635fd8f | identique |
| src/ui/tui_application.cpp | 3f86848b778ac093ffdbeaa684473533f8aefd0cce5e0957e19471cfb077a7f5 | identique |
| src/ui/tui_event_loop.cpp | da3b67cc93aa075dfd3c499aa3a9949fd59c2e1ffdc39fcfeaa9655a6139829c | identique |
| src/ui/tui_view.cpp | c9ffe06c41beb0c2e7eae8f8a77b59dbd665717f053e2149a4abd902b4e68cfd | identique |
| tests/CMakeLists.txt | acbea66e29a459b1c7aacb507dd81041df375d911f90bbbb794c44f8e9e8161b | identique |
| tests/tui_pty_driver.cpp | b24c164699b0a9ad742ed74123799871f57ac07c94de808ee5357758ac3ad2f8 | identique |
| tests/tui_test.cpp | 37baf8bafda417efce32dd8f83b16f0bf8d9776b92a4ebe2bb09be8db2a0c8eb | d3ed76ee8ddab2319a5de395d0c776d0f62672a4d250847697063a14b659f4e1 |

La seule différence de contenu est une correction de formatage dans tests/tui_test.cpp : suppression d’une ligne blanche excédentaire à EOF après vérification du hash source. Aucun comportement de test ou de production n’a été changé. Le rapport de récupération initial ne listait pas les hashes ; ils avaient été consignés dans le récapitulatif précédent, puis revérifiés sur les 17 fichiers avant copie.

## Diff et périmètre

Le diff restauré comporte cinq fichiers suivis modifiés (CMakeLists.txt, README.md, docs/README.md, src/main.cpp, tests/CMakeLists.txt) et douze nouveaux fichiers. Aucun changement dans src/agent.cpp, HTTP/SSE, Landlock, les outils workspace ou la politique write_file. La CLI classique reste le défaut ; TUI est opt-in. ncurses initialise terminfo avant l’appel au confinement Landlock, sans modification des règles. Le checkout UI-4 demeure intact.

## Validation exécutée

Répertoire de build neuf : /home/oper/tmp/build-tui3-revalidation.

La première configuration sans cache local a attendu FetchContent ; elle a été interrompue sans modifier le code. La configuration propre a ensuite utilisé le cache local vérifié nlohmann/json 3.11.3 :

    cmake -S /home/oper/projects/worktrees/local-agent-cpp-tui3 -B /home/oper/tmp/build-tui3-revalidation -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
    cmake --build /home/oper/tmp/build-tui3-revalidation -j2
    ./tests/agent_tests --filter=tui3
    ctest --test-dir /home/oper/tmp/build-tui3-revalidation --output-on-failure
    ./tests/agent_tests

Après la correction de formatage :
- Configuration CMake réussie ; GCC 14.2, libcurl 8.14.1, ncursesw détecté.
- Build complet puis build incrémental réussis.
- TUI-3 : 7 cas, 32 assertions, zéro échec.
- CTest : 11/11 groupes réussis.
- Harnais : 193 cas, 1 228 assertions, zéro échec.
- Scénarios : refus sans TTY, restauration RAII après exception contrôlée, scroll ancré, remplacement de réponse partielle, vrai Agent::send via mock HTTP/PTY, saturation, resize 10×4, neutralisation OSC et erreur HTTP.
- git diff --check réussi ; vérification équivalente des douze fichiers non suivis : zéro diagnostic.

Les scénarios PTY ne sont pas des essais sur terminal physique ou SSH. L’avertissement CMake CMP0135 est non bloquant.

## Verdict

VALIDATION TECHNIQUE RÉUSSIE pour TUI-3 restaurée. Worktree persistant isolé, détaché au SHA de base ; la branche existante reste bloquée par les métadonnées historiques. Aucun commit, push ou merge.
