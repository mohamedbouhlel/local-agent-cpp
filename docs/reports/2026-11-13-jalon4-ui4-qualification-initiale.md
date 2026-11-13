# Jalon 4 — qualification initiale UX v1.1 et réserves UI-4

## Référence et portée

- Dépôt : `mohamedbouhlel/local-agent-cpp`.
- Référence qualifiée : `c2e11daa4028088079db3b8e4f2df01bbdde9436`.
- Branche de qualification : `qualification/jalon4-ui4`.
- Worktree : `/home/oper/tmp/local-agent-cpp-jalon4-ui4`.
- Environnement observé : Linux x86_64, noyau `6.12.115+deb13.1-amd64`, compilateur Debian GCC 14.2.0, CMake 3.31.6, libcurl 8.14.1; CMake a trouvé ncursesw dans `/usr/lib/x86_64-linux-gnu/libncursesw.so`. Le standard C++20 est imposé par le `CMakeLists.txt` racine.
- Build propre : `/tmp/local-agent-cpp-jalon4-ui4-build`, configuré avec `AGENT_BUILD_TESTS=ON`, `CMAKE_BUILD_TYPE=Debug`. Dépendance JSON récupérée par FetchContent. Aucun serveur LLM réel ni terminal physique utilisé.

La présente qualification porte sur deux réserves UI-4 et la complétude des preuves UX disponibles. Elle ne constitue ni une nouvelle qualification Ghostty, ni une clôture du résultat 4 de la roadmap.

## État fonctionnel vérifié dans le code

`EventChannel` est une file FIFO bornée par défaut à 128 événements, 256 KiB de payloads en attente et 8 KiB par payload (`include/ui/agent_worker.hpp`). La comptabilisation du payload couvre notamment les fragments texte et les identifiants d'appel; elle ne borne pas le RSS total ni les allocations d'objets. Une saturation, un payload individuel trop grand, une contention du mutex ou une exception dans l'observateur incrémente le compteur des événements perdus. Après une perte associée à un tour, l'observateur abandonne également les événements suivants de ce tour. Le prochain `RequestStarted` réarme ce mécanisme.

Les compteurs de pertes sont cumulatifs au niveau du canal. `TuiEventLoop::submit_input()` mémorise leur valeur au début du tour. Le worker enregistre les deltas avant/après `Agent::send()` dans `TurnCompletion`; la comparaison avec la base du tour permet donc de distinguer les pertes de ce tour des pertes antérieures. Le snapshot du canal seul n'est pas un compteur par tour.

Pendant le tour, `TuiEventLoop::drain_events()` constate une hausse des compteurs, marque les métriques incomplètes, ajoute un diagnostic et met à jour la vue. À la disponibilité du futur, `collect_completion()` draine une dernière fois les événements publiés, puis propage le résultat de complétion. `MetricCollector::mark_event_details_incomplete()` invalide notamment le total d'octets de réponse. La projection F3 produit un texte `PARTIAL: ...` et une ligne de statut `PARTIAL`. Quand le worker a une réponse finale, le transcript provisoire peut être remplacé par cette réponse plutôt que concaténé, afin d'éviter une duplication. Le statut précise alors que la complétion technique ne vérifie pas la réussite de la tâche.

### Matrice des preuves de saturation jusqu'à F3

| Étape | Code et test observés | Preuve obtenue | Lacune restante |
|---|---|---|---|
| Capacité et perte EventChannel | `include/ui/agent_worker.hpp`, `src/ui/agent_worker.cpp`; `tui2.queue_saturation_reports_loss_and_final_agent_response_can_replace_partial_text` dans `tests/agent_worker_test.cpp` | Test réel `AgentWorker` avec limites réduites : réponse retournée, réponse complète du worker, pertes non nulles et comptées, file bornée, absence de faux `TurnFinished` dans les événements conservés. | Ce test ne démarre pas `TuiEventLoop` et ne lit pas F3. |
| Réponse de complétion portant les pertes | `AgentWorker::run()` calcule les deltas entre snapshots cumulatifs; `TurnCompletion::event_details_incomplete()` dérive du nombre de pertes/erreurs observateur. | Le test worker ci-dessus vérifie `dropped_events` et `event_details_incomplete()`. | La propagation dans une boucle TUI sous saturation n'est pas exercée par ce test. |
| Collecteur incomplet et projection F3 | `src/ui/tui_event_loop.cpp`, `src/ui/metrics.cpp`, `src/ui/tui_view.cpp`; tests `ui4.metrics_view_shows_partial_even_when_request_start_was_lost` et `tui4a.partial_is_explicit_and_bold_without_color` | Le test `ui4` injecte directement `event_details_complete=false`; les tests `tui4a` vérifient la projection textuelle `PARTIAL`, y compris sans couleur. | Ces tests ne saturent pas un `EventChannel`, ne récupèrent pas `TurnCompletion` et ne passent pas par le drainage réel de `TuiEventLoop`. |
| Parcours intégré EventChannel → perte → métriques → F3 | Aucun test intégré identifié dans les tests examinés. | **Non démontré de bout en bout.** La cohérence du chemin est étayée par la lecture du code et les tests unitaires distincts, sans preuve intégrée. | Jonction manquante : exécuter la vraie boucle TUI avec worker et canal à capacité réduite, provoquer une perte déterministe, laisser le worker terminer, puis inspecter les lignes/statut F3 après `collect_completion()`. |

