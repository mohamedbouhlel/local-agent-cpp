# SPEC-UI-001 — Architecture de l'interface console et de l'observabilité

**Version :** 1.1
**Date :** 2026-11-05
**Statut :** orientations ratifiées pour préparation documentaire ; détails signalés « proposition » ou « ouvert » restent à arbitrer avant leur implémentation.
**Base d'observation :** `23b04f582711905c96ff6c5122046145d68e237a` (merge de la PR #15).
**Périmètre :** interface console, événements, état de présentation, progression, métriques, diagnostics et journaux.
**Hors périmètre :** modification des décisions du modèle, TUI plein écran, annulation coopérative et changement des permissions système.

Cette spécification fixe l'architecture cible dès le départ et prévoit une réalisation par phases. Elle ne constitue pas à elle seule une autorisation de commencer UI-1. Les comportements explicitement retenus par le mainteneur sont « ratifiés » ci-dessous ; les détails non tranchés restent des propositions.

## 1. Contexte, objectifs et limites

Le projet possède une CLI C++20, `agent_core`, le streaming SSE, les callbacks `AgentCallbacks`, `PromptReader`, quatre outils workspace et le confinement Linux Landlock. La CLI rend actuellement les deltas et les retours d'outils directement dans `src/main.cpp`. Il n'existe pas encore de modèle de présentation indépendant, de détection TTY/couleur, de collecteur de métriques ou de journal JSONL.

Objectifs : rendre visibles les transitions réellement observées, séparer moteur et rendu, préserver la sortie simple et les pipelines, faciliter les diagnostics, et fournir des métriques traçables. L'interface ne prouve pas la réussite fonctionnelle d'une tâche sur la seule base du texte du modèle.

Non-objectifs : TUI plein écran, animation dans un thread, annulation simulée, dépendance externe supplémentaire, sonde réseau ou génération implicite au démarrage, modification des prompts ou des tool calls.

## 2. Décisions ratifiées et invariants

Les orientations suivantes sont ratifiées pour cette architecture : architecture complète définie dès le départ et mise en œuvre progressive ; contrat canonique d'événements typés ; conservation des trois callbacks historiques avec une voie d'observation distincte et compatible ; publication synchrone en première version ; isolation des exceptions des nouveaux observateurs ; comportement des callbacks historiques conservé ; aucun succès sans transition réelle ; pas de TUI plein écran, de thread d'animation ou d'annulation fictive ; conservation exacte du REPL multiligne ; prise en charge de TTY, SSH et sortie redirigée ; neutralisation des séquences terminal dangereuses ; durées mesurées par horloge monotone ; journaux persistants JSONL désactivés par défaut et alimentés par allowlist de métadonnées ; Landlock inchangé ; séparation des journaux d'exécution et rapports d'ingénierie ; aucune nouvelle dépendance externe.

Invariants :

- **INV-01 — Historique et protocole :** ne jamais valider comme réponse complète un message assistant partiel après échec réseau ; préserver l'ordre et l'appairage assistant/tool ainsi que les `tool_call_id`.
- **INV-02 — Streaming :** préserver l'ordre de réception des deltas SSE, le marqueur `[DONE]` obligatoire et le rejet de `finish_reason="length"`. Une réponse incomplète n'est jamais présentée comme terminée.
- **INV-03 — Outils :** un résultat de présentation reflète l'exécution effective. Une erreur d'outil reste observable même si le modèle poursuit.
- **INV-04 — Écriture :** `write_file` refuse actuellement `overwrite:true` jusqu'à l'isolation système de l'identité. Ne pas modifier cette décision, les écritures atomiques ni leur validation pendant UI-1.
- **INV-05 — Sandbox :** Landlock, validation des chemins, protections symlink/hard-link et permissions ne sont pas élargis pour les besoins UI ou logs.
- **INV-06 — REPL :** respecter `REPL-01` à `REPL-09` et les arbitrages A-1 à A-9, dont `:multi`, `:end`, `exit` exact en simple, CRLF normalisé uniquement dans le bloc, EOF qui abandonne le bloc, et aucune nouvelle ligne finale implicite.
- **INV-07 — Secret :** aucune clé API, aucun en-tête d'authentification, prompt, corps de réponse ou contenu/argument d'outil dans les logs persistants par défaut.
- **INV-08 — Honnêteté :** distinguer opération exécutée, succès technique et résultat fonctionnel vérifié. Le texte du modèle ne constitue pas une preuve d'exécution.
- **INV-09 — Terminal :** aucune séquence ANSI en sortie non interactive ou non compatible ; traiter tout texte externe comme données, jamais comme contrôle terminal.
- **INV-10 — Réversibilité :** le rendu enrichi peut être désactivé sans désactiver le moteur.

