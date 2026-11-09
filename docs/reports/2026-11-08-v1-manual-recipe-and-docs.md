# Préparation documentaire et recette manuelle v1.0

**Révision de recette courante :** `617e9dd56d39d13a145bdb44fcc2b28153da8f87` (merge PR #37)

**Qualification initiale archivée :** `81964be88279754fae11980c4c46f2a2c8ee37fd`

**Branche :** `docs/v1-manual-recipe`

**Type de travail :** documentation et protocole de recette uniquement.

## Résumé

Les guides courants ont été corrigés pour décrire l'approbation explicite de
chaque création `write_file`, la confirmation par défi et les limites des deux
interfaces. Le README ne présente plus les anciens compteurs de tests comme
actuels. Une procédure de recette CLI/TUI dans un workspace temporaire et une
procédure de compilation sur Debian 13 sont fournies ci-dessous.

Aucun essai interactif, build ou test n'a été exécuté pendant cette mise à
jour documentaire. Les résultats automatisés cités sont ceux de la campagne
précédente au SHA exact indiqué; les recettes ci-dessous restent à exécuter
par le mainteneur dans un vrai terminal.

## Documents examinés et changements

- `USAGE.md` : décrit maintenant les données présentées à l'approbation, la
  confirmation CLI/TUI par défi, les refus par défaut, l'expiration de cinq
  minutes et les limites de taille du dialogue.
- `README.md` : actualise le résumé des capacités, remplace le relevé de tests
  devenu obsolète par le résultat daté de la campagne du SHA candidat, et
  précise que l'identité graphique LOFITERM n'est pas livrée.
- `AGENTS.md` : corrige la description erronée d'une écriture autonome et
  distingue le résultat de qualification du candidat des anciens relevés.
- `docs/README.md` : indexe ce rapport.

Les rapports et photographies historiques, notamment `PROJECT_CONTEXT.md`,
`OPEN_QUESTIONS.md` et les rapports de qualification antérieurs, ne sont pas
réécrits. Leurs SHA et résultats restent des observations historiques.

## Contrat d'approbation observé dans le code du candidat

Avant l'écriture, la demande présente le workspace effectif, le chemin relatif,
la taille en octets, les répertoires parents manquants et le contenu intégral.
Le contenu est rendu en ASCII réversible afin que les caractères de contrôle
ne soient pas interprétés par le terminal.

- **CLI :** le provider exige stdin et stderr attachés à un TTY. Toute entrée
  déjà en attente rend la demande indisponible sans la consommer. Après
  présentation, le défi aléatoire de 128 bits est affiché; la seule réponse
  acceptée est exactement `yes ` suivi des 32 chiffres hexadécimaux du défi,
  puis Entrée. Une réponse différente est refusée, EOF annule, et la demande
  expire après cinq minutes.
- **TUI :** le dialogue affiche le contenu parcourable, le défi et la saisie.
  La confirmation exacte est la même (`yes <défi>` suivie d'Entrée). Une
  réponse non exacte saisie puis validée par Entrée refuse; taper `n` seul ne
  ferme pas le dialogue. Échap annule et F10 annule en fermant l'application.
  L'approbation est indisponible si l'écran fait moins de 6 lignes ou 48
  colonnes.
- L'approbation est liée à la demande courante et n'est pas mémorisée. Le
  writer revalide ses contraintes; le refus d'écrasement, la validation des
  chemins, Landlock et la publication atomique restent applicables.

Le défi courant reste visible pendant la demande. Comme tout contrôle basé sur
un terminal, cette confirmation ne protège pas contre une personne ou un
processus capable d'injecter la confirmation exacte après son affichage; ne
pas laisser un terminal de recette partagé ou contrôlé par un tiers.

## Recette manuelle CLI et TUI

### Préparer un workspace dédié

Depuis la racine du checkout candidat correspondant au SHA qualifié :

```sh
set +x
SRC="$PWD"
QUAL_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/local-agent-v1-recipe.XXXXXX")"
printf 'Répertoire de recette : %s\n' "$QUAL_ROOT"
```

Conserver le chemin affiché. Ce répertoire est exclusivement réservé à cette
recette. L'application résout `workspace/` relativement au répertoire courant;
elle créera donc `"$QUAL_ROOT/workspace"`. Ne pas lancer l'application depuis
la racine du dépôt pour ces essais.

Dans chaque terminal interactif de recette, charger la configuration sans
l'imprimer, puis se placer dans le même répertoire :

```sh
set +x
SRC=/chemin/absolu/vers/local-agent-cpp
QUAL_ROOT=/chemin/affiché/local-agent-v1-recipe.XXXXXX
set -a
. "$SRC/.env"
set +a
cd "$QUAL_ROOT"
```

Ne pas activer `set -x`, ne pas exécuter `env`, `printenv` ou une commande qui
afficherait `LLM_API_KEY`. Utiliser un endpoint et un modèle déjà configurés
dans le `.env` du mainteneur; ne pas copier ses valeurs dans le rapport.

### CLI classique

Lancer :

```sh
"$SRC/build-v1/agent" --ui=classic
```

Effectuer les contrôles suivants, un par un :

1. Demander au modèle de créer une seule fois
   `cli-refused/probe.txt` avec un court contenu synthétique non secret.
   Quand l'écran montre le workspace, le chemin, la taille, les parents et le
   contenu, vérifier l'intégralité du contenu échappé, puis répondre `no`.
   Attendu : refus explicite; ni `workspace/cli-refused/` ni le fichier ne
   doivent exister.
2. Demander une création distincte, par exemple
   `cli-approved/probe.txt`, contenant uniquement le marqueur synthétique
   `V1_RECIPE_APPROVED_MARKER`. Lire toute la demande avant décision. Saisir
   exactement `yes ` et les 32 chiffres hexadécimaux affichés, puis Entrée.
   Attendu : le fichier est créé après exécution du writer, avec le contenu
   demandé; aucun fichier existant n'est remplacé.
3. Pour vérifier le refus d'une entrée résiduelle, préparer une ligne dans
   l'entrée avant qu'une nouvelle demande apparaisse, puis provoquer une
   demande de création. Attendu : cette entrée ne l'autorise pas; l'opération
   devient indisponible/refusée et l'entrée reste destinée au REPL. Ne pas
   réutiliser cette entrée comme confirmation.
4. Optionnel : provoquer une nouvelle demande et la laisser expirer sans
   confirmer (délai maximal cinq minutes). Attendu : aucun fichier ni parent
   n'est créé; une réponse tardive ne réactive pas le défi.
5. Quitter avec `exit`.

Le modèle peut ne pas appeler l'outil, ou peut proposer plusieurs appels.
Dans ce cas, le scénario ne compte pas comme exécuté : ne confirmer que la
demande exacte visée et refuser toute demande inattendue. Ne pas confondre le
texte du modèle disant qu'il a écrit avec la présence effective du fichier.

Après la session, vérifier les refus et la création approuvée :

```sh
test ! -e "$QUAL_ROOT/workspace/cli-refused"
test -f "$QUAL_ROOT/workspace/cli-approved/probe.txt"
test "$(cat "$QUAL_ROOT/workspace/cli-approved/probe.txt")" = "V1_RECIPE_APPROVED_MARKER"
```

Le premier contrôle confirme également l'absence du répertoire parent du cas
refusé.

### TUI dans Ghostty

Ouvrir Ghostty dans un terminal interactif, charger `.env` comme ci-dessus et
se placer dans le même `QUAL_ROOT`. Vérifier que la taille est au moins de 6
lignes par 48 colonnes pour que le dialogue d'approbation puisse accepter une
décision. Lancer :

```sh
"$SRC/build-v1/agent" --ui=tui
```

Scénarios :

1. Vérifier l'ouverture plein écran, l'absence de flux synthétique initial,
   puis les six vues avec F1–F6. Vérifier que la saisie multiligne et le
   curseur restent présents lors des changements de vue; F9 envoie uniquement
   depuis Conversation. Redimensionner, revenir à Conversation et vérifier le
   scroll ancré ainsi que les éventuels marqueurs `PARTIAL`.
2. Dans Conversation, demander la création de
   `tui-refused/probe.txt`. Dans le dialogue temporaire, parcourir tout le
   contenu avec les flèches et PageUp/PageDown; vérifier le workspace, le
   chemin, la taille et les parents. Saisir `no`, puis appuyer sur Entrée.
   Attendu : refus visible,
   aucune création et aucune reprise automatique de la conversation comme si
   l'écriture avait réussi.
3. Demander une création distincte de
   `tui-approved/probe.txt` avec le contenu exact
   `V1_RECIPE_TUI_APPROVED_MARKER`. Examiner tout le contenu; saisir exactement
   `yes <défi affiché>` dans le dialogue puis Entrée. Attendu : seule cette
   création est exécutée.
4. Provoquer une demande sur un troisième chemin et appuyer sur F10 pendant
   l'approbation. Attendu : la demande est annulée, aucun fichier ni parent
   n'est créé, l'application finit sa fermeture et le terminal revient
   utilisable. Le worker peut devoir terminer son opération réseau active;
   l'interface ne promet pas une annulation immédiate de la requête LLM.
5. Lancer une requête sans outil et fermer avec F10 pendant l'activité. Noter
   le temps de fermeture observé, la réponse finale/erreur et la restauration
   du terminal. Ne pas présenter un seul essai comme garantie de durée bornée.

Après la session TUI :

```sh
test ! -e "$QUAL_ROOT/workspace/tui-refused"
test ! -e "$QUAL_ROOT/workspace/tui-close"
test -f "$QUAL_ROOT/workspace/tui-approved/probe.txt"
test "$(cat "$QUAL_ROOT/workspace/tui-approved/probe.txt")" = "V1_RECIPE_TUI_APPROVED_MARKER"
```

Si le terminal devient inférieur à 6×48 pendant l'approbation, ne pas
approuver : le résultat attendu est l'indisponibilité et l'absence d'écriture.
Après F10, vérifier que le shell répond normalement. Un test PTY automatisé
ne remplace pas ce contrôle Ghostty.

### Vérification d'un fichier existant et nettoyage de recette

Ne pas tenter de remplacer un fichier existant. Pour vérifier l'invariant sans
risque, créer manuellement un fichier synthétique dans le workspace avant la
requête, noter son empreinte, puis demander au modèle de le réécrire. Attendu :
le writer refuse; comparer l'empreinte après le tour.

Ne supprimer le répertoire temporaire qu'après avoir vérifié son chemin exact
et les résultats, et uniquement si le contenu est bien synthétique :

```sh
printf 'À supprimer après vérification : %s\n' "$QUAL_ROOT"
# Après contrôle visuel du chemin : rm -rf -- "$QUAL_ROOT"
```

### Éléments à rapporter

Pour chaque scénario, consigner `PASS`, `FAIL` ou `NOT RUN`, le mode, le
terminal et `TERM`, le SHA du binaire, l'existence des chemins, les tailles et
empreintes des seuls fichiers synthétiques, l'issue visible et le résultat de
la restauration terminal. Noter si un outil n'a pas été appelé ou si le
modèle a fait plusieurs appels. Ne pas consigner clé API, prompts privés,
contenu réel du workspace, défis d'approbation ou transcriptions de session.

## Compilation Debian 13 depuis les sources

Sur une installation Debian 13 propre, installer les prérequis système :

```sh
sudo apt update
sudo apt install --yes git cmake g++ build-essential libcurl4-openssl-dev libncurses-dev ca-certificates
```

Cloner et sélectionner explicitement le candidat qualifié :

```sh
git clone https://github.com/mohamedbouhlel/local-agent-cpp.git
cd local-agent-cpp
git checkout 617e9dd56d39d13a145bdb44fcc2b28153da8f87
cmake -S . -B build-v1 -DAGENT_BUILD_TESTS=ON
cmake --build build-v1 --parallel
ctest --test-dir build-v1 --output-on-failure
./build-v1/tests/agent_tests
```

La configuration CMake récupère nlohmann/json v3.11.3 avec FetchContent; un
accès réseau à GitHub est requis lors d'une configuration sans cache. L'archive
est référencée par URL mais aucun `URL_HASH` n'est fixé dans CMake. Cette
procédure décrit une compilation depuis les sources, pas une installation
système.

Après compilation, le binaire est `build-v1/agent`. Pour lancer la CLI ou la
TUI, charger le `.env` sans l'afficher et se placer dans un répertoire de
travail contenant le workspace voulu, comme dans la recette ci-dessus. Le
projet ne déclare actuellement ni cible CMake `install`, ni paquet Debian, ni
artefact distribué. Il n'y a donc pas de procédure d'installation système ou
de paquet v1.0 à qualifier.

## Résultats précédemment exécutés et limites

Sur le SHA `81964be88279754fae11980c4c46f2a2c8ee37fd`, la qualification
précédente a observé un build propre réussi, **253 cas / 8 044 assertions**,
**CTest 13/13**, une conversation réelle et un `read_file` réel réussis. Les
essais d'approbation humaine, Ghostty sur ce SHA, SSH et environnement Debian
propre n'étaient pas exécutés. Les essais précédents ne sont pas rejoués par
ce rapport.

Les seuls changements de cette intervention sont documentaires. Aucun code,
CMake, test, configuration, rapport historique, commit, push, PR ou merge n'a
été modifié/créé. Aucun secret ou contenu réel du workspace n'est inclus.
