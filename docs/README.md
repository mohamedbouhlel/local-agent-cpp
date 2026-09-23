# Documentation — agent-cpp

Index du répertoire `docs/`. Ce répertoire centralise la documentation du projet,
classée par type. Il ne remplace pas le [`README.md`](../README.md) racine, qui
reste le point d'entrée du projet (prérequis, configuration, build, exécution).

## Documents de référence

Documents **durables**, destinés à être tenus à jour.

| Document | Contenu |
| --- | --- |
| [`context/PROJECT_CONTEXT.md`](context/PROJECT_CONTEXT.md) | état réel du code à HEAD : architecture, config, outils, limites, état de validation |
| [`history/PROJECT_HISTORY.md`](history/PROJECT_HISTORY.md) | reconstitution chronologique du projet depuis l'historique Git, décisions et révisions |
| [`roadmap/ROADMAP.md`](roadmap/ROADMAP.md) | trajectoire et suites possibles, séparant faits, déductions et propositions |
| [`questions/OPEN_QUESTIONS.md`](questions/OPEN_QUESTIONS.md) | informations manquantes, contradictions et points non vérifiables |

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

> Les rapports de phase consignent un résultat à un instant donné. **Ils ne
> remplacent pas les documents de référence** : tout fait durable qui y est établi
> doit être reporté dans `context/`, `history/`, `roadmap/` ou `questions/`.

## Hors de `docs/`

Restent à la racine, pour les outils et les lecteurs :

- [`README.md`](../README.md) — prérequis, configuration, build, exécution ;
- [`USAGE.md`](../USAGE.md) — guide d'utilisation (REPL, outils, limites, dépannage) ;
- [`AGENTS.md`](../AGENTS.md) — instructions de travail et décisions d'architecture ;
- [`CLAUDE.md`](../CLAUDE.md) — renvoi vers `AGENTS.md`.

## Convention de nommage observée

- document de référence : `<SUJET>.md` (majuscules) ;
- rapport de phase : `PHASE_<n>_<SUJET>_REPORT.md` ;
- rapport d'itération d'un même jalon : `PHASE_<n>_<SOUS-ÉTAPE>_REPORT.md`
  (ex. `..._R2_...`, `..._V_REPRODUCIBILITY_...`, `..._V2_...`, `..._V3_...`,
  `..._GIT_INTEGRATION_...`, `..._GIT_COMMIT_...`) ;
- revue d'un rapport : `PHASE_<n>_<SUJET>_REVIEW_REPORT.md` ;
- audit thématique : `<sujet>-audit.md` (ex. `streaming-audit.md`).
