# LOFITERM — Licence 0BSD et fiche GitHub

Date : 2026-11-13 (Europe/Paris)
Branche : `docs/lofiterm-0bsd-github-metadata`
SHA de base : `be2beab6ee97dd508e100dcaac4d7ef43ff780da`
Statut : préparation documentaire soumise à revue ; ne constitue ni une release ni une publication de `main`.

## Constats vérifiés sur GitHub

Dépôt privé `mohamedbouhlel/local-agent-cpp`. Les champs `description`, `homepage`, `topics` et `license` étaient vides au SHA de base. Issues actives ; Projects actifs ; Wiki et Discussions désactivés. Aucun changement de visibilité ni de ces fonctions n'est prévu.

## Décision

Licence BSD Zero Clause (`0BSD`, SPDX / OSI) choisie pour autoriser l'usage et la redistribution sans attribution obligatoire du matériel original de LOFITERM. Copyright déclaré : Mohamed Bouhlel, 2026. Cette décision n'efface pas les licences tierces ; `nlohmann/json` est téléchargé par FetchContent, tandis que curl et ncursesw sont des dépendances système.

## Modifications

- Ajout du texte juridique canonique `LICENSE`.
- Alignement des trois README et des guides d'installation et de sécurité FR/EN/ES.
- Mise à jour de `docs/INSTALL_DEBIAN13.md` et du format Debian `packaging/copyright`.
- Indexation du présent rapport dans `docs/README.md`.
- Aucun code applicatif ni rapport historique réécrit.

## Champs About recommandés (à appliquer dans les paramètres du dépôt)

**Description** : `LOFITERM — C++20 terminal AI agent for OpenAI-compatible LLMs, with SSE streaming, ncursesw TUI, workspace tools and Linux Landlock.`

**Homepage** : `https://github.com/mohamedbouhlel/local-agent-cpp/blob/main/docs/README.md` (index documentaire, pas un site produit externe).

**Topics** : `cpp`, `cpp20`, `llm`, `ai-agent`, `terminal-ui`, `tui`, `ncurses`, `linux`, `landlock`, `sse`, `openai-compatible`, `local-ai`.

Les topics GitHub sont publics, y compris pour un dépôt privé. Ces paramètres About ne sont pas modifiés par le commit documentaire : ils nécessitent une action de configuration GitHub distincte.

## Vérifications et limites

Périmètre documentaire et ciblage de remplacement contrôlés avant création de l'arbre Git. Les liens relatifs vers `LICENSE` sont calculés depuis leurs répertoires respectifs. Aucun build, CTest ni test de paquet Debian n'est revendiqué. Avant de distribuer un `.deb`, examiner les licences tierces et inclure leurs notices. Vérifier les droits sur toute contribution tierce éventuelle ; l'auteur des commits n'est pas une preuve absolue de propriété intellectuelle.

## Verdict

**PRÊT POUR REVUE** : intégrer la licence et les corrections documentaires par PR ; renseigner About par une opération GitHub distincte, sans rendre le dépôt public.
