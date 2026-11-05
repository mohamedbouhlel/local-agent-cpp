# AGENTS.md

## Project
Agent IA minimal en C++ : boucle input utilisateur -> appel LLM local
compatible OpenAI (endpoint /v1/chat/completions) -> réponse.

## Setup
cmake -B build

## Build
cmake --build build

## Run
set -a; source .env; set +a
./build/agent

## Test

> Les **118 déclarations de cas** et les **7 groupes CTest Linux** ont été
> vérifiés par lecture du dépôt au SHA `465faae586c8f268d276597df05724431c750085`. Les **403 assertions et
> 7/7 réussis** sont des résultats historiques **rapportés**, non rejoués
> pendant M4. Voir [PROJECT_CONTEXT.md](docs/context/PROJECT_CONTEXT.md).

Suite de tests versionnée dans `tests/` : micro-harnais maison (`test_support.hpp`,
`test_main.cpp`, aucun framework externe) accompagné d'un mock HTTP loopback
(`mock_http_server.*`). 118 cas / 403 assertions répartis en 7 groupes : `sse`,
`workspace`, `sandbox`, `http`, `agent`, `repl`, `mock`.
Désactivée par défaut, elle ne pèse jamais sur l'exécutable de production ;
activation explicite :
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON
cmake --build build-tests
cd build-tests && ctest
Chaque groupe est un test CTest distinct (`TIMEOUT 30`). L'exécutable
`agent_tests` accepte aussi `--filter=<groupe>` et retourne 2 si aucun cas n'est
exécuté (filtre sans correspondance). Un test ou un harnais lie `agent_core`
directement (voir Architecture) sans relister les .cpp.

## Code style
- C++20
- RAII partout, pas de raw pointers sauf nécessité justifiée
- En-têtes dans include/, implémentation dans src/
- Un fichier .cpp par responsabilité (client HTTP, parsing SSE, boucle agent, sandbox)
- Pas de dépendances lourdes : libcurl + nlohmann/json suffisent

## Architecture
- Bibliothèque statique `agent_core` : toute la logique (hors boucle main) —
  - agent.hpp/.cpp          -> état de l'agent, historique, dispatch tool_calls
  - http_client.hpp/.cpp    -> wrapper autour de libcurl
  - prompt_reader.hpp/.cpp  -> lecture d'une interaction REPL (mode simple ou
                               bloc multiligne), flux d'entrée/sortie injectés
  - tool_registry.hpp/.cpp  -> registre d'outils
  - workspace_tools.hpp/.cpp -> registre et lecture des outils sandboxés
  - filesystem_sandbox.hpp/.cpp -> confinement Landlock avant REPL
  - workspace_search.hpp/.cpp -> parcours et recherche du workspace
  - workspace_write.hpp/.cpp -> publication et vérifications des écritures
  - sse_parser.hpp/.cpp -> parsing des événements SSE
  Son include path (`include/`) est PUBLIC, et elle expose transitivement
  CURL::libcurl et nlohmann_json::nlohmann_json (les en-têtes publics
  incluent déjà `<nlohmann/json.hpp>`).
- Exécutable `agent` : `src/main.cpp` uniquement (config + boucle REPL), lié
  à `agent_core` en PRIVATE. Plus d'include path à redéclarer (hérité).
- Conséquence : compiler un test ou un harnais revient à lier `agent_core`
  (`target_link_libraries(<cible> PRIVATE agent_core)`) — plus besoin de
  relister les .cpp un par un ni de redéclarer les dépendances.

