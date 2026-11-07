# TUI-4A — Revue finale, validation et publication

**Date :** 2026-11-07
**Branche :** `feat/tui-4a-navigation-views`
**Base :** `738c77792713b5a83e82824b461b3616a9fd550d`
**Référence distante vérifiée :** `origin/main` et `git ls-remote origin main` désignent tous deux `738c77792713b5a83e82824b461b3616a9fd550d`.

## Périmètre et constats

Revue du diff TUI-4A, de `TuiNavigation`, `TuiViewData`, `TuiView`, `TuiEventLoop`, `TuiApplication`, du driver PTY et des tests. Aucun défaut fonctionnel confirmé n'a nécessité de correction du code produit pendant cette revue. Une lacune de couverture Unicode et de conservation d'un brouillon multiligne dans le scénario PTY a été constatée; les tests ont été renforcés sans changer le moteur ni la logique de navigation.

### Navigation et état

- F1–F6 sélectionnent les pages par ordre de l'énumération; Échap revient à Conversation; Tab change le focus; F9 soumet uniquement depuis Conversation; F10 demande la fermeture.
- Le focus est conservé par page. La saisie, le curseur, le transcript, le viewport et les offsets de scroll restent détenus par la même instance de vue lors des changements de page.
- La boucle continue à traiter les événements et le résultat du même worker lorsque la page Outils est affichée. Le test PTY vérifie qu'un seul POST est reçu et que la réponse peut arriver hors de Conversation.
- Les erreurs de requête, incohérences et pertes observées sont signalées dans Diagnostics; aucune réussite fonctionnelle n'est inférée.
- Aucun changement à `Agent::send()`, HTTP/SSE, Landlock, workspace tools, `overwrite:true` ou au REPL classique n'apparaît dans le diff.

### Rendu et données

- Les lignes conversationnelles et de saisie utilisent `mvwaddstr`, laissant ncurses couper selon les cellules visuelles au lieu d'un plafond exprimé en octets. Le wrapping calcule la largeur avec `wcwidth` sous la locale activée par `TerminalSession`.
- Le nouveau test vérifie la conservation exacte des octets UTF-8 d'un exemple comprenant accent, CJK, marque combinante, emoji/ZWJ, wrap étroit puis largeur large après redimensionnement. Le PTY vérifie aussi que l'échantillon Unicode de la réponse est présent dans la sortie.
- Cela ne valide pas universellement les graphèmes, les variantes de largeur d'emoji, toutes les locales ni tous les terminaux. Le découpage peut ne pas respecter un cluster graphemique complexe même si les octets sont conservés.
- Outils : schémas déclarés et activité effectivement reçue; arguments et résultats exclus. Métriques : indisponibilité explicite. Diagnostics : entrées effectivement observées. Configuration : modèle, timeout et origine d'endpoint expurgée; clé non affichée. Aide : raccourcis implémentés.

## Tests et commandes

Build propre final :

```text
cmake -S . -B /tmp/local-agent-cpp-tui4a-final-build -DAGENT_BUILD_TESTS=ON -DFETCHCONTENT_SOURCE_DIR_JSON=/home/oper/projects/local-agent-cpp/build/_deps/json-src
cmake --build /tmp/local-agent-cpp-tui4a-final-build -j2
```

Les deux commandes ont réussi. CMake a signalé son avertissement développeur CMP0135 existant; il n'a pas bloqué la génération ou la compilation.

Validation avec accès loopback autorisé pour le serveur mock :

```text
ctest --test-dir /tmp/local-agent-cpp-tui4a-final-build --output-on-failure  -> 12/12 groupes réussis
/tmp/local-agent-cpp-tui4a-final-build/tests/agent_tests                    -> 214 cas, 1 345 assertions, zéro échec
/tmp/local-agent-cpp-tui4a-final-build/tests/agent_tests --filter=tui4a     -> 15 cas, 99 assertions, zéro échec
/tmp/local-agent-cpp-tui4a-final-build/tests/agent_tests --filter=tui3      -> 13 cas, 50 assertions, zéro échec
git diff --check                                                             -> réussi
```

Une première exécution dans le sandbox restreint a échoué sur `mock: socket() failed`; les tests ont été relancés avec l'accès loopback autorisé et ont réussi. Les vérifications PTY ne constituent pas une validation manuelle Ghostty. Aucun essai SSH ou serveur LLM réel n'a été réalisé.

## Publication et état Git

`origin/main` n'a pas avancé depuis la base. La recherche GitHub de PR pour `feat/tui-4a-navigation-views` n'a retourné aucune PR correspondante avant publication. Fichiers modifiés prévus : sources TUI-4A, CMake et tests associés, index `docs/README.md`, rapport d'implémentation et présent rapport. Aucun fichier UI-4 n'a été importé.

Le checkout UI-4 `/home/oper/projects/local-agent-cpp` conserve exactement les modifications locales relevées au précontrôle; il n'a pas été modifié par cette revue. Le worktree TUI-4A n'a pas été réinitialisé ni nettoyé.

## Limites et verdict

La qualification inclut build, harnais, CTest et PTY/mock. Elle ne remplace pas une observation manuelle sur Ghostty. La gestion complète des graphemes, la compatibilité multi-terminal et les vues avancées TUI-4B/4C restent hors validation.

**Verdict : VALIDÉ pour la revue technique et la validation automatisée TUI-4A.** Aucun défaut confirmé du code n'a été trouvé; la couverture a été complétée pour Unicode et la conservation multiligne. Publication demandée vers `main` en PR brouillon; aucune fusion ni phase suivante.
