# PROJECT_HISTORY.md — reconstitution chronologique

> Source : `git log --reverse --numstat` + inspection des diffs à chaque
> commit (les messages de commit sont **vérifiés**, pas seulement lus).
> 27 commits, `main`, 2026-09-21 22:26:19 → 2026-09-22 14:07:50 (+0200).
> Toutes les affirmations ci-dessous sont adossées à un diff examiné.

## Vue d'ensemble — 7 phases

| Phase | Commits | Objet |
|---|---|---|
| P0 | `62dc6ce` | squelette + plan v1 inscrit dans AGENTS.md |
| P1 | `5b40cdd`, `422a7cc`, `cf96329`, `dd6848d` | cœur REPL non-streamé |
| P1b | `bf987b7`, `3a19f2d` | typage des erreurs HTTP, config documentée |
| P2 | `1db2a3c`, `7f0fcec`, `654344e` | outils lecture sandboxés + tool-calling |
| P2b | `bf85ca1`, `00e8f32` | troncature tour-aware, rejet des binaires |
| P3 | `d44ad82`, `b457469` | documentation (limitation + création de USAGE.md) |
| P4 | `e1ba5a3`, `cdb8810`, `bee17e9`, `87eee25`, `116f61f`, `c8467fe` | `write_file` + durcissement (atomicité, permissions) + docs |
| P5 | `47ccea9`, `8f015bf` | extraction de `agent_core` |
| P6 | `e0b1771`, `fcf11cb`, `a462373`, `40dc6c4`, `4f33b62` | passage en streaming SSE |

## P0 — `62dc6ce chore: init project structure for agent-cpp`

Diff examiné : crée `CMakeLists.txt` (21 l.), `.env.example` (3 l.),
`.gitignore` (9 l.), `README.md` (24 l.), `AGENTS.md` (65 l.), `CLAUDE.md`
(2 l.), et **deux fichiers vides** : `include/agent.hpp`, `src/main.cpp`.

**Fait notable** : le plan v1 est déjà écrit dans `AGENTS.md`, avant tout
code. Il contient une section « Décisions d'architecture (v1) » avec quatre
décisions :

1. *Streaming : désactivé (`stream: false`)*, mais « http_client doit quand
   même être conçu avec une interface qui pourrait accepter un callback de
   streaming plus tard, **sans réécriture complète** ».
2. *Historique borné à 20 messages*, « troncature simple (drop des plus
   anciens), pas de résumé ».
3. *Tool-calling : hors scope de cette itération.*
4. *Erreurs réseau : message clair mentionnant `LLM_BASE_URL`.*

Ces quatre points constituent l'ossature du plan implicite ; les trois
premiers seront **révisés** (voir § « Décisions vs révisions »).

`.env.example` initial : `LLM_MODEL=` **vide**, pas de
`LLM_TIMEOUT_SECONDS`.

## P1 — cœur REPL non-streamé

- `5b40cdd feat(http): add minimal libcurl POST wrapper` — `http_client.hpp`
  (14 l.) + `http_client.cpp` (113 l.) : RAII curl (`CurlGlobal`,
  `CurlHandle`, `CurlHeaders`), `write_callback` accumulant la réponse,
  `CURLOPT_TIMEOUT`. Les échecs sont des `std::runtime_error`.
- `422a7cc feat(tools): add inert tool registry infrastructure` —
  `tool.hpp`, `tool_registry.hpp`, `tool_registry.cpp` : `struct Tool`,
  registre `std::map`, `schemas_json()`. Le message dit « inert » à juste
  titre : aucun outil n'est encore enregistré, aucun dispatch n'existe.
- `cf96329 feat(agent): add conversation agent with bounded history and
  OpenAI-compatible send()` — `agent.hpp` (32 l.) + `agent.cpp` (71 l.) :
  `AgentConfig`, `Message`, historique borné (drop des plus anciens dans
  `append_message`), payload `{model, messages, stream:false}`, extraction de
  `choices[0].message`.
- `dd6848d feat(cli): add REPL loop with clear error on unreachable server` —
  `main.cpp` (71 l.) : `getenv_or_default`, `timeout_from_env`, bannière,
  invite `> `, `exit`, ligne vide ignorée, capture unique
  `std::exception` avec message anglais `Unable to contact the local LLM
  server at ...`.

## P1b — erreurs typées et config documentée