## 3. Architecture cible et frontières

```text
Agent / HttpClient / SSE / ToolRegistry
                 │ observation typée
          AgentEventObserver (sync)
                 │
       PresentationController ── MetricCollector
          │             │
    RichRenderer   PlainRenderer   StructuredLogger (optionnel)
```

Le moteur publie les faits et conserve sa logique métier. L'adaptateur d'événements maintient la compatibilité avec l'API historique. Le contrôleur met à jour un état dérivé et distribue vers métriques et renderers. Les renderers n'appellent pas le moteur et n'influencent pas ses décisions. Le logger sérialise une projection autorisée, sans accès aux flux bruts.

Les noms exacts des types, la durée de vie du sink, l'injection dans `Agent::send` et le mécanisme d'adaptateur sont **propositions à valider en UI-1**. Aucun changement de signature publique ne doit être effectué avant revue de compatibilité.

## 4. Contrat événementiel C++20

### 4.1 Forme logique proposée

Contrat conceptuel (noms indicatifs, pas encore API ratifiée) :

```cpp
struct EventMetadata {
    std::uint32_t schema_version;
    EventType type;
    std::chrono::system_clock::time_point timestamp_utc;
    std::chrono::steady_clock::duration elapsed;
    SessionId session_id;
    std::optional<TurnId> turn_id;
    std::optional<RequestId> request_id;
    std::optional<ToolCallId> tool_call_id;
    std::uint64_t sequence;
};

struct UIEvent {
    EventMetadata metadata;
    EventPayload payload; // std::variant de payloads typés
};
```

`EventType` est un enum stable dans le code ; la représentation JSON utilise des noms stables en minuscules avec version de schéma. `EventPayload` est un `std::variant` dédié, sans dictionnaire libre ni sérialisation d'objets arbitraires. L'identifiant de session est opaque ; le format et le générateur restent **ouverts**. L'horodatage UTC sert à la lecture ; seules les durées fondées sur `steady_clock` servent aux mesures.

### 4.2 Événements et charges utiles

| Événement | Payload minimal proposé | Source / limite actuelle |
|---|---|---|
| `SessionStarted`, `SessionEnded` | mode de sortie, résultat de fin observé | démarrage/fin CLI |
| `RequestStarted` | numéro de tour, longueur d'entrée éventuellement omise | `Agent::send` |
| `HttpRequestStarted` | numéro d'itération | appel HTTP effectif |
| `HttpResponseStarted` | statut HTTP | à instrumenter ; définition exacte à ratifier |
| `TextDelta` | fragment éphémère en mémoire | callback de delta existant; ne pas persister le texte |
| `ToolStarted` | nom outil | juste avant handler effectif |
| `ToolSucceeded`, `ToolFailed` | nom, durée, état technique | handler terminé; résultat exclu des logs |
| `HttpRequestFinished` | durée, statut, octets reçus, issue | mesures à instrumenter |
| `WarningRaised` | code et catégorie | avertissement effectivement émis |
| `RequestFailed` | code/catégorie et étape | exception classée par source |
| `TurnFinished` | état technique final et compteurs | terminaison réelle du tour |
| `SessionMetrics` | métriques agrégées autorisées | optionnel, à décider |

`ToolProgress` n'est pas prévu dans la première version : aucun outil actuel ne publie un avancement mesuré. Les tokens et tokens/s ne sont affichés que si le serveur fournit une mesure à sémantique confirmée. Le temps jusqu'au premier delta de contenu ne doit pas être nommé « temps jusqu'au premier token » sans preuve.

### 4.3 Ordre, corrélation et transitions

Proposition : publication synchrone, ordonnée par un compteur monotone `sequence` au niveau session. Chaque requête HTTP possède un `request_id`; chaque appel outil garde son `tool_call_id`; événements de tour possèdent `turn_id`. Les débuts précèdent leurs fins ; une opération possède au plus un terminal. Événement terminal signifie seulement que l'opération s'est terminée selon son contrat technique.

