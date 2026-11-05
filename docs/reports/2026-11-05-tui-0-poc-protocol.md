# TUI-0 — Arbitrages techniques et protocole du POC backend

**Date :** 2026-11-05
**Branche :** `docs/tui-0-architecture-revision`
**SHA de base :** `ba1ee2e9b3535f3e5a6b79a4d544dd5b3a2ab81f`
**Statut :** recommandations proposées au mainteneur; protocole préparé, aucun POC exécuté.

Ce document complète le rapport [TUI-0 initial](2026-11-05-tui-0-architecture-revision.md). Il affine les propositions de queue de ce premier rapport. En cas de différence sur la coalescence et la conservation des événements, le présent complément prévaut : **aucune coalescence de `TextDelta` et aucune garantie de conservation de chaque terminal d'opération en cas de surcharge**.

## 1. Arbitrages argumentés proposés

| Sujet | Recommandation TUI-0 | Justification / limite | Statut |
|---|---|---|---|
| Propriété du terminal | Le thread UI est l'unique propriétaire du terminal, backend, renderer et état de présentation. | Évite les écritures concurrentes et rend le teardown centralisé. Les callbacks/worker ne font aucun I/O terminal. | À ratifier |
| Worker Agent | Un worker unique possède `Agent`; un seul `Agent::send()` peut être en vol. | `send()` est bloquant et les callbacks s'exécutent dans son thread. Aucun accès concurrent à Agent n'est ajouté. | À ratifier |
| Cycle de vie worker | Thread persistant; entrée capacité 1; refus d'une deuxième soumission en vol; fermeture refuse le nouvel input, termine le tour en cours, puis `join()` avant destruction de l'Agent et de l'observateur. Aucun thread détaché. | Ownership RAII simple, durée de vie de l'Agent et des captures explicite. | À ratifier |
| File | File événementielle à capacités fixes en items et octets; copie limitée avant allocation; dépôt sans attente de disponibilité; séquences UI-1 conservées jusqu'à une perte déclarée. | Une file qui attend la capacité peut ralentir le callback de streaming; une file sans limite peut croître sans fin. | À ratifier et mesurer |
| Saturation | Premier overflow de delta pose un latch `display_incomplete`, cesse la livraison des nouveaux détails de ce tour, n'essaie pas de resynchroniser la projection après le trou, et continue le moteur. Pas de suppression silencieuse. | Le streaming d'écran devient incomplet, mais l'Agent conserve son comportement et le worker n'est pas bloqué par le rendu. | À ratifier |
| Événements terminaux | Mailbox dédiée, une entrée fixe par tour pour `RequestFailed` et `TurnFinished`, plus un résultat final worker. Ces deux événements sont au plus un chacun par tour selon le contrat Agent. | Préserve l'issue du tour même si la queue normale est saturée. Ne garantit pas `HttpRequestFinished`/`ToolSucceeded`/`ToolFailed` individuels sous surcharge. | À ratifier; test de course requis |
| Détails perdus | Vue et métriques se déclarent incomplètes si perte; `TurnFinished` donne ses agrégats réellement produits par Agent. Ne reconstituer aucun résultat de tool individuel manquant. | Les agrégats ne restaurent pas les détails ni l'ordre des opérations omises. | À ratifier |
| `TextDelta` | Aucun regroupement dans la première version. Dépôt un événement par événement et maintien de `metadata.sequence`. | Fusionner des deltas changerait le nombre de fragments et introduirait une plage de séquences que `PresentationState`/`MetricCollector` ne définissent pas. | À ratifier |
| Observateur défaillant | `on_event` de l'adaptateur doit être sans exception; si exception inattendue, UI-1 désactive l'observateur. `on_observer_failure` ne fait que poser un latch; notification unique depuis le thread UI. | Aucune publication récursive ni diagnostic concurrent. Le moteur poursuit; le canal worker reste la source du résultat final. | Compatible en principe; doit être testé avec queue réelle |
| Renderer défaillant | Le thread UI capture l'erreur, désactive le renderer, restaure le terminal, garde résultat worker et utilise stderr simple seulement si possible. | Une erreur de présentation ne change pas le résultat Agent. | À ratifier |
| Fermeture pendant `send` | Politique « fermer après tour » : l'UI cesse d'accepter de nouveaux tours, indique qu'elle attend, puis joint le worker. | Sans annulation coopérative, aucun arrêt immédiat honnête n'est possible. | À ratifier |
| Durée d'attente | Aucune borne globale garantie : timeout configuré par requête HTTP; plusieurs requêtes peuvent se suivre. Une fermeture peut donc dépasser un timeout. | Ne pas promettre une durée d'arrêt calculée à partir du seul timeout HTTP. | Fait déduit des contrats présents; à revalider au démarrage TUI-2 |
| Perte du terminal | Détecter fermeture/HUP quand possible, arrêter le rendu, teardown best effort et attendre le worker avant sortie normale. SIGKILL/crash dur ne sont pas récupérables. | Une perte de terminal et un ordre de terminaison du processus ne sont pas toujours détectables avant le signal OS. | À ratifier |
| CLI non interactive | Garder un chemin classique indépendant; pipes/redirections ne lancent aucun backend TUI; stdout conversation, stderr diagnostic; Plain sans ANSI. | Évite de mélanger un écran plein format avec données de script. | Mainteneur a ratifié la CLI classique indépendante; détail de sélection à ratifier |

