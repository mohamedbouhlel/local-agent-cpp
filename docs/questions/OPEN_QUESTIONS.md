# OPEN_QUESTIONS.md — informations manquantes, contradictions, non-vérifiables

> **Révision M3 du 2026-11-05.** Inventaire initial issu de l'audit historique `4f33b62` ; vérification de l'état actuel au SHA `add64b893ad447e76fb12b90e266ec8eab2f2956`. Les mentions « ancien constat », « résolu », « ouvert » et « non vérifié » s'appliquent au périmètre indiqué. La photographie technique est dans [PROJECT_CONTEXT.md](../context/PROJECT_CONTEXT.md) (branche M2 non fusionnée lors de M3).
>
> **Règle de lecture :** les commentaires sur une configuration `.env` ou un remote *local* sont des **observations historiques non recontrôlées** ; ils ne doivent pas être présentés comme le contenu du dépôt distant actuel. Les décisions non prises restent ouvertes.

## Bloc 1 — Validations et absence de tests

### Q1. Les validations citées sont-elles reproductibles ?
**[RÉSOLU POUR LA VERSIONNABILITÉ, CI TOUJOURS OUVERTE]** À `4f33b62`, aucune suite de tests n'était versionnée (fait historique). À `add64b893ad447e76fb12b90e266ec8eab2f2956`, les fichiers `tests/` contiennent **118 déclarations `TEST(...)`** ; `tests/CMakeLists.txt` déclare **7 groupes CTest sous Linux** (vérifiés par lecture). `AGENTS.md` et PR #5 **rapportent** **403 assertions, 7/7 groupes réussis** ; **aucun test n'a été rejoué pendant M3**. Compteurs historiques : 66/264 (P3.3), 75/298 (R0), 106/372 (`31f3e2a`), 107/376 (relevé intermédiaire).
**Reste ouvert séparément :** aucun workflow CI dans l'arbre Git examiné ; reproduction sur une machine propre non vérifiée pendant M3.
**Preuves :** `tests/CMakeLists.txt`, `tests/*_test.cpp`, `AGENTS.md`, PR #5 ; `ROADMAP.md` P1/P2 réalisés.

### Q2. Le refactor `agent_core` est-il destiné aux tests, ou à autre chose ?
`[CORRECTION — état post-31f3e2a]` `agent_core` est désormais exercé par les tests versionnés, notamment les tests agent et REPL. La question initiale sur l'absence de tests est devenue obsolète.
**Pourquoi ça compte** : soit l'étape suivante est bien les tests (P1), soit
la bibliothèque visait un autre consommateur (embarquement, autre binaire)
non documenté.
**Ce qui trancherait** : une intention écrite (AGENTS.md/README) sur le ou les
futurs consommateurs d'`agent_core`.

### Q3. `HttpClient::post()` doit-il rester ?
`[RÉSOLU — décision du mainteneur (P3.1) : **CONSERVER**]`
`[FAIT]` Défini et fonctionnel, **aucun appelant en production** (`grep` : seul
`post_streaming` est utilisé par `Agent`). Conservé explicitement lors du
passage au streaming (`e0b1771`), où son corps a été extrait dans
`perform_request()` (partagé avec `post_streaming()`).
`[CORRECTION]` L'affirmation initiale « API publique **non testée** » est
**périmée** : `post()` est **couvert par 4 tests** — succès, statut non-2xx,
échec curl, réponse tronquée (`tests/http_client_test.cpp`).
**Décision** : la méthode est **conservée**. Elle délègue à un helper partagé,
ne coûte rien à maintenir et reste disponible pour un appelant non-streamé ;
l'absence d'appelant en production ne suffit pas à la déclarer inutile.
**Trace** : préparation
[`PHASE_3_HTTPCLIENT_POST_REPORT.md`](../reports/PHASE_3_HTTPCLIENT_POST_REPORT.md) ;
réconciliation
[`PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md`](../reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md).

## Bloc 2 — Cible LLM et secrets

### Q4. Quel serveur et quel modèle sont réellement ciblés ?
**[OUVERT / NON VÉRIFIÉ]** `README.md` cite llama.cpp, Ollama, LM Studio et vLLM **comme exemples**. `src/main.cpp` et `.env.example` exposent des valeurs génériques. Aucune configuration serveur/modèle cible unique n'est attestée par les fichiers **suivis** consultés au SHA `add64b893ad447e76fb12b90e266ec8eab2f2956`.
**Historique seulement :** un audit antérieur signalait une configuration `.env` locale avec clé non vide ; cette configuration n'a **pas été inspectée pendant M3** et n'établit pas le serveur actuellement utilisé.
**Ce qui trancherait :** documentation d'un environnement de référence explicitement validé par le mainteneur (ROADMAP P13/P16).

