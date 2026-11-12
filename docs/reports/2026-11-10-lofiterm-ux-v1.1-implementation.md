# LOFITERM UX v1.1 — rapport d'implémentation

**Date :** 2026-11-10<br>
**Dépôt :** `mohamedbouhlel/local-agent-cpp`<br>
**Branche :** `feat/lofiterm-ux-v1.1`<br>
**HEAD / base :** `46d653fe9e788c6410129b7224edd33d870af593`<br>
**État :** candidat local non commité, non publié.

## Résumé

L'identité conversationnelle LOFITERM, les rubriques Configuration/Aide, les
interactions souris de base et la présentation Markdown sont intégrées dans la
TUI existante. Le moteur, SSE, les outils, Landlock, les autorisations,
MetricCollector et la CLI classique n'ont pas été modifiés. Le build propre et
les validations automatisées finales passent : **289/289 cas, 8 429 assertions,
CTest 14/14**.

Le verdict reste **PARTIEL** : aucune recette physique Ghostty n'a été exécutée
pour ce candidat; la molette n'a pas de test PTY dédié; la mise en forme
Markdown est un sous-ensemble de présentation avec emphase au niveau de la
ligne plutôt que des plages inline indépendantes; la visionneuse `.md` est
reportée. Ce rapport ne qualifie pas l'expérience graphique finale.

## Base, provenance et préservation

- `origin/main` vérifié avant intervention :
  `46d653fe9e788c6410129b7224edd33d870af593`.
- Le worktree isolé a été créé à partir de ce commit. Aucun commit n'a été créé;
  le SHA de HEAD reste donc le SHA de base et les modifications sont dans le
  worktree.
- La spécification source externe était
  `/home/oper/Téléchargements/LOFITERM-SPEC-UX-001-v1.1-consolidee-2026-11-10.md`
  (348 lignes). Sa copie intégrale est
  `docs/specifications/SPEC-LOFI-001-v1.1.md`, avec une note de provenance qui
  distingue le statut de proposition du document source et l'autorisation
  d'implémentation de la présente mission.
- **Rectification ajoutée lors de la revue finale :** un plan séparé existe
  dans l'espace de travail du mainteneur à
  `/home/oper/Téléchargements/LOFITERM-PLAN-EXECUTION-UX-2026-11-10.md`.
  Il était effectivement accessible et n'avait pas été consulté lors de la
  première implémentation. Le §13 de la spécification n'est donc pas présenté
  comme son substitut autorisé. Cet écart de procédure reste consigné.
- Le checkout principal `main`, le tag `v1.0.0-technical`, le stash UI-4 et les
  autres worktrees n'ont pas été modifiés.

## Architecture et comportement réalisés

- `MarkdownPresenter` est un composant de présentation de `agent_tui`; il ne
  modifie pas les messages source ni les événements de l'agent. Il traite les
  titres, paragraphes, listes simples et numérotées, citations, séparateurs,
  emphases, code inline, liens textuels et clôtures de code.
- Les clôtures Markdown incomplètes restent littérales pendant le streaming;
  quand la clôture arrive, le transcript complet est reprojeté. Les langages
  `cpp`/`c++`, `python`/`py`, `bash`/`sh`, `json`, `yaml`/`yml` et `diff` sont
  reconnus. Le lexer est volontairement léger : ce n'est pas un parseur de
  grammaire, et les mots-clés sont colorés à l'intérieur de chaque ligne.
- `M` bascule la projection de Conversation entre Markdown et texte brut
  uniquement lorsque le focus est sur le contenu. Le texte stocké reste
  inchangé. Les liens sont du texte; aucun HTML, code ou URL n'est exécuté ou
  ouvert.
- La réponse assistant est étiquetée `LOFITERM Bot:`; sur une largeur où ce
  préfixe ne laisserait plus de cellule de contenu, le rendu compact utilise
  `Bot:`. Les continuations restent indentées.
- F5 Configuration et F6 Aide comportent trois rubriques chacune. Flèches +
  Enter ouvrent/ferment les rubriques; Échap ferme d'abord une rubrique ouverte
  puis revient à Conversation. À propos affiche version/commit indisponibles
  plutôt que de les inventer, la plateforme Linux, le dépôt et les limites.
