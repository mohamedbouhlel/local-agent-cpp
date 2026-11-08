# Autorisation explicite de `write_file` — implémentation

**Date :** 2026-11-08
**Branche :** `feat/write-file-explicit-approval`
**SHA de base :** `8fa77cd9f8b56fd8091ecdccfd3a0148f66325bb`
**Worktree :** `/home/oper/projects/worktrees/local-agent-cpp-write-approval`
**Statut :** implémentation non publiée, en attente de revue.

## Objectif

Exiger une décision explicite pour chaque création `write_file`, dans la CLI classique et la TUI, sans modifier Landlock, la validation du writer ni le refus d'écrasement.

La conception confrontée à cette implémentation est `2026-11-08-write-approval-architecture.md`, lue depuis le worktree documentaire séparé `/home/oper/projects/worktrees/local-agent-cpp-write-approval-design`. Elle n'est pas présente dans cette branche de code. Le SHA de base local et distant observé au début de l'implémentation est le SHA indiqué ci-dessus.

## Modifications réalisées

- Ajout de `ApprovalProvider`, de demandes structurées et d'un résultat explicite (`approved`, `denied`, `cancelled`, `unavailable`, `expired`) dans `include/workspace_write_authorization.hpp`.
- Ajout d'un préflight sans création de répertoires : workspace canonique, chemin relatif confiné, absence de cible existante, taille maximale conservée à 200 000 octets, liste des parents manquants et contenu exact dans la demande.
- Ajout de `PreparedWrite`, non copiable et consommable une seule fois. `write_workspace` n'accepte désormais que ce type et réexécute les vérifications d'ouverture, de symlink et de non-écrasement au moment de publier.
- Le registre prépare les outils avant exécution. `write_file` ne peut atteindre le writer par le chemin normal du registre qu'après obtention d'un `PreparedWrite`; l'absence de provider échoue en mode fermé. Les outils de lecture restent sans approbation.
- La CLI exige un terminal interactif pour l'entrée et le diagnostic, affiche le contenu dans une représentation ASCII échappée, et n'accepte que la ligne exacte `yes`. Réponse vide ou autre ligne : refus; EOF : annulation; délai : expiration. Toute décision retournée après l'échéance est reclassée `expired`.
- Le REPL classique utilise `FdInputStreamBuffer`, qui lit un octet par appel depuis stdin. Cela évite que `std::istream` pré-lise une ligne d'approbation déjà tapée lorsque le provider attend sur le même descripteur avec `poll()`.
- La TUI utilise un `TuiApprovalBroker` séparé du canal de télémétrie. Le worker attend sur le broker, tandis que le thread ncurses continue de traiter le clavier, le rendu et le resize. Les identifiants périmés sont rejetés. F10/fermeture annule l'attente; aucune annulation de `Agent::send()` n'est ajoutée.
- La boîte de dialogue expose le chemin, la taille, les parents à créer et le contenu échappé, navigable par défilement. Sous 6 lignes ou 20 colonnes, `y` ne peut pas approuver et retourne `unavailable`.
- Ajout d'événements de demande/résolution d'autorisation contenant seulement le nom de l'outil, le statut et la corrélation existante. Aucun contenu, argument ou secret de fichier n'est inclus. Les refus ne génèrent ni `ToolStarted`, ni `ToolFailed`, ni faux résultat de writer; un résultat `role=tool` explicite est transmis au modèle.
- Le TUI conserve une notice visible en conversation pour les refus, annulations, expirations et indisponibilités, en plus du diagnostic consultable.

## Fichiers concernés

Les fichiers de production modifiés sont `CMakeLists.txt`, `include/tool.hpp`, `include/tool_registry.hpp`, `include/prompt_reader.hpp`, `include/ui/agent_event.hpp`, `include/ui/presentation_state.hpp`, `include/ui/tui_application.hpp`, `include/ui/tui_event_loop.hpp`, `include/ui/tui_view.hpp`, `include/workspace_tools.hpp`, `include/workspace_write.hpp`, `include/workspace_write_authorization.hpp`, `include/ui/tui_approval_broker.hpp`, `src/agent.cpp`, `src/main.cpp`, `src/prompt_reader.cpp`, `src/tool_registry.cpp`, `src/ui/agent_worker.cpp`, `src/ui/presentation_controller.cpp`, `src/ui/presentation_state.cpp`, `src/ui/tui_application.cpp`, `src/ui/tui_event_loop.cpp`, `src/ui/tui_view.cpp`, `src/ui/tui_approval_broker.cpp`, `src/workspace_tools.cpp`, `src/workspace_write.cpp` et `src/workspace_write_authorization.cpp`.

