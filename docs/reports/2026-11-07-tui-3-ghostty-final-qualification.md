# TUI-3 — clôture de qualification Ghostty et préparation de TUI-4

Date : 2026-11-07\
SHA examiné : `222139b1bb5bf536148466c5c9ca49b28644b4f1`\
Branche documentaire : `docs/tui-3-ghostty-final-qualification`\
Worktree documentaire : `/home/oper/projects/worktrees/local-agent-cpp-tui3-ghostty-docs`\
Worktree de validation consulté : `/home/oper/projects/worktrees/local-agent-cpp-tui3-validation` (détaché, propre au SHA examiné)

## Objectif et portée

Cette itération consigne les essais interactifs Ghostty rapportés par le
mainteneur, examine deux observations résiduelles à partir des sources et des
rapports versionnés, puis prépare le périmètre fonctionnel des six vues TUI.
Elle ne modifie aucun code produit, test, règle Landlock ou outil workspace.
Les essais automatisés ne sont pas rejoués ici.

## Vérifications Git et périmètre

- Le worktree de validation est détaché au SHA demandé et son statut est
  propre.
- La référence locale `origin/main` pointe sur ce même SHA; le graphe montre
  le merge de la PR #28. Une tentative de vérification distante a échoué car
  `github.com` ne résolvait pas dans l'environnement. L'état GitHub en temps
  réel n'est donc pas vérifié par cette itération.
- Le checkout principal est sur `feat/ui-4-observable-metrics`, en retard par
  rapport à `origin/main`, avec des modifications UI-4 suivies et non suivies.
  Son statut a été relevé avant et après le travail documentaire et n'a pas
  changé. Aucun de ces fichiers n'a été lu comme source à intégrer ni modifié.
- Les documents sont préparés dans un worktree distinct créé depuis le SHA
  examiné. Aucun commit, push ou merge n'a été effectué.

## Qualification manuelle Ghostty

Les faits ci-dessous sont **confirmés par le mainteneur/utilisateur dans la
demande**, et non observés directement par l'agent durant cette itération.
Environnement indiqué : Debian 13, Ghostty (`TERM=xterm-ghostty`), UTF-8,
terminal interactif, ncursesw 6.5 et agent connecté à un serveur LLM configuré.

Les dix contrôles rapportés comme réussis sont :

1. Ouverture plein écran sans deltas synthétiques au démarrage.
2. Connexion LLM et réponse progressive.
3. Un seul préfixe `Agent:` par message.
4. Indentation des lignes suivantes.
5. Un seul préfixe `You:` par message.
6. PageUp/PageDown et suivi ancré du scroll.
7. Redimensionnement sans dédoublement.
8. Envoi multiligne avec F9.
9. Fermeture avec F10.
10. Restauration `termios` confirmée.

Le mainteneur indique également que le journal comporte trois sorties avec
code 0 et `termios restored: yes`. Le fichier de journal lui-même n'a pas été
retrouvé dans les emplacements temporaires inspectés pendant cette itération;
ces trois lignes sont donc rapportées par le mainteneur, pas vérifiées
indépendamment ici. Aucun prompt, contenu de réponse réel ou secret n'est
reproduit dans ce document.

### Résultats automatisés, distincts des essais manuels

Le rapport versionné [correctif multiligne](2026-11-06-tui-3-multiline-rendering-fix.md)
associe ses résultats au code issu de `e783a3c` puis intégré dans le SHA
examiné. Il rapporte : 13 cas TUI-3 et 50 assertions; CTest 11/11; harnais
complet 199 cas et 1 246 assertions, sans échec. Ce sont des résultats
historiques rapportés dans ce rapport, pas une réexécution de cette mission.
Les PTY automatisés ne remplacent pas l'observation Ghostty.

SSH, autres terminaux physiques, autres locales et accessibilité par lecteur
d'écran restent non vérifiés.

## Anomalie A — ligne observée `Agent: .`

### Faits établis dans le code

- `TuiEventLoop::handle_event()` traite un événement `TextDelta` en
  l'ajoutant au message assistant actif via `TuiView::append_assistant_delta()`.
  Les frontières de deltas ne créent pas de message supplémentaire.
- `TuiView::rendered_conversation_rows()` produit le préfixe du rôle une fois
  pour la première ligne logique du message, puis indente les lignes
  suivantes. Le correctif et ses tests couvrent les fragments SSE, les
  nouvelles lignes et les frontières entre tours.
