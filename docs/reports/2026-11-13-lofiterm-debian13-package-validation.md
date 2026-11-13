# LOFITERM — Validation du paquet Debian 13 courant

Date : 2026-11-13

## Référence

- Source qualifiée : `60e2e554479841e2822b8b3d6e601222ad2ab7c3`.
- Branche : `packaging/debian13-main-integration`.
- Packaging repris du worktree historique à `46d653f`.
- Aucune modification du code de production ni des tests.

## Artefact vérifié

- Fichier : `lofiterm_1.0.0-1+local.g60e2e55_amd64.deb`.
- Taille : 282 880 octets.
- SHA-256 :
  `f76a9a0d7aea6e479277ba1c8354344ad7c55d974d49016c188d57dc8266ef68`.

Le paquet a été construit localement par le script existant
`packaging/build-deb.sh`, avec CMake Release et GCC 14.2.0.
La configuration CMake et la compilation C++20 ont réussi.

## Installation et exécution

Image utilisée : `debian:trixie-slim`, Debian 13 amd64.

Après un premier blocage DNS du réseau Docker bridge, les
dépendances ont été installées pendant la construction d'une
image temporaire avec `--network host`. La vérification
fonctionnelle a ensuite été exécutée avec `--network none`.

Résultats observés :

- Installation APT du paquet et de ses dépendances : PASS.
- Présence de `/usr/bin/lofiterm` : PASS.
- Refus d'une configuration utilisateur trop accessible : PASS.
- `read_file` avec serveur HTTP/SSE mock : PASS.
- `write_file` approuvé, octets exacts et résultat d'outil : PASS.
- `write_file` refusé, sans création du fichier ou de son parent : PASS.
- Workspace XDG indépendant du répertoire de lancement : PASS.
- TUI sous PTY, lancement et fermeture F10 : PASS.
- Désinstallation APT : PASS.
- Conservation des sentinelles de configuration et workspace : PASS.

Les tests utilisent un serveur mock loopback, sans LLM réel.
Le CTest complet n'a pas été rejoué lors de cette qualification.
Aucune mesure de performance, recette Ghostty physique ou
vérification strace Landlock supplémentaire n'est revendiquée.

## Limites

- Qualification limitée à Debian 13 amd64 en conteneur.
- Pas d'installation sur l'hôte.
- Pas de publication ni de redistribution du `.deb`.
- Licence de redistribution non définie (`UNKNOWN`).
- L'ancien rapport du 10 novembre conserve son SHA historique.

## Verdict

**PASS — paquet Debian 13 local construit, installé,
exercé et désinstallé sur le SHA indiqué.**

La distribution publique reste bloquée par la décision de licence.
