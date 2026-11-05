# SPEC-UI-001 — Architecture console et TUI

**Version :** 2.0 — proposition consolidée TUI-0
**Date :** 2026-11-05
**Statut :** décision de périmètre ratifiée ; architecture backend, choix de dépendance et détails d'interaction encore proposés ou ouverts. Ce document n'autorise pas à lui seul une implémentation.
**Base d'étude :** `ba1ee2e9b3535f3e5a6b79a4d544dd5b3a2ab81f` (`origin/main`, après merge PR #21).
**Périmètre :** TUI plein écran interactive, CLI classique indépendante, événements, rendu, progression, métriques, diagnostics et journaux.
**Hors périmètre :** modification du comportement décisionnel du LLM, modification des permissions système, TUI web, exécution de commandes shell depuis l'interface.

Cette version remplace explicitement le non-objectif « TUI plein écran » de la v1.1. Le contenu technique de la v1.1 est remplacé par le contrat ci-dessous ; son existence, son ancien périmètre et ses décisions historiques restent traçables dans Git et dans le rapport TUI-0. Les décisions non expressément ratifiées ci-dessous sont des propositions, non des engagements.

## 1. Décision, contexte et objectifs

### 1.1 Décision du mainteneur

La future interface console doit pouvoir offrir une **véritable TUI plein écran**, avec menu interactif, navigation clavier, panneaux, conversation défilante et saisie multiligne. La CLI classique reste une voie distincte, utilisable avec scripts, pipes, sorties redirigées et terminaux incompatibles. La décision porte sur le produit et le périmètre, pas sur une bibliothèque précise.

### 1.2 Faits techniques au SHA de base

Le dépôt est un agent C++20. `Agent::send()` est synchrone et bloque pendant les requêtes HTTP/SSE et les appels d'outils. `AgentEventObserver::on_event(const AgentEvent&)` publie synchroniquement dans le thread de l'agent. Le chemin de présentation existant est `AgentEvent → PresentationController → PresentationState → ConsoleRenderer`; `MetricCollector` consomme les événements après acceptation de l'état. `main.cpp` conserve le `PromptReader` classique et appelle `run_console_turn()`.

Le `Agent` conserve les callbacks historiques `on_text_delta`, `on_tool_call`, `on_tool_result`, la signature `Agent::send()` et le `ToolOutcome`. L'observateur est détenu par `std::shared_ptr`; une exception de nouvel observateur le désactive pour la durée de vie de l'agent. Les événements ont des IDs de session/tour/requête/appel d'outil, un numéro séquentiel, un horodatage UTC et un elapsed monotone. `TextDelta` transporte temporairement le fragment nécessaire au streaming.

Le mode terminal courant est non plein écran : Plain/Rich, capacités terminal sondées, sanitizer des données externes. Aucun raw mode ni restauration explicite termios n'est actuellement nécessaire. Le REPL utilise `PromptReader`: `:multi`, `:end`, `exit` exact en mode simple, EOF et conservation du contenu selon REPL-01…REPL-09.

Le moteur applique Landlock sous Linux et refuse de démarrer si le confinement ne peut être établi. `write_file` refuse `overwrite:true`. Le rapport d'incident fusionné par PR #21 documente des écritures non sollicitées et des tours non convergents rapportés lors d'un essai manuel ; les causes racines restent à départager. Une TUI ne constitue ni un garde de dispatch ni une preuve de pertinence d'une action.

### 1.3 Objectifs

- Offrir une expérience TUI plein écran à l'utilisateur interactif.
- Garder une CLI classique sélectionnable et fiable pour les pipes, scripts et terminaux non compatibles.
- Garder moteur, événements, état, UI et backend terminal séparés.
- Préserver les résultats réels, les erreurs, les corrélations et la confidentialité.
- Garder l'UI réactive pendant les requêtes, sans bloquer indéfiniment un callback libcurl.
- Restaurer au mieux l'état du terminal après succès, erreur et interruption récupérable.

### 1.4 Non-objectifs et interdits