- Le contenu assistant est affiché tel quel après sanitisation. Une réponse
  dont le texte vaut littéralement `.` est donc rendue `Agent: .` sans
  frontière artificielle créée par le renderer.

### Conclusion et limite

Le chemin de code examiné rend **possible et cohérent** l'affichage rapporté
si le modèle a effectivement émis un point comme contenu. Il ne prouve pas
que la réponse réelle de la session Ghostty était `.` : aucun contenu de cette
session n'a été conservé ni consulté, par confidentialité. Aucun défaut de
frontière TUI n'est démontré et aucun test/correctif de code n'est justifié
par les éléments disponibles. Si l'observation se reproduit, un test sûr
pourra injecter un delta synthétique égal à `.` et vérifier un unique message
assistant; cela ne permettrait toujours pas d'identifier le contenu historique
de la session.

## Anomalie B — 1 242 contre 1 246 assertions

Les preuves versionnées donnent deux bases comparables : le rapport de
revalidation TUI-3 annonce 193 cas / 1 228 assertions; le rapport du correctif
multiligne annonce 199 cas / 1 246 assertions. Le diff du commit `e783a3c`
ajoute six cas nets dans `tests/tui_test.cpp`; les comptes des rapports
augmentent de six cas et de dix-huit assertions. Cela établit le passage de
1 228 à 1 246 dans la version du correctif fusionnée.

Le nombre 1 242, cité comme relevé du build de qualification Ghostty, ne figure
pas dans les rapports versionnés accessibles au SHA examiné. Le journal de
qualification n'a pas été trouvé dans `/home/oper/tmp` selon une recherche
ciblée de noms de fichiers. Aucun SHA, binaire ou snapshot des sources de tests
associé à ce relevé 1 242 n'a été retrouvé. Il est donc impossible de démontrer
si ces quatre assertions d'écart viennent d'un build intermédiaire, d'une
variante des sources ou d'une erreur de transcription. L'écart reste **non
expliqué**; aucun test n'a été modifié pour forcer un total.

## Périmètre des six vues TUI

La matrice suivante décrit les données réellement accessibles dans l'arbre au
SHA examiné et les limites de l'interface actuelle. Les interactions proposées
ne changent ni le moteur ni ses permissions.

| Vue | Données existantes | Réutilisation et interactions proposées | Limites et critères d'acceptation |
|---|---|---|---|
| Conversation | `TuiView` conserve les messages de session; `TuiEventLoop` reçoit les deltas, états d'outil, erreurs et résultat final via `AgentWorker`. | Réutiliser l'éditeur multiligne, F9, scroll ancré, sanitizer et résultat asynchrone; navigation vers cette vue et conservation du focus. | Pas de duplication du delta/réponse finale; scroll ancré; contenu saisi et rendu inchangé au resize; échec/overflow indiqué incomplet. |
| Outils | `AgentEvent` expose nom, `tool_call_id`, durée et issue technique des appels exécutés; `ToolRegistry::schemas_json()` expose les schémas. | Vue consultative : outils déclarés et, pendant/après les tours, appels effectivement observés; sélectionner/faire défiler sans déclencher d'outil. | Les événements ne transportent pas arguments ni résultats; ne pas inventer leur contenu ou succès. Distinguer outil déclaré d'outil exécuté. |
| Métriques | Événements portent durées monotones et compteurs finaux; `PresentationState` conserve des compteurs d'événements. | Prévoir emplacement et état « indisponible »; brancher un `MetricCollector` seulement après disponibilité validée et autorisation distincte. | Aucun `MetricCollector` n'existe dans ce HEAD. Les fichiers non commités du checkout UI-4 ne sont pas une dépendance disponible. Pas de zéro, tokens/s, ETA ou valeur calculée sans source. Tests: indisponibilité explicite et aucune métrique fabriquée. |
| Diagnostics | `RequestFailed` contient une catégorie; le résultat du worker transporte l'exception; la TUI affiche un diagnostic sanitizé et un état incomplet en cas de pertes. | Liste consultative des erreurs/états de la session, navigation/scroll; garder les détails techniques expurgés et séparés du texte modèle. | Le protocole n'offre pas encore un historique structuré de diagnostics ni des codes complets. Aucune prompt, clé API, argument ou résultat d'outil. Tests de catégories réellement émises, assainissement et absence de secrets. |
| Configuration | `AgentConfig` contient endpoint, modèle, clé API et timeout; `TuiApplication` détient la configuration et les outils. Le workspace est capturé par le callback de confinement dans `main`, pas exposé à la vue. | Affichage en lecture seule des champs explicitement autorisés; endpoint expurgé; capacité terminal constatée si fournie. | Ne jamais rendre la clé API. Chemin workspace et capacités devront être transmis explicitement si retenus. Pas d'édition de configuration dans la TUI initiale. Tests de redaction et exactitude. |
| Aide | Les touches présentes sont F9 envoi, F10/Escape sortie, Tab focus, PageUp/PageDown scroll, Enter nouvelle ligne. | Aide consultative accessible depuis toutes les vues; retour à la vue précédente. | N'afficher que les raccourcis effectivement pris en charge. Tests de concordance aide/touches et terminal étroit. |

