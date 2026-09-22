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
| [`reports/PHASE_1_1_REPORT.md`](reports/PHASE_1_1_REPORT.md) | mise en place du harness de tests reproductible |
| [`reports/PHASE_1_1_REVIEW_REPORT.md`](reports/PHASE_1_1_REVIEW_REPORT.md) | revue ciblée du harness ci-dessus |

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
- revue d'un rapport : `PHASE_<n>_<SUJET>_REVIEW_REPORT.md`.