### Q5. Quelle est la politique de gestion de `LLM_API_KEY` ?
**[OUVERT]** `src/main.cpp` lit `LLM_API_KEY` dans l'environnement ; `.env.example` prévoit une valeur vide. Aucune politique détaillée de provisionnement/rotation n'a été établie dans les sources consultées.
**Historique seulement :** la mention d'une clé non vide dans l'ancien `.env` local n'a pas été recontrôlée en M3 ; **aucune valeur secrète n'a été lue ou reproduite**.
**Ce qui trancherait :** décision du mainteneur sur la portée et la gestion des identifiants, puis mise à jour de `USAGE.md`.

## Bloc 3 — Contradictions internes à la documentation

### Q6. `AGENTS.md` contient deux puces partiellement contradictoires sur la troncature.
`[FAIT]` La puce « Historique de conversation : … Troncature simple (drop des
plus anciens), pas de résumé » (l. 69-71) est contredite par la puce suivante
« la troncature raisonne en tours complets » (l. 72-81). La première n'a pas
été réécrite lors de `bf85ca1`.
**Pourquoi ça compte** : un lecteur qui s'arrête à la première puce décrit un
comportement qui n'existe plus.
**Ce qui trancherait** : réconcilier les deux puces ([`ROADMAP.md`](../roadmap/ROADMAP.md) P11).

### Q7. La section s'appelle toujours « Décisions d'architecture (v1) ».
`[FAIT]` Le titre n'a pas évolué alors que trois décisions v1 ont été révisées
(streaming, tool-calling, troncature) et que d'autres capacités ont été
ajoutées hors plan.
**Pourquoi ça compte** : le mot « v1 » laisse croire à une base figée alors
que la section mélange décisions initiales, révisions et invariants ajoutés.
**Ce qui trancherait** : renommer/scinder la section (décisions initiales vs
invariants courants) ([`ROADMAP.md`](../roadmap/ROADMAP.md) P11).

### Q8. La prédiction v1 « sans réécriture complète » a-t-elle été tenue ?
`[FAIT]` La décision v1 exigeait un `http_client` « conçu avec une interface
qui pourrait accepter un callback de streaming plus tard, sans réécriture
complète ». `e0b1771` a extrait le corps de `post()` dans `perform_request()`
(numstat 56/10) pour partager le setup curl. `post()` n'acceptait aucun
callback.
**Pourquoi ça compte** : c'est un écart entre l'intention annoncée et le coût
réel ; sa qualification (« réécriture partielle acceptable » vs « échec de la
prédiction ») est un jugement, non un fait du dépôt.
**Ce qui trancherait** : une note rétrospective dans `AGENTS.md` qualifiant
cet écart.

## Bloc 4 — Choix implicites non documentés

### Q9. `finish_reason` non exploité : choix ou oubli ?
`[RÉSOLU — Phase R0]` `finish_reason == "length"` est refusé et le marqueur
`"[DONE]"` est requis ; voir `src/agent.cpp` et les tests agent associés.

### Q10. La durabilité (fsync) de `write_file` est-elle un compromis assumé ?
`[FAIT]` `AGENTS.md`/`USAGE.md` documentent l'atomicité (rename) et le
compromis hard-link, mais **rien** sur la durabilité après crash : aucun
`fsync` du fichier temporaire ni du répertoire avant/après `rename`.
**Pourquoi ça compte** : « atomique » peut être lu comme « durable », ce qui
n'est pas garanti sans `fsync`. La distinction visibilité / durabilité n'est
pas écrite.
**Ce qui trancherait** : [`ROADMAP.md`](../roadmap/ROADMAP.md) P7 (implémenter `fsync` ou documenter la
limite).

### Q11. `list_files` sans plafond : assumé ?
`[FAIT]` `search_files` plafonne à 50 résultats, `list_files` n'a aucun
plafond et peut produire une sortie arbitrairement grande (et une branche
visiblement morte).
**Pourquoi ça compte** : cohérence interne des outils et maîtrise du contexte
envoyé au modèle.
**Ce qui trancherait** : [`ROADMAP.md`](../roadmap/ROADMAP.md) P4.

