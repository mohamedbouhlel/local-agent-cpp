# Réconciliation documentaire LOFITERM après les PR #58 et #60

Date : 2026-11-14

## Référence et isolation

- Dépôt : `mohamedbouhlel/local-agent-cpp`.
- Worktree : `/home/oper/tmp/local-agent-cpp-pr58-pr60-reconciliation`.
- Branche : `docs/lofiterm-pr58-pr60-reconciliation`.
- Base vérifiée : `origin/main` à `62c69b766093ca586b58a23d8379a325940d3fc5`.
- Le worktree documentaire précédent, basé sur `39646ff`, a été laissé intact.
- Le checkout principal reste sur son HEAD local `fe681d3fb2f71f4a3aaa2b0d996df16f5896b923` et conserve trois rapports non suivis préexistants; ils n'ont pas été modifiés. Aucun autre worktree n'a été modifié.

## Constats initiaux

1. La section datée du 12 novembre dans `docs/roadmap/ROADMAP.md`, exacte pour le SHA `fe681d3` et les informations réseau alors disponibles, s'intitulait « État courant » et se déclarait encore référence courante.
2. Cette photographie affirmait qu'aucun paquet Debian ni fichier de licence n'était présent dans `main`; les FAQ française, anglaise et espagnole indiquaient encore que la licence de redistribution devait être clarifiée.
3. Ces affirmations étaient devenues obsolètes après les merges #58 et #60. La recette Debian locale et la qualification associée ne sont toutefois pas équivalentes à une publication officielle.

## Vérification GitHub et contenu des PR

Le `git fetch origin main` a observé `origin/main` à `62c69b766093ca586b58a23d8379a325940d3fc5`, merge commit de la PR #60. Les commits de merge présents dans l'historique vérifié sont :

- PR #58 : `39646ffade1c71ece9995fde7770dcf428e45d3a`, intitulé « Merge pull request #58 from mohamedbouhlel/packaging/debian13-main-integration ».
- PR #60 : `62c69b766093ca586b58a23d8379a325940d3fc5`, intitulé « Merge pull request #60 from mohamedbouhlel/docs/lofiterm-0bsd-github-metadata ».

La liste des fichiers modifiés retournée par GitHub pour la PR #58 comprend 11 fichiers : `README.md`, `docs/INSTALL_DEBIAN13.md`, `docs/README.md`, deux rapports de qualification, `packaging/build-deb.sh`, `packaging/control.in`, `packaging/copyright`, `packaging/lofiterm`, `packaging/lofiterm.conf.example` et `packaging/test-installed-package.py`.

Pour la PR #60, GitHub retourne 14 fichiers : `LICENSE`, les trois README, `docs/INSTALL_DEBIAN13.md`, `docs/README.md`, le rapport 0BSD, six guides utilisateur FR/EN/ES, et `packaging/copyright`. Le contenu de `LICENSE` est la BSD Zero-Clause License (0BSD); `packaging/copyright` précise que l'octroi 0BSD porte sur le matériel original et que les dépendances gardent leurs licences respectives.

## Packaging, qualification et publication

- **Sources présentes :** la recette, les métadonnées Debian, le lanceur, la configuration d'exemple et le script de vérification installée sont versionnés dans `main` depuis #58.
- **Qualification locale rapportée :** `docs/reports/2026-11-13-lofiterm-debian13-package-validation.md` rattache les essais à la source `60e2e554479841e2822b8b3d6e601222ad2ab7c3` et à un conteneur Debian 13 amd64. Le rapport décrit build du paquet, installation, parcours outil, confinement fonctionnel et désinstallation; il précise que le paquet n'a pas été publié et que le CTest complet n'a pas été rejoué. Ce rapport n'est pas une qualification au SHA courant `62c69b7`.
- **Licence du code original :** 0BSD est présente dans `LICENSE` depuis #60. Cela autorise la redistribution du matériel original sous les conditions de cette licence; une distribution binaire doit aussi respecter les licences et notices des dépendances tierces.
- **Publication officielle :** le 2026-11-14, `gh release list --repo mohamedbouhlel/local-agent-cpp --limit 20 --json tagName,name,isLatest` a retourné `[]`. `docs/INSTALL_DEBIAN13.md` qualifie le paquet de construction locale, non de publication officielle, et indique qu'il n'existe pas de dépôt APT public. Aucun fichier `.deb` n'est créé ou publié par cette intervention.

Ainsi, trois états restent distincts : les **sources de packaging sont intégrées**, une **qualification locale est rapportée à un SHA antérieur**, et aucune **release GitHub ni distribution APT officielle** n'a été observée lors du contrôle ci-dessus.

## Corrections documentaires

- Dans la roadmap, la photographie du 12 novembre est conservée et renommée « Photographie historique de convergence produit ». Son texte précise désormais qu'elle était la référence courante à la date de rédaction, et non au HEAD actuel.
- Une actualisation datée du 14 novembre, rattachée au SHA `62c69b766093ca586b58a23d8379a325940d3fc5`, consigne les merges #58/#60, leurs portées, le SHA du paquet local qualifié et le résultat de la liste des releases.
- Les FAQ française, anglaise et espagnole reprennent la même information sur la licence 0BSD, la recette Debian 13 locale, l'absence de release officielle/dépôt APT et les obligations tierces; les deux traductions comportaient la même affirmation obsolète.
- `docs/README.md` indexe ce rapport.

Fichiers modifiés ou ajoutés :

- `docs/roadmap/ROADMAP.md`
- `docs/user/fr/06-tutoriels-depannage-faq.md`
- `docs/user/en/06-tutorials-troubleshooting-faq.md`
- `docs/user/es/06-tutoriales-solucion-problemas-faq.md`
- `docs/README.md`
- `docs/reports/2026-11-14-lofiterm-pr58-pr60-documentary-reconciliation.md`

Les mentions obsolètes conservées dans la photographie du 12 novembre et les archives historiques restent attribuées à leur période/SHA. Les rapports antérieurs n'ont pas été réécrits.

## Contrôles effectués

- HEAD et `origin/main` vérifiés après fetch : `62c69b766093ca586b58a23d8379a325940d3fc5`.
- Merges et listes de fichiers des PR #58 et #60 vérifiés dans le DAG et via l'API GitHub.
- `LICENSE`, `packaging/copyright`, le guide d'installation et le rapport de validation locale lus.
- Inventaire des affirmations de licence et de publication recherché dans les README, manuels et guides utilisateur actuels.
- Liste des releases GitHub interrogée; résultat vide.
- 174 liens et ancres locaux vérifiés dans les six fichiers; cibles présentes et ancres résolues.
- Fences Markdown équilibrées dans les six fichiers.
- `git diff --check` réussi; inventaire final conforme aux six fichiers documentaires ci-dessus, aucun fichier staged.
- Aucun build, test applicatif ou CTest exécuté : cette correction est documentaire.
- Aucun code, CMake, test, tag, worktree historique ou contenu du checkout principal modifié.

## Limites et verdict

La qualification du paquet reste liée au SHA source `60e2e55` et à l'environnement rapporté; elle n'a pas été rejouée sur le HEAD courant. La liste GitHub des releases est une observation du 14 novembre 2026 et peut évoluer. L'absence de dépôt APT est établie ici par les déclarations du dépôt, pas par un audit exhaustif de services externes de distribution.

**Verdict : PRÊT POUR REVUE.** La photographie historique est préservée et clairement datée, la FAQ est réconciliée avec les merges, et la distinction entre sources, qualification locale et publication est explicite.