- Le thread UI ncursesw traite `KEY_MOUSE`. Les onglets sélectionnent F1–F6,
  le clic dans le composeur déplace le focus, les rubriques F5/F6 sont
  sélectionnables et la molette appelle le scroll de la vue active. Pendant
  une approbation, les événements souris sont consommés sans décider;
  l'approbation exige toujours la confirmation textuelle exacte existante.
  `TerminalSession` restaure le masque souris antérieur avant `endwin()`.
- `NO_COLOR` conserve les indications d'état et désactive les paires de
  couleurs; un repli ASCII remplace les puces, citations et séparateurs
  décoratifs lorsque le terminal est détecté non-Unicode.
- La visionneuse `.md` de F2 n'est pas intégrée. `read_file` et sa validation
  descriptor-based sont des helpers internes à `src/workspace_tools.cpp`, sans
  API de lecture sûre réutilisable depuis la présentation. Copier ou extraire
  ce contrôle sortirait du périmètre et demanderait une revue sécurité séparée.

## Fichiers modifiés

| Fichier | Motif |
|---|---|
| `CMakeLists.txt` | Ajouter le renderer Markdown à la cible `agent_tui`. |
| `include/ui/markdown_presenter.hpp`, `src/ui/markdown_presenter.cpp` | Nouveau sous-ensemble de projection Markdown, indépendant du moteur. |
| `include/ui/tui_view.hpp`, `src/ui/tui_view.cpp` | Préfixe bot, styles, projection Markdown/code, rubriques, About et hit-tests. |
| `include/ui/tui_navigation.hpp`, `src/ui/tui_navigation.cpp` | État des rubriques et du focus. |
| `include/ui/terminal_session.hpp`, `src/ui/terminal_session.cpp` | Activation conditionnelle et restauration du masque souris. |
| `src/ui/tui_event_loop.cpp` | Clics, molette, activation des rubriques et bascule texte brut. |
| `tests/tui_navigation_test.cpp`, `tests/tui_test.cpp` | Projections, navigation, Markdown, PTY souris et non-régressions. |
| `tests/CMakeLists.txt` | Plafond de temps dédié au groupe TUI-3. |
| `docs/specifications/SPEC-LOFI-001-v1.1.md` | Copie source indexée, historique du statut préservé. |
| `README.md`, `USAGE.md`, `docs/README.md`, `docs/roadmap/ROADMAP.md` | Instructions et état documentaire à jour. |
| `docs/reports/2026-11-10-lofiterm-ux-v1.1-implementation.md` | Ce rapport. |

Le diff ne touche pas `Agent::send()`, le transport HTTP/SSE, ToolRegistry,
workspace, Landlock, les contrats d'événements, les politiques d'approbation,
MetricCollector, `src/main.cpp` ou le REPL classique.

## Tests et commandes réellement exécutés

Référence avant modification, build propre au même SHA de base :

- `cmake -S . -B /tmp/local-agent-cpp-lofiterm-ux-v11-baseline -DCMAKE_BUILD_TYPE=Debug -DAGENT_BUILD_TESTS=ON`
- `cmake --build /tmp/local-agent-cpp-lofiterm-ux-v11-baseline --parallel 2`
- `ctest --test-dir /tmp/local-agent-cpp-lofiterm-ux-v11-baseline --output-on-failure` — **14/14**.
- Harnais complet — **280 cas, 8 346 assertions, 0 échec**.

Validation du candidat :

- Configuration propre :
  `cmake -S . -B /tmp/local-agent-cpp-lofiterm-ux-v11-final -DCMAKE_BUILD_TYPE=Debug -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-lofiterm-ux-v11-baseline/_deps/json-src` — PASS.
- Build propre : `cmake --build /tmp/local-agent-cpp-lofiterm-ux-v11-final --parallel 2` — PASS, GCC 14.2, CMake 3.31, libcurl 8.14.1, ncursesw. CMake émet l'avertissement préexistant CMP0135; aucune erreur.
- Tests ciblés UX :
  `/tmp/local-agent-cpp-lofiterm-ux-v11-final/tests/agent_tests --filter=ux11` — **9 cas, 80 assertions, 0 échec** en exécution loopback autorisée. Couvre clic d'onglet, clic interdit pendant approbation, rubriques, À propos, hit-test, formatage, texte brut, fences fragmentées/incomplètes et langages reconnus.
