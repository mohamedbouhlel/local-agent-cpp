# Installation locale de LOFITERM sur Debian 13 amd64

Ce paquet installe la commande `lofiterm`. Il s'agit d'un paquet local de
qualification : le dépôt consulté ne déclare pas de licence de redistribution.
Ne publiez ni le paquet ni une archive binaire avant clarification explicite de
la licence.

## Installation

Depuis le répertoire contenant le `.deb` :

```sh
sudo apt install ./lofiterm_1.0.0-1_amd64.deb
```

`apt` installe les dépendances runtime déclarées par le paquet. Le binaire
technique est placé dans `/usr/lib/lofiterm/agent`; la commande publique est
`/usr/bin/lofiterm`.

## Configuration utilisateur

La configuration facultative se trouve dans
`$XDG_CONFIG_HOME/lofiterm/config`, ou par défaut
`~/.config/lofiterm/config`. Elle accepte une affectation par ligne, sans
syntaxe shell, expansion de variable ni guillemets. Seules les clés ci-dessous
sont permises. Les variables déjà présentes dans l'environnement ont priorité.

```sh
set -eu
config_dir="${XDG_CONFIG_HOME:-$HOME/.config}/lofiterm"
config_file="$config_dir/config"
install -d -m 700 "$config_dir"
if [ -e "$config_file" ]; then
  printf 'Configuration already exists: %s\n' "$config_file" >&2
  exit 1
fi
install -m 600 /usr/share/doc/lofiterm/lofiterm.conf.example "$config_file"
```

Éditez ensuite `config` pour définir les valeurs réelles :

| Clé | Rôle | Valeur par défaut |
| --- | --- | --- |
| `LLM_BASE_URL` | Endpoint Chat Completions compatible OpenAI | `http://localhost:8080/v1/chat/completions` |
| `LLM_MODEL` | Identifiant de modèle attendu par le serveur | `local-model` |
| `LLM_API_KEY` | Clé envoyée dans l'en-tête Bearer | vide |
| `LLM_TIMEOUT_SECONDS` | Timeout réseau positif en secondes | `60` |

Le lanceur refuse une configuration lisible par le groupe ou les autres
utilisateurs. Il ne l'affiche pas. Il refuse également les clés inconnues et
les lignes sans forme `NOM=VALEUR`.

## Workspace et lancement

Le workspace est créé sous
`$XDG_DATA_HOME/lofiterm/workspace`, ou par défaut
`~/.local/share/lofiterm/workspace`. `LOFITERM_DATA_DIR` permet de sélectionner
un autre répertoire de données absolu. Le lanceur change vers ce répertoire
avant d'exécuter le programme : démarrer `lofiterm` depuis un autre dossier
ne crée donc pas un `workspace/` dans le répertoire courant.

```sh
lofiterm --ui=tui
lofiterm --ui=classic
```

La CLI classique reste le mode par défaut. `write_file` demande une
approbation explicite pour chaque création; l'écrasement reste interdit.
Landlock doit être disponible et activable, sinon le programme refuse de
démarrer.

## Désinstallation

```sh
sudo apt remove lofiterm
```

Cette commande supprime le programme système. La configuration et le workspace
dans le home ne sont pas supprimés. Leur suppression est une action manuelle
distincte; vérifiez leur contenu avant toute suppression. Aucun script de
désinstallation du paquet ne touche aux données utilisateur.

## Portée

Cette procédure concerne Debian 13 amd64 et un paquet construit localement. Ce
n'est pas un dépôt APT, un paquet publié ni une installation qualifiée sur
d'autres versions de Debian ou d'autres distributions. Aucun `make install`
ou packaging source universel n'est ajouté par cette procédure.
