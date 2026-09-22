# Audit — état réel du streaming (avant toute nouvelle implémentation)

> **Passe de contrôle en lecture seule.** Aucune ligne de code modifiée, aucun
> correctif appliqué, même là où une dette est constatée. Un seul fichier créé :
> ce rapport.
>
> **Correction de prémisse** : le contexte fourni (« le dernier état confirmé est
> le refactor CMake `47ccea9`/`8f015bf` ; le streaming n'est pas confirmé ») est
> **en retard**. Le streaming a été implémenté, committé **et validé** depuis
> (§2), et la documentation a été migrée vers `docs/`. L'objectif reste
> inchangé : établir ce qui existe réellement, sans rien implémenter.
>
> Référence : branche `main`, HEAD `4f33b623c17ff6e23e7b38694300128d51a9d418`.
> Méthode : lecture des fichiers à HEAD, `git log`, build, test de fumée, et un
> mock SSE hors dépôt (un scénario, texte fragmenté). Toute affirmation ci-dessous
> est adossée à un fichier/ligne ou à une sortie observée.

## 1. État constaté par composant

### 1.1 `include/http_client.hpp` + `src/http_client.cpp` — streaming côté HTTP

**Une méthode de streaming existe, à côté de `post()`.** Signatures exactes
(`include/http_client.hpp`) :

```cpp
// :16
std::string post(const std::string& url, const Headers& headers,
                 const std::string& body, long timeout_seconds) const;

// :24-28
std::string post_streaming(const std::string& url, const Headers& headers,
                           const std::string& body, long timeout_seconds,
                           const std::function<void(const std::string&)>& on_data) const;
```

Implémentation (`src/http_client.cpp`) :

| Élément | Ligne | Rôle |
|---|---|---|
| `write_callback` | `:51` | callback non-streamé (accumule le corps) |
| `struct StreamSink` | `:58` | `{std::string* body; const std::function<void(const std::string&)>* on_data;}` |
| `streaming_write_callback` | `:63` | accumule **et** appelle `on_data(fragment)` |
| `perform_request` | `:84` | setup curl commun ; `CURLOPT_WRITEFUNCTION` `:117`, `CURLOPT_WRITEDATA` `:118` |
| `HttpClient::post` | `:138-146` | chemin non streamé, **conservé** |
| `HttpClient::post_streaming` | `:148-158` | construit un `StreamSink` (`:155`) et retourne le corps complet (`:156`) |

Erreurs : `HttpError` (`include/http_client.hpp:8`) sur échec curl et sur statut
non-2xx ; le corps accumulé est inclus dans le message. `CURLOPT_TIMEOUT` posé
(`:116`) ; **pas de `CURLOPT_CONNECTTIMEOUT`**.

### 1.2 `include/sse_parser.hpp` + `src/sse_parser.cpp` — framing SSE

**Les deux fichiers sont présents.** Interface (`include/sse_parser.hpp:12-20`) :

```cpp
class SseParser {
public:
    std::vector<std::string> feed(const std::string& chunk);
private:
    std::string buffer_;
};
```

Logique réellement implémentée (`src/sse_parser.cpp`) :

| Aspect | Implémentation | Ligne |
|---|---|---|
| Bufferisation inter-chunks | `buffer_ += chunk` puis boucle jusqu'à épuisement | `:82`, `:86-98` |
| Séparateur d'événement | ligne vide `\n\n` **ou** `\r\n\r\n` ; un `\n` final isolé n'est **pas** un séparateur (attente de la suite) | `:14-30` |
| Extraction `data:` | lignes `data:` uniquement, préfixe retiré, espace initial optionnel ; commentaires `:` et champs `event:`/`id:`/`retry:` ignorés | `:35-53` |
| Multi-`data:` | plusieurs lignes `data:` d'un même événement concaténées par `\n` | `:65-73` |
| `[DONE]` | **retourné tel quel** — le parser ne le traite pas ; c'est l'appelant qui le reconnaît (commentaire `include/sse_parser.hpp:10-11`) | — |
| `flush()` | **absent** : un événement final non terminé par une ligne vide reste bufferisé et est perdu | — |

Aucune dépendance à `nlohmann/json` : le parser ne connaît pas le schéma OpenAI.

### 1.3 `src/agent.cpp` — payload et reconstruction

- **Payload : `stream: true`** — `src/agent.cpp:50` (`{"stream", true}`). **Aucune
  occurrence de `stream: false`** dans le dépôt.
- **L'ancien chemin non-streamé n'est plus utilisé** : `extract_assistant_message`
  n'existe plus ; il n'est mentionné que dans un commentaire (`:146-147`). La
  colonne vertébrale actuelle est :

| Élément | Ligne | Rôle |
|---|---|---|
| `struct ToolCallAccumulator` | `:60` | `id`, `name`, `arguments` |
| `struct StreamingMessageAccumulator` | `:70-73` | `content` + `std::map<std::size_t, ToolCallAccumulator> tool_calls` (accès **par index**) |
| `has_tool_calls` | `:75` | détection `tool_calls` non vide |
| `apply_tool_call_delta` | `:82` | `index` requis (entier ≥ 0) sinon delta **ignoré** ; `id` **assigné**, `name`/`arguments` **concaténés** |
| `apply_stream_delta` | `:115` | gardes `choices`/`delta` ; `content` non vide → accumulation + `on_text_delta` (`:133-135`) |
| `build_assistant_message` | `:148` | assemble `{role:"assistant", content, tool_calls?}` ; `arguments` vide normalisé en `"{}"` (`:167`) |
| appel streaming | `:209-233` | `client.post_streaming(...)` ; `[DONE]` ignoré (`:218`) ; JSON invalide ignoré (`:225-229`) |
| invariants | `:205-208`, `:235-241` | `append_message` de l'assistant **après** le retour de `post_streaming` ; `trim_history()` `:198`,`:240` |

### 1.4 `include/agent.hpp` — callbacks

**La structure existe** (`include/agent.hpp:25-28`) :

```cpp
struct AgentCallbacks {
    std::function<void(const std::string&)> on_text_delta;
    std::function<void(const std::string&)> on_tool_call;
};
```

**Signature exacte** (`:34`) : `std::string send(const std::string& input, const AgentCallbacks& callbacks = {});`
— callbacks **optionnels** (défaut `{}`), donc `send(input)` reste utilisable.

### 1.5 `src/main.cpp` — affichage du REPL

**L'affichage est progressif** (pas d'un bloc) :

