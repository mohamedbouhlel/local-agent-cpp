# TUI-4B.1 — Revue finale de rétention des panneaux

## Précontrôle

- Worktree : `/home/oper/projects/worktrees/local-agent-cpp-tui4b1`.
- Branche : `feat/tui-4b1-bounded-panel-history`.
- Base/HEAD de travail : `6ff8a0d2ffde48b02986c556de9daec4a760e2ff`.
- `origin/main` distant vérifié par `git ls-remote` : même SHA `6ff8a0d2ffde48b02986c556de9daec4a760e2ff`.
- Worktree UI-4 principal préservé. Son statut avant et après la revue est identique : modifications locales dans CMake, documentation UI-4, PresentationController et MetricCollector/tests correspondants.
- Les spécifications TUI, la roadmap réconciliée, le rapport d'implémentation et le diff complet ont été examinés.

## Constats et correction

### Défaut confirmé — dépassement transitoire de la collection

La première version de `BoundedHistory::add()` insérait le nouvel élément dans `entries_`, puis retirait les anciens. Pendant ces opérations, la collection elle-même pouvait momentanément contenir 33 entrées ou plus que 16 384 octets de texte, malgré le respect des plafonds au retour de la méthode. Aucun lecteur UI synchrone ne pouvait observer cet état intermédiaire, mais il ne respectait pas strictement l'invariant demandé pour l'historique conservé.

Correction minimale : `entries_` est maintenant une `std::list<std::string>`. Le nœud entrant est alloué dans une liste locale avant mutation, les anciennes entrées sont ensuite évincées, puis le nœud est transféré par `splice`. L'allocation qui peut échouer a donc lieu avant les évictions, et la collection historique ne dépasse pas ses limites pendant la transition. La simulation des évictions parcourt les éléments avec un itérateur, sans accès indexé. Les données temporaires de sanitation et le nœud préparé restent hors de la collection bornée; ils ne constituent pas une borne RSS globale.

### Constats conformes

- Les plafonds sont appliqués simultanément : 32 entrées et 16 384 octets de `std::string::size()` après sanitation.
- Une entrée supérieure au plafond d'octets est rejetée sans éviction; le compteur de rejet augmente.
- Les entrées acceptées sont évincées en FIFO. Les compteurs d'éviction et de rejet sont distincts par historique Outils/Diagnostics.
- Les additions de taille sont formulées par soustraction à partir des invariants et les compteurs vérifient leur capacité avant incrément.
- Les données sont neutralisées avant stockage. Aucun contenu d'argument/résultat d'outil, prompt ou clé n'est ajouté par cette modification.
- Les historiques des deux panneaux sont indépendants de l'EventChannel. L'EventLoop garde son statut technique séparément; la réduction des historiques ne modifie ni le statut, ni la conversation, ni le worker.
- Les avertissements d'historique partiel sont ajoutés seulement si une éviction ou un rejet a eu lieu. Les pertes EventChannel ne sont pas comptées ni reformulées par BoundedHistory.
- Le scroll est ajusté des nouvelles lignes rendues et de la croissance des avertissements repliés, sans suivre automatiquement le bas lorsque le suivi est suspendu.
- Aucun changement au moteur, HTTP/SSE, Landlock, outils workspace, `overwrite:true`, REPL classique ou MetricCollector.

### Limites de couverture relevées

Les compteurs `uint64_t` sont protégés contre le rebouclage, mais il n'est pas réaliste de les faire atteindre leur limite dans le harnais; le chemin d'overflow n'a pas été exécuté. Les tests vérifient le texte et les octets logiques stockés, pas la capacité interne des allocateurs, la mémoire temporaire ni le RSS. Les changements de navigation pendant des insertions sont exercés par les tests PTY/TUI existants et par le maintien du scroll dans le test ciblé; aucun test de terminal physique n'a été fait dans cette revue.

## Fichiers concernés

- `include/ui/bounded_history.hpp`, `src/ui/bounded_history.cpp` : type partagé, plafonds, rejet, éviction et compteurs; conteneur staging/final corrigé en revue.
- `include/ui/tui_view.hpp`, `src/ui/tui_view.cpp` : intégration indépendante aux deux panneaux, sanitation, avertissements et ancrage du scroll.
- `tests/bounded_history_test.cpp`, `tests/CMakeLists.txt` : huit tests du groupe `tui4b1`.
- `CMakeLists.txt` : ajout du composant à `agent_tui`.
- `docs/README.md` et le présent rapport.

## Couverture des huit tests TUI-4B.1

Les **6 109 assertions** ne sont pas huit assertions globales : la majorité provient des contrôles d'invariants répétés à chacune des 2 000 insertions de la charge soutenue. Les cas couvrent :

1. historique vide, ajout à 32 puis 33e entrée, FIFO et ordre conservé;
2. éviction due au plafond d'octets avant le plafond du nombre;
3. limites personnalisées simultanées et rejet oversized sans éviction;
4. chaîne vide, texte UTF-8 multioctet et comptage après sanitation;
5. compteurs Outils/Diagnostics indépendants, avertissement conditionnel et contenu FIFO;
6. scroll de Diagnostics après éviction;
7. 2 000 insertions avec contrôle après chaque ajout de la borne d'entrées, de la somme réelle des tailles et des compteurs exacts;
8. rejet d'une entrée de 16 385 octets et affichage explicite du compteur.

Les tests TUI-3 couvrent en plus le streaming, le rendu et la restauration terminal; TUI-4A inclut le scénario PTY avec réponse pendant navigation. La conversation n'utilise pas BoundedHistory et ses essais de régression sont inclus dans ces groupes.

## Validation finale exécutée

Configuration et compilation propres après correction :

```text
cmake -S . -B /tmp/local-agent-cpp-tui4b1-final -DAGENT_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-tui4b1-build/_deps/json-src
cmake --build /tmp/local-agent-cpp-tui4b1-final -j2
```

Résultats finaux sur la version corrigée :

- `agent_tests --filter=tui4b1` : 8 cas, 6 109 assertions, zéro échec.
- `agent_tests --filter=tui4a` : 15 cas, 99 assertions, zéro échec.
- `agent_tests --filter=tui3` : 13 cas, 50 assertions, zéro échec.
- `ctest --test-dir /tmp/local-agent-cpp-tui4b1-final --output-on-failure` : 13/13 groupes réussis.
- Harness complet : 222/222 cas, 7 454 assertions, zéro échec.
- Les suites ont été exécutées avec l'accès requis aux sockets loopback; les tests PTY/mock concernés ont réussi.
- `git diff --check` : réussi avant publication.

## Limites mémoire et risques résiduels

La garantie porte sur le nombre d'entrées et la somme des octets de texte UTF-8 stockés dans chacun des deux historiques après sanitation. Elle ne borne pas la capacité allouée par `std::string`/`std::list`, les copies ou buffers temporaires de sanitation/rendu, le transcript conversationnel, le brouillon de saisie, l'EventChannel ou le RSS du processus. Les plafonds restent des paramètres initiaux, non optimisés par mesure. L'exception de débordement des compteurs est définie mais non atteinte par les tests.

## Verdict

Revue favorable après correction du dépassement transitoire confirmé. Les validations propres et les tests requis passent. Aucun commit, push, merge ou changement du checkout UI-4 n'a été effectué pendant la revue. Aucun travail TUI-4B.2 ou TUI-5 n'a commencé.