État proposé : `IDLE → SUBMITTED → WAITING → STREAMING` (si un delta arrive) et/ou `TOOL_RUNNING → WAITING`, puis `COMPLETED` ou `FAILED`. `WAITING` couvre les périodes sans signal intermédiaire ; aucun état `CANCELLED` en l'absence d'annulation implémentée. Les transitions précises autour des en-têtes HTTP, tool calls invalides et callbacks défaillants sont **à ratifier avant UI-1**.

## 5. Compatibilité et exceptions des observateurs

Garder sans changement les champs et sémantiques historiques `on_text_delta`, `on_tool_call`, `on_tool_result` et `ToolOutcome`. Ajouter une voie distincte d'observation typée, facultative. La stratégie de surcharge/constructeur et l'adaptateur sont des **propositions** ; prévoir un test de compatibilité qui prouve le même ordre et les mêmes valeurs aux anciens consommateurs.

Les exceptions des nouveaux observateurs UI ne traversent jamais un callback C libcurl. Elles sont interceptées à la frontière C++ de publication ; une politique proposée désactive l'observateur défaillant et signale au mieux un diagnostic non récursif sur `stderr`. La politique exacte (désactivation par événement ou par session) est **ouverte**. Les exceptions des callbacks historiques conservent leur comportement actuel ; ne pas les avaler silencieusement ni changer leur propagation sans décision séparée. Toute exception issue du code appelé par le callback C libcurl doit néanmoins être contenue à cette frontière pour respecter l'ABI C, sans transformer une erreur en succès.

## 6. Présentation terminal et capacités

Modes conceptuels : riche interactif, texte simple, sortie structurée. Détection TTY, `TERM=dumb`, `NO_COLOR`, capacité Unicode et largeur terminal ; fallback ASCII lorsque nécessaire. La détection et la priorité explicite des options restent à spécifier. Aucun ANSI en sortie redirigée. Échapper/neutraliser ESC, séquences CSI/OSC, contrôles C0/C1 et séquences dangereuses dans les données externes avant rendu ; le choix exact de représentation (suppression ou échappement visible) reste ouvert et doit être testé.

Palette sémantique proposée : bleu info, vert succès constaté, ambre avertissement, rouge erreur, violet activité modèle, gris secondaire. Les couleurs ne portent pas seules l'information. Pas de plein écran, pas de thread spinner, pas d'animation concurrente avec le flux. Aucun numéro de version ni état de connectivité non vérifié ; pas de sonde réseau implicite. SSH suit les règles TTY/capacités du terminal attaché.

## 7. Progression et métriques

Utiliser `std::chrono::steady_clock` pour durée de tour, requêtes HTTP, outils et latence jusqu'au premier delta observé. Compter seulement les requêtes, itérations, appels/outils échoués, octets reçus et avertissements effectivement observés. Une durée en cours peut être mesurée au moment de l'affichage ; sans rafraîchissement asynchrone, elle n'est pas animée en continu.

Progression indéterminée : libellé d'activité, sans pourcentage. Progression déterminée uniquement avec `total > 0` observé et unités homogènes. Les itérations consommées sont un budget, pas un pourcentage d'achèvement. Aucune estimation d'ETA ou de tokens/s sans source réelle.

## 8. Journaux JSONL et confidentialité

La persistance JSONL est **désactivée par défaut**. JSONL est un format d'événements, pas la sortie conversationnelle machine : séparation stdout/stderr/fichier et options CLI exactes restent **ouvertes**. Chaque ligne contient une version de schéma, type stable, horodatage, séquence, corrélateurs disponibles, état et payload autorisé. Sérialisation explicite depuis une allowlist ; aucun dump générique d'événement.

Allowlist initiale proposée : noms/type d'événement, codes stables, statuts, durées, nombres, taille reçue et identifiants opaques. Exclure par défaut clé API, Authorization, URL avec credentials, prompts, deltas, corps HTTP, arguments et résultats d'outils. Taille d'événement bornée. Redaction d'URL à spécifier/tester. Politique de rotation, nom/chemin, permissions et défaillance disque restent ouverts.

Landlock n'est jamais élargi pour écrire des logs. Si une persistance est retenue, le chemin doit rester dans les accès déjà permis, probablement sous `workspace/`; un fichier caché n'est pas privé, car un outil peut le lire s'il connaît son chemin. La séparation des logs d'exécution dans workspace et des rapports d'ingénierie `docs/reports/` est obligatoire. Aucun log ne doit être traité comme preuve d'audit inviolable.

