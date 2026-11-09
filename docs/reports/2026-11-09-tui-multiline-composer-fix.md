# Correction du composeur multiligne TUI

## Référence et périmètre

- SHA demandé comme référence : `617e9dd56d39d13a145bdb44fcc2b28153da8f87`.
- `origin/main` vérifié avant le travail : `1c259dca6fc9bbf8b35ab663e4fa1b8da288baa3`.
- Le worktree de correction a été créé depuis ce `origin/main`. L'écart avec le SHA demandé ne contenait que six changements documentaires, dont les rapports associés à la PR #37; aucun changement de code produit.
- Branche : `fix/tui-multiline-composer`.
- Les changements sont limités à l'affichage/navigation du composeur, leurs tests, et cet index/rapport.
- Aucun changement au protocole LLM, à `write_file`, à son autorisation, au sandbox ou à Landlock.

## Cause vérifiée

Le texte du composeur était déjà conservé intégralement dans `TuiEventLoop::input_`; le curseur était un offset d'octets UTF-8, et F9 déplaçait la chaîne complète vers `AgentWorker::submit()`. Le défaut ne venait donc pas d'une troncature du prompt logique.

Le rendu précédent ne découpait que sur les retours à la ligne physiques, n'affichait qu'un nombre fixe de lignes et calculait la position verticale du curseur à partir du nombre de `\n`. ncurses effectuait ensuite le wrapping visuel indépendamment de ce calcul. Le curseur pouvait ainsi se retrouver hors de la zone du composeur, les lignes supérieures ou finales être invisibles et les lignes longues déborder visuellement. Les flèches haut/bas ne naviguaient pas entre les lignes visuelles.

Avant correction, le test PTY à grille ncurses reproduisait trois écarts : le curseur n'était pas dans le composeur à la position testée, la première ligne attendue n'était pas visible dans le viewport, et la fin du message ne se trouvait pas sur la ligne du curseur. Le test d'intégration de F9 vérifie séparément les octets logiques envoyés.

## Correction

`TuiView` construit maintenant une disposition du composeur en lignes visuelles, avec largeur calculée en cellules terminal (`wcwidth`) et correspondance entre frontières d'octets UTF-8 et coordonnées visuelles. Le viewport vertical se déplace pour garder le curseur visible, et le dessin reste limité aux lignes et colonnes de la fenêtre du composeur.

Dans le composeur, haut/bas déplacent le curseur entre lignes visuelles en conservant la colonne souhaitée; Home/End vont au début/à la fin de la ligne visuelle courante. Les insertions, suppressions et mouvements horizontaux réinitialisent cette colonne préférée. Le recalcul à chaque dessin utilise les dimensions courantes, donc le redimensionnement réorganise l'affichage sans modifier `input_`.

Le test PTY envoie rapidement le prompt exact fourni dans la reproduction, y compris les lignes vides, effectue Home/End, redimensionne le pseudo-terminal, envoie F9 et compare le champ `messages[0].content` de la requête mockée au texte d'origine, caractère pour caractère. Il ne déclenche aucun outil d'écriture.

Un contrôle a aussi révélé que le test unitaire de largeur UTF-8 dépendait du locale global alors qu'il appelle `TuiView` sans créer de `TerminalSession`. Le test fixe désormais explicitement `C.UTF-8` pendant son exécution puis restaure le locale antérieur. La vraie session TUI initialise déjà `LC_CTYPE` dans `TerminalSession`.

## Fichiers concernés

- `include/ui/tui_view.hpp`
- `src/ui/tui_view.cpp`
- `include/ui/tui_event_loop.hpp`
- `src/ui/tui_event_loop.cpp`
- `tests/tui_test.cpp`
- `docs/README.md`
- `docs/reports/2026-11-09-tui-multiline-composer-fix.md`

## Validation exécutée

Répertoire de build propre de la revue finale : `/tmp/local-agent-cpp-tui-multiline-review-build` (la validation initiale avait aussi utilisé `/tmp/local-agent-cpp-tui-composer-build`).

- Configuration CMake et compilation complète depuis le répertoire neuf : réussies. Le premier téléchargement de nlohmann/json a échoué en sandbox faute de résolution DNS de `github.com`; après autorisation réseau, CMake a téléchargé la dépendance épinglée et la configuration ainsi que le build complet ont réussi.
- TUI ciblé : `agent_tests --filter=tui3` — **29 cas, 392 assertions, zéro échec**.
- Harnais complet : **262 cas, 8 104 assertions, zéro échec**.
- CTest : **13/13 groupes réussis** (36,07 s).
- `git diff --check` : réussi.
- Les tests qui ouvrent le serveur HTTP mock loopback ont été exécutés avec l'autorisation réseau locale nécessaire. Une tentative sandboxée préalable a échoué à `socket()` et n'est pas comptée comme une réussite; le rejeu autorisé a passé.

Les résultats sont des tests automatisés, principalement sur pseudo-terminal. Aucun essai manuel Ghostty ou terminal physique n'a été exécuté dans cette intervention.

## Revue finale et recette Debian 13 / `versutus-ia`