Il n'y a pas de défaut fonctionnel reproduit. La conclusion est une lacune de preuve d'intégration, pas la découverte d'une anomalie. Un test futur peut utiliser les interfaces existantes en injectant `EventQueueLimits` faibles au worker, un serveur mock loopback déterministe et le driver/PTy existant; il devra attendre un signal observable de fin plutôt qu'un délai arbitraire, sélectionner F3 après la fin du tour, puis vérifier `PARTIAL` dans la projection réellement dessinée et dans le statut. Il devra également vérifier qu'aucun delta n'est dupliqué si le résultat final remplace le flux provisoire. Aucun test n'a été ajouté pendant cette mission.

## Coût des snapshots et projection F3

Constats statiques au SHA étudié :

- `MetricCollector::snapshot()` retourne une référence constante à `TurnMetrics`.
- `TuiEventLoop` appelle `view_.set_metrics(metrics_.snapshot())` lors des événements traités, de certaines pertes/erreurs, au début du tour et à sa complétion. Comme `TuiView::set_metrics` prend `TurnMetrics` par valeur, l'appel depuis le snapshot lvalue copie les vecteurs avant le déplacement dans la vue.
- `TuiView::set_metrics()` construit les lignes métriques avant remplacement pour préserver l'ancre, affecte les métriques, puis reconstruit ces lignes après remplacement. La boucle rend ensuite la vue lorsque les événements ont marqué l'état comme modifié. La session ncurses utilise un timeout de lecture de 40 ms; ce réglage ne constitue pas une mesure de cadence d'événements ou de coût.
- Le collecteur conserve au plus 64 détails HTTP et 64 détails outil par tour. Les identifiants d'outil conservés sont limités à 16 KiB cumulés; les noms d'outil à 128 octets chacun. Les compteurs scalaires saturent et signalent leur overflow. Ces bornes limitent les données de métriques retenues, pas la mémoire globale du processus, les temporaires de projection, les autres historiques ni la fréquence des redraws.
- La projection F3 forme des lignes texte et des vecteurs; les détails retenus sont donc bornés, mais les copies, recherches et allocations sont répétées au rythme des mises à jour. Les recherches d'éléments du collecteur sont linéaires sur des vecteurs plafonnés.

Aucun benchmark, compteur d'allocations ou profileur de redraw intégré aux sources/tests n'a été trouvé dans le périmètre examiné. Aucune mesure synthétique fiable latence/CPU/RSS n'a donc été produite. Les résultats physiques antérieurs rapportés par le mainteneur ne mesurent pas le coût de snapshots et ne sont pas réattribués à cette campagne.

### Protocole reproductible proposé