## Config LLM local
- LLM_BASE_URL : endpoint local (défaut :
  http://localhost:8080/v1/chat/completions)
- LLM_MODEL    : nom du modèle chargé côté serveur (défaut : local-model)
- LLM_API_KEY  : optionnelle, pour serveurs locaux qui l'exigent
- LLM_TIMEOUT_SECONDS : optionnelle, timeout des requêtes HTTP en secondes
  (défaut : 60)

## Credentials
- Fichier .env (non commité) chargé dans le shell avant de lancer l'agent
  (set -a; source .env; set +a) — pas de parsing dotenv en C++ pour l'instant
- .env.example committé comme documentation des variables attendues
- Le code lit uniquement via std::getenv(), jamais de valeur en dur

## Architecture en vigueur — invariants et révisions

> Les décisions initiales « v1 » sont des **faits historiques**, révisés lors
> de l'implémentation du streaming, du tool-calling et de la troncature par tours.
> Leurs versions d'origine et leurs révisions figurent dans
> [PROJECT_HISTORY.md](docs/history/PROJECT_HISTORY.md).
> L'état technique daté et ses limites sont consolidés dans
> [PROJECT_CONTEXT.md](docs/context/PROJECT_CONTEXT.md) ; les consignes
> normatives de gouvernance restent dans ce fichier.
- Confinement filesystem du processus (Linux) : avant le REPL, l'application
  impose un ruleset Landlock en refusant le démarrage si le noyau ou les
  permissions ne permettent pas son activation. Dans le workspace, la règle
  Landlock accorde uniquement lecture, écriture, suppression de fichiers et
  création de fichiers réguliers/répertoires ; elle refuse l'exécution, les
  liens symboliques et les fichiers spéciaux. Le workspace doit être un vrai
  répertoire, pas un lien symbolique ; la règle est attachée à un descripteur
  ouvert avec `O_NOFOLLOW`. Hors du workspace, la lecture est autorisée sous
  `/etc/ssl/certs` et `/usr/share/ca-certificates`, ainsi que sur
  `/etc/ssl/openssl.cnf`, `/etc/hosts`, `/etc/resolv.conf` et
  `/etc/nsswitch.conf` lorsqu'ils existent. `/etc/ssl/private` reste refusé ;
  les autres chemins, dont `/usr`, `/lib` et `/lib64`, sont refusés après le
  démarrage du processus. Cette politique s'applique à l'agent, pas aux autres
  processus du même compte.
  Les outils de lecture refusent les fichiers dont le compteur de liens
  physiques indique plusieurs noms. Cette vérification ne protège pas contre
  un autre processus du même compte qui modifie le workspace simultanément.
  Le confinement n'est implémenté que sous Linux ; sur les autres systèmes,
  le binaire refuse de démarrer plutôt que de fonctionner sans cette barrière.
- Streaming : actif (stream: true). Les réponses sont consommées en SSE
  (src/sse_parser.cpp, framing uniquement) et le message assistant est
  reconstruit depuis les deltas OpenAI (content concaténé ; tool_calls
  accumulés par index dans src/agent.cpp). Feedback minimal pendant un cycle
  d'outils : callback on_tool_call -> "[appel de <outil>...]".
- Invariant streaming : si la requête échoue en plein flux (coupure, statut
  non-2xx), le message assistant partiellement reconstruit n'est JAMAIS ajouté
  à l'historique — seul le message user du tour y reste, pour pouvoir
  retenter sans historique corrompu.
- Invariant complétude du flux (Phase R0) : un flux LLM n'est accepté comme
  complet que s'il s'est terminé par le marqueur `[DONE]`. L'absence de ce
  marqueur (flux coupé) ou un `finish_reason == "length"` (réponse tronquée par
  la limite de tokens) lève `AgentError` : le message assistant n'est PAS ajouté
  à l'historique et n'est PAS présenté comme une réponse définitive. En fin de
  flux, `SseParser::flush()` restitue la dernière trame éventuellement non
  terminée (auparavant perdue silencieusement).
- Invariant validation des tool calls (Phase R0) : un tool call dont `id` ou
  `function.name` est vide est refusé (`AgentError`) avant tout dispatch. Aucun
  outil n'est exécuté et aucun message `assistant`/`tool` non conforme au
  protocole n'entre dans l'historique.
- État d'exécution explicite (Phase R0) : la boucle n'attribue aucun verdict
  fondé sur les déclarations du modèle. Chaque exécution d'outil est exposée au
  programme via `AgentCallbacks::on_tool_result` (`ToolOutcome{name, ok, result}`)
  et la CLI imprime un récapitulatif `[execution: N outil(s) reussi(s), M en
  echec]`. Un échec de protocole est signalé comme tel (`AgentError`), jamais
  masqué par une réponse de succès.
- `write_file` : le fichier temporaire ouvert est relu et comparé avant la
  publication. Cette vérification ne prouve pas l'état publié en présence
  d'un processus concurrent : une substitution du nom temporaire a été
  reproduite. Voir USAGE.md ; ne pas traiter le workspace comme frontière
  de sécurité contre un processus pouvant le modifier.
- Timeouts (Phase R0) : `CURLOPT_TIMEOUT` (total) est complété par
  `CURLOPT_CONNECTTIMEOUT` (établissement de connexion, borné à `min(timeout,
  10 s)`). Un timeout (`CURLE_OPERATION_TIMEDOUT`) produit un message distinct
  (« HTTP POST timed out after Ns »), séparé des autres échecs curl.
- Historique de conversation : limite de référence de 20 messages (constante
  `max_history_messages`). La troncature retire les tours complets les plus
  anciens, sans résumé. Le dernier tour peut dépasser le plafond : voir
  la limitation ci-dessous et `src/agent.cpp::trim_history()`.
- Limitation connue — tour surdimensionné : trim_history() ne retire jamais
  le dernier tour utilisateur, même s'il dépasse à lui seul
  max_history_messages (20). La troncature raisonne en tours complets (un
  tour commence sur un message role=user) et s'arrête avant le tour en
  cours. Un échange qui enchaîne beaucoup de tool_calls peut donc
  temporairement dépasser le cap de 20 messages. C'est un compromis assumé,
  pas un bug : couper au milieu d'un tour casserait l'appairage
  assistant (tool_calls) -> tool (tool_call_id) attendu par l'API. Le
  dépassement devient plus probable si max_tool_iterations est augmenté
  au-delà de sa valeur actuelle (5).
- Tool-calling : actif via ToolRegistry (dispatch des tool_calls dans
  Agent::send, borné par max_tool_iterations = 5). Quatre outils sandboxés
  dans workspace/ sont exposés au modèle : list_files, read_file,
  search_files et write_file. write_file écrit de façon autonome (aucune
  confirmation humaine), mais refuse d'écraser un fichier existant si
  overwrite n'est pas explicitement true. L'écriture est atomique : contenu
  écrit dans un fichier temporaire du même répertoire puis publié via
  linkat() sans écrasement ou renameat() avec écrasement. Compromis assumé : si la cible écrasée avait des hard links,
  rename() casse le lien — le chemin écrit pointe vers le nouvel inode, les
  autres liens conservent l'ancien contenu.
- Erreurs réseau : si le serveur local est injoignable (curl échoue), 
  afficher un message clair mentionnant LLM_BASE_URL plutôt que de laisser
  remonter une exception brute.

## Current state (observation documentaire au SHA `465faae586c8f268d276597df05724431c750085`)

> Ce résumé n'est pas un rapport de test. La lecture du code au SHA indiqué
> confirme les fonctionnalités ci-dessous ; les succès CTest mentionnés dans
> la section `Test` sont **rapportés** par les travaux antérieurs, non
> réexécutés dans M4.
REPL minimal fonctionnel : lecture utilisateur, appel chat completions
OpenAI-compatible en streaming (affichage progressif), historique borné et
tool-calling actif avec quatre outils sandboxés (list_files, read_file,
search_files, write_file).
Saisie simple **ou bloc multiligne** (spécification
`docs/specifications/repl-multiline-requirements.md`, `REPL-01`→`REPL-09`) :
la ligne exacte `:multi` ouvre le bloc, la ligne exacte `:end` le termine et
déclenche **un seul** `agent.send()`. Le contenu est transmis tel quel (lignes
vides, espaces et tabulations conservés ; CRLF normalisé **dans le bloc**),
sans `\n` final ajouté. Un bloc de **zéro ligne** n'est pas envoyé ; un bloc de
lignes vides l'est. En mode simple, `exit` (correspondance exacte) termine le
REPL ; en multiligne, `exit` est du contenu. EOF en multiligne abandonne le bloc
**sans envoi** puis termine le REPL. En mode simple, `:multi` est donc la seule
ligne « consommée » — déviation assumée et documentée.

## Instructions pour l'agent
- Ne pas ajouter de framework externe sans le demander explicitement
- Garder chaque fichier court et une seule responsabilité
- Commits atomiques, un changement logique par commit, format Conventional
  Commits (feat, fix, refactor, chore, docs, build, test)

## Gouvernance Git et revue
- Toute modification destinée à `main` passe par une branche dédiée et une
  pull request vers `main`, même pour la documentation.
- Ne jamais committer ni pousser directement sur `main`. Ne pas fusionner sa
  propre pull request sans autorisation explicite du mainteneur.
- Avant toute modification, vérifier la branche et l'état Git ; préserver les
  changements existants et ne pas les intégrer sans en vérifier le périmètre.
- Avant d'ouvrir une pull request, vérifier le diff, les fichiers inclus et les
  tests adaptés ; décrire les résultats, les limites et les risques connus.
- Ne pas réécrire l'historique, forcer un push, supprimer une branche ou revenir
  sur des modifications sans autorisation explicite.

## Rigueur et transparence
- Ne jamais inventer de faits, de résultats de tests, de validations, de
  sources, de décisions ou de garanties. Vérifier les affirmations importantes
  dans le code, les commandes ou les documents pertinents et citer les preuves.
- Distinguer les faits observés, les résultats rapportés historiquement, les
  hypothèses, les déductions et les éléments non vérifiés. Signaler clairement
  les incertitudes, les échecs, les accès refusés et les tests non exécutés.
- Ne pas présenter une mitigation partielle, un test réussi ou une absence de
  reproduction comme une preuve de correction complète ou de sécurité totale.
- Signaler immédiatement une erreur ou une régression constatée, son impact et
  ce qui reste à faire ; ne pas la minimiser ni la masquer par une formulation.
- Communiquer simplement et précisément, sans mise en scène, promesse non
  tenue, répétition de plans non exécutés ou déclaration de travail terminé
  lorsque des obligations restent ouvertes.