### Q12. `write_file` sans mode ajout : voulu ?
`[FAIT]` `write_file` remplace intégralement le contenu ; aucun mode
`append`.
**Pourquoi ça compte** : le message d'erreur de dépassement de taille invite à
« écrire en plusieurs fois », mais l'outil ne permet pas d'écrire
incrémentalement dans un même fichier — seule la réécriture complète par
morceaux plus petits que la limite est possible côté modèle.
**Ce qui trancherait** : décision (ajouter un mode ajout, ou reformuler le
message d'erreur) — voir [`ROADMAP.md`](../roadmap/ROADMAP.md) P14.

### Q13. Langue des messages d'outil non tranchée.
`[FAIT]` UI française (`main.cpp`) ; erreurs d'outil anglaises
(`workspace_tools.cpp`, ex. `file does not exist`, `pass overwrite: true`),
renvoyées au modèle et donc potentiellement à l'utilisateur.
**Pourquoi ça compte** : incohérence visible ; et les instructions envoyées au
modèle sont en anglais alors que les énoncés utilisateur sont en français.
**Ce qui trancherait** : [`ROADMAP.md`](../roadmap/ROADMAP.md) P10.

## Bloc 5 — Périmètre et environnement

### Q14. Statut juridique du projet.
`[FAIT]` Aucune licence dans le dépôt.
**Pourquoi ça compte** : réutilisation/redistribution indécidables.
**Ce qui trancherait** : ajouter une licence ([`ROADMAP.md`](../roadmap/ROADMAP.md) P12).

### Q15. Le sandbox dépend-il du répertoire de lancement ?
**[FAIT VÉRIFIÉ ET DÉCISION TOUJOURS OUVERTE]** `src/main.cpp` utilise `prepare_workspace_root(std::filesystem::absolute("workspace"))`. Le `workspace/` est donc relatif au répertoire de lancement. La préparation rejette les racines symboliques et active Landlock ; l'ancien texte décrivant uniquement `create_directories` est dépassé (`src/filesystem_sandbox.cpp`).
**Risque résiduel :** l'utilisateur peut viser le mauvais workspace s'il lance le programme depuis un autre répertoire.
**Ce qui trancherait :** décision de conserver cet ancrage ou de le modifier ; aucune décision nouvelle en M3.

### Q16. `workspace/*` est ignoré mais `workspace/.gitkeep` est suivi.
`[FAIT]` `.gitignore` : `workspace/*` + `!workspace/.gitkeep`.
**Pourquoi ça compte** : c'est cohérent (le dossier vide est préservé, les
fichiers d'exécution ne polluent pas Git), mais aucune note n'explique que le
contenu du sandbox n'est **jamais** versionné — un test qui s'appuie sur un
fichier de `workspace/` échouerait sur un clone neuf.
**Ce qui trancherait** : une note explicite, ou des fixtures de test hors
sandbox.

### Q17. Le dépôt est-il volontairement local ?
**[RÉSOLU QUANT À L'EXISTENCE D'UN DÉPÔT DISTANT ; INTENTION FUTURE NON VÉRIFIÉE]** Le constat historique « aucun remote Git configuré » concernait le worktree local à `4f33b62`. Le dépôt GitHub `mohamedbouhlel/local-agent-cpp` existe désormais ; `main` inclut des PR fusionnées #1 à #5. Cela **ne prouve pas** la configuration des remotes sur une machine locale ou les objectifs futurs de publication/collaboration.
**Preuve :** historique GitHub `main`, PR #1 à #5.
**Ce qui trancherait :** décision explicite sur la stratégie de publication/collaboration, si nécessaire.

## Bloc 6 — Points non vérifiés faute d'accès

> **M3 :** les essais LiteLLM restent des validations historiques rapportées ; l'accès au code actuel ne prouve pas leur reproduction au nouveau SHA.

### Q18. Comportement face à un vrai serveur LLM en streaming.
`[FAIT RAPPORTÉ]` Les rapports du 23 septembre 2026 décrivent un essai nominal
via LiteLLM et un essai multi-étapes partiellement réussi. Ces essais ne
prouvent pas la compatibilité générale des serveurs ni les cas d'erreur.
**Reste à vérifier** : reproduction avec un serveur et modèle identifiés,
notamment les flux interrompus et les erreurs d'outils.

### Q19. Les limites numériques sont-elles calibrées ?
`[NON VÉRIFIÉ]` `max_read_chars = 50'000`, `max_write_chars = 200'000`,
`max_search_results = 50`, `max_history_messages = 20`,
`max_tool_iterations = 5` : aucune justification chiffrée ni mesure n'existe
dans le dépôt.
**Ce qui trancherait** : une note de calibrage, ou des tests de charge.