### Écarts et proposition de séquencement

Au SHA examiné, le produit fournit l'écran de conversation TUI-3 mais pas le
menu ni les six vues navigables. SPEC-UI-001 v2.0 répartit le shell et les
vues Outils/Aide/Config dans TUI-3, la conversation/compositeur dans TUI-4,
puis le `MetricCollector` et les vues métriques/diagnostics dans TUI-5. La
roadmap du même arbre formule certaines étapes différemment, notamment en
regroupant les six vues dans sa ligne TUI-3. Cette divergence de découpage
doit être arbitrée avant de renommer ou d'étendre un jalon.

Séquence recommandée, à ratifier avant implémentation :

1. **TUI-4a — navigation et shell** : menu, focus, dimensions compactes,
   changement entre les six identifiants de vue; Conversation demeure l'écran
   fonctionnel existant. Tests clavier/PTY, aucun nouvel accès moteur.
2. **TUI-4b — vues en lecture seule** : Outils, Aide, Configuration et
   Diagnostics à partir des seules données ci-dessus; tests de redaction,
   cohérence et non-régression du REPL classique.
3. **TUI-4c — consolidation conversation** : préserver streaming, composer,
   ancrage, erreurs et saturation en naviguant entre vues; tests d'intégration
   PTY et ordre des événements.
4. **TUI-5 — métriques** : uniquement après intégration explicitement validée
   d'un `MetricCollector` et définition des métriques disponibles; aucune
   récupération implicite des fichiers UI-4 non commités.
5. **Validation transverse** : terminal étroit, resize, sanitisation,
   restauration, CLI classique, Landlock inchangé et rapport de preuves.

L'ordre exact, en particulier le placement de Diagnostics entre TUI-4 et
TUI-5, ainsi que le sort de la divergence roadmap/specification sont des
propositions et nécessitent décision du mainteneur. L'implémentation TUI-4 n'a
pas commencé dans cette itération.

## Vérifications exécutées

- Lecture de `AGENTS.md`, SPEC-UI-001 v2.0, roadmap, rapports TUI-3, sources
  `TuiView`/`TuiEventLoop`/`TuiApplication`, événements et tests TUI.
- Vérification `HEAD`, statut et worktrees; référence locale `origin/main` au
  SHA demandé; `git ls-remote` tenté, échec DNS.
- Inspection du diff du commit `e783a3c` et du nombre de déclarations de tests
  dans le fichier avant/après ce commit.
- Recherche ciblée des fichiers de journal Ghostty dans `/home/oper/tmp`;
  aucun journal trouvé.
- Création d'une branche/worktree documentaire isolé depuis le SHA de base.
- Aucun build ou test réexécuté; aucune correction de code, aucun changement
  du moteur, de Landlock ou des outils.
- L'état du checkout UI-4 a été vérifié avant et après; il est inchangé.

Contrôles documentaires `git diff --check`, liens et périmètre à exécuter avant
de déclarer le rapport prêt. Aucun commit, push ou merge n'est autorisé ou
effectué dans cette mission.

## Verdict

**TUI-3 : qualification Ghostty rapportée comme réussie par le mainteneur**;
les dix contrôles ne sont pas des observations automatisées de cette session.
L'analyse du code montre qu'une réponse littérale `.` explique l'affichage
`Agent: .`, mais le contenu de la session ne permet pas d'en confirmer la
cause historique. L'écart 1 242/1 246 reste non expliqué faute de provenance
du premier relevé. La préparation des vues TUI-4 est documentaire; l'accès aux
métriques reste bloqué jusqu'à disponibilité validée du collecteur. Aucun
changement de code n'est justifié par les éléments présents.
