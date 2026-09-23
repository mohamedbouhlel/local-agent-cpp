# Réanalyse de la boucle agentique — dernière version de agent.cpp

## Périmètre
Analyse statique du fichier agent.cpp fourni.
Aucune modification de code ni exécution de test effectuée.

## Constats confirmés
- Le prompt utilisateur est ajouté une fois par appel à send().
- Les payloads LLM sont construits depuis history_.
- max_tool_iterations = 5 borne les tours LLM, pas les appels individuels.
- Tous les tool_calls d'une réponse sont exécutés dans la même itération.
- Les erreurs d'outil sont converties en résultats tool error et ajoutées
  à l'historique.
- Un flux sans [DONE] est rejeté.
- finish_reason=length est rejeté.
- Les fragments de texte peuvent être transmis au callback avant
  la validation finale du flux.
- À épuisement des 5 itérations, l'agent lève une AgentError après
  l'exécution des outils du dernier tour, sans requête LLM supplémentaire.
- Le message utilisateur reste dans l'historique après une exception
  HTTP ou de complétude du flux.

## Limites de l'analyse
- main.cpp non réanalysé : comportement exact du REPL multi-ligne non établi.
- Aucune trace des tours LLM du test 02 disponible pour compter les itérations.
- Aucune exécution de test réalisée pendant cette revue.

## Verdict
Analyse statique ciblée réalisée.
Le test 02 ne prouve pas un défaut de conservation du prompt dans send().
La limite de 5 porte sur les tours LLM, et non sur les tool_calls individuels.
Le comportement REPL multi-ligne reste à vérifier dans la version actuelle
de main.cpp.

## Suite
Auditer main.cpp et établir un test instrumenté comptant séparément
les tours LLM, les tool_calls et les résultats d'outils.
## Complément — audit de main.cpp

Le REPL utilise std::getline() dans sa boucle principale.
Chaque ligne non vide est transmise séparément à agent.send().
Il n'existe pas de mécanisme de saisie multi-ligne.

Si un prompt multi-ligne est collé dans le REPL, ses lignes
sont traitées comme des interactions distinctes. Cela peut
expliquer le déroulement observé lors du test agentique 02.

Le test 02 ne permet donc pas, à lui seul, de conclure à un
défaut de conservation du prompt dans Agent::send().

Les compteurs de succès et d'échec des outils sont réinitialisés
pour chaque interaction REPL.

## Verdict consolidé

L'analyse statique de agent.cpp et main.cpp confirme que :
- Agent::send() reçoit une chaîne par appel.
- Le REPL ne préserve pas un prompt multi-ligne collé comme
  une interaction unique.
- Le comportement observé au test 02 peut provenir de la
  couche REPL, sans démontrer un défaut de la boucle agentique.

Aucune compilation ni exécution de test réalisée pendant cette revue.