- Pas de changement des prompts, tool calls, historique, validation SSE ou décisions du modèle.
- Pas d'action depuis un écran UI qui élargit les permissions ou contourne la sandbox.
- Pas de marqueur de réussite fonctionnelle déduit du texte assistant.
- Pas de données externes rendues comme séquences terminal actives.
- Pas de contenu de prompt, arguments/résultats d'outils ou secret dans les diagnostics/logs par défaut.
- Pas d'annulation présentée comme réelle tant que le moteur ne sait pas annuler.
- Pas d'élargissement Landlock pour l'UI ou les journaux.

## 2. Statut des décisions

| Sujet | Statut |
|---|---|
| TUI plein écran avec menu, navigation clavier, panneaux et saisie multiligne | **RATIFIÉ — décision mainteneur TUI-0** |
| CLI classique indépendante, préservée pour scripts/pipes/incompatibilités | **RATIFIÉ — décision mainteneur TUI-0** |
| Préserver `Agent::send`, callbacks historiques, SSE, historique, Landlock, refus `overwrite:true` et contrat REPL classique | **RATIFIÉ — invariants de projet** |
| Frontières moteur/observation/état/UI/backend séparées | **RATIFIÉ — orientation d'architecture** |
| FTXUI comme candidat recommandé | **PROPOSITION — dépendance externe; accord explicite requis avant ajout** |
| Backend ncursesw ou terminal personnalisé | **OPTIONS comparées, aucun choix ratifié** |
| Terminal possédé uniquement par le thread UI; worker Agent séparé | **RECOMMANDATION TUI-0 — à ratifier avant POC intégré** |
| Worker persistant unique avec au plus une tâche `send` en vol | **RECOMMANDATION TUI-0 — à ratifier** |
| File de capacité fixe, non bloquante, avec signal explicite de perte | **RECOMMANDATION TUI-0 — à ratifier et mesurer** |
| Mode de sélection `auto|classic|tui`, raccourcis et disposition des panneaux | **PROPOSITIONS à arbitrer** |
| Capacités de souris, historique de session, politiques de dépassement de file | **OUVERT** |

## 3. Architecture cible

```text
                    ┌────────────────────────────┐
                    │ CLI classique (existante)  │
                    │ PromptReader + Plain/Rich  │
                    └──────────────┬─────────────┘
                                   │ Agent::send()
┌──────────────────────────────────▼─────────────────────────┐
│ agent_core : Agent / HTTP / SSE / ToolRegistry / sandbox    │
└──────────────────────────────────┬─────────────────────────┘
                                   │ AgentEvent, observer UI-1
                         ┌─────────▼──────────┐
                         │ Event bridge/queue │  candidat
                         └─────────┬──────────┘
                                   │ valeur possédée
                  ┌────────────────▼─────────────────┐
                  │ PresentationState + MetricCollector│
                  └────────────────┬─────────────────┘
                                   │ snapshot
                         ┌─────────▼──────────┐
                         │ TuiApplication    │
                         │ navigation / input │
                         └─────────┬──────────┘
                                   │
                         ┌─────────▼──────────┐
                         │ TUI backend        │
                         └────────────────────┘
```

La CLI classique et la TUI réutilisent le même moteur et les mêmes événements, mais ne rendent jamais simultanément les mêmes `TextDelta`. La TUI possède son propre modèle de saisie; elle ne réutilise pas `PromptReader` en mode interactif plein écran. `Agent::send(std::string, callbacks)` reste l'API du moteur.

### 3.1 Responsabilités

- **Moteur** : exécute les requêtes, outils et protocole; émet uniquement les événements factuels existants ou dûment instrumentés. Ne connaît ni écran ni backend.
- **Event bridge** : copie les événements reçus dans une file de valeurs possédées; ne conserve aucune référence au `AgentEvent` emprunté.
- **EventDispatcher/queue** : préserve l'ordre séquentiel et corrèle la livraison; expose fermeture, surcharge et erreur explicitement.
- **PresentationState** : projection déterministe des événements acceptés, sans effets moteur ni conservation de fragments.
- **MetricCollector** : agrégats issus uniquement des métadonnées observées et d'horloges monotones.
- **TuiApplication** : boucle propriétaire de l'état UI, du focus, de l'écran et de l'interaction clavier.
- **Backend terminal** : clavier, dimensions, écran alternatif, rendu et restauration RAII. Aucun contrôle terminal direct depuis les données modèle/outils.
- **CLI classique** : reste le repli indépendant et ne prend pas le raw mode.

