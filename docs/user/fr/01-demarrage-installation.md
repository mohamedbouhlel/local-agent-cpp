# Démarrage, installation et configuration

🇫🇷 Français · [🇬🇧 English](../en/01-getting-started.md) · [🇪🇸 Español](../es/01-primeros-pasos.md)

## Prérequis

- Linux avec Landlock activable; sinon le programme refuse de démarrer.
- CMake 3.20 ou plus récent et compilateur C++20.
- libcurl et ncursesw pour construire l'application.
- Accès réseau pendant la configuration CMake pour récupérer `nlohmann/json` 3.11.3 si la dépendance n'est pas déjà disponible.
- Serveur LLM compatible avec l'API OpenAI Chat Completions et le streaming SSE.

## Compiler depuis les sources

Sur Debian/Ubuntu, installez les dépendances système puis compilez :

```sh
sudo apt install build-essential cmake libcurl4-openssl-dev libncurses-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Cela produit `build/agent`; ce n'est pas une installation système. Aucun `make install` n'est fourni.

## Préparer l'environnement

Le programme source lit uniquement les variables d'environnement. Pour utiliser un fichier local :

```sh
cp .env.example .env
chmod 600 .env
# Éditer .env dans un éditeur local.
set -a; . ./.env; set +a
```

Ne copiez pas la clé API dans un ticket, une capture ou l'historique partagé d'un shell. `.env` est un fichier shell sourcé par votre shell, pas un fichier lu par le programme.

## Première exécution sans workspace dans le dépôt

Pour une compilation source, le workspace est créé sous `workspace/` relatif au répertoire courant du processus. Lancez depuis un répertoire utilisateur dédié :

```sh
repo_dir=$PWD
mkdir -p "$HOME/lofiterm-run"
cd "$HOME/lofiterm-run"
"$repo_dir/build/agent" --ui=tui
```

La CLI classique est `--ui=classic`; elle est aussi le mode par défaut. L'agent crée `~/lofiterm-run/workspace` si nécessaire. Les données de test restent alors séparées des sources.

## Installer le paquet local Debian

Une recette pour construire un paquet local Debian 13 amd64 existe dans [`docs/INSTALL_DEBIAN13.md`](../../INSTALL_DEBIAN13.md). Le paquet installé fournit `/usr/bin/lofiterm`; utilisez `sudo apt install ./lofiterm_1.0.0-1_amd64.deb`. Ce n'est ni un dépôt APT ni un paquet publiquement distribuable : la licence de redistribution n'est pas déclarée dans le dépôt.

Le paquet sépare l'application et les données utilisateur : configuration sous `${XDG_CONFIG_HOME:-~/.config}/lofiterm/config`, workspace sous `${XDG_DATA_HOME:-~/.local/share}/lofiterm/workspace`. `LOFITERM_CONFIG_FILE` et `LOFITERM_DATA_DIR` acceptent des chemins absolus pour déplacer ces emplacements. Le lanceur change de répertoire vers le dossier de données avant de lancer l'agent, donc le cwd courant ne détermine pas le workspace du paquet.

## Configuration du serveur LLM

| Variable | Défaut | Signification |
|---|---|---|
| `LLM_BASE_URL` | `http://localhost:8080/v1/chat/completions` | URL complète de l'endpoint. |
| `LLM_MODEL` | `local-model` | Nom/identifiant attendu par le serveur. |
| `LLM_API_KEY` | vide | Clé facultative envoyée comme Bearer. |
| `LLM_CONNECT_TIMEOUT_SECONDS` | 10 s (ou `min(LLM_TIMEOUT_SECONDS,10)`) | Délai de connexion. |
| `LLM_IDLE_TIMEOUT_SECONDS` | 60 s par défaut historique | Inactivité entre données de réponse. |
| `LLM_TOTAL_TIMEOUT_SECONDS` | 300 s | Durée totale maximale. |
| `LLM_TIMEOUT_SECONDS` | 60 s | Compatibilité; fallback d'inactivité dans l'exécutable. |

Les trois timeouts modernes sont indépendants; tout entier doit être positif. Le client exige une fin SSE complète selon son contrat et signale les flux incomplets.

Le fichier de configuration du paquet a une grammaire simple `NOM=VALEUR` et autorise quatre clés : `LLM_BASE_URL`, `LLM_MODEL`, `LLM_API_KEY`, `LLM_TIMEOUT_SECONDS`. Les variables d'environnement ont priorité. L'exemple s'installe sous `/usr/share/doc/lofiterm/lofiterm.conf.example`; copiez-le avec des permissions privées selon le manuel Debian.

## Vérifier le serveur avant usage

Depuis l'interface, posez une question sans demander d'action sur des fichiers. Si le serveur échoue, vérifiez l'URL, le modèle et l'authentification côté serveur. Les fournisseurs compatibles OpenAI ne sont pas tous qualifiés par ce dépôt; leurs capacités SSE et tool-calling peuvent différer.

Suite : [interface classique](02-interface-classique.md), [TUI](03-interface-tui.md) et [index documentaire](../../README.md).
