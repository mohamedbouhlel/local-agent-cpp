# Revue — fraîcheur du consentement `write_file`

**Date :** 2026-11-08
**Dépôt / branche :** `local-agent-cpp`, `feat/write-file-explicit-approval`
**Base testée :** `8fa77cd9f8b56fd8091ecdccfd3a0148f66325bb`
**État :** modifications locales non commitées. Aucun commit, push, PR ou merge.

## Objet et conclusion

Cette revue traite les deux défauts de consentement anticipé relevés dans le premier rapport : une saisie CLI partielle commencée avant la demande et une touche `y` TUI déjà mise en tampon. Le verdict `NON PRÊT` du complément précédent décrivait correctement l'état antérieur; le présent rapport enregistre le correctif et les preuves ajoutées.

Le provider exige désormais une confirmation exacte propre à la demande : `yes <défi courant>`. Un `yes` ou `y` seul, un défi erroné ou celui d'une demande antérieure ne peut pas approuver. Le résultat ne permet une écriture qu'après le passage de la demande d'autorisation existante; la décision de refus n'appelle pas le writer.

## Mécanisme observé dans le code

`generate_approval_challenge()` obtient 16 octets via Linux `getrandom()` avec `GRND_NONBLOCK`, puis produit 32 caractères hexadécimaux (128 bits). Il n'existe pas de repli prévisible : une erreur d'entropie conduit à `UNAVAILABLE`. Le défi est généré séparément pour chaque présentation. `matches_approval_confirmation()` n'accepte que la chaîne complète `yes ` suivie exactement des 32 caractères, sans suffixe; la comparaison du défi inspecte tous les caractères.

Dans la CLI, les détails du workspace, du chemin, des répertoires parents, de la taille et du contenu échappé sont écrits puis vidés avant l'affichage du défi. Les contrôles de disponibilité d'entrée par `poll(..., 0)` restent en place avant et après le rendu des détails. Une entrée complète déjà en attente rend la demande indisponible sans consommer les octets, afin de préserver l'entrée du REPL. La réponse est ensuite lue sur le descripteur TTY sans prélecture par `std::istream`.

Dans la TUI, le défi est généré lorsque la boucle UI prend en charge la demande du broker. Le texte de confirmation est un buffer local au dialogue et est effacé lorsque celui-ci se ferme. `y` n'est qu'un caractère; seule la saisie complète du défi courant suivie d'Enter approuve. Le broker continue de valider l'identifiant et l'échéance. Le défi n'est ajouté ni aux événements d'agent ni aux diagnostics. Le dialogue refuse l'approbation si l'écran n'offre pas au moins 6 lignes et 48 colonnes, afin que la confirmation soit affichable; l'inspection du contenu reste défilable.

Les objets d'autorisation et le writer n'ont pas été refondus dans cette correction. L'objet `PreparedWrite` existant reste non copiable et consommable une fois; le writer reçoit les arguments prévalidés qu'il contient et refait ses validations filesystem. Les événements de télémétrie ne reçoivent pas le contenu proposé ni le défi.

## Termios, REPL et buffers ncurses

- En mode canonique `termios`, les caractères d'une ligne incomplète peuvent rester dans le buffer de ligne du noyau jusqu'à Enter. Un `poll()` à délai nul n'établit donc pas la date de saisie de chacun des caractères. La sécurité contre le cas testé ne dépend pas de cette datation : le préfixe `yes` commencé avant la demande, complété par Enter après affichage mais sans le défi courant, ne satisfait pas le comparateur exact.
- En mode non canonique, les octets peuvent être disponibles à mesure de la frappe. La CLI ne change pas les attributs `termios`; la règle du défi reste identique.
- `FdInputStreamBuffer` lit le flux sans prélecture C++ qui déplacerait des octets entre `std::cin` et `read(2)`. Il n'attribue pas une provenance temporelle aux données fournies par le noyau. Une ligne complète trouvée lors des contrôles CLI est préservée pour `PromptReader`.
- ncurses utilise le mode cbreak/noecho de la session TUI et `wget_wch()` peut consommer des touches déjà en attente. Aucun `flushinp()`/`tcflush()` n'est utilisé comme preuve de fraîcheur. Une ancienne touche `y` est simplement le préfixe d'une saisie neuve et ne peut correspondre au défi aléatoire complet. Un ancien défi n'est pas égal au défi du nouveau dialogue.

Cette garantie vise le type-ahead accidentel, les préfixes partiels et la réutilisation d'une réponse précédente. Elle ne protège pas contre un processus du même compte qui peut observer le défi après son affichage puis injecter la réponse exacte, contre un terminal compromis, un keylogger ou une personne qui retranscrit le défi courant. La fraîcheur n'est donc pas une preuve cryptographique de l'identité de l'utilisateur ni une protection contre un terminal hostile. Une collision aléatoire est négligeable en pratique mais pas mathématiquement impossible.

