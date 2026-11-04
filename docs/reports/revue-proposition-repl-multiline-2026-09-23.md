# Revue de la proposition REPL multiline — 2026-09-23

## Objet
Revue documentaire de `repl-multiline-requirements-proposal.md`, du rapport de définition des exigences et du rapport de préparation. Aucun accès en écriture au dépôt Git n'a été effectué ; ce rapport est un artefact de revue à intégrer manuellement dans `docs/reports/`.

## Verdict
**PROPOSITION EXPLOITABLE SOUS RÉSERVE DE CORRECTIONS — APPROBATION GLOBALE À DIFFÉRER.**

La proposition distingue correctement les constats, les propositions et les validations non exécutées. Le périmètre reste centré sur la lecture REPL, sans modification annoncée de l'agent, du SSE ou du tool-calling.

## Constat bloquant
### Contradiction REPL-08 / REPL-09
- REPL-08 indique qu'un EOF/Ctrl+D en multiline abandonne le bloc et termine proprement le REPL.
- REPL-09 indique qu'après un abandon par EOF, le REPL revient à l'invite primaire.

Ces résultats sont incompatibles si EOF termine le processus. Décision nécessaire : EOF abandonne le bloc **et termine le REPL**, ou bien le REPL reprend après abandon. Aligner les deux exigences, les scénarios d'acceptation et la matrice de traçabilité.

## Points à arbitrer ou préciser
1. **Conservation exacte :** définir le traitement de CRLF (`\r\n`), l'ajout ou non d'un `\n` final et les limites éventuelles de taille. `std::getline` retire le séparateur de ligne ; la reconstruction doit avoir une règle explicite.
2. **Bloc vide :** distinguer zéro ligne accumulée d'un bloc constitué uniquement de lignes vides. Confirmer la règle d'envoi pour ce dernier.
3. **Bloc composé d'espaces :** confirmer qu'il constitue du contenu et qu'il est envoyé, conformément à la conservation annoncée.
4. **Ligne `:end` littérale :** la sentinelle byte-exacte rend impossible la saisie de cette ligne comme contenu en multiline. Faire ratifier cette limitation.
5. **Intégration G1/CMake :** l'ajout de `PromptReader` à `agent_core` et les changements CMake doivent rester conditionnés à une autorisation explicite.
6. **Emplacement documentaire :** ratifier `docs/specifications/` et prévoir l'indexation documentaire lors de l'intégration.

## Recommandation de conduite
1. Corriger la contradiction EOF/retour à l'invite.
2. Faire arbitrer les décisions D-1 à D-7, sans transformer les recommandations de Codex en décisions tacites.
3. Mettre à jour la proposition et sa matrice de traçabilité.
4. Soumettre la version corrigée à une approbation explicite REPL-01 à REPL-09.
5. Seulement après approbation, préparer l'implémentation et ses tests, avec rapport durable dans `docs/reports/`.

## Limites
Revue documentaire uniquement. Aucun code, test, build, CMake ou état Git n'a été vérifié ou modifié dans le dépôt pendant cette revue. Ce rapport n'a pas été écrit directement dans le dépôt.