1. Construire un petit exécutable de qualification hors dépôt qui réutilise `MetricCollector` et `TuiView`, avec des événements synthétiques déterministes et aucun contenu de conversation.
2. Préparer des profils bornés (aucun détail, puis saturation des plafonds 64 HTTP/64 outils et 16 KiB d'identifiants), et produire une séquence documentée d'événements `RequestStarted`, HTTP, fragments texte et outils. Fixer et consigner exactement le nombre total d'événements et les répétitions.
3. Mesurer séparément le temps de `consume`, la copie/snapshot et `set_metrics` + projection F3; rapporter médiane et quantiles, ainsi que les allocations via un allocateur/profileur compatible. Garder le rendu terminal réel séparé de la projection.
4. Pour le coût de rendu, utiliser un PTY local reproductible et mesurer la durée entre l'injection d'événements et l'écran rendu; relever cadence, dimensions, build, machine, CPU et RSS de pic avec un outil système. Ne pas inclure de serveur LLM.
5. Répéter plusieurs fois à charge identique, conserver les données synthétiques et le protocole, et qualifier les chiffres comme mesures de cette machine uniquement. N'établir aucun seuil contractuel après observation.

## Matrice des 16 scénarios UX v1.1

Les statuts physiques ci-dessous reprennent la matrice détaillée du rapport `2026-11-10-lofiterm-ux-v1.1-physical-qualification.md`. Ils ne transforment pas ses observations génériques en PASS complets. La preuve automatisée désigne les tests présents/exécutés lors de la présente campagne lorsqu'ils sont identifiés; elle ne remplace pas la recette physique.

| # | Scénario officiel | Preuve automatisée disponible | Preuve physique rapportée | Sous-scénario restant | Statut |
|---:|---|---|---|---|---|
| 1 | Ouverture plein écran, fermeture F10, restauration terminal | Tests PTY couvrent des fermetures; pas tous les détails de restauration physique. | Restauration rapportée PASS, ouverture/F10 non détaillés séparément. | Vérifier le cycle complet et l'état terminal dans Ghostty. | PARTIAL |
| 2 | F1–F6, Tab, Échap, F9, F10 et raccourcis par focus | `tui4a` vérifie F1–F6, Escape/focus, Tab et soumission limitée à Conversation. | Six vues et clavier déclarés PASS globalement. | Chaque touche/focus et interaction combinée non inventoriés individuellement. | PARTIAL |
| 3 | Clic onglets, changement de page et focus | Tests `ux11` couvrent hit-test et clic d'onglet PTY. | Souris déclarée PASS sans gestes détaillés. | Confirmer focus exact et comportement physique complet. | PARTIAL |
| 4 | Molette, scroll long, absence de redraw continu au mouvement pointeur | Couverture ciblée de navigation; pas de mesure physique de cadence/redraw. | Molette déclarée PASS. | Contenu long et absence de redraw continu non détaillés. | PARTIAL |
| 5 | Rubriques F5/F6 par Entrée, Échap et clic | `ux11` couvre transitions d'ouverture/sélection/fermeture clavier et clic. | Non mentionné spécifiquement dans les 11 contrôles. | Parcours physique des rubriques. | NOT RUN |
| 6 | Brouillon multiligne et position de lecture conservés en navigation | `tui4a` teste conservation transcript/ancre/resize et PTY navigation avec brouillon. | Non mentionné spécifiquement. | Vérification Ghostty explicite du brouillon et du viewport. | NOT RUN |
| 7 | Réponse réelle avec éléments Markdown précis | Tests `ux11` couvrent la projection sûre et le streaming fragmenté. | Markdown et streaming réel déclarés PASS globalement. | Échantillons détaillés titres, listes, citations, liens, etc. | PARTIAL |
| 8 | Coloration fences C++, Python, Bash, JSON, YAML, diff | Tests couvrent labels supportés et projection des blocs de code. | Coloration déclarée PASS, langages individuels non précisés. | Vérifier chaque langage et rendu physique. | PARTIAL |
| 9 | `M` bascule texte brut sans détourner la saisie | Tests UX ciblent le mode Markdown/texte; l'exécution complète du raccourci dans le composeur n'est pas prouvée ici. | Mode texte brut déclaré PASS, touche et absence d'interférence non précisées. | Vérifier M pendant édition et hors composeur. | PARTIAL |
| 10 | Streaming long, réactivité et intégrité du contenu final | Tests de projection fragmentée; aucun test de performance utilisateur. | Streaming réel déclaré PASS, durée/fluidité/intégrité détaillée non rapportées. | Durée, fluidité et absence de duplication à établir. | PARTIAL |
| 11 | Tailles 120×40, 100×30, 80×24, 60×20 et retour | `tui4a` couvre tailles de référence et terminal inférieur au minimum. | Redimensionnement déclaré PASS sans dimensions consignées. | Dimensions exactes de la recette physique. | PARTIAL |
| 12 | `NO_COLOR`, contraste et restauration | Test automatisé UI4 vérifie métriques CLI sans couleur; ne qualifie pas le thème TUI physique complet. | `NO_COLOR` non rapporté; restauration générale déclarée PASS. | TUI physique monochrome et contraste. | PARTIAL |
| 13 | Refus `write_file` sans création | Tests d'approbation couvrent les décisions négatives via harness. | Refus d'écriture déclaré PASS, vérification filesystem non précisée. | Preuve physique du filesystem de workspace isolé. | PARTIAL |
| 14 | F10 pendant approbation, annulation sans création | Tests PTY/approbation couvrent fermeture et annulation automatisées. | Annulation déclarée PASS sans association explicite au geste F10 et sans inventaire filesystem détaillé. | Répéter le scénario précis et vérifier les chemins. | PARTIAL |
| 15 | Erreur d'endpoint inaccessible | Tests transport couvrent des erreurs; aucune recette TUI physique documentée ici. | Non mentionné. | Vérifier texte et présentation dans Ghostty. | NOT RUN |
| 16 | Perte contrôlée d'événements affichée `PARTIAL` | Test worker saturation et tests isolés de projection `PARTIAL`; aucun parcours intégré. | Non mentionné; aucune saturation n'est inférée d'un streaming normal. | Déclencher perte EventChannel déterministe puis constater F3. | NOT RUN |

Cette matrice ne remplace pas les critères NR-01…NR-35. Elle synthétise les 16 scénarios officiels et maintient explicitement les limites documentées; cette mission n'a pas exécuté de campagne physique.

## Tests et commandes exécutés

Configuration et compilation propres :

```sh
cmake -S . -B /tmp/local-agent-cpp-jalon4-ui4-build -DAGENT_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/local-agent-cpp-jalon4-ui4-build -j2
```

Résultat : configuration et build réussis avec GCC 14.2.0 et C++20. CMake a affiché l'avertissement développeur CMP0135/`DOWNLOAD_EXTRACT_TIMESTAMP`; aucune erreur de compilation. Des avertissements existants `-Wmissing-field-initializers` concernant `AgentConfig::http_timeouts` sont apparus dans plusieurs fichiers de tests.

`ctest --test-dir /tmp/local-agent-cpp-jalon4-ui4-build -N` a confirmé 14 groupes : `sse`, `workspace`, `http`, `agent`, `ui`, `ui3`, `ui4`, `tui2`, `tui3`, `tui4a`, `tui4b1`, `repl`, `mock`, `sandbox`. Il n'existe pas de groupe CTest nommé `ux11`; `ux11` est un filtre interne du harnais. Aucun filtre sans correspondance n'a été exécuté.

| Vérification | Commande / sélection | Résultat réel |
|---|---|---|
| `ui4`, `tui2`, `tui4a` CTest | `ctest --test-dir /tmp/local-agent-cpp-jalon4-ui4-build --output-on-failure -R '^(tui2|ui4|tui4a)$'` | 3/3 groupes PASS. |
| `tui2` compteurs du harnais | `agent_tests --filter=tui2` | 13 cas exécutés, 117 assertions, 0 échec. |
| `ui4` compteurs du harnais | `agent_tests --filter=ui4` | 14 cas exécutés, 97 assertions, 0 échec. |
| `tui4a` compteurs du harnais | `agent_tests --filter=tui4a` | 20 cas exécutés, 167 assertions, 0 échec. |
| Tests marqués `ux11` | `agent_tests --filter=ux11` | 14 cas exécutés, 179 assertions, 0 échec. Ce filtre n'est pas un groupe CTest. |

Les filtres directs ont d'abord été tentés dans le sandbox : les tests utilisant le mock HTTP y ont échoué à la création du socket (`mock: socket() failed`). Ils ont été rejoués hors sandbox afin d'autoriser le loopback et ont tous réussi. Le résultat CTest ciblé précédent était déjà 3/3. Ces exécutions ne sont pas une nouvelle exécution des 14 groupes complets.

## Limites, risques et décision proposée

- Aucune perte ni anomalie F3 n'a été reproduite. Le code présente une voie cohérente de propagation de perte; la preuve d'intégration jusqu'à F3 manque.
- La fréquence exacte de production d'événements dépend du flux Agent et n'est pas bornée par le collecteur métrique. Les limites des détails conservés ne sont pas une borne de mémoire globale.
- Le coût CPU, allocations, latence écran et RSS sous flux soutenu reste **non mesuré**; aucun seuil n'est proposé.
- Les observations physiques des rapports antérieurs sont attribuées à leur mainteneur et à leurs SHA historiques, pas à cette exécution.
- Les statuts des 16 scénarios restent tels que justifiés dans la matrice; pas de qualification Ghostty/physique dans cette mission.
- Risque résiduel : un flux de nombreux événements peut provoquer copies de snapshots, projections F3 répétées et redraws; son impact utilisateur n'est pas établi. Il n'y a donc pas de base pour optimiser ni pour conclure à un problème de performance.

**Décision proposée :** ajouter, dans une qualification ultérieure autorisée, un seul test déterministe reliant le vrai canal saturé à la projection F3, puis mesurer les coûts snapshot/projection avec le protocole ci-dessus. Garder le résultat 4 ouvert jusqu'à décision explicite sur ces preuves ou acceptation documentée des réserves.

## Verdict

**PARTIAL.** La compilation propre et les groupes ciblés réussissent; les tests établissent séparément les pertes worker, la propagation d'incomplétude dans les métriques et le texte `PARTIAL` en F3. Aucun défaut fonctionnel n'a été reproduit. La chaîne intégrée `EventChannel` saturé → événements perdus → `MetricCollector` incomplet → `PARTIAL` visible en F3 n'est pas testée de bout en bout, et le coût des snapshots/projections n'est pas mesuré. Cette mission ne clôt donc pas le résultat 4 de la roadmap.
