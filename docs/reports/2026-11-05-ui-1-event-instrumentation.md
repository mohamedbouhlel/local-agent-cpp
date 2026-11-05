# UI-1 — Instrumentation événementielle

**Date :** 2026-11-05
**Branche :** `feat/ui-1-event-instrumentation`
**SHA de base :** `a986af4935ba3dea7d18339c42c3dc18651cf308` (`origin/main`, merge de la PR #16)
**État de livraison :** changements non commités ; aucun push ni merge effectué.

## Objectif

Ajouter un contrat canonique d'événements techniques C++20, observer l'activité réellement exécutée par `Agent::send()` et empêcher toute exception C++ de traverser les callbacks C de libcurl. Les renderers, commandes locales, métriques agrégées et journaux JSONL restent hors périmètre.

## Modifications

- `include/ui/agent_event.hpp` : identifiants typés de session, tour, requête HTTP et tool call ; `EventType`, payloads `std::variant`, métadonnées horodatées et `AgentEvent` avec type dérivé du payload. `TextDelta` est un contenu transitoire nécessaire au streaming ; aucun mécanisme de journalisation n'est ajouté.
- `include/agent.hpp`, `src/agent.cpp` : ajout d'un constructeur facultatif recevant un `std::shared_ptr<EventObserver>`. L'objet observateur est détenu par l'agent. L'ancienne construction et `send(input, callbacks)` restent disponibles avec leurs signatures. Les trois callbacks historiques sont conservés.
- `src/http_client.cpp` : les callbacks libcurl sont `noexcept`, retiennent `std::exception_ptr` et demandent l'arrêt du transfert en retournant zéro. Après le retour de `curl_easy_perform`, l'exception d'origine est relancée avant la traduction des erreurs curl.
- `tests/agent_events_test.cpp` et `tests/http_client_test.cpp` : couverture du contrat événementiel, des callbacks historiques et de l'exception levée pendant un callback de streaming, y compris le cas où le callback historique lève `AgentError`.
- `tests/CMakeLists.txt` : ajout du nouveau fichier de tests à la cible existante.
- `docs/README.md` : index de ce rapport.

## API observateur

```cpp
namespace agent::ui {
class EventObserver {
public:
    virtual ~EventObserver() = default;
    virtual void on_event(const AgentEvent& event) = 0;
    virtual void on_observer_failure() = 0;
};
}

Agent(ToolRegistry&, AgentConfig,
      std::shared_ptr<agent::ui::EventObserver> observer);
```

Les appels sont synchrones. Si `on_event` lève, l'observateur est désactivé pour le reste de la durée de vie de cet `Agent`; `on_observer_failure()` est appelé une seule fois et toute exception de diagnostic est interceptée. Cet appel de diagnostic doit écrire directement son diagnostic et ne pas republier un événement. La construction historique `Agent(ToolRegistry&, AgentConfig)` n'installe aucun observateur.

L'ordre global des événements est indiqué par un compteur croissant par session. Les horodatages muraux utilisent `system_clock`; les durées écoulées et les durées HTTP/outils utilisent `steady_clock`. Les identifiants de tool call sont copiés depuis les appels du modèle après validation de leur identifiant et nom. Aucun prompt, argument, résultat d'outil, corps HTTP ou clé API n'est ajouté aux métadonnées.

`HttpRequestFinished(completed)` constate uniquement le retour normal du transport. Le contrôle ultérieur de `[DONE]`, `finish_reason` et du protocole peut encore faire échouer le tour. Les échecs de handler d'outil sont signalés et transmis au modèle selon le comportement antérieur ; le tour peut ensuite aboutir normalement.

## Invariants et limites

Les modifications ne touchent pas les fichiers Landlock, les outils workspace, `write_file`, `PromptReader`, le parseur SSE, le payload OpenAI, la politique d'historique ni la limite d'itérations. La réponse assistant partielle reste exclue de l'historique en cas d'échec de transport/callback ou d'incomplétude du protocole. Les exceptions des callbacks historiques continuent de sortir de `Agent::send()` avec leur type et leur message ; elles ne sont pas supprimées.

Les événements sont synchrones et leur diffusion n'est pas destinée à un accès concurrent ou à une réentrée dans le même `Agent`. L'identifiant de session est un compteur process-local, pas un identifiant persistant ni globalement unique. Les erreurs de transport sont catégorisées comme HTTP dans ce premier contrat ; une classification plus fine est hors de UI-1.

## Vérifications exécutées

Les commandes ont été exécutées dans une construction propre hors du dépôt, `/tmp/local-agent-cpp-ui1-build`, configurée avec les sources JSON déjà présentes dans le dépôt :

```text
cmake -S . -B /tmp/local-agent-cpp-ui1-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-ui1-build
ctest --test-dir /tmp/local-agent-cpp-ui1-build --output-on-failure
./agent_tests
```

Résultats observés sur le dernier état du code de cette branche avant la seule modification documentaire de ce rapport et de son index :

- Configuration CMake : réussite ; avertissement de développement CMake existant concernant CMP0135.
- Compilation complète : réussite.
- CTest : **7/7 groupes réussis**, 0 échec.
- Harnais complet : **134/134 cas, 818 assertions, 0 échec**.
- Lors du premier passage du groupe agent, un test nouvellement écrit vérifiait les mauvais indices d'événements. Ce test a échoué ; ses attentes ont été corrigées puis le groupe et les validations complètes ci-dessus ont réussi. Aucun échec n'est omis de ce récit.
- La revue finale a aussi détecté que l'exception historique `AgentError` était conservée mais pouvait être étiquetée comme erreur de protocole dans l'événement. La classification a été corrigée, un test dédié ajouté et le build, les sept groupes CTest et le harnais complet ont été rejoués avec succès sur cet état final.

Les tests de loopback ont été exécutés avec l'autorisation nécessaire. Aucun serveur LLM réel n'a été utilisé ni requis pour ces tests déterministes.

## Périmètre et vérifications de non-régression

Le diff de code concerne `include/agent.hpp`, `src/agent.cpp`, `src/http_client.cpp`, `tests/CMakeLists.txt`, `tests/http_client_test.cpp`, ainsi que les nouveaux `include/ui/agent_event.hpp` et `tests/agent_events_test.cpp`. Aucun diff n'a été constaté dans `CMakeLists.txt`, `src/main.cpp`, les sources Landlock/sandbox, `src/workspace_tools.cpp`, `src/prompt_reader.cpp` ou `src/sse_parser.cpp`.

Les cas ajoutés couvrent notamment l'ordre et la corrélation des événements, plusieurs requêtes et outils, tool calls entrelacés, récupération après échec d'outil, flux incomplet, `finish_reason="length"`, tool call invalide, erreur HTTP, timeout, limite d'itérations, désactivation de l'observateur défaillant, diagnostic défaillant, absence de contenu sensible dans les événements inspectés, et préservation du type/message d'une exception historique à travers libcurl.

## Anomalies, réserves et statut Git

- Les tests établissent les cas exécutés ci-dessus ; ils ne prouvent pas l'absence de toute fuite ou de tout défaut hors de ces scénarios.
- Le hook `on_observer_failure()` est le point de diagnostic de l'application. Cette phase n'ajoute pas de renderer ni de diagnostic UI séparé.
- La base vérifiée est `a986af4935ba3dea7d18339c42c3dc18651cf308`. Le worktree contient les modifications UI-1 non commitées ; il n'existe donc pas de SHA de commit UI-1 à citer.
- Aucun commit, push, merge ou travail UI-2 n'a été réalisé.

## Verdict

**VALIDÉ pour le périmètre UI-1 implémenté et les validations effectivement exécutées**, sous réserve de revue du diff. La branche est prête à être examinée ; UI-1 n'est pas publiée ni fusionnée.
