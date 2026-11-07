# TUI-4B.1 — Rétention bornée des panneaux Outils et Diagnostics

## Référence et périmètre

- Base : `6ff8a0d2ffde48b02986c556de9daec4a760e2ff`.
- Branche : `feat/tui-4b1-bounded-panel-history`.
- Worktree : `/home/oper/projects/worktrees/local-agent-cpp-tui4b1`.
- Build propre : `/tmp/local-agent-cpp-tui4b1-build`.
- Objectif : plafonner les deux historiques de panneaux indépendamment, sans changer le moteur, le transport, Landlock, les outils workspace ou la CLI classique.

## Règles implémentées

Le nouveau `agent::ui::BoundedHistory` est un composant FIFO basé sur `std::deque<std::string>`. Chaque instance applique séparément :

- au plus 32 entrées;
- au plus 16 384 octets de chaînes conservées;
- aucune expiration temporelle.

`TuiView` possède une instance pour les activités Outils et une autre pour Diagnostics. Les textes sont d'abord passés par le sanitizer terminal existant; `std::string::size()` mesure ensuite leurs octets UTF-8 conservés, et non les caractères, cellules terminal ou RSS.

Une entrée individuelle au-delà de 16 384 octets après neutralisation n'est pas retenue. Son compteur `rejected_too_large_count` augmente; elle ne provoque aucune éviction d'entrées existantes. Une entrée acceptée évince les plus anciennes jusqu'au respect simultané des deux plafonds. Les compteurs d'éviction et de rejet sont indépendants entre les deux vues. Les dépassements arithmétiques des compteurs déclenchent `std::overflow_error` au lieu de reboucler.

La vue correspondante ajoute un avertissement explicite uniquement lorsque des entrées ont été évincées ou rejetées, avec les comptes exacts. Les avertissements sont inclus dans les lignes défilables. Le scroll hors du bas est ajusté du nombre de lignes de la nouvelle entrée et de toute ligne d'avertissement nouvellement ajoutée; les évictions à l'ancien début ne déplacent pas le point logique de consultation. L'ajout de ces entrées ne modifie pas `status_`, le transcript de conversation, ni l'état technique du tour. Les appels restent synchrones sur le thread UI existant.

## Fichiers concernés

- `include/ui/bounded_history.hpp`, `src/ui/bounded_history.cpp` : contrat FIFO, plafonds et compteurs.
- `include/ui/tui_view.hpp`, `src/ui/tui_view.cpp` : remplacement des deux vecteurs non bornés, projection des avertissements et maintien du scroll.
- `tests/bounded_history_test.cpp`, `tests/CMakeLists.txt` : huit tests TUI-4B.1 et groupe CTest dédié.
- `CMakeLists.txt` : ajout du fichier source à `agent_tui`.
- `docs/README.md` : index du présent rapport.

Aucun fichier de moteur `agent_core`, HTTP/SSE, workspace, Landlock, `src/main.cpp`, REPL ou MetricCollector n'a été modifié.

## Tests et résultats observés

Configuration propre :

```text
cmake -S . -B /tmp/local-agent-cpp-tui4b1-build -DAGENT_BUILD_TESTS=ON
```

La configuration a réussi et a utilisé `nlohmann_json` v3.11.3 via FetchContent. Le téléchargement a attendu dans le sandbox restreint; la configuration a ensuite été relancée avec l'autorisation réseau nécessaire.

Build :

```text
cmake --build /tmp/local-agent-cpp-tui4b1-build -j2
```

Build réussi après correction d'une erreur d'adaptation d'itérateurs lors de la première compilation. Un premier test ciblé a aussi détecté une attente de scroll incorrecte lorsque l'avertissement de rétention apparaît; le test et l'ajustement d'ancrage ont été corrigés avant la validation finale.

Résultats finaux :

- Groupe `tui4b1` : 8 cas, 6 109 assertions, zéro échec.
- TUI-4A : 15 cas, 99 assertions, zéro échec.
- TUI-3 : 13 cas, 50 assertions, zéro échec.
- CTest : 13/13 groupes réussis.
- Harness complet : 222/222 cas, 7 454 assertions, zéro échec.
- Les tests PTY et HTTP mock présents dans les groupes TUI concernés ont été exécutés; ils ne constituent pas une qualification manuelle d'un terminal physique.
- `git diff --check` : vérifié après intégration documentaire.

Le test de charge ajoute 2 000 entrées de 700 octets, contrôle après chaque insertion que le nombre d'entrées et la somme exacte des octets restent sous leurs limites, puis vérifie l'ordre et les compteurs finaux. Les tests couvrent aussi les entrées vides, UTF-8 multioctet, neutralisation terminal, rejet oversized, FIFO par nombre et par octets, indépendance entre panneaux, avertissements et scroll après éviction.

## Limites et risques résiduels

Ces plafonds limitent uniquement le nombre et la somme des octets textuels conservés dans les deux historiques. Ils ne bornent pas la mémoire totale du processus : allocations et capacité des conteneurs, texte temporaire de sanitation, lignes temporaires de rendu, transcript conversationnel et brouillon de saisie restent hors de cette limite. Ils ne bornent pas non plus le canal `EventChannel`, dont les limites et compteurs ont une sémantique distincte.

Les valeurs 32 entrées et 16 KiB sont les paramètres initiaux demandés, sans mesure d'usage démontrant qu'ils sont optimaux. Les compteurs sont des `uint64_t`; une saturation théorique est traitée par exception, mais n'a pas été testée en atteignant réellement la limite numérique. L'interface n'ajoute aucune donnée de prompt, argument ou résultat sensible; elle conserve uniquement les lignes déjà fournies à ses méthodes, après sanitation.

## Verdict

Implémentation et validations automatisées TUI-4B.1 réussies sur la base indiquée. Cela ne valide pas une borne RSS globale, ne qualifie pas l'ergonomie sur un terminal réel et n'autorise pas TUI-4B.2 ou TUI-5. Aucun commit, push ou merge n'a été effectué.
