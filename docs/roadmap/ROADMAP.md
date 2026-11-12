# ROADMAP.md — trajectoire reconstituée et suite logique

> Ce document distingue **trois registres**, jamais mélangés :
> - **FAIT** : vérifié dans le code/diff au **SHA d'observation explicitement identifié** (référence de fichier ou commit).
> - **DÉDUCTION** : inféré de l'historique et de l'état du code, sans intention explicite dans le dépôt.
> - **PROPOSITION** : suggestion de l'audit, non décidée par le projet.
>
> Les propositions de développement ci-dessous ne sont pas des décisions
> d'implémentation. Les photographies plus anciennes de
> ([`PROJECT_CONTEXT.md`](../context/PROJECT_CONTEXT.md), [`PROJECT_HISTORY.md`](../history/PROJECT_HISTORY.md))
> restent datées et ne remplacent pas l'état courant ajouté en tête.

## État courant et convergence produit — 2026-11-12

> **Photographie de code et de Git :** `main` / `origin/main` local au SHA
> `fe681d3fb2f71f4a3aaa2b0d996df16f5896b923`, merge PR #49. La résolution
> DNS de GitHub a échoué pendant cette intervention; la référence distante
> fraîche n'a donc pas pu être interrogée. Le SHA local `origin/main` concorde
> avec le SHA de référence communiqué et avec le HEAD local. Le tag annoté
> `v1.0.0-technical` conserve son objet `3079c4ec769acacfc3298610943a69599199db17`
> et pointe sur `b1d194288b02b5df094953affd763776991d3e0a`.
>
> Les sections A–H et la mise à jour UX ci-dessous sont des photographies et
> propositions **datées à leurs SHA historiques**, non des statuts actuels.
> Cette section est la référence courante de la roadmap au SHA ci-dessus;
> elle ne réécrit ni les archives ni le gel technique. Les libellés de preuve
> sont `CODE VÉRIFIÉ`, `GIT VÉRIFIÉ`, `TEST RAPPORTÉ`, `RECETTE MAINTENEUR`,
> `RISQUE NON PROUVÉ`, `PROPOSITION` ou `DÉCISION OUVERTE`.

### Capacités présentes et niveau de preuve

| Capacité | État au SHA courant | Preuve et niveau | Réserve / effet pour la livraison |
|---|---|---|---|
| Agent C++ et conversation | Code de génération OpenAI-compatible en streaming SSE, reconstruction de `content` et `tool_calls`; `[DONE]` requis, `finish_reason=length`, JSON SSE malformé et flux coupés traités comme échecs. | **CODE VÉRIFIÉ** : `src/agent.cpp`, `src/sse_parser.cpp`, `src/http_client.cpp`; corrections SSE dans PR #36/#49 et rapports datés. | La session AnythingLLM ~87 s n'a pas été comparée au payload. Le mock 62 s est une preuve historique de transport sur le candidat, pas une qualification d'une génération réelle >60 s sur le main fusionné. |
| Délais HTTP | Connexion, inactivité de réponse et durée totale ont des paramètres distincts; les défauts de l'exécutable sont 10/60/300 s. `LLM_TIMEOUT_SECONDS` reste le fallback idle de l'exécutable; la compatibilité des anciens appels C++ conserve son ancien total. | **CODE VÉRIFIÉ** : `src/main.cpp`, `src/runtime_config.hpp`, `src/http_client.cpp`; PR #49. | **TEST RAPPORTÉ** : candidat PR #49, build propre avant durcissement final puis rebuild incrémental, 304 cas / 8 702 assertions et CTest 14/14 sur les deux dernières campagnes complètes. Une exécution intermédiaire a échoué sur une assertion non identifiée; ces résultats ne sont pas rejoués sur le merge. |
| Mesures réseau/SSE | Distinction des octets du corps HTTP, trames `data:` SSE, fragments texte livrés par le parser et trames portant des champs de raisonnement. | **CODE VÉRIFIÉ** : `include/ui/agent_event.hpp`, `src/agent.cpp`, `src/ui/metrics.cpp`, `src/ui/tui_view.cpp`. | Aucun compteur ne représente des tokens ou un débit token/s. Les octets HTTP ne sont pas des octets réseau complets. Le corps SSE brut est temporairement tamponné; le corps non-2xx peut apparaître dans `HttpError`. |
| Outils workspace | `list_files`, `read_file`, `search_files`, `write_file`; lecture/search bornées selon leurs contrats et création seule pour l'écriture. | **CODE VÉRIFIÉ** : `src/workspace_tools.cpp`, `src/workspace_search.cpp`, `src/workspace_write.cpp`, `src/tool_registry.cpp`. | Le modèle peut faire plusieurs appels jusqu'à cinq itérations par envoi. Aucun résultat textuel du modèle ne prouve à lui seul l'effet réel; les résultats d'outil sont exposés séparément. Les plafonds de contexte/calibrage ne sont pas des garanties de tokens. |
| Approbation des créations | `write_file` passe par une demande exacte, défi aléatoire et `PreparedWrite`; seule l'approbation permet la consommation. `overwrite:true` et l'écrasement restent refusés. | **CODE VÉRIFIÉ** : `src/workspace_write_authorization.cpp`, `src/workspace_write.cpp`, `src/workspace_tools.cpp`, `src/ui/tui_event_loop.cpp`; rapports v1.0 et UX. | La politique est par opération et ne constitue pas une isolation face à d'autres processus du même UID. Les refus/annulations doivent rester distincts d'une erreur Landlock. |
| Landlock / fichiers | Sous Linux, confinement requis au démarrage; racine `workspace/` calculée relativement au CWD. Les outils refusent chemins absolus et `..`, liens selon les vérifications applicables; l'écriture est sans écrasement. | **CODE VÉRIFIÉ** : `src/main.cpp`, `src/filesystem_sandbox.cpp`, `src/workspace_tools.cpp`, `src/workspace_write.cpp`; `AGENTS.md`, `USAGE.md`. | Landlock ne confine pas les autres processus du même compte; course concurrente et durabilité après crash sans `fsync` restent des limites documentées. Un lancement depuis un autre répertoire change le workspace effectif. |
| CLI / REPL | Mode classique par défaut, `--ui=classic` explicite, saisie simple et bloc multiligne; sortie réponse et diagnostics séparés. | **CODE VÉRIFIÉ** : `src/main.cpp`, `src/prompt_reader.cpp`, `src/ui/presentation_controller.cpp`; tests versionnés. | Le mode classique et les interactions physiques n'ont pas été rejoués dans cette mission documentaire. |
| TUI six vues | ncursesw, worker, événements, navigation F1–F6, Conversation, Outils, Métriques, Diagnostics, Configuration et Aide; F9 envoi, F10 sortie, souris conditionnelle. | **CODE VÉRIFIÉ** : `src/ui/tui_event_loop.cpp`, `src/ui/tui_navigation.cpp`, `src/ui/tui_view.cpp`, `src/ui/terminal_session.cpp`; intégration par PR #30/#47. | La visionneuse `.md` locale demandée par SPEC-LOFI-001 v1.1 reste reportée. Certains scénarios officiels restent partiels ou non exécutés. |
| Markdown / identité UX | Affichage `LOFITERM Bot`, sous-rubriques, mode Markdown/texte brut, présentation de blocs et colorations lexicales simples, sanitisation terminal. | **CODE VÉRIFIÉ** : `src/ui/markdown_presenter.cpp`, `src/ui/tui_view.cpp`, `src/ui/terminal_text_sanitizer.cpp`; SPEC v1.1 et rapports UX. | Ce n'est pas un renderer Markdown complet ni un lexer sémantique. La conformité Unicode universelle et tous les terminaux ne sont pas qualifiés. Les anciens statuts « implémentation non effectuée » décrivaient l'état antérieur à PR #47. |
| Historique Outils/Diagnostics | `BoundedHistory` limite chaque historique à 32 entrées et 16 384 octets de texte neutralisé; évictions/rejets sont affichés. | **CODE VÉRIFIÉ** : `include/ui/bounded_history.hpp`, `src/ui/bounded_history.cpp`, `src/ui/tui_view.cpp`; PR #32 et rapport de vérification scroll. | Cette limite n'est pas une borne RSS globale : buffers temporaires, transcript, brouillon et snapshots ne sont pas inclus. L'ancre elle-même évincée entraîne un repositionnement et un indicateur `PARTIAL` après correctif ultérieur intégré. |
| MetricCollector / F3 | Collecteur réel intégré dans la CLI et TUI, mesures HTTP/outils/tour, IDs corrélés, bornes sur détails, statuts indisponibles ou partiels. | **CODE VÉRIFIÉ** : `include/ui/metrics.hpp`, `src/ui/metrics.cpp`, `src/ui/presentation_controller.cpp`, `src/ui/tui_event_loop.cpp`, `src/ui/tui_view.cpp`; PR #43/#44. | **TEST RAPPORTÉ** : pré-fusion 275 cas / 8 196 assertions, CTest 14/14; recette mainteneur UI-4 dix contrôles. Le lien complet saturation EventChannel → `PARTIAL` visible en F3 n'est pas prouvé de bout en bout; coût CPU/RSS des snapshots non mesuré. Les métriques ne sont ni tokens ni mémoire RSS. |
| Qualification UX physique | 11 contrôles rapides annoncés PASS par le mainteneur; les 16 scénarios officiels sont classés individuellement dans le rapport de qualification. | **RECETTE MAINTENEUR** : `2026-11-10-lofiterm-ux-v1.1-physical-qualification.md`; tests et fixes PTY par PR #48. | Les 11 contrôles génériques ne satisfont pas automatiquement tous les détails des 16 scénarios : plusieurs restent PARTIAL/NOT RUN, dont `NO_COLOR`, erreur d'endpoint, `PARTIAL` après saturation, et dimensions précises/échantillons de langages. Ce n'est pas un défaut reproduit. |
| Sessions / contexte / cache / mémoire | Agent garde un historique JSON en RAM par instance et coupe les anciens tours complets à 20 messages lorsque possible; max 5 itérations outils. Aucun catalogue de sessions/reprise, budget tokens, compaction, mémoire persistante ou cache applicatif n'est identifié. | **CODE VÉRIFIÉ** : `src/agent.cpp`, `include/agent.hpp`; comparaison au code et étude SCCM externe actualisée. | La limite en messages n'est pas une limite de tokens; le dernier tour peut dépasser 20 messages. Cache KV éventuel du serveur LLM : **INCONNU** et hors du client. Ce sont des absences constatées, pas des fonctionnalités à développer automatiquement. |
| Build, tests et exploitation | C++20 / CMake, `agent_core`, `agent_tui`, binaire technique `agent`; 14 groupes CTest déclarés sous Linux. | **CODE VÉRIFIÉ** : `CMakeLists.txt`, `tests/CMakeLists.txt`; 304 déclarations `TEST(...)` au SHA lu. | Les déclarations ne sont pas des tests exécutés. **NOT RUN** dans cette mission. `AGENTS.md` cite 13 groupes et omet le groupe `ui4` présent dans CMake : écart documentaire. Aucun workflow CI suivi n'est présent dans l'arbre. |
| Version produit / installation | Le tag `v1.0.0-technical` fige une baseline technique antérieure; aucun `install()` CMake, version de projet exploitable, paquet `.deb` ou fichier de licence n'est présent dans l'arbre `main`. | **GIT VÉRIFIÉ** : arbre de `fe681d3`; `CMakeLists.txt`; tag vérifié localement. | Un worktree local `packaging/lofiterm-debian13` contient des modifications et fichiers non suivis, mais ils ne sont pas dans `main` et aucun `.deb` n'a été établi ici. Le choix licence, source archive vs `.deb`, version produit et autorisation de redistribution restent ouverts. |
| SCCM | Une étude SCCM actualisée existe hors dépôt; aucune architecture complète de sessions/cache/contexte/mémoire n'est intégrée. | **CODE VÉRIFIÉ** et document externe consulté : `/home/oper/Téléchargements/LOFITERM-SCCM-etude-actualisee-2026-11-10.md`. | Proposition d'évolution seulement. L'ancienne étude exacte du 8 novembre indiquée comme source dans des échanges n'a pas été retrouvée dans les emplacements explorés; aucune des options SCCM n'est ratifiée par ce rapport. |

