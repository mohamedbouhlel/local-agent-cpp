# Qualification des limites de ressources et de contexte

**Date :** 2026-11-13
**SHA qualifié :** `381d7dd2049bbb74653675d5460b27d94629b8c0`
**Branche de travail documentaire :** `qualification/resource-context-boundaries`
**Worktree :** `/home/oper/tmp/local-agent-cpp-resource-context`

## Portée et méthode

La référence distante `origin/main` a été vérifiée à `381d7dd2049bbb74653675d5460b27d94629b8c0` avant la qualification. Le worktree isolé a été créé sur ce SHA. Le checkout principal et les autres worktrees n'ont pas été modifiés. Les contrôles portent sur `src/workspace_tools.cpp`, `src/workspace_search.cpp`, `src/ui/bounded_history.cpp`, `include/ui/bounded_history.hpp`, `src/agent.cpp` et les tests associés.

Les vérifications fonctionnelles synthétiques ont utilisé le build propre `/tmp/local-agent-cpp-resource-context-build` et un petit programme temporaire `/tmp/resource_probe.cpp`. Le programme a généré uniquement des fichiers synthétiques dans un répertoire temporaire dédié, puis l'a supprimé lui-même. Les temps ci-dessous sont des mesures ponctuelles locales, non des benchmarks ni des garanties de performance. RSS non mesuré. Aucun serveur LLM réel n'a été utilisé.

## Limites et mesures observées

| Domaine | Mécanisme constaté dans le code | Vérification et mesure | Classement |
|---|---|---|---|
| `read_file` | Lit au plus 50 001 octets pour détecter le dépassement, retourne au plus 50 000 octets de contenu et ajoute un marqueur si le fichier est plus long. Le marqueur parle de « characters », mais la limite est appliquée aux octets de `std::string`. | Fichier synthétique de 50 000 octets : sortie 50 000 octets, sans marqueur, 0,082 ms. Fichier de 50 001 octets : préfixe conservé de 50 000 octets, marqueur présent, sortie totale 50 045 octets, 0,066 ms. Le test versionné `workspace.limite_lecture_50000_et_marqueur` a également réussi. | **Limite explicitement appliquée** en octets. **Écart de libellé** « caractères »/octets; aucune anomalie fonctionnelle observée sur ces données ASCII. La frontière Unicode n'a pas été mesurée. |
| `search_files`, nombre de résultats | La recherche s'arrête à 50 correspondances et ajoute un marqueur lorsqu'une 51e correspondance est rencontrée. | Fixtures distinctes : 49 correspondances → 49 lignes de résultat, pas de marqueur, 1 069 octets, 0,058 ms; 50 → 50 lignes, pas de marqueur, 1 091 octets, 0,046 ms; 51 → 50 lignes plus marqueur (51 lignes de sortie), 1 130 octets, 0,045 ms. Le test versionné couvre 60 correspondances et confirme le plafond, mais pas chacun des seuils 49/50/51; ces seuils ont été vérifiés par le probe temporaire. | **Limite explicitement appliquée** au nombre de correspondances. |
| `search_files`, taille des lignes et parcours | Aucun plafond de taille de ligne, de nombre d'octets inspectés ou de durée n'est appliqué. La ligne est accumulée dans une `std::string` jusqu'au saut de ligne; toute ligne correspondante entière est ajoutée au flux de sortie. Si moins de 50 matches existent, le parcours peut examiner tous les fichiers admissibles. | Une ligne synthétique de 4 097 octets contenant une correspondance a été retournée intégralement; sortie 4 104 octets, 0,208 ms. Aucun épuisement de ressource n'a été observé. | **Ressource non bornée dans le code inspecté** pour les octets par ligne et les octets parcourus. Risque structurel, pas un épuisement mémoire reproduit. |
| `list_files` | Parcours récursif des fichiers réguliers non cachés; aucune limite de nombre de chemins ou de taille totale de sortie dans ce chemin. | Jeu synthétique : 1 fichier → 7 octets de sortie; 2 → 14 octets; 4 → 28 octets. Temps ponctuels : 0,014 ms, 0,014 ms et 0,016 ms respectivement. Les sorties croissent avec le nombre et la longueur des chemins; aucun plafond n'a été constaté. | **Ressource non bornée dans le code inspecté** pour le nombre de chemins, la sortie totale et la profondeur de parcours. Aucune dégradation ni saturation n'a été démontrée par ce petit jeu. |
| `BoundedHistory` | Valeurs par défaut : 32 entrées, 16 384 octets de texte conservé. FIFO jusqu'au respect simultané des deux limites. Une chaîne individuelle plus grande que le plafond est rejetée et comptabilisée séparément. Les compteurs d'éviction/rejet vérifient leur overflow. | Groupe `tui4b1` exécuté : 10 cas, 6 125 assertions, zéro échec. Il couvre l'état vide, plafond d'entrées/FIFO, éviction par octets, limites simultanées, rejet, UTF-8 multioctet, neutralisation avant comptage, indépendance des panneaux et 2 000 insertions avec invariants vérifiés après chaque ajout. | **Limite explicitement appliquée** aux entrées et aux octets textuels conservés. Ce n'est pas une borne RSS : allocations de nœuds, capacités, objets de rendu et argument d'entrée temporaire ne sont pas inclus. |
| Historique Agent / contexte | `max_history_messages = 20` est une cible de troncature par tours complets; `trim_history()` retire les tours anciens mais préserve le tour courant. Le nombre d'itérations d'outils est limité à 5. Aucun tokenizer ni budget de tokens n'est présent. | Groupe `agent` exécuté : 44 cas, 1 010 assertions, zéro échec. `historique_tronque_par_tours_complets` vérifie 12 tours simples et un payload de requête d'au plus 20 messages. Aucun test de charge tokenisée n'est possible ou pertinent sans tokenizer et profil de modèle. Le code ne garantit pas que le tour courant reste sous 20 messages, notamment si une réponse contient beaucoup d'appels d'outils. | **Limite documentée mais non équivalente à un budget de contexte.** Pas de comptabilité tokens; taille du dernier tour non plafonnée par ce mécanisme. |