Les tests modifiés ou ajoutés sont `tests/CMakeLists.txt`, `tests/agent_e2e_test.cpp`, `tests/agent_streaming_test.cpp`, `tests/filesystem_sandbox_test.cpp`, `tests/tui_pty_driver.cpp`, `tests/tui_test.cpp`, `tests/workspace_tools_test.cpp`, `tests/test_approval_provider.hpp` et `tests/write_authorization_test.cpp`.

Ce rapport et `docs/README.md` sont les seuls changements documentaires. Aucun changement au protocole SSE, à Landlock, aux règles d'outils de lecture, au contrat REPL ou à la politique `overwrite:true` n'a été intentionnellement apporté.

## Vérifications exécutées

Configuration propre, utilisant la copie locale déjà disponible de nlohmann/json afin d'éviter un téléchargement réseau :

```text
cmake -S . -B /tmp/local-agent-cpp-write-approval-clean -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-ghostty-qualification-build/_deps/json-src
cmake --build /tmp/local-agent-cpp-write-approval-clean -j2
cmake --build /tmp/local-agent-cpp-write-approval-clean --clean-first -j2
```

La configuration et la compilation propre ont réussi. CMake a émis son avertissement de politique `CMP0135` déjà associé à `FetchContent`; la compilation a aussi affiché une collision de nom de FIFO `GMfifo3` du jobserver avant de poursuivre. Les cibles `agent`, `agent_core`, `agent_tui`, `agent_tests` et `tui_pty_driver` ont été construites.

Après les derniers changements, commandes et résultats observés :

| Vérification | Résultat |
| --- | --- |
| `agent_tests --filter=workspace` | 44 cas exécutés, 151 assertions, zéro échec |
| `agent_tests --filter=agent` | 36 cas exécutés, 621 assertions, zéro échec |
| `agent_tests --filter=tui3` | 16 cas exécutés, 73 assertions, zéro échec; inclut approbation PTY, fermeture pendant approbation et resize à 10×4 |
| `cmake --build /tmp/local-agent-cpp-write-approval-clean --clean-first -j2` | rebuild complet réussi sur la version finale |
| `ctest --test-dir /tmp/local-agent-cpp-write-approval-clean --output-on-failure` | 13/13 groupes réussis |
| `/tmp/local-agent-cpp-write-approval-clean/tests/agent_tests` | 240/240 cas, 7 583 assertions, zéro échec |
| `git diff --check` | réussi |

Les sockets loopback des tests mock ont échoué lors d'une première exécution du binaire dans le bac à sable (`mock: socket() failed`). Le harnais complet a ensuite été relancé avec l'autorisation loopback et a réussi. Le CTest complet a réussi dans l'environnement courant.

## Scénarios effectivement vérifiés

- Approbation enregistre le contenu exact, crée les parents seulement après décision et publie le fichier attendu.
- Refus, annulation, indisponibilité et expiration n'entraînent pas de création.
- Provider absent et préflight invalide échouent sans création de répertoire.
- Une cible apparue après approbation n'est pas écrasée.
- Une approbation est demandée à nouveau pour un appel distinct; le refus du second appel ne réutilise pas le premier.
- Un `PreparedWrite` ne peut être copié et ne peut être consommé deux fois.
- La lecture ne sollicite pas le provider.
- CLI PTY : seule la ligne exacte `yes` approuve; `YES` est refusé; provider avec des descripteurs non interactifs retourne `unavailable`.
- Le PTY écrit à la fois la ligne de prompt et `yes` avant la lecture; `PromptReader` consomme le prompt et le provider lit ensuite l'approbation restante sur le même terminal.
- Agent/mock HTTP : un refus produit le résultat de refus associé au bon `tool_call_id`, puis le modèle peut continuer; aucun fichier ni parent n'est créé, aucun callback historique de résultat d'exécution n'est annoncé, et les événements n'affichent aucun démarrage d'outil.
- TUI PTY : le dialogue affiche le contenu d'inspection, `y` autorise la création réelle, F10 pendant une demande annule et ne crée rien, et un terminal réduit à 10×4 refuse l'approbation.
- La suite TUI six vues, les tests du worker, la CLI classique, le sandbox et le writer atomique restent verts dans CTest.

