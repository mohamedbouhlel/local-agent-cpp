# LOFITERM — convergence documentaire après PR #49

Date : 12 novembre 2026
Dépôt : `mohamedbouhlel/local-agent-cpp`
Branche documentaire : `docs/lofiterm-post-pr49-convergence`
Worktree : `/home/oper/tmp/local-agent-cpp-doc-convergence`
SHA observé : `fe681d3fb2f71f4a3aaa2b0d996df16f5896b923`

## Objet et méthode

Cette mise à jour documentaire rapproche la roadmap des sources de code et de
l'historique Git local après les PR #47–#49. Elle n'apporte aucun changement
applicatif. Les rapports d'essais sont traités comme des preuves datées sur
leurs propres révisions; aucun de leurs compteurs n'est attribué à nouveau au
SHA courant. Aucun build, CTest, test LLM ou essai physique n'a été exécuté.

Les états de preuve employés sont : **CODE VÉRIFIÉ**, **GIT VÉRIFIÉ**,
**TEST RAPPORTÉ**, **RECETTE MAINTENEUR**, **RISQUE NON PROUVÉ**,
**PROPOSITION**, **DÉCISION OUVERTE** et **NOT RUN**.

## État Git et préservation

- HEAD local `main` et référence locale `origin/main` :
  `fe681d3fb2f71f4a3aaa2b0d996df16f5896b923`.
- La tentative de lecture de la référence distante par `git ls-remote origin`
  n'a pas abouti, la résolution DNS de GitHub étant indisponible. L'égalité
  rapportée ci-dessus est celle des références locales; elle ne constitue pas
  un rafraîchissement live de GitHub.
- Les merges présents dans le DAG local attestent les intégrations #47
  (UX v1.1, `9a8857be…`), #48 (oracle PTY, `fcd50afe…`) et #49 (HTTP/SSE,
  HEAD `fe681d3…`). Le statut distant actuel des PR postérieures n'a pas pu
  être interrogé.
- Tag annoté `v1.0.0-technical` : objet
  `3079c4ec769acacfc3298610943a69599199db17`, commit cible
  `b1d194288b02b5df094953affd763776991d3e0a`. Aucun tag n'a été modifié.
- Le worktree principal avait trois rapports non suivis préexistants; ils sont
  restés intacts. Le worktree local de packaging contenait des changements
  suivis et non suivis préexistants; il est resté intact. Le worktree UX
  récupéré était propre. Un ancien chemin HTTP absent est encore listé
  `prunable`; aucun nettoyage ni prune n'a été effectué.
- La présente branche documentaire a été créée dans un worktree neuf basé sur
  le SHA ci-dessus. Seuls `docs/roadmap/ROADMAP.md`, `docs/README.md` et le
  présent rapport sont dans son périmètre.

Ces constats décrivent les worktrees inventoriés au début et à la fin de la
mission; ils ne garantissent pas l'état ultérieur du dépôt distant.

## Capacités et limites établies

