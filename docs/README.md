# Documentation — agent-cpp

Index du répertoire `docs/`. Ce répertoire centralise la documentation du projet,
classée par type. Il ne remplace pas le [`README.md`](../README.md) racine, qui
reste le point d'entrée du projet (prérequis, configuration, build, exécution).

## Documents de référence

Documents **durables**, destinés à être tenus à jour. Le contexte technique est
une **photographie datée à un commit**, non une promesse de synchronisation
permanente avec `main`.

| Document | Contenu |
| --- | --- |
| [`context/PROJECT_CONTEXT.md`](context/PROJECT_CONTEXT.md) | photographie vérifiée au SHA indiqué dans le document : architecture, configuration, limites et niveaux de preuve |
| [`history/PROJECT_HISTORY.md`](history/PROJECT_HISTORY.md) | reconstitution chronologique du projet depuis l'historique Git, décisions et révisions |
| [`roadmap/ROADMAP.md`](roadmap/ROADMAP.md) | trajectoire et suites possibles, séparant faits, déductions et propositions |
| [`questions/OPEN_QUESTIONS.md`](questions/OPEN_QUESTIONS.md) | informations manquantes, contradictions et points non vérifiables |

## Spécifications

Documents normatifs validés et spécifications en cours de ratification ; le statut est indiqué dans chaque entrée.

| Spécification | Contenu |
| --- | --- |
| [`specifications/repl-multiline-requirements.md`](specifications/repl-multiline-requirements.md) | **REPL multiline** — exigences `REPL-01` → `REPL-09` **approuvées, implémentation vérifiée au SHA daté de M4** (D-1 à D-8 et arbitrages A-1 à A-9) : déclenchement `:multi`, terminateur `:end`, conservation exacte, unicité de l'envoi, blocs vides, `exit`, EOF/Ctrl+D, retour à l'invite |
| [`specifications/console-ui-architecture-requirements.md`](specifications/console-ui-architecture-requirements.md) | **SPEC-UI-001 v2.0** : contrats de TUI plein écran et CLI classique; les choix effectivement intégrés et les rapports datés prévalent sur les propositions historiques |
| [`specifications/SPEC-LOFI-001-v1.1.md`](specifications/SPEC-LOFI-001-v1.1.md) | SPEC-LOFI-001 v1.1 — identité UX LOFITERM, aide, souris, Markdown et non-régression; copie de l'étude source avec son statut historique |

## Rapports de phase

Artefacts **datés**, liés à un jalon précis.