- `bf987b7 fix(errors): distinguish HTTP failures from unexpected
  responses` — introduit `struct HttpError : std::runtime_error` dans
  `http_client.hpp`, fait lever `HttpError` au lieu de `runtime_error`, et
  scinde la capture dans `main.cpp` en deux branches. **Le message d'erreur
  passe de l'anglais au français** (`Impossible de contacter LLM_BASE_URL
  (...)`, `Reponse du serveur inattendue: ...`).
- `3a19f2d docs: document LLM defaults and timeout` — `.env.example` :
  `LLM_MODEL=` → `LLM_MODEL=local-model`, ajout de
  `LLM_TIMEOUT_SECONDS=60`. `README.md` et `AGENTS.md` documentent les 4
  variables et leurs défauts. `AGENTS.md` « Current state » passe de
  « Structure de build uniquement » à « REPL minimal fonctionnel … registre
  d'outils inerte ».

## P2 — outils de lecture sandboxés et tool-calling

- `1db2a3c feat(tools): add sandboxed workspace file tools` — crée
  `workspace_tools.hpp`/`.cpp` (237 l.) avec **déjà** `sandbox_path`,
  `canonical_workspace_root`, `is_inside_workspace`, `is_hidden_path`, et les
  trois outils `list_files`, `read_file`, `search_files` avec leurs
  constantes (`max_read_chars = 50'000`, `binary_probe_bytes = 4096`,
  `max_search_results = 50`). Ajoute `workspace/.gitkeep` et ignore
  `workspace/` dans `.gitignore`. `appears_binary` existe **déjà** mais
  n'est utilisé que par `search_files`.
- `7f0fcec feat(agent): add tool_call dispatch loop` — `agent.cpp` : boucle
  `max_tool_iterations`, `has_tool_calls`, exécution via `tools_.execute`,
  conversion des erreurs d'outil en `"tool error: " + what`, message
  `role:tool` avec `tool_call_id`, et `throw runtime_error("tool call loop
  exceeded maximum iterations")`. `tool_registry.hpp` gagne `empty()`.
- `654344e feat(cli): register workspace tools on startup` — `main.cpp`
  crée `<cwd>/workspace` (`std::filesystem::absolute("workspace")` +
  `create_directories`) et appelle `register_workspace_tools`.

## P2b — deux corrections issues de l'usage réel

- `bf85ca1 fix(agent): make history truncation tool-call aware` — diff
  examiné : l'ancienne troncature dans `append_message` (drop brut des plus
  anciens) est **remplacée** par `trim_history()`, qui découpe **par tours
  complets** en repérant les messages `role=user`. `trim_history()` est
  appelé en tête de boucle et avant chaque `return`. Motivation inscrite dans
  le code : ne pas casser l'appairage `assistant (tool_calls)` →
  `tool (tool_call_id)`. C'est la révision de la décision v1 « troncature
  simple ».
- `00e8f32 fix(tools): reject binary files in read_file` — diff : `read_file`
  appelle `appears_binary` et lève `cannot read binary file: <chemin>`, et
  `appears_binary` est **déplacé** avant `read_file` (le numstat 18/15 est un
  déplacement + ajout, pas une réécriture). Motivation implicite : envoyer du
  binaire au LLM n'a pas de sens.

## P3 — documentation