## 4. Backend et compatibilité

La comparaison d'architecture TUI-0 est consignée dans le rapport. La recommandation provisoire est FTXUI, conditionnée à une approbation distincte de la dépendance, à un pin de version et à des tests sur l'environnement Linux réellement supporté. ncursesw est une alternative système mature mais de plus bas niveau. Un backend maison n'est pas recommandé sans contrainte qui l'impose.

La base CMake déclarée du projet est 3.20. Toute intégration proposée doit vérifier le CMake minimal du tag FTXUI choisi, les options de build et le mode d'acquisition reproductible. Aucun téléchargement par branche flottante. Le backend choisi doit être optionnel/configuré afin que la CLI classique reste compilable ou disponible sans mode TUI.

Le projet refuse actuellement de démarrer hors Linux en raison du confinement. Cette spécification ne déclare pas de support runtime multiplateforme. Les interfaces de présentation peuvent rester portables, mais le binaire ne doit pas désactiver Landlock pour rendre la TUI disponible.

## 5. Modèle d'événements et exécution réactive

### 5.1 Événements

Le contrat canonique reste `agent::ui::AgentEvent`; ne pas créer un type miroir TUI. Les types UI-1 actuels sont `RequestStarted`, `HttpRequestStarted`, `TextDelta`, `ToolStarted`, `ToolSucceeded`, `ToolFailed`, `HttpRequestFinished`, `RequestFailed`, `TurnFinished`. L'ordre source est `metadata.sequence`; IDs et timestamps restent inchangés. Les deltas ne sont pas persistés comme métriques ou logs.

### 5.2 Propriété et cycle de vie recommandés

**Proposition à ratifier :** un seul thread possède exclusivement le terminal, le backend, `TuiApplication`, `PresentationState` et `MetricCollector`. Aucun worker n'écrit directement dans stdout/stderr du terminal ni n'appelle le backend. Cela élimine les redraw concurrents et donne un point unique de restauration RAII.

Un worker persistant unique possède l'`Agent` et exécute au plus un `Agent::send()` à la fois. La TUI ne soumet une interaction qu'à l'état idle; elle peut garder un brouillon éditable pendant le tour mais ne lance pas de second appel concurrent. L'`Agent`, les callbacks historiques et toute donnée capturée par ceux-ci restent confinés au worker. Les objets capturés survivent jusqu'au join. L'API publique de l'agent ne devient pas thread-safe par cette composition.

Le worker utilise une tâche d'entrée à capacité un et un canal de complétion à un résultat. Cycle : démarrer thread après construction des objets capturés; accepter une tâche uniquement quand aucune n'est en vol; exécuter `send`; publier résultat technique/exception; attendre la prochaine tâche; à fermeture, refuser les nouvelles soumissions, finir la tâche en cours, demander l'arrêt, puis `join` avant de détruire l'Agent, l'observateur ou le backend. Destruction et join sont RAII, sans détacher de thread.

### 5.3 File bornée, perte et conservation des statuts — arbitrage proposé

Il est impossible, pour un flux de taille et de débit non bornés, de garantir simultanément (a) callback toujours non bloquant, (b) aucune perte, et (c) mémoire bornée. Il faut donc exposer le compromis. Recommandation TUI-0 : privilégier mémoire bornée et réactivité; une saturation peut rendre le transcript incomplet, mais ne doit jamais être silencieuse.

Le POC doit tester une file à capacité fixe en nombre d'éléments et en charge utile, avec copies d'événements dont les chaînes sont validées/limitées avant allocation. La valeur numérique de la capacité ne sera choisie qu'après mesures du débit/délai de rendu. Si une allocation peut dépasser ces bornes, le prototype ne démontre pas la borne mémoire demandée. Le callback producteur effectue un dépôt opportuniste de durée bornée; si un verrou ne peut être acquis immédiatement, l'événement texte est perdu et le flux passe en état dégradé. Aucun callback ne dort en attendant la capacité. Le choix précis entre SPSC atomique et mutex `try_lock` doit être justifié par le POC et testé avec TSAN si disponible; pas de queue lock-free présumée correcte.