| Rapport | Objet |
| --- | --- |
| [`reports/streaming-audit.md`](reports/streaming-audit.md) | audit en lecture seule de l'état réel du streaming avant implémentation (framing SSE, reconstruction des deltas) |
| [`reports/PHASE_1_1_REPORT.md`](reports/PHASE_1_1_REPORT.md) | mise en place du harness de tests reproductible (micro-harnais maison, 64 cas / 239 assertions, 5 groupes) |
| [`reports/PHASE_1_1_REVIEW_REPORT.md`](reports/PHASE_1_1_REVIEW_REPORT.md) | revue indépendante du harness ci-dessus : représentativité, déterminisme, couverture, faux positifs |
| [`reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md`](reports/PHASE_1_1_R2_MOCK_TIMEOUT_REPORT.md) | remédiation du constat R-01 : mock HTTP borné (`SO_RCVTIMEO`, réponse `408`, réveil au destructeur) |
| [`reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md`](reports/PHASE_1_1_V_REPRODUCIBILITY_REPORT.md) | audit de versionnabilité et de reproductibilité de la suite de tests (écarts E1–E3) |
| [`reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md`](reports/PHASE_1_1_V2_REPRODUCIBILITY_FIX_REPORT.md) | correction des écarts E2/E3 (isolation des répertoires de test par suffixe de PID, chemin absolu supprimé) |
| [`reports/PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md`](reports/PHASE_1_1_V3_GIT_INTEGRATION_PREPARATION_REPORT.md) | préparation en lecture seule de l'intégration Git : liste exacte des fichiers à intégrer, absence de hors-périmètre |
| [`reports/PHASE_1_1_GIT_INTEGRATION_REPORT.md`](reports/PHASE_1_1_GIT_INTEGRATION_REPORT.md) | intégration Git contrôlée : staging explicite des 23 fichiers, sans commit ni push |
| [`reports/PHASE_1_1_GIT_COMMIT_REPORT.md`](reports/PHASE_1_1_GIT_COMMIT_REPORT.md) | commit contrôlé `1abe663` (23 fichiers) : contrôles préalables, contenu, contrôles post-commit |
| [`reports/PHASE_1_1_DOC_2_REPORT.md`](reports/PHASE_1_1_DOC_2_REPORT.md) | cohérence documentaire après commit : index complet des rapports, mise à jour d'`AGENTS.md` §Test, contrôles de liens |
| [`reports/PHASE_1_1_GIT_3_AUDIT_REPORT.md`](reports/PHASE_1_1_GIT_3_AUDIT_REPORT.md) | audit du périmètre documentaire post-commit : périmètre attendu confirmé, contrôles, anomalies A-1→A-4 |
| [`reports/PHASE_1_2_MOCK_SSE_REPORT.md`](reports/PHASE_1_2_MOCK_SSE_REPORT.md) | mock SSE versionné : scénarios de bout en bout (texte fragmenté, tool call fragmenté avec `tool_call_id`) ; 66/66 cas |
| [`reports/PHASE_3_HTTPCLIENT_POST_REPORT.md`](reports/PHASE_3_HTTPCLIENT_POST_REPORT.md) | P3 — préparation d'arbitrage sur `HttpClient::post()` : contrat, usages, couverture (4 cas), options A/B, recommandation |
| [`reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md`](reports/PHASE_3_1_HTTPCLIENT_POST_RECONCILIATION_REPORT.md) | P3.1 — réconciliation documentaire après décision A : `post()` conservé et déjà testé (ROADMAP, Q3, contexte) |
| [`reports/PHASE_3_2_GIT_CLOSURE_AUDIT_REPORT.md`](reports/PHASE_3_2_GIT_CLOSURE_AUDIT_REPORT.md) | P3/P3.1 — audit de clôture Git : périmètre, cohérence, vérifications, verdict |
| [`reports/PHASE_3_3_TEST_DOCUMENTATION_RECONCILIATION_REPORT.md`](reports/PHASE_3_3_TEST_DOCUMENTATION_RECONCILIATION_REPORT.md) | P3.3 — dette E-1 : suite de tests « présente » (66 cas / 264 assertions, 5 groupes), CI absente ; ROADMAP §A et Q1 réconciliés |
| [`reports/PHASE_R0_EXECUTION_LOOP_RELIABILITY_REPORT.md`](reports/PHASE_R0_EXECUTION_LOOP_RELIABILITY_REPORT.md) | R0 — fiabilisation de la boucle : complétude du flux (`[DONE]`), `flush()`, validation des tool calls, preuve d'écriture, verdict d'exécution (75 cas / 298 assertions) |
| [`reports/PHASE_R0_DOCUMENTATION_RECONCILIATION_REPORT.md`](reports/PHASE_R0_DOCUMENTATION_RECONCILIATION_REPORT.md) | R0 — réconciliation documentaire des compteurs : référence courante **75 cas / 298 assertions** (R0) vs relevé historique **66 / 264** (P3.3) |
| [`reports/PHASE_GIT_CLOSURE_AUDIT_REPORT.md`](reports/PHASE_GIT_CLOSURE_AUDIT_REPORT.md) | audit de clôture Git : inventaire R0 / P3.3, matrice de séparation, fichiers mixtes et risques avant staging |
| [`reports/PHASE_GIT_CLOSURE_EXECUTION_REPORT.md`](reports/PHASE_GIT_CLOSURE_EXECUTION_REPORT.md) | clôture Git en 2 commits — **constat d'arrêt (BLOQUÉ)** : fichier non suivi inattendu hors listes autorisées |


