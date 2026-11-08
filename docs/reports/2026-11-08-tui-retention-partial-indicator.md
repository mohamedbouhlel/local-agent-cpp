# Correctif de rétention visible après fusion de la PR #32

**Date :** 2026-11-08
**Dépôt :** `mohamedbouhlel/local-agent-cpp`
**Base distante vérifiée :** `0d447185d28e253d9636d79f139983e07b3c00dd`
**Branche :** `fix/tui-retention-partial-indicator`
**Worktree :** `/home/oper/projects/worktrees/local-agent-cpp-tui-retention-fix`

## Cause du correctif manquant

Le commit de merge `0d447185d28e253d9636d79f139983e07b3c00dd` a pour parents `6ff8a0d2ffde48b02986c556de9daec4a760e2ff` et `9256c25525b1835687b127afb0e0bae6c818db71`. Ce second parent est le HEAD commité de la branche de la PR #32 et correspond au HEAD constaté dans le worktree candidat.

Le correctif de scroll et ses tests étaient des modifications **non commitées** dans ce worktree, postérieures au HEAD de la PR. Le merge a donc inclus le code de rétention commité, mais pas les modifications locales. Il ne s’agit pas d’une perte de commit : les changements locaux ont été copiés de façon ciblée dans le présent worktree; l’original a été préservé.

La référence `origin/main` a été vérifiée en direct avec `git ls-remote` et pointe sur le commit de merge ci-dessus.

## Correctif repris

- Calcul de l’intervalle visible des lignes historiques avant insertion et détermination des lignes supprimées par l’éviction FIFO.
- Si une ligne d’historique du viewport est évincée, repositionnement au début du contenu encore retenu et maintien du mode « suivi suspendu ».
- Si les lignes visibles restent retenues, conservation de la logique d’offset actuelle.
- `status_line()` affiche `PARTIAL` avec les compteurs disponibles sur les vues Outils/Diagnostics; cette ligne reste indépendante du scroll. Le préfixe tient dans un terminal de 10 colonnes.
- Tests vérifiant l’identité des lignes visibles conservées, le repositionnement après éviction de la ligne visible, le statut compact et l’absence de ce statut dans les quatre autres vues.

`BoundedHistory`, ses plafonds FIFO et le code déjà fusionné par #32 n’ont pas été réintroduits dans le diff. Aucun changement au moteur, aux permissions, à Landlock, au REPL ou au checkout UI-4.

## Fichiers du diff correctif

- `include/ui/tui_view.hpp`
- `src/ui/tui_view.cpp`
- `tests/bounded_history_test.cpp`
- `docs/README.md`
- `docs/reports/2026-11-08-tui-retention-partial-indicator.md`

## Validation

Build propre dans le répertoire `/tmp/local-agent-cpp-tui-retention-fix-build`, configuré depuis ce worktree. CMake a utilisé la source locale déjà disponible de nlohmann/json; GCC 14.2.0, libcurl et ncursesw ont été détectés. Un avertissement développeur CMake sur CMP0135/timestamps est apparu.

Résultats effectivement obtenus :

```text
agent_tests --filter=tui4b1 : 10 cas, 6 125 assertions, 0 échec
agent_tests --filter=tui4a  : 15 cas, 99 assertions, 0 échec
agent_tests --filter=tui3   : 13 cas, 50 assertions, 0 échec
ctest --test-dir /tmp/local-agent-cpp-tui-retention-fix-build --output-on-failure
                              13/13 groupes, 0 échec
```

Les suites ciblées TUI-3/TUI-4A et CTest comprennent des tests PTY/mock loopback; ils ont été exécutés avec les permissions de sockets nécessaires. Cela ne constitue pas un nouvel essai sur Ghostty ou un terminal physique. `git diff --check` est vérifié après ajout du présent rapport.

## Limites

Le test de largeur 10 vérifie la ligne exacte produite par `status_line()`, fonction employée par `TuiView::draw()` pour écrire la ligne d’état dans ncurses. Le test ne capture pas une session interactive physique. L’indication courte `PARTIAL` est visible; les compteurs détaillés sont tronqués dans les terminaux trop étroits. Les plafonds de `BoundedHistory` bornent les entrées et les octets textuels conservés, pas le RSS global ni les allocations temporaires.

## État de publication

La branche corrective a été créée depuis le main fusionné. Seuls les fichiers listés ci-dessus sont modifiés dans le nouveau worktree. Aucun commit, push, merge ou PR n’a été créé. Le worktree candidat d’origine et le checkout UI-4 sont préservés.

## Verdict

**PR corrective prête pour revue.** La cause de l’absence dans le merge est expliquée par la frontière entre le HEAD commité de la PR et les modifications locales non commitées. Le diff de cette branche contient uniquement le correctif manquant, les tests correspondants et leur documentation; build, tests ciblés et CTest complet réussissent.