Politique de surcharge proposée : arrêter de mettre en file les deltas de ce tour après le premier overflow; poser un latch atomique `display_incomplete`; ne pas tenter une resynchronisation partielle de `PresentationState` après un trou de séquence. Les évènements non terminaux suivants peuvent être omis et l'UI ne prétend plus présenter le détail complet. Les deltas ne sont pas regroupés dans la première version : coalescer des événements change leur nombre/sequence attendu par `PresentationState`/`MetricCollector` et exigerait un contrat explicite de plage de séquences et de nombre de fragments.

**Garantie proposée limitée :** réserver un emplacement fixe par tour pour le résultat final du worker (retour normal ou `exception_ptr`) et des emplacements pour `RequestFailed` et `TurnFinished` (au plus un de chacun par tour selon le contrat AgentEvent). Ces emplacements ne sont pas la queue ordinaire. Le worker remet le résultat avant d'indiquer sa complétion; le thread UI consomme la queue restante, puis les éléments réservés dans l'ordre de `metadata.sequence`, en marquant explicitement les séquences manquantes. La vue peut donc toujours annoncer l'issue technique réelle du tour, même si le texte ou des détails intermédiaires sont incomplets.

Cette garantie **ne couvre pas chaque terminal d'opération** (`HttpRequestFinished`, `ToolSucceeded` ou `ToolFailed`) pendant une saturation. Les détails individuels éventuellement omis sont signalés comme incomplets; les compteurs finaux fournis par `TurnFinished` restent présentables. Aucun succès d'outil individuel n'est fabriqué. Si une conservation exacte de tous les événements terminaux d'opération est exigée, il faut une borne amont démontrée sur leur nombre et leur taille, ou accepter de la contre-pression bloquante; ce choix reste ouvert et ne peut être promis dans la présente architecture.

### 5.4 Erreurs d'observateur et renderer

L'observateur de queue est conçu pour ne pas lancer d'exception. Si une exception survient néanmoins, UI-1 désactive définitivement cet observateur; `on_observer_failure()` ne publie pas récursivement et ne fait qu'armer un latch atomique. La TUI signale ensuite une seule fois « observabilité indisponible » depuis son thread. Le worker continue le moteur; le résultat final de `send` reste consultable par le canal de complétion. Les callbacks historiques gardent leur propagation et ne sont pas désactivés/masqués par la politique UI.

Une erreur de renderer est capturée dans le thread UI : arrêter les appels au renderer défaillant, restaurer le terminal, garder l'issue worker et utiliser stderr classique seulement si le descripteur reste disponible. Ne jamais transformer une erreur de rendu en échec d'outil ni en succès moteur.

### 5.5 Fermeture, interruption et perte de terminal

Sans annulation coopérative, la fermeture pendant `send()` ne peut pas garantir une fin rapide. La politique recommandée est « fermer après le tour courant » : l'UI cesse d'accepter de nouvelles interactions et affiche qu'elle attend; le worker continue jusqu'au retour/exception du moteur, puis est joint. Le timeout courant est par requête HTTP et le tour peut effectuer plusieurs requêtes/outils; aucun timeout global de tour n'est démontré. La durée d'attente peut donc dépasser un timeout HTTP et n'a pas de borne globale garantie.

À réception de SIGINT/SIGTERM/SIGHUP récupérable, le handler ne fait que notifier le thread UI; celui-ci restaure le terminal et applique la politique de fermeture. Un signal OS ou une terminaison forcée peut toujours tuer le processus; SIGKILL, crash noyau et perte d'alimentation ne peuvent pas être interceptés. Sur perte de terminal détectée, cesser immédiatement tout rendu, restaurer best-effort, drainer ou jeter les événements UI selon la politique bornée et attendre la fin du worker avant sortie. Si la terminaison du processus est imposée de l'extérieur, aucune restauration n'est garantie.

### 5.6 Sortie CLI non interactive