| Domaine | État au SHA observé | Preuve / provenance | Réserve actuelle |
|---|---|---|---|
| Agent, HTTP et SSE | **CODE VÉRIFIÉ** : requête OpenAI-compatible en streaming, assemblage du texte et des appels d'outils, fin `[DONE]` exigée; JSON SSE invalide, coupure et `finish_reason=length` conduisent à un échec. | `src/agent.cpp`, `src/http_client.cpp`, `src/sse_parser.cpp`; historique PR #36/#49. | Aucune génération réelle de plus de 60 secondes n'a été qualifiée sur le main post-#49. Le test mock actif de 62 secondes, rapporté pour le candidat, ne vaut pas cet essai réel. |
| Délais et compteurs | **CODE VÉRIFIÉ** : délais distincts de connexion, inactivité et total; compteurs distincts d'octets de corps HTTP, charges SSE, fragments texte et trames avec métadonnées de raisonnement. | `src/runtime_config.hpp`, `src/main.cpp`, `src/http_client.cpp`, `src/agent.cpp`, `src/ui/metrics.cpp`. | Aucun compteur de tokens ou de tokens/s. Le corps brut SSE, y compris le raisonnement, est temporairement tamponné par le client; un corps non-2xx peut faire partie d'une erreur HTTP. |
| Outils et approbation | **CODE VÉRIFIÉ** : quatre outils workspace; `write_file` exige une approbation par défi pour l'opération et crée uniquement un fichier absent. | `src/workspace_tools.cpp`, `src/workspace_write.cpp`, `src/workspace_write_authorization.cpp`. | Cinq itérations d'outils maximum par envoi. Les actions déjà effectuées ne sont pas annulées par un échec ultérieur du tour. Aucune écriture n'est autorisée par défaut en cas d'absence de décision. |
| Landlock | **CODE VÉRIFIÉ** sous Linux; l'application refuse le démarrage si le confinement requis ne peut pas être établi. | `src/filesystem_sandbox.cpp`, `src/main.cpp`, `USAGE.md`. | Landlock ne confine pas les autres processus du même UID. Le workspace `workspace/` dépend du répertoire courant; pas de `fsync`, donc pas de garantie de durabilité après crash. |
| CLI / REPL | **CODE VÉRIFIÉ** : mode classique et bloc multiligne disponibles; `--ui=classic` explicite. | `src/main.cpp`, `src/prompt_reader.cpp`, contrôleur de présentation. | Pas de recette CLI rejouée dans cette mission. |
| TUI / UX v1.1 | **CODE VÉRIFIÉ** : six vues ncursesw, navigation, souris, composeur, identité LOFITERM, rendu Markdown/coloration lexicale et mode texte brut. | `src/ui/tui_event_loop.cpp`, `src/ui/tui_navigation.cpp`, `src/ui/tui_view.cpp`, `src/ui/markdown_presenter.cpp`; intégration #47. | Visionneuse locale `.md` reportée; conformité Unicode universelle et autres terminaux non qualifiés. La qualification mainteneur ne démontre pas chaque sous-critère officiel. |
| Outils/Diagnostics bornés | **CODE VÉRIFIÉ** : chaque historique possède des plafonds de 32 entrées et 16 384 octets textuels après neutralisation. | `include/ui/bounded_history.hpp`, `src/ui/bounded_history.cpp`, `src/ui/tui_view.cpp`. | Pas une borne RSS globale : allocations temporaires, transcript, composeur et snapshots sont exclus. |
| MetricCollector / F3 | **CODE VÉRIFIÉ** : mesures dérivées d'événements réels, corrélation et états partiels disponibles dans F3. | `include/ui/metrics.hpp`, `src/ui/metrics.cpp`, contrôleurs et vue. | Chaîne saturation `EventChannel` → perte → collector partiel → affichage F3 non démontrée de bout en bout; coût des snapshots non mesuré. |
| Session / contexte / mémoire | **CODE VÉRIFIÉ** : historique de messages JSON en mémoire, tronqué par tours complets autour de 20 messages quand possible; identifiant de session runtime; limite d'itérations outils de 5. | `src/agent.cpp`, `include/agent.hpp`. | Pas de reprise durable, budget token exact, compaction, cache applicatif ou mémoire inter-session identifiés. Le dernier tour peut dépasser 20 messages. Cache du serveur : **INCONNU**. |
| Build et livraison | **CODE VÉRIFIÉ** : C++20/CMake, cibles `agent_core`, `agent_tui`, `agent`; 14 groupes Linux déclarés dans CTest; 304 déclarations `TEST(...)` relevées dans les sources. | `CMakeLists.txt`, `tests/CMakeLists.txt`, fichiers de tests. | Les déclarations ne sont pas des résultats d'exécution. **NOT RUN** dans cette mission. L'index de groupes dans `AGENTS.md` annonce 13 et omet `ui4`. Aucun workflow CI, `install()` CMake, `.deb` ou licence n'a été trouvé dans l'arbre étudié. |

Les déclarations de tests ci-dessus ne sont pas des résultats d'exécution. Les
chiffres associés aux PR restent des **TESTS RAPPORTÉS** : UI-4 pré-fusion,
275 cas / 8 196 assertions / 14 groupes, et HTTP/SSE candidat #49, 304 cas /
8 702 assertions / CTest 14/14 lors de deux campagnes complètes rapportées.
Une exécution intermédiaire a échoué sur une assertion non identifiée. Ces
résultats ne sont pas rejoués sur `fe681d3`.

La recette UX rapide de 11 contrôles a été déclarée PASS par le mainteneur;
elle n'est pas une preuve exhaustive des 16 scénarios officiels. Les
correctifs PTY #48 et sélection du texte du défi sont intégrés dans
l'historique local, mais `NO_COLOR`, erreurs contrôlées, saturation `PARTIAL`
et combinaisons précises dimensions/langages demeurent partiels ou non
démontrés dans le rapport de qualification mainteneur. Cette formulation
conserve la provenance et n'invente pas de correspondance de checklist.

## Réconciliation des documents datés

- `docs/context/PROJECT_CONTEXT.md` est une photographie plus ancienne. Ses
  mentions de MetricCollector absent, d'anciens délais, de groupes de tests
  antérieurs et d'un ancien contrat d'écriture sont des constats historiques,
  non l'état après #49. Elle n'a pas été réécrite rétroactivement.
- Les sections antérieures de `docs/roadmap/ROADMAP.md`, notamment H datée du
  7 novembre et la mise à jour UX du 10 novembre, décrivent leurs SHA et leur
  époque. Elles sont conservées; la section courante ajoutée en tête prévaut
  pour l'état observé ici.
- `PROJECT_HISTORY.md` et `OPEN_QUESTIONS.md` restent des archives d'évolution
  et de décisions; leurs statuts ne sont pas silencieusement convertis en
  photographie présente.
