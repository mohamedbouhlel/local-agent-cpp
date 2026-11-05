# Agent CPP

Agent conversationnel en ligne de commande, écrit en C++20, qui échange en streaming SSE avec un serveur LLM compatible OpenAI (`POST /v1/chat/completions`). Il expose quatre outils sur un répertoire `workspace/` : `list_files`, `read_file`, `search_files`, `write_file`.

> **Plateforme d'exécution : Linux avec Landlock opérationnel.** L'application refuse de démarrer sans ce confinement, notamment sur macOS ; un compilateur C++20 ne suffit donc pas à rendre le binaire utilisable sur ces systèmes. Voir [USAGE.md](USAGE.md#5-outils-disponibles) pour les limites de sécurité. État vérifié par lecture du code au commit `465faae586c8f268d276597df05724431c750085`, 2026-11-05.

## Prérequis

- Linux avec prise en charge de Landlock et autorisation d'activer les règles filesystem (sinon échec au démarrage).
- CMake >= 3.20 et compilateur C++20.
- libcurl (dépendance système), par exemple sous Debian/Ubuntu : `sudo apt install libcurl4-openssl-dev`.
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

La configuration est chargée depuis **l'environnement du processus** (`std::getenv`) ; le binaire ne lit pas directement `.env`. Les valeurs par défaut figurent dans [USAGE.md](USAGE.md#2-configuration) et `.env.example`. `workspace/` est relatif **au répertoire de lancement** ; le programme crée ce répertoire s'il manque et rejette une racine qui est un lien symbolique.

## Vérifier les tests versionnés

```sh
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Les tests sont désactivés par défaut. Le code versionné contient **118 déclarations `TEST(...)`** réparties en **7 groupes CTest sous Linux**. Le relevé historique de M4 indiquait **403 assertions**; le 2026-11-05, après le correctif de désactivation de l'écrasement, le harnais a été relancé : **118 cas / 407 assertions réussis**, CTest **7/7 groupes**.

## Documentation

- [USAGE.md](USAGE.md) : configuration, REPL simple/multiligne, outils, limites et dépannage.
- [docs/README.md](docs/README.md) : index des documents de référence, spécifications et rapports.
- [docs/context/PROJECT_CONTEXT.md](docs/context/PROJECT_CONTEXT.md) : photographie technique datée, limites et niveau de preuve.
- [AGENTS.md](AGENTS.md) : consignes et gouvernance des modifications.