| Comportement | Ligne |
|---|---|
| `on_text_delta` → `std::cout << fragment; std::cout.flush();` | `:75-79` |
| `on_tool_call` → `std::cout << "[appel de " << name << "...]\n";` | `:80-82` |
| `send(input, callbacks)` puis `std::cout << "\n"` | `:85-86` |
| Nouvelle ligne avant le message d'erreur si du texte a déjà été affiché (`streamed_text`) | `:90-92`, `:96-98` |
| Erreurs sur `stderr` uniquement | `:93`, `:99` |

**Feedback pendant les tool_calls : oui** — une ligne `[appel de <outil>...]` est
émise avant l'exécution de chaque outil.

**Constat annexe** : `[appel de ...]` est écrit sur **`stdout`** (`:81`), donc
mélangé au flux du texte de réponse.

### 1.6 `CMakeLists.txt`

`src/sse_parser.cpp` **est compilé dans `agent_core`** — `CMakeLists.txt:19-25`,
ligne `:22`. `agent_core` expose `include/` (`:26`) et `CURL::libcurl` +
`nlohmann_json` en PUBLIC (`:27`) ; l'exécutable `agent` ne lie que `agent_core`
(`:32`).

### 1.7 Support TTY / couleurs

**Aucun** : `grep` sur `src/` et `include/` ne trouve ni `isatty`, ni `TERM`, ni
code ANSI/échappement, ni mot-clé de couleur. La sortie utilisateur est
exclusivement `std::cout`/`std::cerr` en texte brut (`src/main.cpp:33,60,64,76,81,86,93,99`).

## 2. Commits liés — trouvés vs attendus

Tous les commits annoncés comme attendus **existent** :

| Attendu | Présent | Hash |
|---|---|---|
| `feat(http): add streaming POST...` | **oui** | `e0b1771 feat(http): add streaming POST with chunk callback` |
| `feat(http): add generic SSE...` | **oui** | `fcf11cb feat(http): add generic SSE event parser` |
| `feat(agent): stream responses...` | **oui** | `a462373 feat(agent): stream responses with tool-call feedback` |
| `feat(cli): stream final answers...` | **oui** | `40dc6c4 feat(cli): stream final answers and show tool call feedback` |
| `docs: document streaming...` | **oui** | `4f33b62 docs: document streaming behavior and tool-call feedback` |