### Réconciliation des archives et statuts

- Les merges présents dans le DAG local de `main` sont #47 (`9a8857be`, UX
  v1.1), #48 (`fcd50afe`, correctif de l'oracle PTY) et #49 (`fe681d3`, HTTP/SSE).
  Ce sont des **GIT VÉRIFIÉ** au dépôt local; l'accès réseau n'a pas permis un
  rafraîchissement live du remote pendant cette intervention.
- La photographie de `PROJECT_CONTEXT.md` est explicitement arrêtée à un SHA
  antérieur et contient des statuts désormais périmés (MetricCollector absent,
  anciens délais, anciens groupes de tests, ancien contrat writer). Elle reste
  une archive datée et ne doit pas servir de description courante sans nouvelle
  révision. De même, les sections historiques de `OPEN_QUESTIONS.md` et
  `PROJECT_HISTORY.md` conservent leur portée temporelle.
- Dans cette roadmap, la section H de 2026-11-07 décrit à juste titre son état
  d'alors mais dit que MetricCollector est absent et propose la rétention comme
  future. PR #43 et #32 ont depuis livré ces éléments. La mise à jour UX du
  2026-11-10 décrivait le candidat avant PR #47 et ne couvrait pas PR #48/#49.
  Elles ne sont pas effacées; la présente section prévaut pour l'état courant.
- Les comptes de tests diffèrent par SHA et par étape. Les résultats de PR #43,
  #47, #48 et #49 sont **TESTS RAPPORTÉS** sur leurs candidats respectifs,
  jamais relabellisés comme un nouvel essai de `fe681d3`. Pour #49, deux passes
  complètes à 304/8 702 et CTest 14/14 sont consignées après un échec isolé
  non identifié; build propre avant le dernier parseur strict, rebuild
  incrémental après. Ce passif reste visible.
- Les mesures ont des unités distinctes : octets du corps HTTP; nombre de
  charges `data:` SSE (dont `[DONE]` selon le compteur); fragments de texte
  livrés comme `TextDelta`; aucun compteur de tokens n'est présent. Un fragment
  peut contenir une quantité variable de texte et ne vaut pas un token.

### Sessions, cache, contexte et mémoire — qualification au code