## 9. Commandes locales et contrat REPL

Commandes candidates, interprétées exclusivement en mode simple : `/help`, `/status`, `/tools`, `/metrics`, `/trace`, `/clear`, `/config`, `/quit`. `/reset` est exclu. `//` est proposé pour transmettre un slash initial littéral. Sémantique de `/clear` sans terminal, format et contenu de `/status` et échappement `//` restent à ratifier avant implémentation.

En mode multiligne, chaque ligne est du contenu jusqu'à `:end` conformément à la spécification REPL approuvée ; aucune commande UI ne s'intercale, aucun trim, aucune normalisation hors de celle déjà ratifiée, aucun saut de ligne final ajouté. Les commandes ne sont jamais envoyées au LLM.

## 10. Diagnostics et codes

Codes candidats : `CFG_INVALID`, `HTTP_CONNECT_FAILED`, `HTTP_TIMEOUT`, `HTTP_STATUS_ERROR`, `SSE_INCOMPLETE`, `LLM_PROTOCOL_ERROR`, `TOOL_UNKNOWN`, `TOOL_ARGUMENT_INVALID`, `TOOL_EXECUTION_FAILED`, `WORKSPACE_ACCESS_DENIED`, `WORKSPACE_WRITE_FAILED`, `AGENT_ITERATION_LIMIT`, `UI_RENDER_FAILURE`, `LOG_WRITE_FAILURE`. La classification ne doit pas parser des chaînes d'exception fragiles. L'API actuelle `HttpError` est principalement textuelle ; l'ajout de catégories structurées est une proposition d'UI-5, hors du premier contrat UI-1 sauf nécessité démontrée. Chaque diagnostic expose cause observée, opération, impact et résolution si connue ; pas de secret.

## 11. Matrice exigences / tests

| Exigence | Composant | Invariant | Test / critère d'acceptation |
|---|---|---|---|
| Événements corrélés et ordonnés | observer/adaptateur | INV-01 à 03 | séquence, début/fin, IDs tool préservés ; aucune transition fabriquée |
| Compatibilité callback | adaptateur Agent | INV-01, 03 | anciens callbacks reçoivent mêmes valeurs et ordre qu'avant |
| Isolation callback UI | frontière observer/HTTP | INV-02, 08 | exception UI n'échappe pas à callback C ; callback historique conserve sa politique |
| État et verdict technique | contrôleur | INV-03, 08 | échecs/incomplet restent distincts d'une fin réussie |
| TTY, couleurs et fallback | capabilities/renderers | INV-09, 10 | TTY, SSH simulé, redirection, `TERM=dumb`, `NO_COLOR`, ASCII, largeur étroite |
| Neutralisation terminal | renderers | INV-09 | ESC/CSI/OSC/C0/C1 des données n'exécutent ni n'émettent de séquence active |
| REPL et commandes | dispatcher/PromptReader | INV-06 | tests REPL existants + tests mode simple, slash littéral, bloc intact |
| Métriques et progression | MetricCollector | INV-08 | horloge monotone, compteurs de sources réelles, aucun pourcentage sans total |
| JSONL et allowlist | logger | INV-05, 07 | JSON par ligne, schéma stable, secret et contenus absents |
| Landlock et logs | logger/sandbox | INV-05 | permissions inchangées, chemins permis seulement, échec explicite |
| Retour plain sans UI | renderer/CLI | INV-10 | moteur utilisable sans renderer riche ni logger |

Les tests précis sont à ajouter par phase ; aucune ligne de cette matrice ne prétend qu'un test UI existe déjà.

## 12. Plan progressif UI-1 à UI-8