`git log --oneline` : **27 commits**, sommet `4f33b62`, branche `main` (aucun
remote, un seul worktree). Le refactor CMake cité dans le contexte est à
`47ccea9`/`8f015bf`, soit **5 commits sous le sommet** : il y a donc bien eu des
développements postérieurs non reflétés dans le contexte fourni.

## 3. Cohérence doc ↔ code

| Document | Affirmation | Verdict |
|---|---|---|
| `AGENTS.md:60-64` | « Streaming : **actif (stream: true)** … consommées en SSE (`src/sse_parser.cpp`, framing uniquement) … tool_calls accumulés **par index** dans `src/agent.cpp`. Feedback minimal : `on_tool_call` → `"[appel de <outil>...]"` » | **Cohérent** avec §1.3/1.4/1.5. Aucune mention résiduelle de `stream: false` (`grep` : une seule occurrence de « stream » dans la section, en `:60`). |
| `AGENTS.md:65-68` | « Invariant streaming : si la requête échoue en plein flux … le message assistant partiellement reconstruit n'est **JAMAIS** ajouté à l'historique » | **Cohérent** : `src/agent.cpp:205-208` + `append_message` après `post_streaming` (`:235-241`). |
| `USAGE.md:11` | « `POST /v1/chat/completions`, **en streaming SSE** … affiche la réponse au fur et à mesure » | **Cohérent**. |
| `USAGE.md:76-84` | « La réponse s'affiche **progressivement** … une ligne `[appel de <nom d'outil>...]` s'affiche pendant l'exécution » | **Cohérent** avec `src/main.cpp:75-82`. |

**Aucune documentation obsolète constatée sur le streaming.** Aucun décalage
« doc dit `stream: false` / code fait du streaming », ni l'inverse.

## 4. Build et test de fumée (résultats observés)

| Contrôle | Commande | Résultat |
|---|---|---|
| Build | `cmake --build build` | **rc=0** — `[ 75%] Built target agent_core` / `[100%] Built target agent` (aucune recompilation : artefacts à jour) |
| Fumée | `printf 'exit\n' \| ./build/agent` | **rc=0** — bannière `agent-cpp ready. Type 'exit' to quit.` + invite `> ` puis sortie propre. **Aucun appel réseau** (sortie avant tout `send`) |
| **Streaming de bout en bout** | mock SSE hors dépôt (1 scénario, texte fragmenté, écritures TCP de 5 o espacées de 4 ms) + **binaire réel** `build/agent` | **rc=0**, texte attendu **PRESENT**, et **14 lectures distinctes** de `stdout` espacées d'environ 55 ms ⇒ **affichage réellement progressif, calé sur la cadence du mock** |

Extrait de la chronologie observée (arrivée des octets sur `stdout`) :

```
     7 ms  "agent-cpp ready. Type 'exit' to quit.\n> "
    59 ms  "Bonj"
   115 ms  "our,"
   170 ms  " cec"
   ...
   722 ms  "ente"
   796 ms  "\n> "