## Corrections issues des essais

1. La première version du test de fermeture attendait la chaîne « no file was created » dans l'écran de conversation. Le résultat existait en Diagnostics, mais la notice n'était pas nécessairement visible dans la vue Conversation. Le TUI ajoute maintenant aussi une notice non sensible en conversation pour les issues non approuvées; le test de fermeture PTY correspondant passe.
2. Le premier test compact lançait directement ncurses en 10×4 et n'atteignait pas le dialogue. Il a été remplacé par un scénario valide : démarrage normal, ouverture du dialogue, resize à 10×4, puis tentative `y`. Le refus `unavailable` et l'absence de création sont vérifiés.
3. Une simulation de saisie anticipée a montré que mélanger `std::cin` (qui peut pré-lire) et `read(STDIN_FILENO)` n'offre pas une livraison cohérente de la ligne suivante. Le chemin CLI de production utilise donc maintenant le streambuf sans prélecture; le test PTY de partage du terminal passe.

## Limites et risques résiduels

- Les tests de CLI/TUI sont fondés sur des pseudo-terminaux, pas sur une qualification manuelle Ghostty.
- Le timeout opérationnel complet de cinq minutes n'a pas été attendu pendant les tests; les voies d'expiration sont couvertes avec des échéances déjà passées et les décisions simulées.
- La fermeture TUI annule la demande d'approbation, mais `Agent::send()` n'est pas annulable. Le worker peut poursuivre le tour et effectuer des requêtes HTTP de continuation avant de terminer; la TUI attend sa fin. Aucune écriture non approuvée n'est permise pendant cette continuation.
- Le contenu fait partie de la demande de contrôle pour permettre son inspection; il n'est pas placé dans les événements de télémétrie. La mémoire temporaire contient donc le contenu du fichier pendant le tour.
- L'approbation porte sur le chemin et le contenu préflightés. Le writer revalide les composants et refuse une cible devenue existante; comme auparavant, un autre processus du même compte n'est pas isolé par Landlock.
- L'issue du writer après approbation reste un événement d'exécution normal; un refus n'est pas classé comme échec sandbox ou échec de writer.

## État Git et périmètre

Le HEAD du worktree demeure le SHA de base; aucun commit, push, PR ou merge n'a été effectué. Le checkout principal UI-4 a été contrôlé avant et après et conserve son état local préexistant. Le répertoire `build-approval/` généré lors des premiers essais demeure non suivi dans le worktree; le build propre final se trouve sous `/tmp` et ce répertoire n'appartient pas au diff de source.

## Verdict

**PRÊT POUR REVUE**, sous réserve d'une revue contradictoire du diff. Les scénarios principaux d'approbation/refus, le chemin Agent réel avec serveur mock, les interactions PTY des deux interfaces, la fermeture TUI, le refus en terminal compact, le build propre et les 13 groupes CTest ont été exécutés avec succès. Aucun résultat ne constitue une qualification manuelle sur Ghostty ni une preuve de sécurité absolue contre les processus externes du même compte.

## Complément — revue de sécurité de la saisie anticipée

**Date :** 2026-11-08
**SHA testé :** `8fa77cd9f8b56fd8091ecdccfd3a0148f66325bb` (modifications locales non commitées sur cette base).

### Constat confirmé

La revue a confirmé un défaut bloquant dans le comportement décrit plus haut : un `yes` déjà en attente sur le TTY pouvait être lu par `ConsoleWriteApprovalProvider` après qu'il ait affiché une demande ultérieure. Le test initial `cli_provider_consumes_approval_already_buffered_by_prompt_reader` avait démontré cette acceptation; il ne prouvait pas la sécurité du consentement et son résultat avait été interprété à tort comme une propriété utile.