### Pourquoi la garantie de queue est volontairement limitée

Pour un producteur de débit arbitraire, une file avec mémoire bornée ne peut pas garder chaque événement si le consommateur ralentit, sauf à bloquer/ralentir le producteur ou à supprimer des événements. Dans ce dépôt, le producteur est dans le chemin de streaming libcurl; bloquer ce callback affecterait la lecture du flux. La proposition choisit donc la disponibilité de l'UI et une mémoire de file bornée, accepte une perte explicite des détails de présentation et réserve uniquement l'issue terminale de tour.

La garantie proposée n'est pas une garantie « aucun événement perdu ». Elle est limitée à : résultat final du worker disponible une fois; `RequestFailed` et `TurnFinished` conservés dans des emplacements dédiés; tout overflow des événements ordinaires signalé; aucune sortie déclarée complète après perte. Les terminaux HTTP/tool individuels peuvent manquer sous surcharge. Exiger leur conservation exacte impose de démontrer des bornes amont sur nombre/taille, ou de décider d'une contre-pression qui peut bloquer les callbacks. Cette alternative reste à arbitrer.

Le POC doit aussi tester le cas où l'observateur échoue avant `TurnFinished`: UI-1 désactive alors l'observateur, mais le worker doit tout de même terminer la tâche et transmettre son résultat. Ce résultat indique seulement l'issue technique de `Agent::send`, pas l'accomplissement fonctionnel de la demande.

## 2. Protocole comparatif FTXUI / ncursesw

### 2.1 Isolation et méthode

- Exécuter le POC ultérieurement dans une branche/worktree propre dédié et un répertoire temporaire distinct; ne pas importer la bibliothèque dans la cible `agent_core` ni changer le CMake principal.
- Deux petits programmes autonomes C++20, mêmes données/scénarios, même machine Debian et même harness pseudo-terminal. Versions du compilateur, libc, noyau, terminfo, terminal émulé et paquets enregistrées.
- FTXUI : version/tag et commit exacts épinglés; vérifier son CMake minimum, ses options et cibles; acquisition propre répétable, sans `main` flottant. À la date de consultation du rapport TUI-0, la release visible était v7.0.3; vérifier de nouveau avant POC, sans présumer qu'elle est encore courante.
- ncurses : enregistrer versions `ncursesw`/`panelw` et `pkg-config`/CMake; confirmer que la bibliothèque wide est effectivement liée. Ne pas installer de dépendance dans le dépôt ni modifier son environnement de build.
- Chaque backend implémente le même écran: menu 6 items, zone conversation scrollable, zone saisie multiligne, panneau statut, focus clavier, layout compact et flux d'événements synthétiques.
- Le faux producteur tourne dans le worker du POC; il publie un flux déterministe d'événements avec séquences. Il n'appelle ni Agent, ni HTTP, ni outils, et n'accède pas au workspace.
- Capturer sortie pty, temps d'interaction, état `termios` avant/après, état alternate-screen si observable, résultats de tests et dépendances. Ne pas traiter une capture terminal comme preuve de sécurité générale.

### 2.2 Charge de test queue et navigation

Utiliser pour comparer les backends un producteur synthétique de **100 `TextDelta`/s, 1 KiB par delta pendant 10 s**, avec événements d'outil/fin prédéfinis et séquences strictement croissantes. Pendant ce flux, injecter navigation menu/panneau et édition texte. P95 entre injection d'une touche et traitement visuel proposé **≤100 ms** sur la machine de test; seuil candidat, à ratifier avant de le traiter comme critère produit.

Test séparé de surcharge: capacité de test choisie explicitement dans le harness (exemple candidat 128 items et 256 KiB de payload, delta maximum 8 KiB; ces nombres sont des paramètres expérimentaux, pas des valeurs de production). Suspendre artificiellement le consommateur 2 s, continuer le producteur et vérifier : mémoire de queue dans les limites de payload/count instrumentées; premier overflow marqué; aucune attente de capacité côté producteur; aucun coalescing; aucune projection annoncée complète; `RequestFailed`/`TurnFinished` et résultat worker disponibles une fois; pertes de détails HTTP/tool comptabilisées comme perte, non reconstruites.