```

Les artefacts du mock (script + répertoire de travail) ont été créés **hors
dépôt** (`/tmp`) et **supprimés** après usage ; le dépôt n'a pas été touché.

## 5. Écarts identifiés

| # | Écart | Catégorie | Sévérité | Preuve / portée |
|---|---|---|---|---|
| E1 | **Aucun écart sur le streaming lui-même** : fonctionnalité présente, câblée, documentée et fonctionnelle de bout en bout | — | **INFO** | §1, §2, §4 |
| E2 | Le **contexte de la demande était obsolète** de 5 commits + toute la phase tests/documentation ; le streaming n'était pas « non confirmé » mais **livré** | constat de coordination, pas de code | **INFO** | §2 (`47ccea9` est 5 commits sous HEAD) |
| E3 | `SseParser` **sans `flush()`** : un événement final non terminé par une ligne vide est silencieusement perdu | fonctionnalité incomplète (cas limite) | **MINEUR** | `src/sse_parser.cpp` (aucun `flush`) ; comportement **testé et consigné** comme observé (`tests/sse_parser_test.cpp`) ; sans impact avec un serveur qui termine ses événements et envoie `data: [DONE]\n\n` |
| E4 | `buffer_` SSE **non borné** (croissance mémoire si un serveur n'émet jamais de ligne vide) | risque potentiel | **MINEUR** | `src/sse_parser.cpp:82,98` ; non reproduit |
| E5 | `HttpClient::post()` **conservé mais plus appelé** par `Agent` (code mort applicatif, mais **testé**) | dette technique | **MINEUR** | `src/http_client.cpp:138-146` ; aucun appel dans `src/agent.cpp` |
| E6 | `finish_reason` **jamais lu** : l'arrêt repose sur `[DONE]` + fin de réponse HTTP | fonctionnalité incomplète | **MINEUR** | absent de `src/agent.cpp` |
| E7 | **Pas de `CURLOPT_CONNECTTIMEOUT`** (seul `CURLOPT_TIMEOUT`) : un hôte injoignable peut consommer tout le délai | dette technique | **MINEUR** | `src/http_client.cpp:116` |
| E8 | Streaming couvert par le **harness versionné** (groupes `sse`, `http`, `agent`, `mock` dans `tests/`) mais **pas testé contre un vrai serveur LLM**, et les helpers de reconstruction (`apply_*`, `build_assistant_message`, `trim_history`) ne sont atteignables que par l'API publique | testé mais incomplet | **MINEUR** | `tests/` (9 fichiers) ; couverture indirecte documentée dans `docs/reports/PHASE_1_1_REPORT.md` |
| E9 | `[appel de <outil>...]` écrit sur **`stdout`**, mélangé au flux du texte de réponse | choix d'affichage à arbitrer | **INFO** | `src/main.cpp:81` |
| E10 | `AGENTS.md:69-71` conserve une puce « troncature simple (drop des plus anciens) » partiellement contredite par la puce suivante (tours complets) — **sans rapport avec le streaming** | documentation imprécise (hors périmètre) | **INFO** | déjà consigné dans `docs/roadmap/ROADMAP.md` (P11) et `docs/questions/OPEN_QUESTIONS.md` (Q6) |
| E11 | Demande initiale de créer `doc/` (singulier) alors que la documentation est centralisée sous `docs/` (pluriel) | cohérence de structure documentaire | **INFO** | arbitré : rapport écrit dans `docs/reports/streaming-audit.md` |

**Aucun écart de sévérité MAJEUR ou BLOQUANT.** Aucun correctif appliqué
(conformément à la consigne) : E3 à E7 sont des constats, pas des actions.

## 6. Recommandation

**Le projet est prêt pour de nouvelles additions (couleurs ANSI, détection TTY,
indicateurs d'avancement) : le streaming est complet, câblé, documenté et
vérifié de bout en bout, et aucun écart MAJEUR ne subsiste** — les points
E3 à E7 sont des dettes mineures indépendantes de ces ajouts.

Recommandations, une justification chacune :

1. **Engager les ajouts d'ergonomie (couleurs / TTY / indicateurs) sans attendre.**
   Le point d'insertion est unique et identifié — `on_text_delta` et
   `on_tool_call` (`src/main.cpp:73-82`) — donc aucun travail préalable sur le
   streaming n'est requis.
2. **Traiter la détection TTY (`isatty`) dès le premier ajout de couleur**, car
   aucun support TTY n'existe aujourd'hui (§1.7) et une sortie colorée
   inconditionnelle polluerait les sorties redirigées et les tests, alors que le
   harness capture `stdout` tel quel.
3. **Décider du canal de l'indicateur d'outils** (E9) avant d'enrichir
   l'affichage : aujourd'hui `[appel de ...]` est sur `stdout`, ce qui le rend
   indissociable du texte de réponse pour un consommateur du flux.
4. **Ne pas conditionner ces ajouts au traitement de E3–E7** (flush SSE, buffer
   non borné, `post()` mort, `finish_reason`, `CONNECTTIMEOUT`) : ces dettes sont
   documentées, non bloquantes et indépendantes de la couche d'affichage.
5. **Poursuivre la discipline de validation existante** (mock loopback + build
   hors dépôt + groupe CTest dédié) pour tout nouvel affichage, car c'est ce qui
   a permis de démontrer ici le caractère progressif de la sortie (§4).

## 7. État Git au moment de l'audit

```bash
$ git status --short --branch
## main
 M CMakeLists.txt
?? docs/
?? tests/

$ git rev-parse HEAD
4f33b623c17ff6e23e7b38694300128d51a9d418
```

Aucun fichier de production, de configuration ou de test modifié par cette
passe. ` M CMakeLists.txt`, `?? docs/` et `?? tests/` sont **préexistants**
(itérations antérieures) et n'ont pas été touchés.
