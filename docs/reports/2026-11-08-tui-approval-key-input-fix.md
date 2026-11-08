# Correctif de saisie du dialogue d'approbation TUI

## Référence et périmètre

- Base examinée : `81964be88279754fae11980c4c46f2a2c8ee37fd`.
- Branche de travail : `fix/tui-approval-key-input`.
- Worktree isolé : `/home/oper/projects/worktrees/local-agent-cpp-tui-approval-keyfix`.
- Objet : conserver les frappes du dialogue `write_file` jusqu'à validation explicite et empêcher leur fuite vers le champ de saisie de la conversation.
- Aucun changement du writer, de `PreparedWrite`, du contrôle de confinement, de Landlock ou de la politique d'autorisation.

## Reproduction et cause

La reproduction PTY ajoutée avant le correctif tapait `n`, puis `o` séparément. Le gestionnaire appelait `resolve(...denied)` dès la première touche `n`; le dialogue se fermait donc. La touche suivante `o` était traitée par la boucle normale et atteignait le composeur. Le scénario témoin constatait que `Input: no` n'était pas visible dans le dialogue et qu'une requête supplémentaire pouvait être déclenchée par la saisie contaminée.

Cause démontrée dans `src/ui/tui_event_loop.cpp` : le raccourci `n`/`N` refusait immédiatement, sans attendre Entrée. Cela contredisait l'interface de confirmation par défi complet et rendait possible le transfert de touches ultérieures au composeur.

## Correction

- La saisie imprimable reste dans le dialogue et n'est décidée qu'à Entrée. Seule la comparaison exacte avec `yes <défi courant>` autorise; toute autre chaîne soumise refuse. Échap et F10 gardent leur sémantique d'annulation/fermeture.
- Le broker mémorise l'identifiant de la demande expirée. La TUI affiche alors un état d'expiration sans défi actif et consomme les touches jusqu'à fermeture explicite du dialogue; aucune confirmation tardive ne peut réactiver l'approbation.
- Les tests PTY exercent la saisie caractère par caractère, le collage rapide, `no`, l'approbation exacte, l'incomplétude suivie de F10 et l'expiration pendant une saisie partielle. Les scénarios refusés vérifient l'absence de fichier et de répertoire parent ainsi que l'absence de requête parasite.

## Fichiers modifiés

- `include/ui/tui_approval_broker.hpp`
- `include/ui/tui_event_loop.hpp`
- `include/ui/tui_view.hpp`
- `src/ui/tui_approval_broker.cpp`
- `src/ui/tui_event_loop.cpp`
- `src/ui/tui_view.cpp`
- `tests/tui_pty_driver.cpp`
- `tests/tui_test.cpp`
- `docs/README.md`
- `docs/reports/2026-11-08-tui-approval-key-input-fix.md`

## Validation exécutée

Build propre configuré dans `/tmp/local-agent-cpp-tui-approval-keyfix-build` avec `AGENT_BUILD_TESTS=ON`, puis reconstruit après les changements. La première configuration sans accès réseau n'a pas pu récupérer nlohmann/json; la configuration autorisée ensuite a réussi.

- `agent_tests --filter=tui3` : 25 cas, 353 assertions, zéro échec.
- Harnais complet `agent_tests` : 258 cas, 8 065 assertions, zéro échec.
- CTest : 13/13 groupes réussis.
- `git diff --check` : réussi après ajout du rapport et de son entrée d'index.

La reproduction pré-correctif échouait : le dialogue ne conservait pas `no` et le scénario observait une troisième requête au lieu des deux attendues. Un run ciblé après correction a d'abord échoué parce que le helper inspectait la sortie PTY avant d'avoir drainé les octets; la synchronisation de capture du test a été ajustée, puis le test ciblé a réussi. Ce problème de capture n'a pas nécessité de changement de production.

## Limites et verdict

Les tests PTY vérifient le comportement ncurses et les effets filesystem simulés du harness; aucune nouvelle qualification manuelle sur Ghostty n'a été réalisée. Une entrée incorrecte ne peut pas autoriser une création, mais elle refuse la demande lorsqu'elle est soumise. Le test d'expiration utilise un outil PTY réservé au harness avec une échéance courte; le délai de production n'a pas été modifié.

Verdict avant publication : **PRÊT POUR REVUE**. La publication Git est traitée séparément; aucun merge n'est autorisé par ce rapport.