| Phase | Périmètre et fichiers indicatifs | Dépendances / sortie |
|---|---|---|
| UI-1 — Contrat et instrumentation | nouveaux `include/ui/ui_event.hpp`, `src/ui/ui_event.cpp` si sérialisation requise ; `include/agent.hpp`, `src/agent.cpp`, `src/http_client.cpp`, CMake, tests Agent/HTTP | API observer revue, callbacks préservés, ordre/corrélation testés, exceptions C-boundary contenues |
| UI-2 — État et capacités | `presentation_state`, `terminal_capabilities`, tests UI | dépend UI-1; transitions pures, TTY/fallback |
| UI-3 — Rendu et CLI | renderers plain/rich, `src/main.cpp` | dépend UI-2; streaming propre et sanitizer |
| UI-4 — Progression/métriques | `metrics`, tests horloge/compteurs | mesures observables uniquement |
| UI-5 — Erreurs | classification et `ErrorPresenter` | codes stables, aucun parsing fragile non documenté |
| UI-6 — JSONL | logger, sérialisation, tests privacy/échec | choix chemin/permissions/rotation requis ; Landlock inchangé |
| UI-7 — commandes REPL | dispatcher, tests `PromptReader` et CLI | commandes simple seulement, bloc intact, `/reset` absent |
| UI-8 — clôture | tests d'intégration, docs et rapport final | régression, terminal, confidentialité, perf mesurée et verdict |

Les noms de fichiers sont indicatifs jusqu'à l'examen détaillé de chaque phase. Le périmètre de chaque phase nécessite une autorisation distincte ; une phase ne vaut pas autorisation pour la suivante.

## 13. Risques, retour arrière et acceptation finale

Risques principaux : exception propagée depuis un callback libcurl ; altération du contenu SSE ou du REPL ; contrôle terminal injecté par une donnée ; fuite de secrets via JSONL ; conflit de permissions Landlock ; métriques interprétées comme réussite fonctionnelle ; ajout involontaire d'événements au chemin historique.

Retour arrière par phase : retirer l'adaptateur/renderer/logging nouvellement ajouté en préservant l'API historique, puis relancer la suite existante. Ne jamais supprimer ou réécrire l'historique utilisateur pour revenir en arrière. Chaque phase conserve son rapport durable et un diff limité.

Acceptation finale : tests historiques et UI réussis sur la base retenue ; aucun ANSI en sortie plain ; streaming, tool-call et historique conformes ; exceptions UI contenues ; REPL inchangé ; mesures fondées ; JSONL valide, off par défaut, allowlist et secrets absents ; Landlock inchangé ; rapport indiquant séparément observé, non vérifié et échec. Aucun serveur LLM réel n'est requis pour prétendre à une validation unitaire, et aucun résultat de tâche modèle n'est certifié par l'UI seule.

## 14. Registre des décisions

| Sujet | Statut |
|---|---|
| 20 orientations listées au §2 | **RATIFIÉES comme contraintes d'architecture** ; pas une autorisation d'implémentation |
| Trois callbacks historiques conservés ; voie distincte d'événements | **RATIFIÉ** ; signatures exactes de la nouvelle API ouvertes |
| Publication synchrone, observer UI isolé, politique legacy conservée | **RATIFIÉ** ; portée de désactivation d'un observer défaillant ouverte |
| Événements et charges utiles du §4 | **CONTRAT PROPOSÉ** ; cycle exact et sémantique de réponse HTTP à ratifier |
| IDs, sérialisation et ordre par séquence | **PROPOSITION** ; générateur/format des IDs ouvert |
| Modes de sortie, capacités et sanitizer | **ORIENTATION RATIFIÉE** ; détection/option precedence et politique d'échappement ouvertes |
| Métriques monotones, aucune fausse progression | **RATIFIÉ** ; présentation des durées en direct ouverte |
| JSONL désactivé, allowlist, Landlock inchangé | **RATIFIÉ** ; chemin, permissions, rotation, panne et CLI ouverts |
| Commandes simples, `//`, pas de `/reset` | mode simple et exclusion `/reset` ratifiés ; `//` reste une proposition à confirmer |
| Codes diagnostiques et états exacts | **PROPOSITION** ; modèle d'erreur et nomenclature à valider |

## 15. Sources internes

- [`AGENTS.md`](../../AGENTS.md) — architecture et invariants au HEAD de base.
- [`USAGE.md`](../../USAGE.md) — outils, écriture et limites opérationnelles.
- [`repl-multiline-requirements.md`](repl-multiline-requirements.md) — contrat REPL approuvé.
- [`ROADMAP.md`](../roadmap/ROADMAP.md) — faits, propositions et limites documentées.
- [`OPEN_QUESTIONS.md`](../questions/OPEN_QUESTIONS.md) — décision Q20 sur `overwrite:true`.
- [`2026-11-05-ui-0c-specification-ratification.md`](../reports/2026-11-05-ui-0c-specification-ratification.md) — preuves de cette ratification documentaire et résultats de validation.