- L'étude UX et le plan d'exécution du 10 novembre étaient accessibles hors
  dépôt. Le plan prévoit notamment une visionneuse `.md` conditionnelle à la
  réutilisation sûre des mécanismes workspace; la fonction a été reportée dans
  l'implémentation, faute d'API de lecture sécurisée réutilisable démontrée.
- L'étude SCCM actualisée
  `/home/oper/Téléchargements/LOFITERM-SCCM-etude-actualisee-2026-11-10.md`
  était accessible. L'étude spécifique du 8 novembre
  `LOFITERM-etude-architecture-sessions-cache-contexte-memoire-2026-11-08(1).md`
  n'a pas été retrouvée dans les emplacements explorés sous `/home/oper`; son
  contenu n'est donc pas invoqué comme preuve.

La seule roadmap canonique reste `docs/roadmap/ROADMAP.md`. Sa trajectoire
consolidée comprend sept résultats, sans nouvelle nomenclature de sous-phases :
référentiel documentaire; qualification réelle >60 s; robustesse tours,
ressources, sécurité et contexte; complétude UX/UI-4; version produit,
licence, installation et packaging; évolution SCCM séparément autorisée;
maintenance et qualification continue.

## Décisions et risques restant ouverts

**DÉCISION OUVERTE :** licence et droit de redistribution; langue officielle;
modèle/serveur de référence; archive source ou paquet Debian; version publique;
besoin et politique de sessions durables, budget contexte, cache ou mémoire.
Ce rapport ne tranche aucune de ces décisions.

**RISQUES / limites à suivre, sans les déclarer bugs reproduits :**

- qualification réelle d'une génération LLM dépassant 60 secondes après #49;
- saturation `EventChannel` propagée jusqu'à `PARTIAL` visible en F3;
- coût CPU/RSS et cadence des snapshots de métriques;
- limites de contexte exprimées en messages plutôt qu'en tokens;
- effets partiels des outils si un tour ultérieur échoue;
- confidentialité du tampon temporaire SSE et corps d'erreur HTTP;
- dépendance du workspace au CWD et limites du confinement contre un autre
  processus du même UID;
- absence de licence, paquet et installation système dans `main`;
- absence de workflow CI versionné.

## Vérifications exécutées et non exécutées

- **Exécuté :** lecture des documents/code cités; `git status`, `git rev-parse`,
  `git worktree list`, inspection du DAG local, du tag, des arbres CMake et des
  rapports pertinents; contrôle du périmètre du worktree documentaire.
- **Accès distant :** `git ls-remote origin` tenté, échec DNS; pas de preuve
  d'état live GitHub dans cette intervention.
- **Exécuté :** `git diff --check` et contrôle des liens relatifs du rapport,
  de l'index et des références internes de la roadmap.
- **NOT RUN :** compilation, CTest, harnais, test serveur LLM, recette CLI,
  Ghostty, mesures de performance ou installation. Cette mission est
  documentaire et n'a révélé aucune raison de les relancer.
- **Non modifié :** production, tests, CMake, `.env`, tags, branches historiques
  et worktrees préexistants.

## Verdict

**PRÊT POUR REVUE.** La roadmap et ce rapport présentent un état cohérent,
daté et sourcé au SHA local `fe681d3`, en séparant code vérifié, historique
Git, résultats antérieurs rapportés, observations mainteneur et inconnues.
La revue est limitée par l'impossibilité d'interroger GitHub en direct par
DNS et par l'absence d'exécution de tests, conformément au mandat. Ce verdict
qualifie le travail documentaire uniquement; il ne déclare ni la release
produit prête ni les risques ouverts résolus.

## Proposition de message pour la Mission 02 — ne pas exécuter ici

> **MISSION 02 — Qualification réelle HTTP/SSE post-PR #49 au-delà de 60 s**
>
> Sur un worktree isolé du `main` alors vérifié, au SHA exact, effectuer une
> recette ciblée avec le serveur LLM réel configuré par le mainteneur. Utiliser
> un prompt non sensible dont la génération, si le modèle le permet, dépasse
> réellement 60 secondes. Enregistrer sans secret ni contenu privé : SHA,
> environnement, durée observée, statut HTTP, progression temporelle,
> compteurs d'octets HTTP/trames SSE/fragments texte et issue `[DONE]`/tour
> complété. Vérifier qu'aucun délai de 60 secondes n'interrompt le flux actif,
> que la limite totale configurée reste respectée et que les erreurs/flux
> incomplets restent explicites. Distinguer clairement une génération réelle
> du mock 62 s historique; ne pas comparer au comportement d'AnythingLLM sans
> payload et conditions identiques. Ne pas modifier le serveur, les timeouts,
> le code ou la configuration; ne pas lancer une campagne générale. Si aucun
> terminal ou serveur utilisable n'est accessible, consigner `NOT RUN` et
> fournir une procédure exacte. Rapport durable requis; pas de commit, push,
> PR, merge ou tag sans autorisation.