| Domaine | Classement | État observé et frontière |
|---|---|---|
| Cycle de session | **Présent, volatile** | `Agent` reçoit un `SessionId` runtime et conserve `history_` dans son instance. Aucun chargement/sauvegarde ou reprise session n'est identifié; fermeture du processus perd cet état. Le nom SessionId ne signifie pas catalogue durable. |
| Historique conversationnel | **Présent, RAM** | JSON des messages d'un seul agent; trim par tours complets à une cible de 20 messages, mais le dernier tour est préservé et peut dépasser. Les réponses partielles ne sont pas validées comme message assistant complet, mais les actions d'outil déjà exécutées ne sont pas annulables par le simple échec du tour. |
| Budget de contexte | **Absent** | Aucune comptabilité exacte de tokens/tokenizer ni fenêtre de contexte fiable détectée. Limiter les messages ne garantit pas le budget serveur; la taille exacte disponible dépend du backend/modèle et reste une décision/profil à établir. |
| Cache applicatif | **Absent** | Aucun cache réponse LLM, résultat d'outil ou contenu fichier avec clé/invalidation n'est trouvé. BoundedHistory et MetricCollector sont des historiques volatils bornés de présentation/détails, pas un cache sémantique. |
| Cache serveur LLM | **Inconnu** | Le client ne déclare pas/mesure pas de cache KV/prefix du serveur; comportement externe au dépôt. |
| Mémoire persistante entre sessions | **Absente** | Le workspace contient des fichiers utilisateur/outils, pas un mécanisme de souvenirs réutilisés automatiquement. Ne pas assimiler `write_file` à la mémoire de l'agent. |
| Confidentialité et stockage | **Pas de store applicatif identifié** | Pas de politique de conversation persistée, effacement ou chiffrement de store à arbitrer pour le code actuel. Les données de config sont lues par environnement; la TUI masque la clé et expurge l'origine d'URL. Le tampon HTTP SSE transitoire et les erreurs non-2xx restent une frontière de confidentialité documentée. |
| Compatibilité Landlock | **Aucune persistance SCCM intégrée** | Toute future base/store nécessiterait une décision explicite de chemin/droits, revue Landlock et tests; le workspace actuel n'est pas un emplacement privé contre d'autres processus du même compte. |

L'étude plus ancienne `LOFITERM-etude-architecture-sessions-cache-contexte-memoire-2026-11-08(1).md` n'a pas été trouvée localement dans `/home/oper` aux emplacements explorés. Le rapport ETUDE-SCCM-002 actualisé du 10 novembre a été consulté hors dépôt; ses architectures et tests restent des **PROPOSITIONS**, non des décisions.

### Trajectoire unique de convergence

Cette séquence remplace les anciennes propositions de phases du point de vue
du pilotage courant; elle ne renumérote pas les jalons TUI historiques.

| Résultat produit | Travaux et dépendances | Risques à suivre | Clôture vérifiable |
|---|---|---|---|
| **1. Référentiel et convergence documentaire** | Maintenir cette roadmap et le rapport de convergence comme photographie datée; corriger ensuite les guides seulement par changements ciblés et autorisés. | Anciennes photos de `PROJECT_CONTEXT`, `OPEN_QUESTIONS`, `ROADMAP` peuvent être lues comme courantes; accès live GitHub parfois indisponible. | SHA, source et type de preuve présents; liens valides; aucun statut historique réécrit. |
| **2. Qualification réelle du streaming post-PR #49 au-delà de 60 s** | Avec serveur LLM réel accessible, effectuer une requête dont la génération dépasse réellement 60 s, vérifier activité SSE, `[DONE]`, réponse complète et mesure de durée; ne pas afficher/copier secrets ni prompts privés. Le mock 62 s ne remplace pas cet essai. | Réponse modèle non déterministe; comparaison payload AnythingLLM indisponible; total timeout 300 s par défaut. | Résultat humain/automatisé attribué à SHA précis, durée observée >60 s, tour complet et terminal restauré; si non disponible, consigner NOT RUN sans confondre mock et serveur réel. |
| **3. Robustesse des tours, ressources, sécurité et contexte** | Partir des tests Agent/SSE et des limites existantes; qualifier actions partielles après succès d'outil puis erreur, charges workspace/transcript, budget message vs tokens et surfaces d'échec avant de modifier quoi que ce soit. | Effets d'outil non annulables; historique en messages sans budget de tokens; limites `list_files`/`search_files`; races d'autres processus. | Scénarios reproductibles et limites chiffrées/mesurées; toute correction seulement pour défaut reproduit; aucune action métier ou réussite fabriquée. |
| **4. Complétude UX v1.1 et réserves UI-4** | Utiliser la matrice officielle UX/NR, sans réimplémenter les fonctions fusionnées. Fermer seulement les preuves nécessaires pour `EventChannel` saturé → `PARTIAL` → F3 et mesurer le coût des snapshots avant toute optimisation; marquer les autres conditions non testées. | Tests automatisés ne remplacent pas Ghostty; recette 11 contrôles n'établit pas chaque sous-scénario officiel. | Réserves levées par test intégré/mesure, ou acceptées explicitement comme limites; aide et comportement restent cohérents. |
| **5. Version produit, licence, installation et packaging** | Décider langue/doc, version produit, licence autorisée et stratégie (sources reproductibles ou `.deb`) avant publication. Examiner séparément le worktree local packaging, puis construire/installer dans Debian 13 isolé seulement après décision. | Pas de licence, install target, artefact ni version de release dans `main`; Landlock et workspace relatif au CWD compliquent l'installation. | Build depuis environnement propre au SHA final; launch classic/TUI, config externe, workspace prévu, tests install/désinstallation; artefact et licence réellement vérifiés. |
| **6. Évolution SCCM séparément autorisée** | Ne reprendre qu'après arbitrage des sessions, budget contexte, cache et mémoire. Réutiliser l'étude SCCM actualisée comme analyse non normative; définir les besoins sans présumer SQLite, persistance ou auto-mémoire. | Confidentialité, droits filesystem, reprise après effets d'outils, stockage sous Landlock, données de tokens non disponibles. | Décisions explicites et critères d'effacement/scope; aucune donnée ou capacité fictive; autorisation distincte avant code. |
| **7. Maintenance et qualification continue** | Définir une CI Linux reproductible, tests sur SHA de release, suivi des dépendances et rapports physiques/automatisés séparés. | Aucun workflow CI versionné; tests historiques rapportés par commits différents. | Vérifications reproductibles attachées au SHA livré; échecs connus conservés; procédures de release et support documentés. |

### Décisions produit ouvertes

`DÉCISION OUVERTE` : licence et droit de redistribution; source archive ou
paquet Debian; version publique et identité version/SHA; langue officielle des
docs/UI; couple serveur-modèle de référence; politique future des sessions,
cache et mémoire. Cette roadmap ne tranche aucune de ces décisions. La
spécification graphique LOFITERM décrit des intentions et fonctions UX; elle
ne constitue ni un paquet, ni une licence, ni une release complète.

**Mesures de cette mise à jour :** lecture seule des sources et de l'historique;
aucun build, CTest, test LLM ou essai physique n'a été lancé.


> **Réconciliation M3 du 2026-11-05 :** sections A/B mises à jour à `add64b893ad447e76fb12b90e266ec8eab2f2956` ; sections C–F conservent les **déductions et propositions historiques** de l'audit. Les anciennes sections A/B sont préservées en annexe et ne définissent plus l'état courant.