### Rapports supplémentaires et archives historiques

Les entrées ci-dessous complètent l'index initial sans effacer les intitulés des rapports antérieurs. Leur présence **n'atteste pas une validation au HEAD courant** ; lire les commits, dates, conditions d'essai et limites dans chaque rapport.

| Rapport | Objet |
| --- | --- |
| [`2026-09-23-reanalyse-boucle-agentique.md`](reports/2026-09-23-reanalyse-boucle-agentique.md) | Rapport ou revue historique : reanalyse boucle agentique.md |
| [`2026-09-23-repl-arbitrages-ratification.md`](reports/2026-09-23-repl-arbitrages-ratification.md) | Rapport ou revue historique : repl arbitrages ratification.md |
| [`2026-09-23-repl-multiline-execution-preparation.md`](reports/2026-09-23-repl-multiline-execution-preparation.md) | Rapport ou revue historique : repl multiline execution preparation.md |
| [`2026-09-23-repl-multiline-final-review-and-implementation-plan.md`](reports/2026-09-23-repl-multiline-final-review-and-implementation-plan.md) | Rapport ou revue historique : repl multiline final review and implementation plan.md |
| [`2026-09-23-repl-multiline-implementation.md`](reports/2026-09-23-repl-multiline-implementation.md) | Rapport ou revue historique : repl multiline implementation.md |
| [`2026-09-23-repl-multiline-precommit-audit.md`](reports/2026-09-23-repl-multiline-precommit-audit.md) | Rapport ou revue historique : repl multiline precommit audit.md |
| [`2026-09-23-repl-multiline-preparation.md`](reports/2026-09-23-repl-multiline-preparation.md) | Rapport ou revue historique : repl multiline preparation.md |
| [`2026-09-23-repl-multiline-requirements-definition.md`](reports/2026-09-23-repl-multiline-requirements-definition.md) | Rapport ou revue historique : repl multiline requirements definition.md |
| [`2026-09-23-repl-multiline-requirements-ratification.md`](reports/2026-09-23-repl-multiline-requirements-ratification.md) | Rapport ou revue historique : repl multiline requirements ratification.md |
| [`2026-09-23-repl-multiline-review-closure.md`](reports/2026-09-23-repl-multiline-review-closure.md) | Rapport ou revue historique : repl multiline review closure.md |
| [`2026-09-23-repl-spec-rename.md`](reports/2026-09-23-repl-spec-rename.md) | Rapport ou revue historique : repl spec rename.md |
| [`2026-09-23-test-agentique-multi-etapes.md`](reports/2026-09-23-test-agentique-multi-etapes.md) | Rapport ou revue historique : test agentique multi etapes.md |
| [`2026-09-23-validation-tests-agent.md`](reports/2026-09-23-validation-tests-agent.md) | Rapport ou revue historique : validation tests agent.md |
| [`2026-09-23_132149-test-fonctionnel-agent-litellm.md`](reports/2026-09-23_132149-test-fonctionnel-agent-litellm.md) | Compte rendu d'un essai fonctionnel historique avec LiteLLM |
| [`2026-11-04-isolation-systeme.md`](reports/2026-11-04-isolation-systeme.md) | Essai historique de l'isolation par montage et des liens physiques |
| [`2026-11-05-m2-project-context-reconciliation.md`](reports/2026-11-05-m2-project-context-reconciliation.md) | M2 — contexte actuel daté et archive historique conservée |
| [`2026-11-05-m3-history-roadmap-questions-reconciliation.md`](reports/2026-11-05-m3-history-roadmap-questions-reconciliation.md) | M3 — réconciliation historique, roadmap et questions |
| [`REVUE_CLOTURE_POST_31F3E2A.md`](reports/REVUE_CLOTURE_POST_31F3E2A.md) | Rapport ou revue historique : REVUE CLOTURE POST 31F3E2A.md |
| [`revue-proposition-repl-multiline-2026-09-23.md`](reports/revue-proposition-repl-multiline-2026-09-23.md) | Rapport ou revue historique : revue proposition repl multiline 2026 09 23.md |
| [`2026-11-05-m4-guides-spec-index-reconciliation.md`](reports/2026-11-05-m4-guides-spec-index-reconciliation.md) | M4 — guides, statut de spécification et index documentaire |

