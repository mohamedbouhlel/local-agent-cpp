# Revue finale UI-1 — instrumentation événementielle

**Date :** 2026-11-05
**Branche :** `feat/ui-1-event-instrumentation`
**Base vérifiée :** `a986af4935ba3dea7d18339c42c3dc18651cf308` (`origin/main`, merge PR #16)
**Cible de publication :** `main`
**État :** code et documentation validés localement ; aucune fusion effectuée.

## Objectif et précontrôle

Revoir contradictoirement l'intégralité du diff UI-1, corriger seulement un défaut démontré, exécuter une construction propre et la suite complète, puis préparer la publication par commit et PR brouillon.

`AGENTS.md`, SPEC-UI-001 v1.1 et le rapport UI-1 ont été relus. La branche active était `feat/ui-1-event-instrumentation`, HEAD égal à `a986af4935ba3dea7d18339c42c3dc18651cf308`, worktree sans changement non lié au périmètre. Après `git fetch origin main`, `origin/main` était au même SHA. Le commit est le merge de PR #16 et ses parents sont `23b04f582711905c96ff6c5122046145d68e237a` et `e7f4be57bef626b78860aa5373589d1325c81aca`. La branche UI-1 descend directement de ce HEAD.

La recherche GitHub par `feat/ui-1-event-instrumentation` n'a retourné aucune PR existante. Le remote configuré est `https://github.com/mohamedbouhlel/local-agent-cpp.git`.

## Revue contradictoire

| Point contrôlé | Classement | Constat |
|---|---|---|
| Signatures des trois anciens callbacks et de `Agent::send()` | Conforme | Les champs et signatures sont conservés ; le constructeur à deux arguments reste utilisable. Les callbacks gardent leur ordre et leurs valeurs existants. |
| Durée de vie de l'observateur | Conforme | `Agent` conserve un `std::shared_ptr`; le test confirme que l'observateur reste vivant jusqu'à la destruction de l'agent. |
| Ordre, unicité et corrélation | Conforme | Séquence croissante par session ; IDs de tour/requête et IDs de tool call validés associés aux événements. Les événements terminaux de tour sont émis une fois ; chaque requête commencée reçoit son événement terminal. |
| Échec du nouvel observateur | Conforme | Une exception désactive l'observateur pour la durée de vie de l'agent, puis appelle une seule fois `on_observer_failure`; l'exception du diagnostic est interceptée. Aucun nouvel événement n'est publié par la voie de diagnostic. |
| Frontière C libcurl et priorité de l'exception | Conforme | Les deux callbacks C sont `noexcept` et capturent `...` dans `std::exception_ptr`. Après `curl_easy_perform`, cette exception est relancée avant l'examen du code curl, qui reflète l'abandon du callback. |
| Type et message des exceptions historiques | Conforme | Le callback de streaming est testé avec un type personnalisé ; un callback qui lance `AgentError` conserve aussi son type/message et est classé comme callback legacy, pas comme erreur de protocole. |
| Données sensibles dans les métadonnées | Conforme dans le périmètre testé | Aucun prompt, argument ou résultat d'outil n'est copié dans les métadonnées ou les événements d'outil. `TextDelta` transporte le fragment généré nécessaire au streaming, mais aucun logger ni stockage persistant n'est introduit. Le test couvre des sentinelles précises ; il ne constitue pas une preuve universelle d'absence de fuite. |
| Historique, payload, SSE, `[DONE]`, `finish_reason`, tool pairing et limite d'itérations | Conforme | Les modifications d'agent ajoutent la publication et les mesures ; les règles de protocole et le traitement de l'historique restent en place. Les tests complets correspondants passent. |
| REPL, Landlock, validation workspace et politique `write_file` | Conforme | Aucun diff dans ces fichiers ni dans `src/main.cpp`, `src/prompt_reader.cpp`, `src/filesystem_sandbox.cpp`, les outils workspace ou le parseur SSE. `overwrite:true` demeure refusé selon la base. |
| Réentrée/concurrence sur un même `Agent` | Risque non démontré | La publication est synchrone et l'agent n'est pas rendu thread-safe. Aucun test ni exigence UI-1 ne certifie l'usage concurrent/réentrant. |

### Défaut confirmé corrigé

Avant correction, `tools_.execute()` ne capturait que `std::exception`. Un handler qui lançait une exception non standard faisait échouer le tour et propageait correctement l'exception, mais n'émettait pas `ToolFailed` et n'incrémentait pas le compteur d'échecs. Cela omettait une transition réellement observée.

Correction minimale dans `src/agent.cpp` : le `catch (...)` du handler calcule la durée, incrémente le compteur d'échecs, publie `ToolFailed`, puis relance l'exception sans la transformer. Aucun `ToolOutcome` historique n'est ajouté pour ce cas, préservant le comportement antérieur des exceptions non standard. Un test lance la valeur `73`, vérifie sa préservation, l'événement `ToolFailed` et le bilan du tour.

Aucun changement d'architecture ratifiée n'a été nécessaire. La correction précédente de classification d'un `AgentError` levé par un callback historique reste couverte par son test existant.

## Validation propre

Répertoire de build neuf hors dépôt : `/tmp/local-agent-cpp-ui1-review-build`. La source JSON FetchContent présente dans le dépôt a été réutilisée explicitement pour ne pas télécharger une seconde copie ; CURL détecté : `/usr/lib/x86_64-linux-gnu/libcurl.so`, version 8.14.1.

Commandes exécutées :

```text
cmake -S . -B /tmp/local-agent-cpp-ui1-review-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-ui1-review-build
ctest --test-dir /tmp/local-agent-cpp-ui1-review-build --output-on-failure
./agent_tests
```

Résultats observés :

- Configuration CMake : réussite. Avertissement de développement CMP0135 sur `DOWNLOAD_EXTRACT_TIMESTAMP`; la configuration se termine avec succès.
- Build complet : réussite, cibles `agent_core`, `agent` et `agent_tests` construites.
- CTest : **7/7 groupes réussis, 0 échec**.
- Harnais complet : **135/135 cas, 851 assertions, 0 échec**.
- Les tests loopback HTTP ont été exécutés avec l'autorisation nécessaire.
- La référence avant cette revue était 134 cas / 818 assertions et 7/7 groupes ; le nouveau cas non standard ajoute un cas et les assertions qu'il exécute. La comparaison ne suggère pas de baisse du nombre de groupes.

Contrôles Git et de contenu : `git diff --check` sans erreur ; le contrôle des espaces de fin des fichiers non suivis n'a rien trouvé ; recherche ciblée de chaînes de secret sans résultat ; diff vide sur les fichiers protégés cités ci-dessus. Avant staging, l'index Git était vide.

## Fichiers destinés au commit

- `include/ui/agent_event.hpp` (nouveau)
- `include/agent.hpp`
- `src/agent.cpp`
- `src/http_client.cpp`
- `tests/agent_events_test.cpp` (nouveau)
- `tests/http_client_test.cpp`
- `tests/CMakeLists.txt`
- `docs/reports/2026-11-05-ui-1-event-instrumentation.md`
- `docs/reports/2026-11-05-ui-1-final-review.md` (nouveau)
- `docs/README.md`

Aucun autre fichier n'est inclus. Il n'y a pas de modification de CMake racine ou de dépendance.

## Réserves et verdict

Les tests valident les scénarios listés, pas une absence générale de défauts ou de fuite. Les événements sont synchrones et l'agent n'est pas thread-safe. La génération d'identifiants de session est process-local. L'avertissement CMP0135 préexiste à UI-1.

**VERDICT : VALIDÉ pour la revue finale et les validations exécutées.** Le diff peut être commité et publié en PR brouillon. La PR ne doit pas être fusionnée sans autorisation distincte. UI-2 n'a pas commencé.