La CLI classique reste un chemin indépendant. stdin/stdout non-TTY, `TERM=dumb`, capacités insuffisantes ou `--ui=classic` ne démarrent pas la session terminal ni le worker TUI; ils utilisent le flux classique actuel, Plain lorsque redirigé, avec stdout pour la conversation et stderr pour les diagnostics. L'option explicite reste une proposition. Un échec d'initialisation TUI doit restaurer ce qui a déjà été modifié avant un repli; aucun ANSI ou contrôle plein écran ne doit être écrit dans un pipe. `--ui=tui` en environnement non compatible doit échouer clairement ou retomber en mode classic selon un choix produit à ratifier.

## 6. Cycle de vie terminal

`TuiApplication` n'initialise le mode interactif qu'après validation des descripteurs TTY et de la capacité backend. Il utilise une session RAII : acquisition partielle vérifiée, écran alternatif/raw ou cbreak/echo contrôlés, dimensions actualisées, sortie et restauration dans le destructeur. En cas d'échec de setup, l'état terminal partiellement modifié est restauré avant repli/erreur.

Les signaux récupérables doivent être relayés vers la boucle normale via un mécanisme sûr (flag `sig_atomic_t`/self-pipe ou mécanisme documenté du backend); aucun rendu complexe ni allocation dans un handler de signal. La sortie normale, exception, EOF et SIGINT/SIGTERM sont testés sous pseudo-terminal. SIGKILL, perte d'alimentation et crash noyau ne sont pas interceptables; documenter la limite et le remède manuel (`stty sane`) sans prétendre à une restauration garantie.

Le backend ne reçoit jamais de chaînes non fiables comme séquences de contrôle. Les contenus conversationnels, noms d'outils et diagnostics externes passent par la neutralisation existante ou une voie équivalente vérifiée. Les contrôles générés par le backend restent séparés des données.

## 7. UX et navigation — proposition

### 7.1 Écrans

1. **Conversation** : historique de session affiché, réponse en streaming, compositeur multiligne, état courant et activité d'outil factuelle.
2. **Outils** : noms, descriptions/schémas présentables et compteurs observés; écran consultatif, aucune commande de permission.
3. **Métriques** : données de `MetricCollector` disponibles, unités et indisponibilités explicites; aucun token/s, pourcentage ou ETA inféré.
4. **Diagnostics** : erreurs et événements techniques corrélés, sans prompt, secret, arguments ni résultat d'outil.
5. **Configuration** : modèle, endpoint expurgé, timeout, workspace et capacités terminal; jamais la valeur de `LLM_API_KEY`.
6. **Aide** : navigation, saisie, statut des touches et voie de sortie.

Le terme « historique » désigne le transcript d'affichage de la session TUI seulement. Il ne modifie pas l'historique conversationnel envoyé au LLM. Une politique de conservation/limite du transcript doit être spécifiée avant implémentation.

### 7.2 Touches proposées

- `Tab` / `Shift+Tab` : prochain/précédent focus; focus visuel explicite.
- Flèches : déplacement dans le menu si celui-ci a le focus; navigation dans la conversation ou le texte sinon.
- `Enter` : activer l'élément sélectionné; dans le compositeur, soumettre selon le raccourci confirmé ci-dessous.
- `Escape` : fermer un panneau/modal ou revenir à Conversation; ne prétend pas annuler une requête.
- `Ctrl+Enter` : proposition de soumission multiligne; `Enter` insère une nouvelle ligne. À valider avec le backend, terminaux et accessibilité.
- `Ctrl+C` : comportement à arbitrer; le choix recommandé est fermeture propre avec avertissement si requête en cours, sans prétendre l'arrêter.
- Raccourcis de navigation `1` à `6` ou touches de fonction : proposition à tester contre la saisie du compositeur.

### 7.3 Mise en page adaptative

Large terminal : menu latéral, conversation principale, panneau contextuel; terminal moyen : panneau unique avec navigation; terminal très étroit/bas : vue compacte sans colonnes simultanées, ou repli vers CLI classique si backend ne peut garantir un affichage lisible. Les seuils minimaux ne sont pas ratifiés. Le redimensionnement ne doit ni perdre le texte saisi ni modifier le transcript.