## Vérifications exécutées

Configuration et compilation propres :

```text
cmake -S . -B /tmp/local-agent-cpp-resource-context-build -DAGENT_BUILD_TESTS=ON  PASS
cmake --build /tmp/local-agent-cpp-resource-context-build -j2                 PASS
```

Résultats des groupes ciblés :

```text
agent_tests --filter=workspace  : 48 cas, 206 assertions, 0 échec
agent_tests --filter=tui4b1     : 10 cas, 6 125 assertions, 0 échec
agent_tests --filter=agent      : 44 cas, 1 010 assertions, 0 échec
```

Le premier lancement du groupe `agent` dans le sandbox a échoué avant les tests, car le mock n'a pas pu créer son socket (`mock: socket() failed`). Après autorisation de l'accès loopback local, la même commande a réussi. Ce premier résultat est une restriction d'exécution, pas un échec du produit.

Le harnais complet, le CTest intégral, un serveur LLM réel, une mesure RSS, et des charges volumineuses de listing/recherche n'ont **pas** été exécutés. Les mesures synthétiques sont volontairement petites et bornées; elles ne permettent aucune extrapolation de performance ou d'épuisement mémoire.

## Classification et réserves structurelles

- **Limites explicitement appliquées :** lecture `read_file` à 50 000 octets; `search_files` à 50 résultats; `BoundedHistory` à 32 entrées et 16 384 octets textuels retenus; 5 itérations d'outils par tour.
- **Limites documentées mais non mesurées :** 20 messages cible de troncature de l'historique Agent; ce seuil ne borne ni les tokens ni nécessairement le nombre de messages du tour courant.
- **Ressources non bornées dans le code inspecté :** sortie et parcours de `list_files`; bytes par ligne et volume de parcours de `search_files`. Les tests et mesures n'ont pas reproduit d'épuisement de ressources.
- **Défaut fonctionnel reproduit :** aucun dans le périmètre testé.
- **Risques non démontrés :** conséquences pratiques des sorties workspace très volumineuses, charge mémoire d'un tour Agent surdimensionné, interaction entre chaînes UTF-8 tronquées par octets et limite `read_file`.

La suite de tests existante confirme plusieurs frontières de code, mais ne transforme pas les limites sans plafond en bugs observés. Toute décision d'ajouter des plafonds à `list_files`, `search_files` ou au contexte doit rester distincte et nécessiter un arbitrage; aucune correction n'a été apportée ici.

## Verdict

**PASS** pour les comportements bornés examinés : les frontières mesurées de lecture, recherche par nombre de résultats et rétention de présentation se comportent comme le code et les tests le prévoient; aucun défaut fonctionnel n'a été reproduit.

Ce PASS ne qualifie pas les ressources globales du processus ni la taille du contexte LLM. Les sorties `list_files` et `search_files` restent structurellement sans plafond de volume, et Agent ne possède pas de budget tokens. Ce sont les limites structurelles ouvertes à conserver dans la roadmap, non des défauts d'épuisement effectivement observés pendant cette mission.