La limite logique du stockage de queue ne doit pas être présentée comme un plafond RSS exact : overhead d'allocateur, piles de threads, objets backend et buffers système doivent être mesurés séparément. Les chaînes doivent être bornées avant copie pour ne pas allouer un `TextDelta` complet avant le contrôle de capacité.

### 2.3 Scénarios et critères par exigence

| # | Critère | Vérification | Réussite / élimination |
|---:|---|---|---|
| 1 | Initialisation plein écran | lancer sous pty TTY; observer écran et mode terminal | Éliminatoire : backend ne peut entrer proprement ou setup partiel ne peut être restauré |
| 2 | Menu navigable | flèches, Tab, Enter, Escape; six entrées | Éliminatoire : navigation clavier de base non fiable |
| 3 | Navigation panneaux/focus | passer Conversation/Outils/Métriques/Diagnostics/Configuration/Aide, vérifier focus | Éliminatoire : focus ambigu ou clavier perdu |
| 4 | Saisie multiligne | `Enter` ajoute LF, soumission dédiée, contenu Unicode/espaces/vide de test connu | Éliminatoire : texte différent du buffer modèle ou perte de lignes |
| 5 | Streaming pendant navigation | faux worker alimente les deltas alors que l'utilisateur navigue | Éliminatoire : navigation bloque au rythme du producteur ou deltas s'affichent dans une autre vue sans règle explicite |
| 6 | Ordre des événements | chaque évènement rendu/consommé vérifie sequence; test sans saturation puis avec saturation | Éliminatoire : réordonnancement silencieux; en surcharge, seuls les détails explicitement perdus peuvent manquer |
| 7 | Resize | augmenter/réduire le pty en cours de flux | Éliminatoire : crash, corruption terminal ou perte de buffer saisie |
| 8 | Terminal étroit | tailles 80x24, 40x12, 20x6 et 10x4 simulées | Réussite si compactage/repli net; éliminatoire si chevauchement avec saisie illisible sans mode de repli |
| 9 | Unicode et contrôles | texte UTF-8, CJK, combinants, ESC/CSI/OSC/C0/C1 fractionnés | Éliminatoire : contrôle de données envoyé comme action terminal; fallback ASCII explicite si glyphes non pris en charge |
| 10 | Arrêt normal/exception | quitter, jeter exception pendant rendu/setup | Éliminatoire : exception laisse terminal en mode non canonique/echo off ou écran alternatif actif |
| 11 | Restauration | capturer `termios` avant/après et sortie de teardown sous pty | Éliminatoire : les champs modifiés ne sont pas restaurés après sortie normale/exception/SIGINT/SIGTERM gérés; SIGKILL exclu/documenté |
| 12 | Pseudo-terminal | harness reproductible automatisé | Éliminatoire : comportement terminal uniquement démontrable manuellement |
| 13 | Debian/C++20 | configuration/compilation clean sur version Debian fixée | Éliminatoire : pas de build propre C++20 supporté |
| 14 | Dépendances/reproductibilité | clean configure; pin FTXUI, paquet ncursesw versionné; inventaire licences et provenance | Éliminatoire : dépendance flottante/introuvable ou build impossible à reproduire selon procédure enregistrée |
| 15 | Moteur/Landlock | POC indépendant; comparer arbre et règles; aucune API d'Agent ni workspace | Éliminatoire : changement ou dépendance sur `agent_core`, accès workspace, permissions ou Landlock |

### 2.4 Sélection du candidat

Tout critère éliminatoire échoué élimine le candidat ou requiert une nouvelle décision documentée avant d'aller plus loin. Parmi les candidats restants, comparer effort mesuré, qualité du rendu, Unicode, restauration, contraintes de packaging et facilité de tests; rendre les observations et les écarts explicites. Si aucun ne passe, ne pas démarrer TUI-2 et proposer une décision au mainteneur. Un candidat qui passe le POC n'est pas automatiquement autorisé à devenir dépendance du produit.

## 3. Points restant au mainteneur

1. Approuver ou remplacer la recommandation FTXUI pour le POC et autoriser la dépendance **dans l'environnement isolé du POC uniquement**.
2. Ratifier le modèle terminal UI-only / worker unique.
3. Ratifier le compromis saturation et la garantie limitée aux statuts du tour, ou demander un autre modèle avec ses coûts.
4. Valider le comportement « fermer après tour » et l'absence de limite globale actuelle.
5. Confirmer le seuil de latence 100 ms et les paramètres de surcharge comme critères POC, ou fournir des seuils différents.
6. Décider ultérieurement la sélection `auto|classic|tui`, les touches et le comportement exact en perte de terminal.

**TUI-0 n'exécute pas le POC et n'ajoute aucune dépendance au projet.**