Le mode TUI est réservé à une entrée/sortie interactive compatible. La politique exacte de sélection CLI/TUI reste ouverte. Proposition : option explicite `--ui=classic|tui|auto`; `auto` n'entre en TUI que si stdin/stdout/stderr et backend passent les contrôles, sinon reste en CLI classique. Aucun mode forcé ne contourne les protections terminal ou Landlock.

## 8. Métriques et progression

UI-4 fournit déjà un collecteur dans la branche de travail TUI-0, mais ces changements ne sont pas fusionnés dans le `main` de référence. Les métriques candidate restent : durée tour, durée HTTP et outils, latence client jusqu'au premier delta, compteurs requêtes/outils/fragments, octets reçus et statut technique final. Les unités et sources sont ceux de `AgentEvent` et `steady_clock`; indisponible n'est pas zéro. Les fragments ne sont pas des tokens. Pas de tokens/s sans source fiable.

La TUI affiche uniquement des états observés : requête en cours, flux reçu, outil en cours/terminé, tour fini/échoué. La progression de génération demeure indéterminée faute de total mesuré. Pas de spinner en thread, pourcentage, ETA ou réussite fonctionnelle inventés.

## 9. Journaux et confidentialité

JSONL persistant reste désactivé par défaut. Toute sérialisation passe par allowlist. Interdits par défaut : clé API, Authorization, prompts, deltas, corps HTTP, arguments/résultats d'outils et URL avec credentials. Toute taille est bornée et les caractères dangereux sont encodés/sanitisés selon le format. Les journaux ne sont pas une preuve d'audit inviolable.

Ne pas élargir Landlock pour logs, crash dumps ou captures terminal. La TUI ne capture pas de transcript persistant par défaut. Un chemin log, permissions, rotation et panne restent une décision future. `docs/reports/` demeure réservé aux rapports d'ingénierie, séparé du workspace runtime.

## 10. Compatibilité et invariants moteur

Les invariants suivants s'appliquent à toute implémentation TUI :

1. `Agent::send`, callbacks `on_text_delta`, `on_tool_call`, `on_tool_result` et `ToolOutcome` restent compatibles.
2. Événements du modèle ordonnés par SSE; `[DONE]` requis; `finish_reason="length"` échoue; aucun assistant partiel ajouté à l'historique.
3. Appairage assistant/tool et IDs `tool_call_id` conservés.
4. Échec outil observé même si le modèle poursuit; aucun résultat fonctionnel déduit.
5. Historique du moteur et limites agentiques inchangés.
6. `PromptReader`/CLI classique préserve exactement `:multi`, `:end`, `exit`, EOF, CRLF et les lignes décrits par REPL-01…REPL-09.
7. La saisie multiligne TUI devient un seul contenu `std::string` passé à `Agent::send`; elle n'insère aucune commande dans le REPL classique.
8. Landlock et validation de chemins restent identiques; TUI n'est pas un mécanisme d'autorisation.
9. `write_file` refuse toujours `overwrite:true`; aucune approbation par menu ne change cette politique.
10. Exceptions d'observateur isolées; exceptions historiques préservées; aucune exception C++ traverse une frontière C libcurl.
11. Sanitizer protège toutes les surfaces terminal; aucune donnée non fiable ne peut déplacer le curseur, modifier le titre ou créer un lien terminal.
12. CLI classique garde stdout/stderr séparés, sans ANSI sur les sorties redirigées.

Le rapport d'incident PR #21 impose une prudence supplémentaire : l'interface peut montrer un tool call et son résultat réellement observés, mais elle ne prévient pas une action non sollicitée. Toute politique d'autorisation d'action est un sujet moteur distinct et ne fait pas partie de TUI-0.

## 11. Matrice exigences / validation

