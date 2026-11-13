# LOFITERM — agent-cpp

Agent conversationnel en ligne de commande, écrit en C++20, qui échange en streaming SSE avec un serveur LLM compatible OpenAI (`POST /v1/chat/completions`). Il expose quatre outils sur un répertoire `workspace/` : `list_files`, `read_file`, `search_files`, `write_file`.

> **Plateforme d'exécution : Linux avec Landlock opérationnel.** L'application refuse de démarrer sans ce confinement. Voir [USAGE.md](USAGE.md#5-outils-disponibles) pour les limites de sécurité. La qualification de convergence a été exécutée au SHA `81964be88279754fae11980c4c46f2a2c8ee37fd` (Debian 13.7); elle ne constitue pas une garantie pour tous les noyaux Linux.

## Prérequis

- Linux avec prise en charge de Landlock et autorisation d'activer les règles filesystem (sinon échec au démarrage).
- CMake >= 3.20 et compilateur C++20.
- libcurl (dépendance système), par exemple sous Debian/Ubuntu : `sudo apt install libcurl4-openssl-dev`.
- ncursesw (dépendance système du binaire, par exemple sous Debian/Ubuntu : `sudo apt install libncurses-dev`).
- Accès à la récupération de `nlohmann/json` v3.11.3 pendant la configuration CMake (`FetchContent`).
- Serveur LLM répondant au protocole compatible OpenAI en streaming SSE. llama.cpp, Ollama, LM Studio et vLLM sont des **exemples**, pas des environnements validés dans ce document.

## Compiler

```sh
cmake -S . -B build
cmake --build build
```

## Configurer et lancer

```sh
cp .env.example .env
# Adapter LLM_BASE_URL / LLM_MODEL / LLM_API_KEY si nécessaire
set -a; . ./.env; set +a
./build/agent
```

La CLI classique est le mode par défaut. Pour ouvrir l'interface plein écran
LOFITERM ncursesw depuis un terminal interactif, lancer `./build/agent --ui=tui` ;
`./build/agent --ui=classic` sélectionne explicitement le mode classique.

La configuration est chargée depuis **l'environnement du processus** (`std::getenv`) ; le binaire ne lit pas directement `.env`. Les valeurs par défaut figurent dans [USAGE.md](USAGE.md#2-configuration) et `.env.example`. `workspace/` est relatif **au répertoire de lancement** ; le programme crée ce répertoire s'il manque et rejette une racine qui est un lien symbolique.

## Installation locale Debian 13

Un paquet `.deb` pour Debian 13 amd64 peut être construit avec
`packaging/build-deb.sh`. Il installe la commande `lofiterm`, qui
propose les modes classique et TUI. Le lanceur utilise une
configuration et un workspace utilisateur sous les chemins XDG.

Voir [INSTALL_DEBIAN13.md](docs/INSTALL_DEBIAN13.md).

La qualification sur conteneur est décrite dans `docs/reports/`.
La licence de redistribution n'étant pas définie, ce packaging
reste réservé à l'installation et à l'évaluation locales.

## Tests

```sh
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Les tests sont désactivés par défaut. Pour les activer et les lancer, utilisez les commandes ci-dessus. Les résultats de qualification sont consignés par SHA dans `docs/reports/`; leurs compteurs sont historiques et ne décrivent pas automatiquement le HEAD courant.

`write_file` ne crée un fichier qu'après une approbation explicite, propre à chaque demande. L'interface présente le chemin, la taille, les répertoires parents concernés et le contenu intégral sous une forme inspectable; une confirmation exacte par défi est requise. Le refus, l'annulation, l'expiration ou l'indisponibilité de l'interface n'autorisent aucune création. L'écrasement reste interdit. Voir [USAGE.md](USAGE.md#5-outils-disponibles).

Le binaire propose une CLI classique et une TUI ncursesw sélectionnée avec `--ui=tui`. Dans la TUI, F1–F6 ouvrent les six vues, F9 envoie depuis Conversation, F10 quitte et `M` bascule Markdown/texte brut lorsque le contenu de Conversation est focalisé. Les onglets, le composeur et la molette sont accessibles à la souris si le terminal la prend en charge. Le nom technique du binaire reste `agent`; LOFITERM est le nom public affiché.

## Documentation

- [USAGE.md](USAGE.md) : configuration, REPL simple/multiligne, outils, limites et dépannage.
- [docs/README.md](docs/README.md) : index des documents de référence, spécifications et rapports.
- [SPEC-LOFI-001 v1.1](docs/specifications/SPEC-LOFI-001-v1.1.md) : identité et évolution UX de la TUI LOFITERM.
- [docs/context/PROJECT_CONTEXT.md](docs/context/PROJECT_CONTEXT.md) : photographie technique datée, limites et niveau de preuve.
- [AGENTS.md](AGENTS.md) : consignes et gouvernance des modifications.