## Suivi des écarts documentaires M4/M5 — `2b54563f650e0995603472261ecfa306f53bd654`

- **[RÉSOLU DOCUMENTAIREMENT — M4]** Le `README.md` ne présente plus macOS
  comme plateforme d'exécution prise en charge ; Linux/Landlock est annoncé.
- **[RÉSOLU DOCUMENTAIREMENT — M4]** `AGENTS.md` décrit la troncature par
  **tours complets** et sa section d'architecture ne porte plus le titre
  « Décisions d'architecture (v1) ». Les items historiques **Q6/Q7**
  sont requalifiés dans [OPEN_QUESTIONS.md](../questions/OPEN_QUESTIONS.md).
- **[RÉSOLU DOCUMENTAIREMENT — M4]** La spécification REPL conserve ses
  décisions normatives, mais précise en introduction que le REPL multiligne
  a été implémenté ; ses formulations d'avant implémentation restent des
  archives datées.
- **[FAIT NOUVEAU — PR #10]** La règle Landlock du workspace interdit
  désormais l'accès `LANDLOCK_ACCESS_FS_EXECUTE` au SHA
  `2b54563f650e0995603472261ecfa306f53bd654`. Les autres limites de sécurité, notamment la concurrence
  entre processus, restent distinctes et **non levées** par ce changement.
- **[VALIDATION NON REJOUÉE]** Le code déclare encore 118 cas CTest répartis
  en 7 groupes sous Linux. L'extension d'un test par la PR #10 ne prouve
  pas un nouvel état **7/7 passés** ; le relevé **403 assertions** reste
  rapporté antérieurement.
- **[PROPOSITIONS HISTORIQUES]** Les sections C–F et l'ancienne annexe A/B
  demeurent des documents de proposition ou d'archive ; aucun choix produit
  nouveau n'est approuvé par M5.

## A. FAITS — photographie bornée au SHA `add64b893ad447e76fb12b90e266ec8eab2f2956`

> **Contrôle M3 :** lecture du dépôt GitHub au SHA ci-dessus. Les réussites de tests décrites par les rapports ou PR ne sont **pas** des exécutions M3. Le contexte technique détaillé est dans [PROJECT_CONTEXT.md](../context/PROJECT_CONTEXT.md) (branche M2, non encore fusionnée dans `main` lors de M3).

| Sujet | État et statut de preuve | Sources |
|---|---|---|
| Application CLI | Implémentée : C++20, `agent_core`, `agent`, REPL simple et multiligne | `CMakeLists.txt`, `src/main.cpp`, `src/prompt_reader.cpp` |
| API LLM | POST streaming SSE, reconstruction deltas et appels d'outils, `[DONE]` obligatoire, `finish_reason=length` refusé | `src/agent.cpp`, `src/http_client.cpp`, `src/sse_parser.cpp` |
| Outils | `list_files`, `read_file`, `search_files`, `write_file` ; limites intégrées au code | `src/workspace_tools.cpp`, `src/workspace_search.cpp`, `src/workspace_write.cpp` |
| Confinement | Linux Landlock imposé avant le REPL ; workspace réel (racine non symbolique), opérations nofollow à descripteurs et vérifications de publication | `src/filesystem_sandbox.cpp`, `src/workspace_tools.cpp`, `src/workspace_write.cpp` |
| Tests versionnés | **118 déclarations `TEST(...)`**, **7 groupes CTest sous Linux**, suite activable avec `-DAGENT_BUILD_TESTS=ON` : **FAITS DE CODE VÉRIFIÉS** | `tests/*_test.cpp`, `tests/CMakeLists.txt` |
| Exécution des tests | **403 assertions, 7/7 groupes sans échec : VALIDATION RAPPORTÉE** par `AGENTS.md` et PR #5, **non rejouée en M3** | `AGENTS.md`, PR #5 |
| CI | Aucun workflow GitHub Actions suivi dans le HEAD examiné | Arbre Git distant |
| `HttpClient::post()` | API conservée selon la décision P3.1, tests versionnés ; pas d'appelant dans la production examinée | `src/http_client.cpp`, `tests/http_client_test.cpp`, rapport P3.1 |

**États précédents à ne pas confondre :** à `4f33b62` aucun test versionné ; relevés historiques **66/264**, **75/298**, **106/372**, puis **107/376**. Ils restent exploitables comme jalons historiques mais ne sont plus des compteurs courants. Voir [PROJECT_HISTORY.md](../history/PROJECT_HISTORY.md).

## B. FAITS ET LIMITES — dettes à requalifier sur la base courante

- **[FAIT VÉRIFIÉ]** `src/workspace_search.cpp` limite la recherche à **50 résultats** mais ne borne pas la taille d'une ligne ni le volume total parcouru ; `list_files` n'impose pas de limite explicite au nombre de chemins affichés. Les propositions **P4/P5** restent des propositions, non décidées.
- **[FAIT VÉRIFIÉ]** `src/agent.cpp` conserve des limites de compilation **20 messages** et **5 itérations d'outils**. Leur calibration n'est pas démontrée ; proposition **P9/P16**, non décidée.
- **[FAIT VÉRIFIÉ — après décision Q20]** `write_file` refuse maintenant tout écrasement; son schéma ne présente plus `overwrite`. Les créations sous Linux partent d'un inode `O_TMPFILE` publié par `linkat(AT_EMPTY_PATH)`. Aucun `fsync` n'est effectué : la durabilité après crash n'est pas garantie. La course antérieure d'écrasement est reproduite dans Q20; elle ne s'applique plus au chemin runtime, puisque celui-ci refuse le remplacement.
- **[FAIT VÉRIFIÉ]** Les opérations de fichiers incluent des garde-fous contre les liens symboliques et liens physiques multiples ; **[LIMITATION]** les accès concurrents par d'autres processus du même UID et la fenêtre entre publication et vérification ne sont pas éliminés. La revue sécurité complète est **non vérifiée**.
- **[FAIT VÉRIFIÉ]** Le programme refuse de démarrer hors Linux dans `src/filesystem_sandbox.cpp` ; la mention de macOS dans `README.md` est une dette documentaire M4.
- **[FAIT VÉRIFIÉ]** `workspace/` dépend du répertoire de lancement (`src/main.cpp`).
- **[FAIT VÉRIFIÉ]** La section « Décisions d'architecture (v1) » de `AGENTS.md` comporte encore des formulations historiques incompatibles avec la troncature par tours ; correction documentaire M4.
- **[FAIT VÉRIFIÉ]** Aucun workflow CI ni fichier de licence présent dans l'arbre Git consulté.
- **[FAIT VÉRIFIÉ]** La fonctionnalité REPL multiligne est implémentée ; sa spécification approuvée contient encore une phrase d'avant implémentation, à réconcilier en M4.
- **[NON VÉRIFIÉ]** Modèle/serveur LLM de référence, calibrage des limites, comportement sur environnement réel et résultats nouveaux de compilation/CTest.
- **[RÉSOLU HISTORIQUEMENT]** `SseParser::flush()`, `CURLOPT_CONNECTTIMEOUT`, validation de `[DONE]`, traitement de `finish_reason=length`, maintien testé de `HttpClient::post()` ; ne pas les présenter comme dettes ouvertes.

## C. DÉDUCTIONS — trajectoire et logique du projet

- **D1** — La trajectoire suit quatre axes, dans cet ordre : (1) *REPL
  minimal fonctionnel*, (2) *agent capable d'agir sur des fichiers*,
  (3) *robustesse des mutations de fichiers* (atomicité, permissions),
  (4) *qualité de l'interaction* (streaming, feedback). `[DÉDUCTION]` depuis
  la séquence P1 → P2 → P4 → P6.
- **D2** — Le refactor du build (`47ccea9`) a été fait **en préparation de
  tests** ; une suite versionnée utilise désormais `agent_core`.
- **D3** — La documentation est traitée comme un livrable de première classe :
  presque chaque itération a son commit `docs`. Un prochain cycle serait
  incomplet sans mise à jour de `AGENTS.md`/`USAGE.md`. `[DÉDUCTION]`
- **D4** — Le projet privilégie l'outillage fait main (harnais `/tmp`, mock
  serveur) plutôt que des dépendances de test. Cela découle de
  « Pas de dépendances lourdes » et « Ne pas ajouter de framework externe
  sans le demander explicitement » (`AGENTS.md`). `[DÉDUCTION]` — conséquence
  importante : proposer Catch2 exige un accord explicite du mainteneur.