| [`2026-11-05-m5-documentation-final-consistency.md`](reports/2026-11-05-m5-documentation-final-consistency.md) | M5 — contrôle des liens, statuts, HEAD post-PR #10, preuves et limites de clôture |
| [`2026-11-05-ui-0c-specification-ratification.md`](reports/2026-11-05-ui-0c-specification-ratification.md) | UI-0C — SPEC-UI-001 et validation au merge de la PR #15 (7/7 groupes, 118 cas / 407 assertions) |
| [`2026-11-05-ui-1-event-instrumentation.md`](reports/2026-11-05-ui-1-event-instrumentation.md) | UI-1 — contrat `AgentEvent`, instrumentation de l'agent et protection des callbacks libcurl ; résultats de validation sur la branche de travail |
| [`2026-11-05-ui-1-final-review.md`](reports/2026-11-05-ui-1-final-review.md) | Revue contradictoire UI-1, correction minimale d'un échec d'outil non standard, et validation propre avant publication |
| [`2026-11-05-ui-2-presentation-state-terminal-capabilities.md`](reports/2026-11-05-ui-2-presentation-state-terminal-capabilities.md) | UI-2 — état de présentation dérivé des événements et capacités terminal injectables ; résultats de validation au SHA de base indiqué |
| [`2026-11-05-ui-2-final-review.md`](reports/2026-11-05-ui-2-final-review.md) | Revue contradictoire UI-2, correction de l'incohérence documentaire et validation propre avant publication |
| [`2026-11-05-ui-3-console-renderers.md`](reports/2026-11-05-ui-3-console-renderers.md) | UI-3 — renderers Plain/Rich, neutralisation des contrôles terminal et intégration CLI; résultats au SHA de base indiqué |
| [`2026-11-05-ui-3-final-review.md`](reports/2026-11-05-ui-3-final-review.md) | Revue finale UI-3, vérification des frontières de flux terminal, tests propres et publication |
| [`2026-11-05-tui-0-architecture-revision.md`](reports/2026-11-05-tui-0-architecture-revision.md) | TUI-0 — étude comparative, architecture cible proposée, impacts UI-4 à UI-8 et décisions encore ouvertes |
| [`2026-11-05-tui-0-poc-protocol.md`](reports/2026-11-05-tui-0-poc-protocol.md) | Complément TUI-0 — arbitrages worker/queue et protocole éliminatoire du POC FTXUI/ncursesw |
| [`2026-11-05-tui-poc-1-backend-comparison.md`](reports/2026-11-05-tui-poc-1-backend-comparison.md) | TUI-POC-1 — comparaison expérimentale isolée de FTXUI et ncursesw sur PTY; mesures et limites |
| [`2026-11-05-tui-poc-2-functional-comparison.md`](reports/2026-11-05-tui-poc-2-functional-comparison.md) | TUI-POC-2 — parité fonctionnelle, harness élargi et recommandation ncursesw avec limites |
| [`2026-11-05-tui-poc-closure-and-ncurses-qualification.md`](reports/2026-11-05-tui-poc-closure-and-ncurses-qualification.md) | Clôture POC-1/2 — revalidation PTY, mesures rejouées et qualification complémentaire ncursesw |
| [`2026-11-05-tui-poc-3-ncursesw-real-terminal-qualification.md`](reports/2026-11-05-tui-poc-3-ncursesw-real-terminal-qualification.md) | TUI-POC-3 — observations utilisateur Ghostty, correction du viewport et qualification ncursesw avec limites |
| [`2026-11-06-tui-1-async-agent-tui-contract.md`](reports/2026-11-06-tui-1-async-agent-tui-contract.md) | TUI-1 — prototype isolé du worker et du transfert d'événements borné; tests normaux et ThreadSanitizer, sans intégration moteur |
| [`2026-11-06-tui-1-final-review.md`](reports/2026-11-06-tui-1-final-review.md) | TUI-1 — revue contradictoire, couverture renforcée, validation propre et limites avant publication |
| [`2026-11-06-tui-2-real-agent-async-bridge.md`](reports/2026-11-06-tui-2-real-agent-async-bridge.md) | TUI-2 — pont FIFO asynchrone testé avec le vrai `Agent::send()`, issue conservée hors file, saturation et fermeture documentées |
| [`2026-11-06-tui-2-final-review.md`](reports/2026-11-06-tui-2-final-review.md) | TUI-2 — revue contradictoire, garde self-join et validation complète/ThreadSanitizer avant publication |
| [`2026-11-06-tui-3-ncursesw-integration.md`](reports/2026-11-06-tui-3-ncursesw-integration.md) | TUI-3 — infrastructure plein écran ncursesw, intégration `AgentWorker`, essais PTY et limites |
| [2026-11-06-tui-3-recovery-validation.md](reports/2026-11-06-tui-3-recovery-validation.md) | TUI-3 — restauration depuis les événements source et revalidation après récupération |
| [2026-11-06-tui-3-final-review.md](reports/2026-11-06-tui-3-final-review.md) | TUI-3 — revue contradictoire après récupération, résultats et risques résiduels |
| [2026-11-06-tui-3-multiline-rendering-fix.md](reports/2026-11-06-tui-3-multiline-rendering-fix.md) | TUI-3 — préfixe de rôle unique par message, indentation des lignes suivantes et tests de wrapping/streaming |
| [2026-11-07-tui-3-ghostty-final-qualification.md](reports/2026-11-07-tui-3-ghostty-final-qualification.md) | TUI-3 — clôture de qualification Ghostty rapportée par le mainteneur, analyse des anomalies et préparation du périmètre TUI-4 |
| [2026-11-07-tui-4a-navigation-six-views.md](reports/2026-11-07-tui-4a-navigation-six-views.md) | TUI-4A — navigation ncursesw, six vues fondées sur les données accessibles et validation CTest/PTY |
| [2026-11-07-tui-4a-final-review.md](reports/2026-11-07-tui-4a-final-review.md) | TUI-4A — revue contradictoire, couverture UTF-8/multiligne renforcée et résultats finaux avant publication |
| [2026-11-07-tui-4a-ghostty-qualification.md](reports/2026-11-07-tui-4a-ghostty-qualification.md) | Qualification manuelle TUI-4A sous Ghostty, rapportée par le mainteneur au SHA `51ae208` |
| [2026-11-07-post-tui4a-roadmap-reconciliation.md](reports/2026-11-07-post-tui4a-roadmap-reconciliation.md) | Réconciliation de la roadmap avec TUI-4A fusionnée et options proposées pour la suite |
| [2026-11-07-tui-4b1-bounded-panel-history.md](reports/2026-11-07-tui-4b1-bounded-panel-history.md) | TUI-4B.1 — rétention FIFO bornée, comptée en octets UTF-8 neutralisés, pour les historiques Outils et Diagnostics |
| [2026-11-07-tui-4b1-final-review.md](reports/2026-11-07-tui-4b1-final-review.md) | TUI-4B.1 — revue contradictoire, correction du dépassement transitoire de la collection et validation complète |
| [2026-11-05-ui-4-observable-metrics.md](reports/2026-11-05-ui-4-observable-metrics.md) | UI-4 — rapport historique du chantier local MetricCollector; résultats antérieurs, non réexécutés à ce SHA |
| [2026-11-10-ui-4-metric-collector-integration.md](reports/2026-11-10-ui-4-metric-collector-integration.md) | UI-4 — intégration de métriques dérivées des événements réels dans CLI/TUI, avec limites et résultats au SHA de base |
| [2026-11-10-ui-4-final-review.md](reports/2026-11-10-ui-4-final-review.md) | UI-4 — revue critique de MetricCollector, propagation de PARTIAL, saturation EventChannel et limites de mesure |
| [2026-11-10-ui4-physical-qualification.md](reports/2026-11-10-ui4-physical-qualification.md) | UI-4 — qualification physique Debian 13/Ghostty sur le périmètre rapporté, avec réserves de saturation et de performance |
| [2026-11-08-tui-retention-partial-indicator.md](reports/2026-11-08-tui-retention-partial-indicator.md) | Correctif de scroll et indicateur persistant après éviction FIFO, postérieur à la fusion de la PR #32 |
| [2026-11-08-write-approval-implementation.md](reports/2026-11-08-write-approval-implementation.md) | Autorisation explicite par création `write_file`, intégration CLI/TUI, tests et limites avant revue |
| [2026-11-08-write-approval-fresh-challenge-review.md](reports/2026-11-08-write-approval-fresh-challenge-review.md) | Revue du consentement anticipé CLI/TUI, défi par demande et résultats de validation |
| [2026-11-08-tui-approval-key-input-fix.md](reports/2026-11-08-tui-approval-key-input-fix.md) | Correction de la saisie caractère par caractère du dialogue TUI d'approbation `write_file` et validation PTY |
| [2026-11-08-v1-proto-001-malformed-sse-fix.md](reports/2026-11-08-v1-proto-001-malformed-sse-fix.md) | Correctif V1-PROTO-001 : trame JSON SSE malformée rejetée, tests de régression CLI/TUI et résultats de validation |
| [2026-11-08-tui-final-event-drain-on-close.md](reports/2026-11-08-tui-final-event-drain-on-close.md) | Correction de la notification d'approbation perdue lors de la fermeture TUI |
| [2026-11-08-v1-manual-recipe-and-docs.md](reports/2026-11-08-v1-manual-recipe-and-docs.md) | Documentation corrigée de l'approbation `write_file` et protocole de recette manuelle/compilation Debian au candidat v1.0 |
| [2026-11-09-v1-final-qualification-consolidation.md](reports/2026-11-09-v1-final-qualification-consolidation.md) | Consolidation post-PR #37, résultats automatisés datés, observations Ghostty rapportées et réserves de qualification v1.0 |
| [2026-11-09-tui-multiline-composer-fix.md](reports/2026-11-09-tui-multiline-composer-fix.md) | Composeur TUI multiligne : viewport borné, curseur visuel, navigation par lignes et validation PTY du contenu transmis |
| [2026-11-10-lofiterm-v1-physical-qualification.md](reports/2026-11-10-lofiterm-v1-physical-qualification.md) | Complément de recette Debian 13/Ghostty au SHA `78aab20` : build, tests et parcours réel d'approbation write_file rapportés par le mainteneur |
| [2026-11-10-lofiterm-graphic-integration.md](reports/2026-11-10-lofiterm-graphic-integration.md) | Intégration graphique LOFITERM à la TUI : palette adaptative, six vues, monochrome, tests PTY et limites de qualification |
| [2026-11-10-lofiterm-ux-v1.1-implementation.md](reports/2026-11-10-lofiterm-ux-v1.1-implementation.md) | Implémentation UX LOFITERM v1.1 : rubriques, souris, présentation Markdown, tests et limites au SHA de branche |
| [2026-11-10-lofiterm-ux-v1.1-final-review.md](reports/2026-11-10-lofiterm-ux-v1.1-final-review.md) | Revue ciblée UX v1.1 : matrice UX-01…UX-20, écarts souris/performance, plan séparé, recette Ghostty et verdict |
| [2026-11-10-lofiterm-ux-v1.1-targeted-corrections.md](reports/2026-11-10-lofiterm-ux-v1.1-targeted-corrections.md) | Corrections ciblées de la revue UX v1.1, résultats des tests et mesure comparative du rendu Markdown |
| [2026-11-10-lofiterm-ux-v1.1-physical-qualification.md](reports/2026-11-10-lofiterm-ux-v1.1-physical-qualification.md) | Recette Ghostty rapportée par le mainteneur : 11 contrôles déclarés PASS; correspondance officielle partielle et réserves restantes |
| [2026-11-11-lofiterm-ux-v1.1-approval-mouse-selection-fix.md](reports/2026-11-11-lofiterm-ux-v1.1-approval-mouse-selection-fix.md) | Suspension temporaire du reporting souris dans WRITE APPROVAL, tests PTY et sélection native Ghostty sans Maj confirmée par le mainteneur |
| [2026-11-11-lofiterm-ux-v1.1-click-focus-colors-fix.md](reports/2026-11-11-lofiterm-ux-v1.1-click-focus-colors-fix.md) | Clics non interactifs sans changement de focus/redraw, styles ncurses stables et résultats automatisés; contrôle visuel Ghostty restant |
| [2026-11-12-lofiterm-ux-v1.1-tui3-pty-oracle-fix.md](reports/2026-11-12-lofiterm-ux-v1.1-tui3-pty-oracle-fix.md) | Correction de l'oracle PTY `typed_no`, résultats TUI-3/UX, harnais et CTest; limite sur la trace historique non conservée |
| [2026-11-10-lofiterm-v1-final-corrections.md](reports/2026-11-10-lofiterm-v1-final-corrections.md) | Clôture v1.0 : synchronisation PTY du dialogue d'approbation, preuves du retour des résultats d'outils et validation finale |
| [2026-11-10-final-git-ui4-v1-freeze-audit.md](reports/2026-11-10-final-git-ui4-v1-freeze-audit.md) | Audit du SHA `b1d1942` : état Git, préservation du travail UI-4 local et décision sur le gel du socle technique v1.0 |
| [2026-11-10-v1-technical-tag-freeze.md](reports/2026-11-10-v1-technical-tag-freeze.md) | Publication et vérification du tag annoté `v1.0.0-technical` sur le SHA `b1d1942` |

