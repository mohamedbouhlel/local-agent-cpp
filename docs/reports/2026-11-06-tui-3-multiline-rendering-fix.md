# TUI-3 — correction du rendu des messages multilignes

Date : 2026-11-06

Base : `44d1d81cf3d818c4547c7b131f1e316dad9eda2b` (`main`, PR #27 fusionnée)

Branche : `fix/tui3-multiline-rendering`

Worktree : `/home/oper/projects/worktrees/local-agent-cpp-tui3-multiline-fix`

## Diagnostic

Dans `TuiView::conversation_rows()` (désormais `rendered_conversation_rows()`), le transcript d’un `TuiMessage` était séparé sur chaque `\n`, puis le même préfixe de rôle était ajouté à chaque ligne physique avant le wrapping. C’est la cause directe des répétitions `Agent:` / `You:` et non le découpage des deltas SSE.

`TuiEventLoop::handle_event()` transmet chaque `TextDelta` à `append_assistant_delta()`. Cette méthode ajoute les fragments à l’unique message assistant actif. Les frontières de fragments n’ajoutent donc pas de rôle ni de message; elles ne faisaient que révéler la règle de présentation appliquée ensuite au transcript.

Le wrapping visuel était calculé sur le texte préfixé entier, sans préfixe de continuation. Les lignes issues du wrapping n’étaient ainsi pas alignées sous le texte du message.

## Correction

La projection des lignes est désormais exposée comme `TuiView::rendered_conversation_rows(columns)`, fonction déterministe également utilisée par le rendu et le calcul de l’ancrage du scroll.

- Le préfixe `You:`, `Agent:` ou `*` est appliqué une fois à la première ligne logique du message.
- Les lignes explicites suivantes et les lignes créées par le wrapping commencent par des espaces de continuation de même largeur que le préfixe.
- La segmentation conserve les lignes vides, les espaces, le saut de ligne final et le texte original stocké dans `TuiMessage`.
- Le marqueur visuel `[incomplete]` est rendu une seule fois après les lignes du message, sans remplacer les lignes vides du contenu.
- Les fragments streaming restent ajoutés au message assistant courant; aucun message n’est créé par fragment.
- Le viewport est toujours calculé à partir des lignes rendues, afin que l’ancrage suive les lignes visuelles actuelles.
- Le remplacement d’une réponse partielle après saturation remplace le texte du message existant et conserve un seul préfixe de rôle.

Aucun changement n’a été apporté au moteur, au protocole HTTP/SSE, à Landlock, aux outils workspace, au checkout UI-4 ou aux six vues futures.

## Tests ajoutés ou renforcés

Les tests `tui3` vérifient les messages mono- et multilignes, les lignes vides et espaces initiaux, le saut de ligne final, le contenu partiel incomplet avec lignes vides, des fragments réunis au milieu d’une ligne et autour d’un saut de ligne, le wrapping étroit avec indentation, le redimensionnement entre fragments sans perte de texte, les tours successifs et l’alternance utilisateur/assistant. Le test de remplacement après perte de détails vérifie aussi que la réponse finale ne s’ajoute pas au fragment antérieur.

Le test existant du viewport reste actif. Le scénario PTY intégré continue de couvrir le vrai `AgentWorker`, le mock HTTP, le redimensionnement 10×4 et la restauration terminal. Les tests de projection ne constituent pas une nouvelle session interactive Ghostty.

## Validation observée

Build propre final dans `/tmp/local-agent-cpp-tui3-multiline-final`, après configuration CMake fraîche avec le cache local déjà récupéré de nlohmann/json :

```sh
cmake -S . -B /tmp/local-agent-cpp-tui3-multiline-final \
  -DAGENT_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-ghostty-qualification-build/_deps/json-src
cmake --build /tmp/local-agent-cpp-tui3-multiline-final -j2
/tmp/local-agent-cpp-tui3-multiline-final/tests/agent_tests --filter=tui3
ctest --test-dir /tmp/local-agent-cpp-tui3-multiline-final --output-on-failure
/tmp/local-agent-cpp-tui3-multiline-final/tests/agent_tests
```

Résultats après correction :

- Build : réussi.
- TUI-3 ciblé : **13 cas, 50 assertions, zéro échec**.
- CTest : **11/11 groupes réussis**.
- Harnais : **199 cas, 1 246 assertions, zéro échec**.
- Scénarios loopback/PTy exécutés avec l’accès loopback autorisé.

Pendant la mise au point, un test a d’abord échoué car son attendu omettait un espace conservé avant le wrapping; l’attendu a été ajusté pour vérifier la conservation effective de cet espace. Aucun texte applicatif n’a été supprimé. Un build initial a également détecté une référence de fonction renommée oubliée et l’usage de `CHECK_EQ` avec un `vector<string>` non imprimable par le macro; ces erreurs de compilation/test ont été corrigées avant les validations finales.

Référence antérieure : 193 cas, 1 228 assertions et 11 groupes CTest. Le différentiel du harnais est de **6 cas nets et 18 assertions**.

## Limites et verdict

Les résultats confirment le formatage des lignes, le texte stocké inchangé, le wrapping, le remplacement après perte d’événements et la conservation du test de scroll. Ils ne remplacent pas un nouvel essai ergonomique Ghostty après correction.

**VALIDÉ pour le correctif de présentation et les tests automatisés exécutés.** Aucun commit, push ou merge n’a été effectué; aucun travail sur les six vues n’a commencé.
