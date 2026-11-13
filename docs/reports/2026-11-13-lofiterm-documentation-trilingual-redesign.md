# Refonte de la documentation utilisateur LOFITERM — 13 novembre 2026

## Référence et périmètre

- Dépôt : `mohamedbouhlel/local-agent-cpp`.
- Branche documentaire : `docs/lofiterm-user-docs-trilingual`.
- Worktree isolé : `/home/oper/tmp/local-agent-cpp-user-docs-trilingual`.
- SHA source vérifié : `39646ffade1c71ece9995fde7770dcf428e45d3a`.
- Aucun changement de code, de configuration, de spécification, de roadmap ou d'archive historique n'est inclus.
- Le checkout principal et les autres worktrees ont été laissés intacts. Le checkout principal était en retard sur la référence distante et contenait des rapports non suivis préexistants; ils n'ont pas été touchés.

Cette intervention remplace la présentation utilisateur des README et manuels racine, organise sept guides par langue et ajoute des illustrations. Elle ne constitue ni une release produit ni une nouvelle qualification fonctionnelle.

## Contenu livré

- README racine et manuel `USAGE.md` en français, avec équivalents `README.en.md`, `README.es.md`, `USAGE.en.md` et `USAGE.es.md`.
- Sept guides utilisateur par langue : démarrage/installation/configuration, interface classique, TUI, outils workspace, sécurité/limites, tutoriels/dépannage/FAQ et architecture simplifiée.
- Index trilingue dans `docs/README.md`, conservant les références techniques et archives déjà présentes.
- Logo et icône SVG LOFITERM dessinés pour cette documentation.
- Quatre captures de terminal réelles : Conversation (F1), Activité (F3), Configuration (F5) et Aide (F6).

Couverture : README et USAGE dans les trois langues, plus 7 guides français, 7 anglais et 7 espagnols. Les langues sont accessibles depuis les en-têtes de navigation des documents et depuis l'index.

## Sources et exactitude fonctionnelle

Les descriptions ont été confrontées au code présent au SHA source, notamment `src/main.cpp`, `src/agent.cpp`, `src/workspace_tools.cpp`, `src/workspace_search.cpp`, les vues et la boucle TUI, ainsi qu'aux sources de configuration et de packaging. Les points documentés comprennent :

- sélection CLI/TUI, vues F1–F6, raccourcis et REPL multiligne;
- outils réellement exposés et approbation par défi propre à chaque création, sans écrasement;
- confinement Landlock, chemins permis, limites d'isolation vis-à-vis d'autres processus du même UID et absence de garantie `fsync`;
- limites observées des outils : lecture jusqu'à 50 000 octets, recherche limitée à 50 lignes correspondantes, sans plafond de longueur pour une ligne individuelle, et absence de plafond de nombre d'entrées dans `list_files`;
- métriques F3 présentées comme mesures d'octets, trames, fragments et durées, sans les assimiler à un nombre ou débit de tokens;
- historique de conversation limité par nombre de messages et tronqué par tours complets, sans tokenizer ni mémoire persistante inter-session;
- distinction entre le binaire source `agent`, la commande du paquet local `lofiterm`, le tag `v1.0.0-technical` et une release produit;
- paquet Debian 13 amd64 décrit comme recette locale; aucune licence de redistribution n'est déclarée.

La revue contradictoire a relevé une omission démontrée : le plafond de contenu `write_file` n'était pas indiqué dans les tableaux d'outils et les manuels. Les six passages FR/EN/ES ont été complétés avec la limite de **200 000 octets**, conformément à `src/workspace_write_authorization.cpp`.

Les instructions de lancement depuis les sources choisissent un répertoire de workspace utilisateur distinct. Le workspace source reste `workspace/` sous le répertoire courant du processus. La configuration `.env` est chargée par le shell dans ces exemples; le binaire source ne lit pas lui-même ce fichier.

## Illustrations

Les captures SVG ont été extraites du rendu de l'application réellement compilée au SHA source, dans un PTY tmux de 110×32, avec configuration par défaut et sans requête LLM. Elles représentent donc les cellules terminal observées, mais **ne sont pas des captures Ghostty et ne valent pas qualification physique**. Quatre vues sont illustrées; les deux autres vues ne sont pas présentées par capture.

Les six SVG (logo, icône et quatre captures) ont été analysés comme XML. Les images sont référencées depuis le README français, anglais et espagnol.

## Vérifications effectuées

Build propre, sans cible de test :

```sh
cmake -S . -B /tmp/lofiterm-doc-capture-build -DCMAKE_BUILD_TYPE=Release -DAGENT_BUILD_TESTS=OFF
cmake --build /tmp/lofiterm-doc-capture-build -j2
```

Résultat observé : configuration et compilation réussies au SHA source. Le binaire a été lancé dans un PTY pour produire les captures ci-dessus. Aucun serveur LLM n'a été appelé. Les tests automatisés, CTest, Ghostty et la recette physique n'ont pas été exécutés dans cette intervention.

Contrôles documentaires exécutés après rédaction :

- liens Markdown locaux relatifs dans les documents utilisateur et l'index : 324 cibles et ancres vérifiées, aucun lien manquant;
- couverture : 7 fichiers guide dans chacune des trois langues;
- SVG : 6 fichiers analysés avec le parseur XML;
- recherche de motifs de clé API littérale : aucune affectation détectée dans les documents concernés;
- `git diff --check` : réussi.

Ces contrôles ne remplacent pas une revue éditoriale native par un locuteur professionnel de chaque langue. Ils contrôlent la cohérence des fichiers, liens et sujets, pas toutes les nuances linguistiques.

## Fichiers concernés

Documents modifiés :

- `README.md`
- `USAGE.md`
- `docs/README.md`

Documents ajoutés :

- `README.en.md`
- `README.es.md`
- `USAGE.en.md`
- `USAGE.es.md`
- 21 guides sous `docs/user/fr/`, `docs/user/en/` et `docs/user/es/` (sept chacun)
- `docs/assets/lofiterm-logo.svg`
- `docs/assets/lofiterm-icon.svg`
- `docs/assets/screenshots/tui-f1-pty-capture.svg`
- `docs/assets/screenshots/tui-f3-pty-capture.svg`
- `docs/assets/screenshots/tui-f5-pty-capture.svg`
- `docs/assets/screenshots/tui-f6-pty-capture.svg`
- `docs/reports/2026-11-13-lofiterm-documentation-trilingual-redesign.md`

Aucune source C++, aucun test, fichier de configuration, document de gouvernance, spécification ou rapport historique n'a été modifié.

## Limites et verdict

- Les captures prouvent un rendu de terminal PTY pour quatre vues au SHA indiqué; elles ne prouvent ni un rendu identique sur Ghostty, ni le comportement avec une réponse LLM active.
- L'exécution des tests et une requalification physique n'étaient pas le but de cette intervention et sont `NOT RUN`.
- La documentation est prête à être relue dans les trois langues; la qualité de traduction n'a pas été certifiée par des réviseurs natifs.
- Aucun commit, push, PR, merge ou tag n'a été effectué.

**Verdict : documentation utilisateur trilingue préparée pour revue.** Les limites de qualification, de distribution et de licence sont conservées explicitement.
