# LOFITERM v1.0 — paquet local Debian 13 amd64

## Résultat et portée

Un paquet `.deb` a été construit depuis les sources, puis installé et exercé dans un conteneur Debian 13 amd64 isolé. Cela valide l'installation locale sur cette cible, pas une publication publique, un dépôt APT ou d'autres systèmes.

- Commit de base `main` après fusion de la correction v1.0 : `46d653fe9e788c6410129b7224edd33d870af593` (merge PR #46).
- Branche de distribution : `packaging/lofiterm-debian13`.
- Paquet : `lofiterm_1.0.0-1_amd64.deb`.
- SHA-256 : `f8daccde655a06cc7c73f79ec9f0499dc050f43d98b8fe03018e05c7e2fc3654`.
- Taille : 258 452 octets.
- Les différences applicatives de ce worktree ne modifient que le test PTY; aucun fichier de production n'a été modifié pour l'empaquetage.

`origin/main` était égal au SHA de base au contrôle effectué. Les changements de packaging et de documentation restent locaux et non committés; aucun push, PR de packaging, tag ou publication binaire n'a été fait.

## Correctifs v1.0 clôturés avant l'empaquetage

La PR #46 a été fusionnée dans `main` au commit ci-dessus. Le test TUI `tui3.typed_no_is_retained_until_enter_without_leaking_to_composer` recherchait `Input: no` contigu dans les octets bruts du PTY. Ncurses intercale des séquences de contrôle pendant le redraw, ce qui pouvait casser la chaîne brute alors que le dialogue conservait les touches. Le test normalise ces séquences avant l'assertion et ajoute un cas de régression utilisant les octets de contrôle observés. Cette correction porte sur l'observation du test, pas sur le comportement de production.

Les appels d'outils répétés ont été confrontés au code et aux tests : `Agent::send()` ajoute l'appel assistant puis son résultat `role=tool` corrélé par `tool_call_id` avant le tour suivant. Un refus d'écriture est transmis explicitement comme refus sans création; une erreur d'exécution est renvoyée comme `tool error`; la limite existante de cinq itérations échoue explicitement (`tool call loop exceeded maximum iterations`). Les tests `agent.refus_write_file_est_transmis_au_modele_sans_creer_de_fichier` et `agent.boucle_outils_limitee_a_cinq_iterations` passent. Aucun défaut reproductible du programme dans ce chemin n'a été constaté pendant cette campagne. Aucun serveur LLM réel n'a été utilisé : le comportement de modèles locaux qui répètent un appel malgré le résultat reste non qualifié et dépendant du modèle/configuration.

## Environnement et validations automatisées

Build et essais dans un conteneur basé sur Debian GNU/Linux 13 (trixie), architecture amd64, noyau hôte `6.12.115+deb13.1-amd64` : GCC/G++ `14.2.0-19`, CMake `3.31.6`, libcurl `8.14.1`, ncursesw `6.5`, nlohmann/json `3.11.3` obtenue par CMake FetchContent.

Configuration/build propre, CTest et harnais relancés après le correctif du test PTY :

```sh
cmake -S /src -B /out/build -DCMAKE_BUILD_TYPE=Release -DAGENT_BUILD_TESTS=ON
cmake --build /out/build --parallel 2
ctest --test-dir /out/build --output-on-failure
/out/build/tests/agent_tests
CMAKE_BUILD_PARALLEL_LEVEL=2 BUILD_DIR=/out/package-build \
  OUTPUT_DIR=/out/packages-release /src/packaging/build-deb.sh
```

Résultats : CTest **14/14 groupes PASS**, durée finale 37,99 s; harnais **281/281 cas**, **8 344 assertions**, zéro échec. Le build Release distinct du paquet a réussi. Avant correction de l'assertion PTY, une exécution CTest avait échoué sur `run.dialog_kept_typed_no`; après correction, le groupe `tui3` a passé **5/5 répétitions** puis le CTest complet.

## Paquet et inspection

`dpkg-deb --info` rapporte `Package: lofiterm`, version `1.0.0-1`, architecture `amd64`; dépendances déclarées : `libc6`, `libcurl4t64`, `libgcc-s1`, `libncursesw6`, `libstdc++6`, `libtinfo6`. Le contenu inspecté est :

- `/usr/bin/lofiterm` : lanceur public et lecture de configuration.
- `/usr/lib/lofiterm/agent` : binaire technique.
- `/usr/share/doc/lofiterm/INSTALL_DEBIAN13.md`.
- `/usr/share/doc/lofiterm/lofiterm.conf.example`.
- `/usr/share/doc/lofiterm/copyright`.

Aucun `.env`, secret, clé API ou fichier privé n'est inclus. Le script de construction utilise un répertoire de staging temporaire et `dpkg-deb --build --root-owner-group`.

## Installation et essais du paquet installé

Installation par `apt-get install` dans un conteneur Debian 13 isolé ne contenant initialement que les dépendances runtime, Python 3 et strace pour la recette. Le script `packaging/test-installed-package.py`, exécuté contre le paquet final réinstallé, a réussi :

- fichier de configuration trop accessible refusé avant création du workspace;
- `read_file` lu via un serveur HTTP/SSE mock;
- `write_file` approuvé : fichier créé avec les octets exacts, et résultats d'outils vérifiés dans les échanges mock;
- `write_file` refusé : ni fichier cible ni répertoire parent créé, résultat de refus transmis au modèle mock;
- workspace XDG utilisé même depuis un CWD arbitraire;
- `--ui=tui` lancé sous PTY, puis fermeture F10 avec code 0.

Ces essais utilisent un serveur mock, pas LiteLLM réel. La TUI a été testée sous PTY, pas dans Ghostty physique pendant cette mission.

Landlock a été contrôlé sur le binaire installé avec `strace`. Les appels observés incluent `landlock_create_ruleset`, plusieurs `landlock_add_rule` et `landlock_restrict_self(...)=0`; le processus a atteint son REPL. Cela prouve l'activation dans ce noyau/conteneur seulement.

Après le smoke test, `apt-get remove lofiterm` a retiré `/usr/bin/lofiterm` et `/usr/lib/lofiterm/agent`. Des fichiers sentinelles créés dans la configuration et le workspace utilisateur du conteneur sont restés présents. Aucun test de désinstallation n'a été effectué sur l'hôte principal.

## Instructions d'utilisation

Le guide inclus dans le paquet est [`docs/INSTALL_DEBIAN13.md`](../INSTALL_DEBIAN13.md). Il décrit l'installation avec `sudo apt install ./lofiterm_1.0.0-1_amd64.deb`, la configuration `$XDG_CONFIG_HOME/lofiterm/config` (par défaut `~/.config/lofiterm/config`, permissions privées), le workspace `$XDG_DATA_HOME/lofiterm/workspace` (par défaut `~/.local/share/lofiterm/workspace`), les modes `lofiterm --ui=tui`/`--ui=classic`, `LOFITERM_DATA_DIR` absolu et la suppression avec `apt remove lofiterm` sans effacer les données utilisateur.

Le binaire compilé depuis les sources conserve le nom `agent` et son comportement existant; le lanceur du paquet change vers le répertoire de données avant exécution, empêchant le CWD arbitraire de créer un `workspace/` inattendu.

## Licence et distribution publique

Le dépôt examiné ne déclare pas de licence de redistribution. `packaging/copyright` porte donc `License: UNKNOWN` et indique que le paquet est destiné à l'installation/évaluation locale. Le `.deb` est construit et vérifié localement, mais ne doit pas être publié ou redistribué avant décision explicite sur la licence. Aucun dépôt APT ni paquet public n'existe.

## Limites et verdict

**GO limité — paquet installable localement sur Debian 13 amd64.** La construction, l'inspection, l'installation, les parcours mock CLI/TUI, l'activation Landlock et la suppression dans l'environnement isolé ont été observés.

Ce résultat ne qualifie pas l'utilisation avec un serveur LLM réel, Ghostty physique pour ce paquet précis, SSH/autres terminaux, d'autres architectures/distributions ou une installation sur l'hôte principal. La redistribution publique est **NO-GO** faute de licence déclarée. Aucun commit, push, PR, merge additionnel, tag ou release n'a été créé pour le packaging; le `.deb` est un artefact local sous `/tmp`.