| Exigence | Composant | Invariant | Test d'acceptation futur |
|---|---|---|---|
| CLI classique toujours disponible | sélection UI / `main` | REPL-01…09 | tests CLI simples, pipelines, TTY absent, mêmes prompts et appels |
| TUI réactive pendant `send` | worker + file | Agent non concurrent; issue finale conservée; détails perdus signalés explicitement | pseudo-TTY; injecter délai HTTP et vérifier navigation; arrêt/join sans attente indéfinie dans callback |
| Ordre/corrélation | queue + `PresentationState` | séquence et IDs AgentEvent | plusieurs HTTP/outils, trous, duplicatas, surcharge |
| File mémoire bornée | event bridge + mailbox final | callback sans attente de capacité; perte explicitement dégradée | stress de saturation; `RequestFailed`/`TurnFinished` conservés par slots dédiés; opérations individuelles manquantes signalées |
| TextDelta affiché sans duplicata | renderer TUI | flux SSE inchangé | delta fractionné, tool entrelacé, erreurs et reprise |
| Erreurs UI/renderer | TuiApplication/backend | résultat moteur inchangé | renderer qui lève, dégradation, restauration terminal, statut conservé |
| Restauration terminal | TerminalSession RAII | pas de corruption durable | pseudo-TTY fin normale, exception, SIGINT/SIGTERM, setup partiel |
| Navigation et éditeur | composants TUI | contenu envoyé inchangé | Tab, flèches, Enter, Esc, Unicode, multiline, resize, focus |
| Petites dimensions | layout | pas de chevauchement ou contenu perdu | matrice tailles, seuil de repli observé |
| Données terminal hostiles | sanitizer/backend | INV-11 | ESC/CSI/OSC/DCS/C0/C1/bidi fragmentés sur chaque écran |
| Outils et métriques | projection UI | pas de permission/faux succès | erreur outil récupérée, valeurs indisponibles, aucune mutation UI |
| Confidentialité | diagnostics/logger | allowlist, secrets absents | sentinelles prompt/API/arguments/résultats absentes |
| Landlock/overwrite | moteur inchangé | permissions et refus actuels | comparaison de diff + tests sandbox/write existants |

Un pseudo-terminal vérifie le flux applicatif avec un émulateur terminal contrôlé; il ne prouve pas le comportement de tous les émulateurs, polices, lecteurs d'écran ou environnements SSH.

## 12. Roadmap proposée

Chaque phase nécessite une autorisation distincte. La décision d'avoir une TUI ne vaut pas autorisation implicite de chaque changement de code ou dépendance.

| Phase | Périmètre proposé | Critère de sortie |
|---|---|---|
| TUI-0 | Étude architecture et révision normative (présente phase) | décision, comparaison, contrat, risques et roadmap documentés |
| TUI-1 | POC backend et dépendance, sélection CLI/classic/TUI; pas de moteur | build reproductible, option de dépendance ratifiée, TTY/pseudo-TTY et restauration démontrés |
| TUI-2 | Event bridge, worker unique, queue bornée, fermeture/erreurs | ordre jusqu'au point de saturation; perte explicite; issue finale livrée; aucun engagement de conservation des événements individuels sous overload |
| TUI-3 | shell visuel, menus, focus, layout adaptatif et vues Outils/Aide/Config | clavier, dimensions, terminal incompatible, tests contrôlés |
| TUI-4 | Conversation, streaming et compositeur multiligne | texte exact une seule fois, scroll, événements/outils, erreurs partielles |
| TUI-5 | Intégration MetricCollector, vues métriques/diagnostics | mesures seulement observées et indisponibilités explicites |
| TUI-6 | Sécurité terminal, signaux, récupération et accessibilité | tests pseudo-TTY adverses, sanitisation, limites documentées |
| TUI-7 | Logs JSONL optionnels, seulement après décision dédiée | allowlist, confidentialité, Landlock inchangé et panne explicite |
| TUI-8 | régression, documentation et revue finale | CLI/TUI, tests complets, rapport, aucun invariant régressé |

UI-4 métriques/observabilité demeure le jalon en cours séparé; son statut Git doit être résolu avant de baser une implémentation ultérieure. La roadmap TUI réutilise le travail UI-1 à UI-4 lorsqu'il est effectivement intégré, sans le présumer.

## 13. Risques, retour arrière et acceptation

Risques majeurs : dépendance externe et chaîne d'approvisionnement; incompatibilité CMake/plateforme; terminal laissé en mode brut; deadlock ou perte d'événements à la saturation; exception renderer; perte du statut final; concurrence accidentelle sur `Agent`; saisie modifiée; contrôles terminal injectés; confusion entre affichage d'un outil et autorisation; interprétation de « completed » comme réussite fonctionnelle.