- **D5** — Le périmètre volontairement **hors** trajectoire, faute de toute
  trace : persistance d'historique entre sessions, RAG/embeddings,
  multi-utilisateur, mode serveur, interface graphique, streaming des
  résultats d'outils au fil de l'eau. Rien dans le dépôt ne les évoque.
  `[DÉDUCTION d'absence]`

## D. PROPOSITIONS — roadmap candidat (aucune n'est décidée)

> Chaque item est une **proposition de l'audit**. Les items de la vague 1 sont
> des prérequis aux suivants. Les dépendances externes nécessitant un accord
> explicite (`AGENTS.md`) sont signalées.

### Vague 1 — Rendre l'existant vérifiable (dépend de D2, D3, D4)

- **P1 (RÉALISÉ)** — Cible CMake de tests liant `agent_core`,
  sans framework externe d'abord (`assert` + un `main` par famille), ou avec
  Catch2/GoogleTest **si et seulement si** le mainteneur l'approuve
  (contrainte `AGENTS.md`).
  Couverture prioritaire, dans l'ordre du risque :
  1. `SseParser` : événement coupé entre deux chunks, plusieurs événements
     par chunk, CRLF, commentaires, multi-`data:`, événement final sans ligne
     vide (documenter le comportement attendu).
  2. `trim_history()` : tours complets, tour surdimensionné non tronqué.
  3. Reconstruction des deltas : `tool_calls` entrelacés par `index`,
     `arguments` fragmentés, fragment JSON invalide ignoré, `arguments` vide
     → `"{}"`.
  4. Invariant anti-partiel : échec mid-stream ⇒ historique sans message
     assistant.
  5. `sandbox_path` / `write_file` : `..`, absolu, répertoire cible, refus
     d'écrasement, atomicité, préservation des permissions.
- **P2 (RÉALISÉ)** — Mock serveur HTTP/SSE versionné dans `tests/`,
  sans dépendance externe, pour rendre la validation de bout en bout
  reproductible. Résout Q1/Q3.

### Vague 2 — Boucher les trous de robustesse (indépendants entre eux)

- **P3 (DÉCIDÉ — option A, P3.1)** — `HttpClient::post()` est **conservé** :
  API publique disponible, **déjà couverte par 4 tests**
  (`tests/http_client_test.cpp`), déléguant à `perform_request()` (partagé avec
  `post_streaming()`). L'absence d'appelant en production est un fait, non un
  motif de suppression. Dette §B.1 **close** ; préparation
  [`PHASE_3_HTTPCLIENT_POST_REPORT.md`](../reports/PHASE_3_HTTPCLIENT_POST_REPORT.md),
  réconciliation
  [`PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md`](../reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md).
- **P4 (PROPOSITION)** — Plafonner `list_files` (parité avec
  `max_search_results`) et supprimer la branche morte. Dettes §B.6, §B.7.
- **P5 (PROPOSITION)** — Borner `search_files` (taille de ligne, taille de
  fichier lue) et documenter la complexité du parcours. Dette §B.8.
- **P6 (FAIT — Phase R0)** — `CURLOPT_CONNECTTIMEOUT` ajouté (borne
  `min(timeout, 10 s)`) ; timeout distingué des autres échecs curl. Dette §B.9.
- **P7 (RÉSOLU DOCUMENTAIREMENT)** — `AGENTS.md` et `USAGE.md` indiquent que
  l'écriture n'est pas garantie durable après crash faute de `fsync`. La
  synchronisation disque n'est pas implémentée.
- **P8 (FAIT — Phase R0)** — `SseParser::flush()` ajouté ; le dernier événement
  non terminé est restitué (l'agent exige en outre `[DONE]`). Dette §B.2.

### Vague 3 — Configuration et cohérence

- **P9 (PROPOSITION)** — Rendre les limites configurables par variables
  d'environnement (`LLM_MAX_TOOL_ITERATIONS`, `LLM_MAX_HISTORY_MESSAGES`,
  `LLM_MAX_READ_CHARS`, …) **ou** documenter pourquoi elles restent des
  constantes. Dette §B.10.
- **P10 (PROPOSITION)** — Trancher la langue des messages d'outil (anglais vs
  français) et l'appliquer uniformément dans `workspace_tools.cpp`. Dette
  §B.13.
- **P11 (PROPOSITION)** — Nettoyer `AGENTS.md` : renommer la section
  « Décisions d'architecture (v1) », réconcilier les deux puces sur la
  troncature, retirer l'espace ligne 92. Dette §B.14.
- **P12 (PROPOSITION)** — Ajouter la licence du projet et un `.clang-format`
  cohérent avec le style déclaré. Dette §B.15.
- **P13 (PROPOSITION)** — Documenter le serveur/modèle réellement ciblé
  (README ne cite que des exemples) et la politique de gestion de
  `LLM_API_KEY`. Dette §B.17, Q4.

### Vague 4 — Extensions (propositions pures, sans indice d'intention)

> Ces items n'ont **aucune** trace dans l'historique. Ils sont listés pour
> être explicitement acceptés ou rejetés, pas parce qu'ils seraient attendus.

- **P14 (PROPOSITION)** — Nouvelles capacités d'outils : `delete_file`,
  `move_file`, `apply_patch`/éditions ciblées, ou mode ajout dans
  `write_file`. Dette §B.12.
- **P15 (PROPOSITION)** — Retour de progression plus riche pendant un cycle
  d'outils (nom + arguments, ou durée), au-delà de
  `[appel de <nom>...]`.
- **P16 (PROPOSITION)** — Configurer/ajuster la fenêtre d'historique selon le
  modèle réel (aujourd'hui 20 messages en dur).

## E. Non-objectifs (à ne pas proposer sans demande explicite)

Faute de toute trace dans le dépôt : persistance d'historique entre sessions,
RAG/embeddings, multi-utilisateur, mode serveur/API, interface graphique,
streaming des résultats d'outils. `[DÉDUCTION d'absence — cf. D5]`

## F. Chemin critique recommandé (proposition)

```
P1 + P2  (tests + mock versionné)      — réalisés
   ↓
P4..P8   (dettes de robustesse, indépendantes ; P3 traité)
   ↓