- Tests ciblés TUI-3 :
  `/tmp/local-agent-cpp-lofiterm-ux-v11-final/tests/agent_tests --filter=tui3` — **29 cas, 415 assertions, 0 échec**. La commande a pris 31,58 s lors d'une mesure directe; voir le plafond CTest ci-dessous.
- CTest final : `ctest --test-dir /tmp/local-agent-cpp-lofiterm-ux-v11-final --output-on-failure` — **14/14 groupes PASS**, 36,57 s au dernier run.
- Harnais complet final :
  `/tmp/local-agent-cpp-lofiterm-ux-v11-final/tests/agent_tests` — **289/289 cas, 8 429 assertions, 0 échec**.
- PTY souris : clic F2 sélectionne Outils et F10 quitte/restaure le terminal; le clic seul n'envoie aucune requête. PTY approbation : un clic ne produit ni fichier ni répertoire et ne remplace pas le défi. Ces tests emploient `xterm-256color` et un mock loopback; ils ne valent pas recette Ghostty.
- `git diff --check` : PASS après rédaction du rapport.
- Liens relatifs des documents modifiés et ajoutés : PASS; les cibles locales existent.
- Périmètre : 15 fichiers suivis modifiés et 4 fichiers non suivis ajoutés, tous listés dans le tableau ci-dessus. Aucun fichier protégé n'est modifié.
- État du checkout UI-4 et des autres worktrees : préservé; aucun fichier de ces worktrees n'a été écrit.

Le premier CTest du candidat a expiré sur le groupe `tui3` après 30 s, bien que le groupe passe en exécution directe en 31,58 s. Ce groupe contient 30 répétitions PTY de fermeture d'approbation. Le plafond CTest de `tui3` a donc été fixé à 60 s, sans changer le nombre de répétitions ni ajouter de temporisation; les autres groupes gardent 30 s. Le CTest final a terminé en 36,57 s et est PASS.

Les tests loopback ont échoué en environnement sandboxé (`mock: socket() failed`) puis ont réussi lors des relances avec accès loopback autorisé. Ce sont des restrictions d'exécution, pas une régression du produit.

## Matrice de vérification des critères

Les statuts ci-dessous décrivent les preuves automatisées réellement disponibles
sur le candidat; `PARTIAL` n'est pas une qualification manuelle.