Une première correction par `tcflush(TCIFLUSH)` a été rejetée pendant cette revue : elle éliminait aussi une saisie REPL légitime tapée en avance. Elle n'a pas été conservée.

### Correction appliquée

Le provider CLI vérifie maintenant sans consommer l'entrée TTY :

1. avant d'afficher la demande;
2. après l'affichage complet et le flush du workspace, du chemin, des parents et du contenu, avant d'armer l'invite de décision.

Si des octets sont en attente à l'un de ces points, ou si `poll()` échoue, la décision est `unavailable` et l'écriture est refusée. Les octets ne sont ni vidés ni lus par le provider et restent disponibles au `PromptReader`. Seul un `yes` arrivé après l'affichage de l'invite de décision peut approuver. La demande approuvée, le writer, le protocole TUI, Landlock et les événements ne changent pas.

Tests PTY ajustés/ajoutés dans `tests/write_authorization_test.cpp` :

- `yes` préchargé avant la demande : `unavailable`, sans présentation ni approbation;
- même `yes` encore lisible ensuite comme ligne REPL;
- `yes` envoyé après présentation synchronisée : approbation;
- deux approbations successives, chacune après sa propre présentation;
- `YES` après présentation : refus;
- entrée REPL résiduelle : refus fail-closed et préservation pour la lecture REPL;
- Ctrl-D après présentation : annulation;
- expiration sans réponse : expiration.

### Validation réexécutée

Commandes exécutées dans ce worktree :

```text
cmake --build build-approval -j2
./build-approval/tests/agent_tests --filter=workspace
./build-approval/tests/agent_tests
ctest --test-dir build-approval --output-on-failure
git diff --check
```

Résultats finaux observés après le dernier changement : build incrémental réussi; filtre `workspace` : 45 cas, 167 assertions, zéro échec; harnais complet : 241 cas, 7 599 assertions, zéro échec; CTest : 13/13 groupes réussis. `git diff --check` est passé sur le diff suivi; le scan des espaces de fin dans les deux fichiers de code n'a relevé aucun cas. Le rapport conserve ses deux espaces Markdown intentionnels pour les retours à la ligne. Le test ciblé a échoué une fois pendant l'écriture du nouveau scénario EOF parce que Ctrl-D avait été mis en file avant la demande puis correctement éliminé par la première version de correction; le scénario a été corrigé pour envoyer Ctrl-D après présentation et les validations ci-dessus ont été rejouées.

### Limite et verdict de revue

Pour la CLI, une ligne complète déjà prête lors de l'un des deux contrôles est refusée sans consommation et reste disponible au REPL. Les contrôles utilisent `poll()` à délai nul et ne donnent pas la provenance temporelle des caractères. Une saisie canonique partielle commencée avant la demande puis terminée après l'affichage n'est donc pas distinguable de façon démontrée; les tests ajoutés couvrent une ligne `yes` complète préchargée, pas ce cas partiel.

Pour la TUI, l'inspection de `TuiEventLoop::run()` et `sync_approval_dialog()` révèle un autre chemin à traiter : la boucle synchronise l'apparition du dialogue, le dessine, puis lit la prochaine touche avec `wget_wch()`. Aucune barrière de fraîcheur ne distingue une touche `y` déjà mise en file dans ncurses avant l'apparition du dialogue. Une telle touche peut ainsi être consommée par `handle_approval_key()` comme approbation. Aucun test PTY dédié n'a reproduit ce cas pendant cette revue; le chemin de contrôle est présent et l'absence de test ne l'écarte pas.

Les tests PTY démontrent les lignes CLI complètes préexistantes et l'approbation CLI après présentation synchronisée; ils ne démontrent pas la provenance des entrées partielles ni l'absence de touches TUI prébufferisées. CTest et le harnais passent, mais ne couvrent pas ces deux cas. Une conception de fraîcheur d'entrée commune, qui échoue fermé sans perdre silencieusement l'édition REPL/TUI, reste nécessaire avant publication.

**Verdict de cette revue : NON PRÊT.** Le risque de saisie TUI anticipée n'est pas fermé et la preuve CLI ne couvre pas les lignes canoniques partielles. Aucun commit, push, merge ou PR n'a été effectué. Le checkout UI-4 n'a pas été modifié.