P9..P13  (config, cohérence, doc)
   ↓
P14..P16 (extensions, sur décision explicite)
```

Justification : tout le reste est plus sûr une fois qu'une suite de tests
versionnée existe, et cette suite est le prolongement direct du refactor
`agent_core` (§D2) — c'est l'étape dont le projet s'est donné les moyens sans
encore la franchir.

## G. Révision TUI — TUI-0, 2026-11-05

> Cette section est une proposition architecturale fondée sur le code au SHA
> `ba1ee2e9b3535f3e5a6b79a4d544dd5b3a2ab81f`. Elle complète les sections
> historiques ci-dessus sans réécrire leurs décisions. La décision mainteneur
> reçue pour TUI-0 remplace explicitement l'ancien non-objectif « TUI plein
> écran » de SPEC-UI-001 v1.1. Elle ratifie le périmètre produit, pas le choix
> d'une bibliothèque ni l'ajout d'une dépendance.

### Faits observés

- UI-1 à UI-3 sont présents dans `origin/main`; UI-4 existe comme changements
  locaux non commités sur `feat/ui-4-observable-metrics` au SHA
  `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb`, et n'est donc pas inclus dans la
  base de cette section.
- L'observateur `AgentEvent` est actuellement synchrone dans le thread de
  `Agent::send()`. La requête HTTP/SSE est bloquante pour cet appelant.
- La CLI classique utilise `PromptReader`, les renderers Plain/Rich, et un
  appel `run_console_turn()` par interaction. Le moteur et la CLI imposent
  Landlock sous Linux; `write_file` refuse `overwrite:true`.
- Le rapport d'incident fusionné par PR #21 décrit des créations de fichiers
  non sollicitées dans un essai rapporté et des tours non convergents. Les
  causes racines ne sont pas établies par ce rapport. Une TUI ne constitue pas
  un contrôle d'autorisation.

### Proposition d'architecture et comparaison

| Option | Atouts observés/documentés | Coûts et risques | Position proposée |
|---|---|---|---|
| ncursesw | C API éprouvée, entrée clavier, fenêtres, resize; wide API disponible via configuration CMake `CURSES_NEED_WIDE` | Gestion manuelle du modèle UI, focus/menu/éditeur multiligne, panneaux/scroll, cycle de vie et erreurs; les signaux/redimensionnements demandent une intégration attentive | Alternative si dépendance système préférée et si l'équipe accepte le code d'orchestration bas niveau |
| FTXUI | C++ API de composants, menus, conteneurs, événements clavier/souris, layout DOM; changelog documente input multiligne; CMake expose Screen/DOM/Component | Nouvelle dépendance externe, acquisition/reproductibilité à décider; vérifier version/API et restauration terminal sur Linux; ne pas supposer une garantie de terminal après crash | Candidat recommandé pour POC, non approuvé pour ajout |
| Backend personnalisé | Contrôle total, pas de bibliothèque TUI supplémentaire | Le projet devrait maintenir décodage clavier/escape, termios, focus, scroll, dimensions, largeur Unicode, redraw, resize, restauration/signaux et tests pty; risque de sécurité/maintenance le plus élevé | Non recommandé sans contrainte démontrée qui exclut les deux autres |

Sources techniques vérifiées lors de TUI-0 : [FTXUI Getting Started](https://github.com/ArthurSonzogni/FTXUI/blob/main/doc/getting-started.md), [guide CMake FTXUI](https://github.com/ArthurSonzogni/FTXUI/blob/main/doc/installation_cmake.md), [changelog FTXUI](https://github.com/ArthurSonzogni/FTXUI/blob/main/CHANGELOG.md), [versions publiées](https://github.com/ArthurSonzogni/FTXUI/releases), [ncurses HOWTO](https://invisible-island.net/ncurses/NCURSES-Programming-HOWTO.html), [CMake FindCurses](https://cmake.org/cmake/help/latest/module/FindCurses.html). Ces sources ne valent pas validation locale du backend.

### Séquencement candidat

1. **TUI-0 — étude et spécification** : présente section et SPEC v2.0 en proposition.
2. **TUI-1 — décision de dépendance et POC terminal** : comparer FTXUI/ncursesw en builds Debian/C++20 isolés du projet; appliquer le protocole et les critères éliminatoires du rapport complémentaire TUI-0. Aucun changement moteur ni dépendance au projet principal.
3. **TUI-2 — worker et EventDispatcher** : isoler l'appel synchrone dans un worker, copier les événements dans une file bornée et traiter explicitement saturation et fermeture. Proposition : issue finale par canal réservé; détails individuels omis après overflow avec état dégradé. Aucune promesse de zéro perte globale sous mémoire bornée et sans contre-pression.
4. **TUI-3 — navigation et vues** : conversation/outils/métriques/diagnostics/configuration/aide, focus clavier, compactage et terminal incompatible.
5. **TUI-4 — conversation et saisie multiligne** : streaming unique, scroll, saisie exacte, soumission sans toucher au REPL classique.
6. **TUI-5 — métriques et diagnostic** : intégrer le `MetricCollector` UI-4 uniquement après fusion ou port contrôlé de sa branche validée.
7. **TUI-6 — sécurité terminal et résilience** : sanitizer de toutes les surfaces, pseudo-TTY, signaux récupérables, restauration RAII, erreurs de renderer.
8. **TUI-7 — journal JSONL** : seulement après arbitrage du chemin, de la taille et de la politique de panne; Landlock inchangé.
9. **TUI-8 — régression et clôture** : build/revue/tests complets, CLI classique, rapport et limitations résiduelles.

Chaque phase doit recevoir sa propre autorisation. Le choix FTXUI, la file exacte et la sélection CLI/TUI restent ouverts; SPEC-UI-001 v2.0 et son rapport TUI-0 décrivent ces points comme propositions.

**Complément TUI-0 :** le protocole comparatif et les recommandations de cycle
de vie/queue sont détaillés dans
[`2026-11-05-tui-0-poc-protocol.md`](../reports/2026-11-05-tui-0-poc-protocol.md).

## H. Réconciliation TUI après TUI-4A — observation au 2026-11-07

> Cette section est une photographie ajoutée après fusion de PR #30. Elle
> n'efface ni ne renumérote le séquencement candidat historique de la section
> G, ni les phases de SPEC-UI-001 v2.0. Référence de code : `main` au SHA
> `51ae20867357b1b873657585b70d813f7f328194`; qualification Ghostty et compteurs
> manuels/automatisés ci-dessous sont rapportés par le mainteneur.

### État fusionné et écarts de séquencement

| Fonctionnalité | Statut réel au SHA | Preuve | Limitation | Jalon restant |
|---|---|---|---|---|
| POC terminal ncursesw | Réalisé avant intégration produit | Rapports TUI-POC-1 à 3; historique référencé par TUI-3 | Qualification Ghostty rapportée sur le prototype, SSH et autres terminaux non qualifiés | Aucun pour le POC; ses limites restent historiques |
| Pont asynchrone et `AgentWorker` | Fusionné et utilisé par la TUI | `include/ui/agent_worker.hpp`, `src/ui/agent_worker.cpp`, `TuiApplication` | `Agent::send()` n'est pas annulable; fermeture peut attendre | Résilience et cycle de vie futurs selon arbitrage |
| TUI ncursesw de production | Fusionnée : session terminal, worker, événements, conversation, saisie, restauration | `src/ui/terminal_session.cpp`, `src/ui/tui_application.cpp`, `src/ui/tui_event_loop.cpp` | Essais réels varient selon le terminal | Déjà intégré; validation continue |
| Six vues et navigation TUI-4A | Fusionnées : Conversation, Outils, Métriques, Diagnostics, Configuration, Aide | `include/ui/tui_navigation.hpp`, `src/ui/tui_view.cpp`; PR #30 au SHA courant | Certaines vues sont des projections minimales; listes mémoire sans plafond | Enrichissement facultatif, sous décision distincte |
| Conversation et composition multiligne | Intégrées; F9, streaming, scroll et conservation du brouillon | `TuiEventLoop`, `TuiView`, tests TUI-3/TUI-4A | Transcript et saisie n'ont pas de plafond de mémoire constaté | Décider si une politique de rétention UI est nécessaire |
| Vue Outils | Schémas déclarés et événements observés (nom, statut, durée) | `TuiViewData`, `TuiEventLoop` | Pas d'arguments/résultats; historique d'activité non borné | Petit incrément possible : rétention bornée et indicateur d'évictions |
| Vue Diagnostics | Catégories et anomalies réellement observées, messages génériques expurgés | `TuiEventLoop::handle_event`, `TuiView::add_diagnostic` | Pas de journal corrélé complet; liste non bornée | Même incrément possible que l'historique Outils |
| Vue Configuration | Modèle, origine URL expurgée, timeout, présence de clé; schémas outil | `TuiViewData` et `TuiApplication` | Valeur clé cachée; workspace non transmis à la vue | Pas de modification sans besoin validé |
| Vue Métriques / `MetricCollector` | Vue affiche explicitement indisponible; collecteur absent de l'arbre `main` | Projection TUI; inventaire Git de `main` au SHA | Aucun agrégat accessible à cette version | Jalon séparé après revue/autorisation d'une intégration de collecteur |
| JSONL persistant | Non intégré | Aucun composant logger TUI dans l'arbre `main` | Politique et accès de stockage non décidés | Proposition SPEC ultérieure, autorisation nécessaire |
| Qualification Ghostty TUI-4A | Réussite manuelle rapportée 20/20 par le mainteneur; tests automatisés rapportés | [`rapport de qualification`](../reports/2026-11-07-tui-4a-ghostty-qualification.md) | Pas de reproduction indépendante; SSH/autres terminaux non vérifiés | Qualification distincte pour nouveaux environnements |

Le séquencement exécuté diffère des propositions écrites antérieurement :
SPEC-UI-001 v2.0 attribue navigation/vues à TUI-3, conversation/compositeur à
TUI-4, puis métriques/diagnostics à TUI-5; la section G de cette roadmap
regroupait aussi les six vues dans son candidat TUI-3. L'exécution réelle a
livré l'infrastructure/conversation en TUI-3, puis menu et six vues en TUI-4A.
Les numéros normatifs et propositions historiques restent inchangés; la
nouvelle étiquette TUI-4B ci-dessous est une proposition de découpage, pas une
renumérotation ratifiée.

### Prochain incrément — propositions

**Proposition recommandée — TUI-4B : rétention bornée de l'activité Outils et Diagnostics.** Petit changement de présentation utilisant seulement les noms/statuts/durées déjà observés. Définir d'abord des plafonds d'entrées et/ou d'octets; retirer les plus anciennes entrées au-delà du plafond; afficher un compte exact d'entrées omises, sans déduire d'activité manquante. Le mécanisme reste en mémoire, sans journal persistant ni modification du moteur. Prérequis : arbitrage mainteneur sur les limites et le comportement d'éviction. Tests : séquence sous/au-dessus du plafond, ordre conservé, compte exact d'évictions, sanitisation, navigation/scroll et absence d'arguments/résultats. Critères : mémoire de ces deux listes bornée par les limites choisies, aucune perte silencieuse et mêmes événements moteur.

Cette proposition **ne borne pas la mémoire totale de la TUI** : le transcript `messages_` et le brouillon `input_` sont également conservés sans plafond observé. Leur rétention touche directement au texte de conversation/saisie et devrait faire l'objet d'un arbitrage distinct; aucune limite arbitraire n'est proposée ici.

**Option distincte — intégration métriques (jalon TUI-5 selon SPEC).** Prérequis : disposer d'une version du `MetricCollector` revue et intégrée à `main`, puis définir les champs affichés et leur disponibilité. Sources potentielles déjà prévues par `AgentEvent` : durées monotones, nombre de requêtes/outils/deltas, octets réponse et verdict technique. Les deltas ne sont pas des tokens; pas de tokens/s sans mesure fiable. Tests déterministes sur durées, tours échoués, absence de mesures et absence de valeurs artificielles. Les fichiers UI-4 locaux non fusionnés ne sont ni une source approuvée ni une dépendance de la présente base.

L'enrichissement des vues Outils/Diagnostics et l'intégration d'un collecteur sont deux décisions séparées. Aucune phase suivante n'est autorisée par cette réconciliation.

---

## Annexe historique — anciennes sections A et B antérieures à M3

> **ARCHIVE UNIQUEMENT.** Les occurrences d'« actuel », « vérifié », et les compteurs **107/376**, **106/372**, **75/298**, **66/264** ci-dessous se rapportent aux états antérieurs des documents, et non au SHA `add64b893ad447e76fb12b90e266ec8eab2f2956`. Ce passage est conservé pour ne pas perdre le contexte des réconciliations précédentes.

## A. FAITS — ce qui est fait et où

| Fonctionnalité | Statut | Preuve |
|---|---|---|
| REPL (bannière, `exit`/EOF, ligne vide ignorée) | implémenté | `src/main.cpp` |
| Config 4 variables env, tous défauts, aucune obligatoire | implémenté | `src/main.cpp` (`load_config`, `timeout_from_env`) |
| POST OpenAI non-streamé | implémenté, **sans appelant en production** — conservé (décision P3.1), couvert par 4 tests | `http_client.cpp:post()` |
| POST streaming + callback par fragment | implémenté | `http_client.cpp:post_streaming()`, `perform_request()` |
| Parsing SSE générique (frame `\n\n`/`\r\n\r\n`, commentaires, multi-`data:`) | implémenté | `src/sse_parser.cpp` |
| Reconstruction du message depuis les deltas (content + tool_calls par `index`) | implémenté | `src/agent.cpp` (`StreamingMessageAccumulator`) |
| Invariant anti-message-partiel en historique | implémenté | `src/agent.cpp` (`append_message` après `post_streaming`) |
| Affichage progressif + indicateur `[appel de <outil>...]` | implémenté | `src/main.cpp` (callbacks) |
| Historique borné, troncature par tours complets | implémenté | `src/agent.cpp:trim_history()` |
| Boucle de dispatch d'outils, plafond 5 itérations | implémenté | `src/agent.cpp:send()` |
| 4 outils sandboxés (`list_files`, `read_file`, `search_files`, `write_file`) | implémenté | `src/workspace_tools.cpp` |
| Sandbox strict (absolu/`..`/évasion rejetés) | implémenté | `sandbox_path()` |
| `write_file` : garde-fou `overwrite`, taille max, écriture atomique, permissions préservées | implémenté | `87eee25`, `116f61f` |
| `agent_core` (lib) + `agent` (exe fin) | implémenté | `CMakeLists.txt`, `47ccea9` |
| Docs `README.md`, `USAGE.md`, `AGENTS.md`, `CLAUDE.md` | implémenté | HEAD |
| Suite de tests versionnée (micro-harnais maison + mock HTTP/SSE) | **réalisée** (P1.1/P2) | `tests/` (11 fichiers) ; `tests/CMakeLists.txt` ; `AGENTS.md` §Test |
| Exécution de la suite : 6 groupes CTest, **107 cas / 376 assertions** après ajout du test de lien sortant | **vérifiée : 0 échec** | `-DAGENT_BUILD_TESTS=ON` + `agent_tests` |
| CI (intégration continue) | **absente** | `git ls-files` (aucun workflow) |

**Correction (état actuel, P1.1/P2/R0).** La suite de tests est désormais
**versionnée** et **exécutable depuis le dépôt** (`-DAGENT_BUILD_TESTS=ON` puis
`ctest`) ; la validation ne dépend plus de harnais temporaires. Elle couvre
notamment le streaming, la troncature d'historique, le sandbox, les outils et
l'écriture atomique.

**Compteurs — ne pas confondre les états.** Le total a évolué : **66 cas /
264 assertions** au relevé **P3.3** (avant R0), porté à **75 cas / 298
assertions** par **R0** (qui ajoute 9 cas). Le chiffre **66/264** ne vaut donc
que pour l'état **P3.3** ; la référence R0 **75/298** est historique ; après `31f3e2a`, `AGENTS.md` documente **106/372 sur 6 groupes**.

**État historique (audit à `4f33b62`).** Aucune suite de tests n'était alors
versionnée et les validations reposaient sur des harnais **hors dépôt**,
supprimés après usage : c'est ce constat, **aujourd'hui périmé**, qui fondait
[`OPEN_QUESTIONS.md`](../questions/OPEN_QUESTIONS.md) Q1. La **CI** reste
**absente** (fait actuel).

## B. FAITS — dettes techniques et travaux inachevés (constatés, non jugés)

Classés par preuve, sans priorisation (la priorisation est en §D).

1. ~~`HttpClient::post()` : code mort applicatif~~ — **RÉSOLU (P3.1, décision A :
   conservé)**. Fait inchangé : aucun appelant en production (`grep` : seul
   `post_streaming` est utilisé par `Agent`). Mais l'API est **conservée**
   (délégation à `perform_request()`, partagé) et **déjà couverte par 4 tests**
   (`tests/http_client_test.cpp`). Ce n'est plus une dette ouverte (voir
   [`PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md`](../reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md)).
2. ~~**`SseParser` n'a pas de `flush()`**~~ — **RÉSOLU (Phase R0)** :
   `SseParser::flush()` restitue désormais la dernière trame non terminée, et
   l'agent exige le marqueur `[DONE]` pour accepter un flux comme complet
   (`AgentError` sinon).
3. **Chemins SSE exercés** : séparateur CRLF et concaténation de lignes
   `data:` sont couverts dans `tests/sse_parser_test.cpp`.
4. **Complétude du flux** : `finish_reason == "length"` est refusé et
   le marqueur `"[DONE]"` est requis (`src/agent.cpp`).
5. **Commentaire obsolète** : `src/agent.cpp:147` mentionne
   `extract_assistant_message()`, fonction supprimée par `a462373`.
6. **`list_files` sans plafond** de nombre de résultats (à comparer aux 50 de
   `search_files`) : la sortie peut être arbitrairement grande et part dans
   le contexte du modèle.
7. **Branche morte dans `list_files`** : `if (entry.is_directory()) {
   continue; } continue;` — les deux branches font la même chose.
8. **`search_files` non borné par fichier/ligne** : lit des lignes entières
   sans limite de longueur, sans garde de taille de fichier, et reparcourt
   tout le workspace à chaque appel.
9. ~~**Pas de `CURLOPT_CONNECTTIMEOUT`**~~ — **RÉSOLU (Phase R0)** :
   `CURLOPT_CONNECTTIMEOUT` est posé (`min(timeout, 10 s)`) et un timeout produit
   un message distinct.
10. **Limites non configurables** : `max_history_messages = 20`,
    `max_tool_iterations = 5`, `max_read_chars`, `max_write_chars`,
    `max_search_results` sont des `constexpr` de compilation.
11. **`write_file` sans `fsync`** avant `rename` : l'atomicité *de visibilité*
    est garantie (rename), la **durabilité après crash** ne l'est pas. `[FAIT
    de code]` ; l'interprétation est en §C.
12. **Pas de mode ajout** dans `write_file` (remplacement complet uniquement).
13. **Mélange de langues** : l'UI est en français (`main.cpp`), les messages
    d'erreur d'outil renvoyés au modèle sont en anglais
    (`src/workspace_tools.cpp`), et l'historique peut donc contenir les deux.
14. **`AGENTS.md` interne incohérent localement** : la section s'intitule
    toujours « Décisions d'architecture (v1) » alors que des révisions ont eu
    lieu ; la puce « Historique … troncature simple (drop des plus anciens) »
    contredit partiellement la puce suivante (tours complets). Ligne 92 :
    espace en fin de ligne.
15. **Aucun outillage de style** : `AGENTS.md` impose un style C++ mais il
    n'existe ni `.clang-format` ni licence.
16. **Le refactor `agent_core` est désormais exercé par la suite de tests** : les tests agent et REPL sont présents dans `tests/`. L'affirmation « aucun test n'existe » est obsolète depuis `31f3e2a`.
17. **`.env` local** contient un `LLM_API_KEY` non vide (valeur non
    inspectée) : un serveur réel est configuré localement, sans qu'aucun
    serveur/modèle ne soit documenté dans le dépôt.
18. **Le sandbox dépend du répertoire de lancement** : `workspace` est résolu
    par `std::filesystem::absolute("workspace")` (CWD), pas depuis la racine
    du projet. Documenté dans `USAGE.md`, mais c'est une dépendance implicite.

## Mise à jour UX LOFITERM v1.1 — 2026-11-10

La mission UX v1.1 est exécutée dans la branche isolée
`feat/lofiterm-ux-v1.1`, basée sur
`46d653fe9e788c6410129b7224edd33d870af593`. Cette note complète l'historique
ci-dessus sans renuméroter ni effacer les jalons antérieurs. Le lot couvre
l'identité conversationnelle LOFITERM, les rubriques Configuration/Aide, la
souris ncursesw et la présentation Markdown/code dans F1; il ne modifie ni le
moteur ni les règles de sécurité. La visionneuse `.md` de F2 reste reportée :
les primitives de lecture workspace sont internes à `workspace_tools.cpp`, et
les dupliquer ou les extraire ferait dépasser le périmètre de présentation.
L'état final et les critères réellement vérifiés figurent dans
[`le rapport UX v1.1`](../reports/2026-11-10-lofiterm-ux-v1.1-implementation.md).

Les tableaux TUI antérieurs à cette note sont des photographies datées et ne
décrivent plus le HEAD courant : au SHA de base `46d653f`, `MetricCollector`
est intégré et la vue F3 est alimentée par les événements de l'agent. Les
rapports physiques UI-4 et LOFITERM restent des observations datées, pas une
qualification automatique du présent changement UX.