| Critères | Statut | Preuve / réserve |
|---|---|---|
| UX-01, UX-10, UX-16 | PASS automatisé | Préfixe bot, projection Markdown et texte brut; source `TuiMessage` inchangée. |
| UX-02, UX-04 | PASS automatisé | À propos avec version/commit `unavailable`, plateforme, lien, limites; rubrique Premiers pas. |
| UX-03, UX-05 | PASS automatisé partiel | État navigation et touches testés; pas de parcours Ghostty. |
| UX-06 | PARTIAL | PTY clique F2; hit-test couvre positions F1/F2, les six clics physiques ne sont pas exercés. |
| UX-07 | PARTIAL | Handler molette relié au scroll de la vue active; aucun événement molette PTY dédié. |
| UX-08 | PARTIAL | Clavier existant et refus sans TTY couverts; terminal physique sans souris non testé. |
| UX-09 | PASS automatisé | PTY clique pendant approbation; aucune création, la fermeture F10 annule. |
| UX-11 | PASS automatisé limité | Fragments directs dans `TuiView`, fence incomplète littérale; tests SSE existants vérifient les erreurs terminales. Pas de recette LLM réelle. |
| UX-12 | PARTIAL | Sanitizer ANSI/OSC existant et aucun parseur HTML/chargeur d'image; pas de test réseau dédié à une image distante. |
| UX-13 | PARTIAL | Les six identifiants de langage sont reconnus; aucune inspection visuelle des styles sur terminal réel. Lexer volontairement simplifié. |
| UX-14 | PARTIAL | Tests de layout, resize PTY et `NO_COLOR` existants; séquence complète 120×40→80×24→60×20 non observée physiquement. |
| UX-15 | NOT RUN | Aucune mesure CPU/RSS/latence de longues conversations. |
| UX-17 | REPORTÉ | Visionneuse `.md` non implémentée; raison sécurité/réutilisation décrite plus haut. |
| UX-18 | N/A | Aucun chemin de visionneuse ajouté. |
| UX-19 | PASS automatisé | Test PTY existant d'erreur SSE après delta et état incomplet conservé. |
| UX-20 | PASS documentaire | Spécification, aide, README, roadmap et rapport indexés; diff checks requis après ce rapport. |
| NR-01 à NR-06 | PASS automatisé | CLI classique, sélection UI, F9/Enter, REPL multiligne et EOF dans le harnais/CTest. |
| NR-07 à NR-11 | PASS automatisé | SSE, complétude, tool calls et erreurs Agent dans les groupes dédiés. |
| NR-12 à NR-15 | PASS automatisé limité | Tests workspace/sandbox/config et sanitizer; aucune nouvelle permission ou donnée secrète. |
| NR-16 à NR-18 | PASS automatisé | BoundedHistory, ancrage et PARTIAL conservés; aucun changement de ces règles. |
| NR-19 à NR-21 | PARTIAL | Layout, petit terminal, resize et UTF-8 testés en PTY/unitaire; autres locales et recette visuelle non exécutées. |
| NR-22 | PARTIAL | `NO_COLOR` et rendu sans couleur couverts; `TERM=dumb` et redirection non rejoués dans cette mission. |
| NR-23 | NOT RUN | SSH et terminaux physiques 16/256 couleurs non qualifiés. |
| NR-24 | N/A | Aucun sélecteur de thème ou de langue n'est introduit. |
| NR-25 à NR-28 | PASS automatisé limité | Données F3 réelles/indisponibles, F4/F6 et aide testées; pas de test serveur réel. |
| NR-29 | PASS automatisé | Fermeture PTY et tests worker existants; pas d'annulation réseau promise. |
| NR-30 | NOT RUN | Pas de mesure charge/mémoire. |
| NR-31 | NOT RUN | Pas d'installation système ni de paquet Debian dans cette mission. |
| NR-32 | PASS automatisé | CTest 14/14 sur le build final du candidat. |
| NR-33 | PARTIAL | Mock HTTP complet exécuté; LLM réel non testé. |
| NR-34 | PASS documentaire | Instructions reflètent les touches et limites implémentées; résultats datés par ce rapport. |
| NR-35 | PARTIAL | Indications sans couleur préservées; repli ASCII implémenté mais non observé sur terminal matériel non-Unicode. |

## Limites et risques résiduels

- Pas de qualification manuelle Debian 13/Ghostty pour cette branche. PTY ne
  vérifie ni contraste perçu, ni forme des caractères, ni intégration terminal.
- La molette dépend du protocole souris annoncé par le terminal. Le test PTY
  valide un clic d'onglet, pas la molette sur tous les terminaux.
- Le style d'emphase et de code inline est appliqué à la ligne de présentation,
  pas à des plages de caractères; les liens montrent libellé et destination.
  Tables Markdown et grammaires syntaxiques complètes ne sont pas prises en
  charge. `M` fournit la projection brute en cas de rendu inadéquat.
- Le parseur re-projette le message complet lors de la croissance du transcript;
  coût de rendu des longues conversations non mesuré. Les plafonds mémoire du
  transcript et du brouillon ne sont pas modifiés.
- Le rendu Unicode dépend de `LC_CTYPE`, `wcwidth` et du terminal; aucune
  conformité Unicode universelle n'est déclarée.
- Aucune visionneuse F2, aucune mesure d'adoption, aucun packaging ou release.

## Verdict

**PARTIEL — candidat prêt pour revue de code, pas qualifié comme expérience
finale.** Les fonctions de présentation livrées et les non-régressions
automatisées passent sur le SHA `46d653fe9e788c6410129b7224edd33d870af593`.
Les critères encore partiels/non exécutés ci-dessus, notamment la validation
Ghostty, le scroll souris, l'inspection visuelle de la coloration et les
mesures de charge, interdisent de déclarer UX-01…UX-20 et NR-01…NR-35
intégralement qualifiés.