## Scénarios et preuves filesystem

Les tests utilisent le vrai authorizer et le writer pour les parcours CLI, avec PTY et répertoire temporaire. Les refus pertinents contrôlent l'absence des répertoires parents, pas seulement le statut retourné.

| Scénario | Résultat observé |
|---|---|
| Ligne `yes` complète présente avant la demande | `UNAVAILABLE`; aucune création; les octets restent lisibles par le REPL |
| Préfixe canonique `yes` avant demande, Enter après affichage | Refus; aucun répertoire/fichier |
| Défi courant correctement saisi après présentation | Approbation; seul le chemin/contenu de la demande courante est créé |
| Défi précédent présenté à la demande suivante | Refus; aucun répertoire/fichier pour la nouvelle cible |
| Défi courant incorrect | Refus |
| Deux demandes successives | Défis distincts; une confirmation précédente ne satisfait pas la suivante |
| Réponse après expiration | `EXPIRED`; aucun répertoire/fichier |
| EOF / fermeture de la TUI pendant le dialogue | Annulation; aucune création |
| Touche `y` seule, y compris envoyée avec F9 dans le scénario PTY | Refus; aucun répertoire/fichier |
| Terminal TUI trop petit pour inspecter et afficher la confirmation | `UNAVAILABLE`; aucune création |
| Défi courant en TUI | Écriture réelle uniquement après confirmation complète; contenu vérifié sur disque |

Le test PTY TUI envoie F9 et `y` ensemble dans le scénario de saisie prébufferisée et vérifie l'absence de création. L'ordonnancement d'un PTY ne permet pas d'affirmer que le caractère était précisément dans le buffer interne ncurses à l'instant de l'ouverture du dialogue. La preuve combinée repose aussi sur le traitement de la boucle : `y` seul ne valide rien, et le buffer de confirmation est propre à la demande.

Les tests d'expiration utilisent des échéances courtes ou déjà dépassées; le délai maximal configuré de cinq minutes n'a pas été attendu en temps réel. La réponse tardive après expiration est vérifiée comme non créatrice.

## Modifications liées à ce durcissement

- `include/workspace_write_authorization.hpp`, `src/workspace_write_authorization.cpp` : contrat du défi aléatoire et validation exacte; provider CLI.
- `include/ui/tui_event_loop.hpp`, `src/ui/tui_event_loop.cpp`, `include/ui/tui_view.hpp`, `src/ui/tui_view.cpp` : défi et saisie propres à la demande, rendu et seuil d'inspectabilité.
- `tests/write_authorization_test.cpp`, `tests/tui_test.cpp` : scénarios CLI/TUI et vérifications filesystem.
- Le présent rapport complète le rapport initial `2026-11-08-write-approval-implementation.md`; la conclusion antérieure y est conservée comme historique.

## Validation réellement exécutée

Configuration et build propre :

```text
cmake -S . -B /tmp/local-agent-cpp-write-approval-challenge-clean -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-ghostty-qualification-build/_deps/json-src
cmake --build /tmp/local-agent-cpp-write-approval-challenge-clean -j2
```

Résultats après le dernier changement fonctionnel :

- `agent_tests --filter=workspace` : 48 cas, 206 assertions, zéro échec.
- `agent_tests --filter=tui3` : 19 cas, 91 assertions, zéro échec.
- CTest : 13/13 groupes réussis.
- Harnais complet : **247 cas, 7 656 assertions, zéro échec**.
- Les tests de transport loopback ont été exécutés après autorisation de l'accès socket; une première exécution dans le bac à sable avait échoué sur `mock: socket() failed`.
- Le build CMake a émis un avertissement CMP0135 existant et le build un avertissement jobserver; aucun n'a fait échouer la compilation.
- Le contrôle final `git diff --check` et `git diff --cached --check` est consigné dans la restitution de mission.

Ces résultats ne sont pas une qualification manuelle Ghostty. Aucun benchmark ni test contre un serveur LLM externe n'a été exécuté dans cette revue.

## Verdict

**PRÊT POUR PUBLICATION**, dans le périmètre de cette correction. Les réponses anticipées testées, réponses partielles, anciennes confirmations, réponses incorrectes, expiration, fermeture et petit terminal ne permettent pas de créer le fichier; la confirmation exacte courante permet l'écriture prévue. Les validations complètes passent. Le risque résiduel est l'injection de la confirmation courante par un acteur ayant accès au terminal après son affichage, ainsi que les limites de fidélité temporelle des tests PTY décrites ci-dessus.

Aucun commit, push, pull request ou merge n'a été effectué. Les changements restent à la revue de publication. Le checkout UI-4 n'a pas été modifié.