Revue du diff effectuée après vérification de `origin/main` : le SHA courant reste `1c259dca6fc9bbf8b35ab663e4fa1b8da288baa3`, fusion de la PR #38. Le worktree du correctif est sur `fix/tui-multiline-composer`, sans changement concurrent; le checkout UI-4 conserve ses modifications locales. Les autres worktrees n'ont pas été modifiés.

La revue confirme que le prompt logique est une chaîne unique indépendante du viewport; seules ses lignes visuelles sont recalculées. Les tests couvrent lignes vides, wrapping, Home/End, déplacement vertical, terminal étroit, resize et comparaison exacte du contenu envoyé au mock par F9. Le dialogue d'approbation existant conserve ses tests PTY (saisie, confirmation explicite, fermeture, expiration); aucun code d'autorisation n'a changé.

Pour rejouer sur Debian 13 sans toucher aux autres projets, transférer uniquement une archive source du worktree isolé. Sur la machine de développement :

```sh
SRC=/home/oper/projects/worktrees/local-agent-cpp-tui-multiline-composer
tar -C "$SRC" --exclude='./.git' --exclude='./.env' \
  --exclude='./build' --exclude='./build-*' \
  -czf /tmp/local-agent-cpp-tui-multiline-review.tar.gz .
scp /tmp/local-agent-cpp-tui-multiline-review.tar.gz versutus-ia:/tmp/
```

Sur `versutus-ia`, installer au besoin les prérequis Debian (`build-essential`, `cmake`, `pkg-config`, `libcurl4-openssl-dev`, `libncurses-dev`; la configuration CMake récupère nlohmann/json depuis sa source déclarée), puis extraire et compiler dans un répertoire temporaire :

```sh
mkdir -p /tmp/local-agent-cpp-tui-multiline-review/src
tar -xzf /tmp/local-agent-cpp-tui-multiline-review.tar.gz \
  -C /tmp/local-agent-cpp-tui-multiline-review/src
cd /tmp/local-agent-cpp-tui-multiline-review/src
cmake -S . -B build-review -DAGENT_BUILD_TESTS=ON
cmake --build build-review
```

Pour lancer la recette réelle, charger sans afficher le `.env` existant du dépôt (adapter `REPO_CONFIG` au chemin réel sur cette machine), puis utiliser un répertoire temporaire neuf comme cwd. Le programme crée son `workspace/` sous ce cwd :

```sh
REPO_CONFIG=/chemin/vers/le/depot/local-agent-cpp
set -a
. "$REPO_CONFIG/.env"
set +a
BIN=/tmp/local-agent-cpp-tui-multiline-review/src/build-review/agent
APP_TEST_ROOT=$(mktemp -d /tmp/local-agent-cpp-tui-composer-approval.XXXXXX)
cd "$APP_TEST_ROOT"
"$BIN" --ui=tui
```

Dans Ghostty, vérifier F1–F6, coller le prompt de reproduction (avec les lignes vides), déplacer le curseur par flèches/Home/End, redimensionner puis envoyer avec F9. Pour le contrôle d'écriture, employer un workspace temporaire neuf et demander un fichier de test dédié; inspecter le contenu intégral de la demande et ne confirmer que le défi courant après inspection. Quitter avec F10. Après la sortie, depuis un shell :

```sh
find "$APP_TEST_ROOT/workspace" -maxdepth 3 -type f -print
cat "$APP_TEST_ROOT/workspace/nom-du-fichier-de-test"
```

Répéter le scénario de refus dans un autre répertoire temporaire indépendant; répondre `no` puis Entrée, puis vérifier qu'aucun fichier et aucun répertoire parent demandé n'existent. Ne pas réutiliser le workspace d'approbation. Enfin, supprimer manuellement les deux répertoires temporaires uniquement après vérification et seulement s'ils contiennent exclusivement les artefacts de cette recette. Les tests automatisés prouvent le contenu exact du prompt dans la requête mockée; l'essai Ghostty réel vérifie l'interaction et le résultat filesystem, mais ne capture pas indépendamment le payload HTTP.

Ces essais sur `versutus-ia` restent **À EXÉCUTER**. Aucun terminal réel ni serveur LLM n'a été utilisé ici.

## Limites

- Le test PTY confirme le contenu exact transmis et le rendu/cursor sur grille ncurses pour les dimensions essayées; il ne garantit pas le comportement de tous les émulateurs ou de toutes les locales.
- La largeur Unicode s'appuie sur la locale active et `wcwidth`; les séquences invalides suivent un traitement de remplacement par octet, et la conformité Unicode universelle n'est pas revendiquée.
- Aucun plafond de longueur du brouillon n'est ajouté; le stockage logique reste proportionnel au texte saisi.

## Verdict

**PRÊT POUR PUBLICATION**, sous réserve de la recette manuelle Debian 13/Ghostty avant de revendiquer une validation sur terminal réel. La configuration et le build propres, les tests TUI, le harnais complet, CTest et les contrôles documentaires passent. Le défaut de viewport/cursor est reproduit avant correction puis couvert par des tests ciblés. La chaîne logique du prompt est inchangée et le test PTY confirme sa transmission exacte après navigation et redimensionnement. Aucun commit, push, PR ou merge n'a été effectué.
