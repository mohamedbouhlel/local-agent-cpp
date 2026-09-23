# Test agentique 02 — Multi-étapes et persistance

## Objectif
Tester la boucle agentique sur une tâche multi-étapes :
découverte, lecture, écritures, relecture et réponse finale.

## Environnement
- Application : local-agent-cpp
- Backend : LiteLLM
- Workspace : workspace/agentic-test-02

## Résultat observé
- list_files : succès
- read_file risques.txt : succès
- read_file projet.txt et risques.txt : succès
- write_file synthese.md : 3 appels réussis
- read_file synthese.md : succès
- Vérification externe du fichier : contenu conforme

## Défauts observés
1. Le prompt initial a été déclaré incomplet.
2. L'agent a demandé à l'utilisateur de repréciser la tâche.
3. Après lecture des sources, l'agent a demandé le contenu à écrire,
   pourtant spécifié dans le prompt initial.
4. Trois écritures successives ont été effectuées pour un seul fichier.
5. La trace affiche 7 appels d'outils réussis. Vérifier si cela
   dépasse max_tool_iterations = 5 selon la sémantique du compteur.
6. Plusieurs confirmations finales redondantes.

## Verdict
PARTIELLEMENT RÉUSSI.

Le fichier final est correct et relu, mais la conservation du prompt,
l'autonomie et l'efficacité du chaînage ne sont pas satisfaisantes.
La conformité à la limite d'itérations reste à vérifier dans le code.

## Limites
Test nominal uniquement. Pas de test d'erreur outil, d'échec réseau,
de tool call malformé, de concurrence ou de reprise après crash.

## Suite recommandée
Auditer la sémantique de max_tool_iterations et la conservation
du prompt utilisateur dans la boucle agentique avant de poursuivre
les tests de stress.