- `d44ad82 docs(agents): note oversized-turn history limitation` —
  documente la limitation de `trim_history()` (le dernier tour utilisateur
  n'est jamais retiré, même surdimensionné) comme **compromis assumé**, et
  signale que le dépassement devient plus probable si `max_tool_iterations`
  augmente.
- `b457469 docs(usage): add usage guide covering config, REPL, tools, and
  limits` — crée `USAGE.md` (206 l.) et ajoute un renvoi depuis `README.md`.

## P4 — `write_file` et durcissement

- `e1ba5a3 feat(tools): add sandboxed write_file tool` — +81 l. dans
  `workspace_tools.cpp` : `write_file` avec `max_write_chars = 200'000`,
  garde-fou `overwrite`, rejet si la cible est un répertoire, création des
  parents, message `wrote N bytes to <chemin> (created new file |
  overwrote existing file)`. **À ce stade, l'écriture ouvre la cible
  directement en `std::ios::trunc`** — donc perte de données possible en cas
  d'échec en cours d'écriture.
- `cdb8810 docs(agents): fix stale tool-calling status note` — remplace la
  puce « Tool-calling : hors scope » (devenue fausse) et corrige aussi
  « registre d'outils inerte » dans « Current state ». *Note d'audit* : le
  message de commit ne mentionne que la note tool-calling, mais le diff
  corrige deux formulations obsolètes.
- `bee17e9 docs(usage): document write_file tool` — section `write_file` dans
  `USAGE.md` (+39 l.), compteurs « trois » → « quatre », ajout d'une puce de
  limite de taille d'écriture.
- `87eee25 fix(tools): make write_file atomic via temp file + rename` —
  diff : introduction de `temporary_path_for()` (`<nom>.tmp.<ticks
  steady_clock>.<compteur atomic>`) et du garde RAII `TemporaryFileGuard`
  (suppression du temporaire si `commit()` n'a pas été appelé) ; l'écriture
  passe par le temporaire puis `std::filesystem::rename`. L'ordre des
  validations est préservé.
- `116f61f fix(tools): preserve file permissions on write_file overwrite` —
  ajout de la capture `std::filesystem::status(path, ec).permissions()` dans
  un `std::optional` **uniquement si la cible existe**, puis
  `std::filesystem::permissions(temp, perms, ec)` juste avant le `rename`.
  Une création garde les permissions par défaut.
- `c8467fe docs: note hard-link caveat of atomic write_file` — documente dans
  `AGENTS.md` et `USAGE.md` le compromis **assumé** : sur écrasement, un hard
  link est rompu par le `rename`.

## P5 — refactor de build

- `47ccea9 refactor(build): split into agent_core library and thin agent
  executable` — diff de `CMakeLists.txt` uniquement : `agent_core` STATIC
  (include et dépendances **PUBLIC**), `agent` réduit à `src/main.cpp` lié en
  PRIVATE, suppression du `target_include_directories` redondant. Aucun
  fichier source touché.
- `8f015bf docs(agents): document agent_core library structure` — section
  « Architecture cible » remplacée par « Architecture », et note ajoutée
  dans « Test » (un harnais peut lier `agent_core`).

## P6 — streaming SSE

- `e0b1771 feat(http): add streaming POST with chunk callback` — diff :
  ajout de `StreamSink` + `streaming_write_callback`, et **extraction** du
  corps de `post()` dans un helper commun `perform_request()` (numstat
  56/10). `post()` est conservé à l'identique fonctionnellement.
- `fcf11cb feat(http): add generic SSE event parser` — nouveaux
  `include/sse_parser.hpp` (21 l.) + `src/sse_parser.cpp` (107 l.), sans
  dépendance à nlohmann/json ; `src/sse_parser.cpp` ajouté à `agent_core`.
- `a462373 feat(agent): stream responses with tool-call feedback`
  (153/9) — `stream: true`, suppression de `extract_assistant_message`, ajout
  de `ToolCallAccumulator`, `StreamingMessageAccumulator`,
  `apply_tool_call_delta`, `apply_stream_delta`, `build_assistant_message`,
  `AgentCallbacks`, et de la signature `send(input, callbacks = {})`.
  Normalisation des `arguments` vides en `"{}"`. Les appels `append_message`
  sont placés **après** le retour de `post_streaming`, ce qui matérialise
  l'invariant anti-partiel.
- `40dc6c4 feat(cli): stream final answers and show tool call feedback` —
  callbacks dans `main.cpp`, `streamed_text` pour décider d'un `\n` avant le
  message d'erreur.
- `4f33b62 docs: document streaming behavior and tool-call feedback` —
  `AGENTS.md` : puce streaming réécrite (+ puce « Invariant streaming ») et
  « Current state » mis à jour ; `USAGE.md` : affichage progressif et
  indicateur d'outil.

## Décisions vs révisions (traçabilité)

| Décision initiale (v1, `62dc6ce`) | Statut | Révision |
|---|---|---|
| Streaming désactivé, interface prête pour un callback « sans réécriture complète » | **révisé** | `e0b1771`+`a462373` : streaming actif. La prédiction « sans réécriture complète » n'a été **que partiellement** tenue : `post()` n'acceptait pas de callback, il a fallu extraire `perform_request()` (`e0b1771`, numstat 56/10). |
| Historique borné, « troncature simple (drop des plus anciens) » | **révisé** | `bf85ca1` : troncature **par tours complets**, tool-call aware ; limitation résiduelle documentée en `d44ad82`. |
| Tool-calling hors scope | **révisé** | `7f0fcec` : implémenté ; note obsolète corrigée en `cdb8810`. |
| Erreurs réseau : message clair mentionnant `LLM_BASE_URL` | **tenu** | `bf987b7` (typage `HttpError`), renforcé en `40dc6c4` (message sur sa propre ligne après du texte streamé). |
| — | **ajout non planifié** | 4 outils sandboxés, puis `write_file` avec garde-fou, atomicité et préservation des permissions (P2, P4). |
| — | **ajout non planifié** | Extraction de `agent_core` (P5) — motivée par la reproductibilité des harnais de test. |

## Discipline de commit observée

Convention Conventional Commits respectée sur les 27 commits
(`chore`, `feat`, `fix`, `docs`, `refactor`) — seul `build` prévu par
`AGENTS.md` n'est pas utilisé (le refactor de build est passé en
`refactor(build)`). Commits atomiques, chacun compilable. La documentation
est mise à jour **dans la même itération** mais dans des commits `docs`
séparés.

## Vérification message ↔ diff

Les diffs des commits structurants (`62dc6ce`, `bf987b7`, `3a19f2d`,
`bf85ca1`, `00e8f32`, `87eee25`, `116f61f`, `47ccea9`, `e0b1771`,
`a462373`) ont été examinés : **aucune divergence majeure** entre message et
contenu. Deux nuances relevées :

- `cdb8810` annonce ne corriger que la note tool-calling, mais corrige aussi
  « registre d'outils inerte » (même intention, périmètre légèrement plus
  large).
- `00e8f32` apparaît en 18/15 : c'est un déplacement de fonction + un ajout,
  pas une réécriture.
