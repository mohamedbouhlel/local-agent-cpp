# TUI-3 — revue finale après récupération

Date : 2026-11-06
Base : 3a75dfc6b453cee07628dad65398da45de1bb1fc
Worktree : /home/oper/projects/worktrees/local-agent-cpp-tui3 (HEAD détaché)
Rapport d’implémentation conservé : docs/reports/2026-11-06-tui-3-ncursesw-integration.md

## Diff examiné et correction

Le diff des cinq fichiers suivis et des douze nouveaux fichiers a été examiné. Les 17 chemins correspondent au périmètre de restauration. Aucun fichier moteur, HTTP/SSE, Landlock ou workspace n’est modifié.

Un défaut confirmé de formatage a été trouvé dans tests/tui_test.cpp : ligne blanche excédentaire à EOF, détectée par le contrôle des fichiers non suivis. Cette seule ligne a été retirée après vérification de l’empreinte récupérée. Build, test ciblé, CTest et harnais ont tous été rejoués après ce changement. Aucun défaut fonctionnel n’a été reproduit ; aucune refactorisation n’a été faite.

## Constats contradictoires

| Contrôle | Classement | Preuve et portée |
|---|---|---|
| --ui=tui explicite, classique par défaut | Conforme | parseur dédié, test de compatibilité, diff de main.cpp |
| Propriété ncurses et cycle de vie | Conforme | terminal créé sur le thread UI ; worker après Landlock ; fermeture attend le worker |
| Restauration contrôlée | Conforme dans le scénario testé | RAII et test PTY avec exception contrôlée ; SIGKILL non couvert |
| Saturation et réponse finale | Conforme dans le scénario testé | détail incomplet signalé ; réponse finale remplace le fragment au lieu de s’y concaténer |
| HTTP, resize 10×4, OSC hostile | Conforme dans les scénarios PTY | assertions du groupe TUI-3 |
| CLI classique, Landlock, moteur et outils | Conforme par inspection du diff | aucune modification de logique agentique, règles de confinement ou write_file |
| Désynchronisation de PresentationState après perte d’événement | Risque résiduel | presentation_state_desynchronized_ reste actif pour la session ; le rendu continue depuis les événements et le résultat worker, mais la validation interne des séquences ne reprend pas. Le commentaire évoque une frontière de resynchronisation sans réinitialiser l’état. Le contrôle des pertes n’est pas masqué : submit_input() prend un nouvel instantané des compteurs à chaque tour, drain_events() compare les deltas courants et TurnCompletion porte aussi les pertes du worker. Le nouveau tour n’est donc pas marqué incomplet sur la seule base de l’ancien drapeau ; les anomalies de séquence ultérieures ne sont cependant plus validées par PresentationState. Aucun effet trompeur n’a été reproduit ; la couverture TUI-3 ne teste pas deux tours successifs après saturation. Une reprise correcte doit respecter le numéro de séquence monotone de session. |
| Terminal réel et SSH | Non vérifié | validation présente limitée aux PTY automatisés |

La réponse finale remplace les fragments affichés seulement lorsque les détails d’événements sont incomplets ; elle n’est pas concaténée aux deltas. Après un échec, le fragment reste signalé incomplet. Les contenus modèle et noms d’outils passent par le sanitizer.

## Résultats observés

- Build C++20 propre et incrémental : réussi.
- TUI-3 : 7 cas / 32 assertions, zéro échec.
- CTest : 11/11 groupes, zéro échec.
- Harnais complet : 193 cas / 1 228 assertions, zéro échec.
- git diff --check et contrôle des fichiers non suivis : zéro diagnostic.
- Tests PTY avec mock HTTP, fermeture pendant tour, saturation, resize et restauration après exception contrôlée.
- Aucun serveur LLM externe, terminal Ghostty ou SSH utilisé pendant cette validation.

Commandes détaillées et empreintes : voir docs/reports/2026-11-06-tui-3-recovery-validation.md.

## Verdict

REVUE VALIDÉE avec limites documentées. Un seul correctif de formatage a été appliqué. Le risque de validation interne non réactivée après perte reste documenté ; il ne modifie pas le chemin de rendu observé et aucune correction de logique n’a été faite hors des fichiers restaurés. La branche TUI-3 demeure associée aux métadonnées du worktree disparu ; le worktree de validation est détaché au SHA de base. Aucun commit, push ou merge.