Retour arrière : garder la CLI classique comme chemin indépendant; isoler le backend dans une cible et un adaptateur UI; désactiver TUI par option/configuration sans modifier `agent_core`; préserver `Agent::send`; aucun changement Landlock. Si la restauration ou le transfert des événements est ambigu, ne pas activer le mode TUI par défaut.

Acceptation finale : choix backend approuvé; dépendance pin et builds reproductibles; CLI classique sans régression; TUI pilotable au clavier et résiliente aux petites dimensions; flux et tool calls inchangés; événements ordonnés jusqu'à toute perte déclarée; `RequestFailed`/`TurnFinished` et issue worker conservés par leur canal dédié; pertes de détails d'opération signalées, jamais présentées comme complètes; worker/queue sans data race ni attente indéfinie dans le callback; statut terminal toujours honnête; restauration RAII vérifiée en cas normal/exception/signaux récupérables; contenu dangereux neutralisé; Landlock et refus `overwrite:true` inchangés; secrets absents; tests CTest et pseudo-TTY réels exécutés; rapport distinguant faits, échecs et non vérifié.

## 14. Registre des décisions et questions ouvertes

| Décision ou sujet | Statut au TUI-0 |
|---|---|
| TUI plein écran, menu, navigation clavier, panneaux, saisie multiligne | **RATIFIÉ** par le mainteneur; remplace le non-objectif v1.1 |
| CLI classique indépendante | **RATIFIÉ** |
| AgentEvent canonique, callbacks historiques, moteur et sécurité préservés | **RATIFIÉ** |
| FTXUI | **RECOMMANDATION**, décision et ajout de dépendance non ratifiés |
| Thread UI seul propriétaire terminal; worker Agent unique et séquentiel | **RECOMMANDATION TUI-0**, à ratifier |
| File bornée, dépôt sans attente de capacité, flux déclaré incomplet au premier overflow | **RECOMMANDATION TUI-0**, à ratifier |
| Slots dédiés pour `RequestFailed`/`TurnFinished` et résultat worker | **RECOMMANDATION TUI-0**, à ratifier; ne préserve pas tous les terminaux HTTP/tool |
| Aucun coalescing initial des `TextDelta` | **RECOMMANDATION TUI-0**, à ratifier; évite de changer la sémantique sequence/compteurs |
| `--ui=auto|classic|tui` | **PROPOSITION** |
| Raccourcis, touche de soumission, rôle d'Escape/Ctrl+C | **OUVERT** |
| Capacité souris | **OPTIONNEL / OUVERT** |
| Limites du transcript, file, seuil de terminal compact | **OUVERT** |
| Logs persistants et chemin | désactivés par défaut; choix futur requis, Landlock inchangé |
| Autorisation déterministe des tools/mutations | sujet moteur séparé; non résolu par la TUI |

## 15. Sources et documents de référence

- [REPL multiligne approuvé](repl-multiline-requirements.md).
- [Roadmap](../roadmap/ROADMAP.md) et [rapport TUI-0](../reports/2026-11-05-tui-0-architecture-revision.md).
- FTXUI : [architecture Screen/DOM/Component et CMake](https://github.com/ArthurSonzogni/FTXUI/blob/main/doc/getting-started.md), [guide CMake](https://github.com/ArthurSonzogni/FTXUI/blob/main/doc/installation_cmake.md), [changelog](https://github.com/ArthurSonzogni/FTXUI/blob/main/CHANGELOG.md), [versions publiées](https://github.com/ArthurSonzogni/FTXUI/releases).
- ncurses : [Programming HOWTO](https://invisible-island.net/ncurses/NCURSES-Programming-HOWTO.html), [intro et resize](https://invisible-island.net/ncurses/ncurses-intro.html), [CMake FindCurses](https://cmake.org/cmake/help/latest/module/FindCurses.html).
- Ces sources décrivent des capacités de bibliothèque; elles ne remplacent pas les essais de compatibilité sur le backend/version réellement épinglé.