> Les rapports de phase consignent un résultat à un instant donné. **Ils ne
> remplacent pas les documents de référence** : tout fait durable qui y est établi
> doit être reporté dans `context/`, `history/`, `roadmap/` ou `questions/`.

## Hors de `docs/`

Restent à la racine, pour les outils et les lecteurs :

- [`README.md`](../README.md) — prérequis, configuration, build, exécution ;
- [`USAGE.md`](../USAGE.md) — guide d'utilisation (REPL, outils, limites, dépannage) ;
- [`AGENTS.md`](../AGENTS.md) — instructions de travail, invariants et gouvernance ;
- [`CLAUDE.md`](../CLAUDE.md) — renvoi vers `AGENTS.md`.

## Règle de maintien à jour

Chaque document de référence doit préciser, selon sa nature, **le commit
ou la période observée**, son niveau de preuve et les éléments non vérifiés.
Les résultats d'un rapport historique ne deviennent pas des validations
courantes sans réexécution. Après une modification de code, contrôler les
impacts sur `README.md`, `USAGE.md`, `AGENTS.md`, les références
`context/`, `history/`, `roadmap/`, `questions/` et la spécification
concernée ; modifier uniquement les documents affectés.
Chaque itération documentaire est accompagnée d'un rapport durable dans
`docs/reports/`, avec sources, vérifications, limites et verdict.

## Convention de nommage observée

- document de référence : `<SUJET>.md` (majuscules) ;
- rapport de phase : `PHASE_<n>_<SUJET>_REPORT.md` ;
- rapport d'itération d'un même jalon : `PHASE_<n>_<SOUS-ÉTAPE>_REPORT.md`
  (ex. `..._R2_...`, `..._V_REPRODUCIBILITY_...`, `..._V2_...`, `..._V3_...`,
  `..._GIT_INTEGRATION_...`, `..._GIT_COMMIT_...`) ;
- revue d'un rapport : `PHASE_<n>_<SUJET>_REVIEW_REPORT.md` ;
- audit thématique : `<sujet>-audit.md` (ex. `streaming-audit.md`).
