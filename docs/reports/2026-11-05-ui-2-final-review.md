# UI-2 — Revue contradictoire finale

**Date :** 2026-11-05
**Branche :** `feat/ui-2-presentation-state`
**SHA de base :** `12cdca8d876b0238d65745dfffdf6ed3e1aab3bd`
**Référence distante vérifiée :** `origin/main` au même SHA après `git fetch origin main`
**État avant publication :** changements UI-2 locaux, non commités.

## Périmètre et méthode

Revue du diff UI-2 complet, lecture de `PresentationState`, `TerminalCapabilities`, de leurs tests, du contrat `AgentEvent` et des points d'émission UI-1. Vérification des fichiers protégés par comparaison au SHA de base. Aucune modification du moteur, du client HTTP, du parseur SSE, du REPL, des outils, de Landlock ou de la politique `write_file`.

## Constats contradictoires

| Sujet | Classement | Constat |
|---|---|---|
| Transitions et ordre | Conforme | `PresentationState` exige une séquence initiale 1, distingue doublon, séquence antérieure et trou, et ne consomme pas les événements rejetés. Les transitions sont dérivées du type et de la charge utile UI-1. |
| Corrélation | Conforme | Les événements vérifient session, tour, requête active, IDs d'outils et compteurs terminaux. Plusieurs requêtes et outils séquentiels dans un tour sont couverts par les tests. |
| Fin HTTP / fin de tour | Conforme | `HttpRequestFinished` ferme uniquement la requête et rend l'état `waiting`; seul `TurnFinished` rend le tour terminal. |
| Échec outil récupéré | Conforme | `ToolFailed` conserve le compteur d'échec puis permet la poursuite et une fin technique réussie, sans requalifier l'outil en succès. |
| Échec et état terminal | Conforme | `TurnFinished(failed)` requiert un `RequestFailed`. Un état terminal reste stable face à un événement tardif incohérent. |
| Texte du modèle | Conforme | Le texte `TextDelta` n'est pas conservé; seul un compteur est incrémenté. Aucun succès fonctionnel n'est déduit du texte. |
| Effets moteur | Conforme | Le composant ne dépend que de `AgentEvent`; il n'appelle ni Agent ni transport. Aucun fichier moteur n'est modifié par UI-2. |
| stdout / stderr | Conforme | Les deux descripteurs sont sondés séparément, et les capacités sont aussi testables par `TerminalProbe`. |
| ANSI, `TERM`, `NO_COLOR` | Conforme avec limite documentée | TTY requis et `TERM=dumb` refusé. `NO_COLOR` est exposé comme observation distincte : le futur renderer doit l'appliquer à sa politique de couleur. Aucune séquence ANSI n'est produite par ce composant. |
| Largeur | Conforme | `ioctl(TIOCGWINSZ)` est utilisé uniquement pour un TTY; largeur absente ou nulle reste inconnue. Le seuil étroit est 80 colonnes. |
| Unicode | Risque non démontré, limite explicite | TTY + TERM exploitable + locale UTF-8 indiquent une compatibilité probable, pas une garantie matérielle. Le fallback ASCII est exposé. |
| Environnement | Conforme | Détection en lecture seule de l'environnement; aucun changement des variables et aucune sonde réseau. |

**Défaut confirmé :** incohérence documentaire dans le rapport initial UI-2, qui indiquait 20 tests ciblés dans le résumé mais 18 dans la phrase sur les fixtures. Correction minimale : remplacer 18 par 20, cohérent avec les sorties de validation initiales et celles de cette revue. Aucun défaut de code confirmé.

## Fichiers UI-2

- Modifiés : `CMakeLists.txt`, `tests/CMakeLists.txt`, `docs/README.md`.
- Ajoutés : `include/ui/presentation_state.hpp`, `src/ui/presentation_state.cpp`, `include/ui/terminal_capabilities.hpp`, `src/ui/terminal_capabilities.cpp`, `tests/ui_test.cpp`, `docs/reports/2026-11-05-ui-2-presentation-state-terminal-capabilities.md`.
- Ce rapport de revue : `docs/reports/2026-11-05-ui-2-final-review.md`.

## Commandes et résultats

Configuration et compilation propres, dans `/tmp/local-agent-cpp-ui2-review-build` :

```text
cmake -S . -B /tmp/local-agent-cpp-ui2-review-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-ui2-review-build
```

Résultat : configuration et build réussis avec GCC 14.2.0 et libcurl 8.14.1. Avertissement de développement CMake CMP0135 uniquement.

```text
./agent_tests --filter=ui       # depuis /tmp/local-agent-cpp-ui2-review-build/tests
ctest --test-dir /tmp/local-agent-cpp-ui2-review-build --output-on-failure
./agent_tests                   # depuis /tmp/local-agent-cpp-ui2-review-build/tests
```

- Tests ciblés UI : **20/20 cas, 153 assertions, 0 échec**.
- CTest : **8/8 groupes réussis** (`sse`, `workspace`, `http`, `agent`, `ui`, `repl`, `mock`, `sandbox`).
- Harnais complet : **155/155 cas, 1 004 assertions, 0 échec**.
- Tests HTTP loopback : groupes `http`, `agent` et `mock` exécutés et réussis.
- `git diff --check` et `git diff --cached --check` : réussis avant création de ce rapport; à répéter sur le diff de publication final.

## Invariants vérifiés et limites

Le diff ne touche pas `src/agent.cpp`, `src/http_client.cpp`, `src/sse_parser.cpp`, `src/main.cpp`, `src/prompt_reader.cpp`, les outils workspace ni les fichiers Landlock. La politique `overwrite:true` reste inchangée. Les tests UI vérifient séquences, transitions terminales, corrélations, récupération après échec outil, détection injectée, TTY redirigé, `TERM=dumb`, `NO_COLOR`, largeur inconnue/étroite et fallback ASCII.

Limites résiduelles : la projection ne se resynchronise pas après une lacune de séquence; l'appelant doit traiter le statut de rejet. ANSI dépend de l'heuristique TTY/TERM et Unicode d'une heuristique de locale; aucun renderer n'est inclus dans UI-2. Le seul avertissement de compilation observé est CMP0135.

## GitHub et verdict

Après récupération, `origin/main` est resté à `12cdca8d876b0238d65745dfffdf6ed3e1aab3bd`, qui contient la base UI-2. La consultation GitHub de la liste des PR pour `feat/ui-2-presentation-state` a renvoyé `[]` (aucune PR existante au moment de la vérification).

**Verdict : REVUE FAVORABLE — UI-2 validée pour publication en brouillon.** Aucun défaut de code confirmé; une incohérence documentaire mineure a été corrigée. La publication ne comprend que les fichiers UI-2 et ces deux rapports. UI-3 n'a pas commencé; aucune fusion n'est autorisée par cette revue.
