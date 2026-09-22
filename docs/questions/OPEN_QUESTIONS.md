# OPEN_QUESTIONS.md — informations manquantes, contradictions, non-vérifiables

> Questions ouvertes issues de l'audit à HEAD `4f33b62`. Chaque entrée
> indique **la preuve**, **pourquoi ça compte** et **ce qui trancherait**.
> Aucune réponse n'est inventée : quand l'intention n'est pas écrite dans le
> dépôt, elle est déclarée inconnue.

## Bloc 1 — Validations et absence de tests

### Q1. Les validations citées par les itérations ne sont pas reproductibles.
`[FAIT]` Aucun fichier de test, script de validation ou CI n'est suivi par
Git (`git ls-files` ; `AGENTS.md:18` : « Pas de suite de tests pour
l'instant »). Les harnais C++ et mock serveurs utilisés pendant les
itérations ont été créés dans des répertoires temporaires puis supprimés.
**Pourquoi ça compte** : rien dans le dépôt ne prouve le comportement du
streaming, de la troncature, du sandbox ou de l'écriture atomique. Toute
régression future serait silencieuse.
**Ce qui trancherait** : [`ROADMAP.md`](../roadmap/ROADMAP.md) P1/P2 (suite de tests versionnée + mock
SSE versionné).

### Q2. Le refactor `agent_core` est-il destiné aux tests, ou à autre chose ?
`[FAIT + DÉDUCTION]` `47ccea9` et `8f015bf` justifient la bibliothèque par
« compiler un test ou un harnais » ; aucun test n'a suivi.
**Pourquoi ça compte** : soit l'étape suivante est bien les tests (P1), soit
la bibliothèque visait un autre consommateur (embarquement, autre binaire)
non documenté.
**Ce qui trancherait** : une intention écrite (AGENTS.md/README) sur le ou les
futurs consommateurs d'`agent_core`.

### Q3. `HttpClient::post()` doit-il rester ?
`[FAIT]` Défini et fonctionnel, plus appelé (`grep` : seul `post_streaming`
est utilisé). Conservé explicitement lors du passage au streaming.
**Pourquoi ça compte** : API publique non testée et non utilisée = dette ;
ou API volontairement stable pour un futur appelant non-streamé.
**Ce qui trancherait** : décision « garder + tester » vs « supprimer »
([`ROADMAP.md`](../roadmap/ROADMAP.md) P3).

## Bloc 2 — Cible LLM et secrets

### Q4. Quel serveur et quel modèle sont réellement ciblés ?
`[FAIT]` README cite llama.cpp, Ollama, LM Studio, vLLM comme **exemples**.
Aucun serveur ni modèle n'est attesté dans le dépôt. `.env` (local, ignoré)
définit les 4 variables et un `LLM_API_KEY` **non vide** (valeur non
inspectée).
**Pourquoi ça compte** : le plafond de 20 messages est justifié par « la
fenêtre de contexte du modèle local » (`AGENTS.md`) — or cette fenêtre dépend
du modèle, qui n'est pas documenté. L'adéquation du plafond est donc
invérifiable. De même, l'usage d'une clé d'API suggère un serveur qui
l'exige (Ollama/vLLM/LM Studio selon configuration) sans qu'on sache lequel.
**Ce qui trancherait** : documenter serveur + modèle de référence, et la
fenêtre de contexte visée ([`ROADMAP.md`](../roadmap/ROADMAP.md) P13, P16).

### Q5. Quelle est la politique de gestion de `LLM_API_KEY` ?
`[FAIT]` Le `.env` local contient une clé non vide ; `.env` est ignoré par
Git. Aucune mention de rotation, de portée ou de stockage sécurisé.
**Pourquoi ça compte** : le dépôt ne définit pas comment la clé est
provisionnée sur une nouvelle machine, ni ce qu'elle protège.
**Ce qui trancherait** : une note explicite (README/USAGE) sur la
configuration et la sensibilité de cette variable.

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
`[FAIT]` `choices[0].finish_reason` n'est jamais lu ; l'arrêt repose sur
`"[DONE]"` et la fin de réponse HTTP.
**Pourquoi ça compte** : selon les serveurs, `[DONE]` peut manquer ou être
retardé ; `finish_reason` pourrait être le signal robuste.
**Ce qui trancherait** : décision écrite (exploiter `finish_reason` ou
documenter qu'il est volontairement ignoré).

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

### Q15. Le sandbox dépend du répertoire de lancement.
`[FAIT]` `main.cpp` fait `std::filesystem::absolute("workspace")` (donc
relatif au CWD) puis `create_directories`. Lancé depuis un autre répertoire,
l'agent crée et utilise un `workspace/` **à cet endroit**, silencieusement.
C'est documenté dans `USAGE.md`, mais aucune garde n'empêche ce comportement.
**Pourquoi ça compte** : un utilisateur peut croire que les fichiers lus sont
ceux du dépôt alors qu'ils viennent d'ailleurs.
**Ce qui trancherait** : décision (garder/documenter davantage, ou ancrer le
sandbox à un chemin déterministe).

### Q16. `workspace/*` est ignoré mais `workspace/.gitkeep` est suivi.
`[FAIT]` `.gitignore` : `workspace/*` + `!workspace/.gitkeep`.
**Pourquoi ça compte** : c'est cohérent (le dossier vide est préservé, les
fichiers d'exécution ne polluent pas Git), mais aucune note n'explique que le
contenu du sandbox n'est **jamais** versionné — un test qui s'appuie sur un
fichier de `workspace/` échouerait sur un clone neuf.
**Ce qui trancherait** : une note explicite, ou des fixtures de test hors
sandbox.

### Q17. Aucun remote Git configuré : le projet est-il volontairement local ?
`[FAIT]` `git remote -v` vide, aucune branche distante, aucun tag.
**Pourquoi ça compte** : impossible de savoir si la trajectoire prévoit une
publication, un backup, ou une collaboration.
**Ce qui trancherait** : une intention explicite du mainteneur.

## Bloc 6 — Points non vérifiés faute d'accès

### Q18. Comportement face à un vrai serveur LLM en streaming.
`[NON VÉRIFIÉ]` Seuls des mock serveurs (hors dépôt) ont été utilisés dans
l'historique. Aucun échange réel avec llama.cpp/Ollama/vLLM/etc. n'est
attesté dans le dépôt, notamment sur : présence systématique de `[DONE]`,
format exact des `tool_calls` en streaming, gestion des erreurs en cours de
flux par le serveur.
**Ce qui trancherait** : une validation documentée contre un serveur réel
(et un test reproductible, cf. Q1).

### Q19. Les limites numériques sont-elles calibrées ?
`[NON VÉRIFIÉ]` `max_read_chars = 50'000`, `max_write_chars = 200'000`,
`max_search_results = 50`, `max_history_messages = 20`,
`max_tool_iterations = 5` : aucune justification chiffrée ni mesure n'existe
dans le dépôt.
**Ce qui trancherait** : une note de calibrage, ou des tests de charge.
