# UI-0C — Ratification documentaire et validation de référence

- **Date :** 2026-11-05 (Europe/Paris)
- **Dépôt :** `mohamedbouhlel/local-agent-cpp`
- **Branche documentaire :** `docs/ui-0c-specification-ratification`
- **HEAD de référence :** `23b04f582711905c96ff6c5122046145d68e237a`
- **Périmètre :** documentation et validation existante uniquement ; aucun code, CMake ou test modifié.
- **PR #15 :** fermée et fusionnée ; merge commit `23b04f582711905c96ff6c5122046145d68e237a`.

## 1. Objectif

Consolider SPEC-UI-001 et les contrats d'architecture nécessaires à l'examen de UI-1, établir les résultats de référence au HEAD de la PR #15, et laisser les points encore non arbitrés explicitement ouverts. UI-1 n'est pas commencée.

## 2. Contrôle Git et base

À l'inspection initiale, le worktree était propre sur `fix/disable-workspace-overwrite`, HEAD `b34bb99` (branche de correction sandbox), et la référence locale `origin/main` était en retard. La PR #15 a été vérifiée comme fusionnée ; sa référence distante `main` et son merge commit sont `23b04f58`. Après actualisation explicite de la référence distante, une branche documentaire a été créée depuis ce merge commit. La branche locale `main` reste à `f4056c6d03ea54c2bce2f97606bb80f380e34dab`, donc elle n'a pas été utilisée comme base ni modifiée.

Au début de l'édition documentaire, `git status --short --branch` indiquait `docs/ui-0c-specification-ratification...origin/main` sans changements. Les modifications finales sont limitées aux trois chemins indiqués en §4. Aucun commit, push, merge ou mise à jour de branche n'a été fait après la création de la branche.

État actuel des références après rafraîchissement :

- `HEAD` et `origin/main` : `23b04f582711905c96ff6c5122046145d68e237a`.
- `main` local : `f4056c6d03ea54c2bce2f97606bb80f380e34dab` (ancien, non modifié).

## 3. Décisions et faits retenus

La spécification distingue les orientations ratifiées des détails qui restent propositions. Elle consigne notamment la conservation des trois callbacks historiques et l'ajout d'une voie d'observation distincte, synchrone en première version, avec isolation des erreurs UI. Elle fixe également l'absence de TUI, de thread spinner et d'annulation fictive, le respect exact du REPL, les contraintes terminal, les durées monotones, JSONL désactivé par défaut avec allowlist et la conservation de Landlock.

La décision mainteneur Q20 est en vigueur sur le HEAD fusionné : `write_file` refuse `overwrite:true` en attendant une identité système isolée. Le schéma n'expose plus ce champ. Aucun changement à cette règle ni à Landlock n'est proposé.

Les noms/signatures de l'API observer, les identifiants et leur génération, les transitions HTTP exactes, les modes/options CLI, l'échappement précis des contrôles, le chemin/permissions/rotation des logs et la politique de panne restent ouverts ou marqués proposition. La ligne de commande `//` est une proposition et n'est pas présentée comme décision ratifiée.

## 4. Documents modifiés

- `docs/specifications/console-ui-architecture-requirements.md` — nouvelle SPEC-UI-001 v1.1 : architecture, contrat événementiel conceptuel, état, compatibilité, exceptions, terminal, métriques, JSONL, Landlock, commandes, REPL, matrice de traçabilité, phases UI-1 à UI-8, risques et registre d'arbitrages.
- `docs/reports/2026-11-05-ui-0c-specification-ratification.md` — présent rapport de phase.
- `docs/README.md` — index de la spécification et du rapport ; statut de la section adapté pour inclure les documents en cours de ratification.

Aucun document historique n'a été réécrit. `AGENTS.md`, `USAGE.md`, la roadmap et la spécification REPL ont été consultés à titre de cohérence mais ne sont pas modifiés.

## 5. Validation exécutée

Commandes demandées, depuis la racine du dépôt :

```text
cmake -S . -B build-tests -DAGENT_BUILD_TESTS=ON
  exit 0 — configuration terminée. Avertissement CMake dev : CMP0135 / DOWNLOAD_EXTRACT_TIMESTAMP non défini.

cmake --build build-tests
  exit 0 — agent_core, agent et agent_tests construits.

ctest --test-dir build-tests --output-on-failure
  exit 0 — 7/7 groupes passés (sse, workspace, http, agent, repl, mock, sandbox), 0 échec ; durée rapportée 2.57 s.

cd build-tests/tests && ./agent_tests
  exit 0 — cas executes : 118 / 118 ; assertions : 407 ; assertions en echec : 0 ; RESULTAT: OK.
```

Un premier lancement non escaladé de `./agent_tests` a échoué parce que l'environnement refusait `socket()` aux tests loopback : 118 cas parcourus, 218 assertions alors exécutées, 31 échecs. Ce résultat n'est pas présenté comme un échec fonctionnel du code. Le même binaire a ensuite été exécuté avec l'autorisation permettant les sockets loopback et a donné le résultat complet ci-dessus. Le `ctest` non escaladé a, lui, passé les sept groupes.

Le build est une compilation, pas une validation d'interface. Aucun test UI, terminal réel, serveur LLM local ou mesure de performance n'a été exécuté ; ces composants n'existent pas encore.

## 6. Écart documentaire 403 / 407

`403` est un compteur historique rapporté dans d'anciens documents au HEAD antérieur (notamment PR #5 / M3). Le diff de la PR #15 ajoute quatre assertions au fichier de tests workspace par rapport à son premier parent : une vérification du schéma ne contenant plus `overwrite` (+1), deux contrôles dans le cas d'écriture atomique (refus de l'appel et contenu inchangé ; +2), puis le refus dans le cas de permissions (+1). Le test de garde `overwrite` remplace une assertion de succès par une assertion d'exception, sans modifier son nombre. Cela explique l'écart 403→407 entre ces références, sans transformer le relevé historique en résultat actuel. Cette mission a rejoué le harnais au merge de PR #15 et a observé `118` cas, `407` assertions et `0` échec. Les nombres 403 et 407 appartiennent à des révisions différentes.

## 7. Cohérence et vérifications documentaires

- Cohérence `AGENTS.md` / `USAGE.md` : callbacks, SSE, résultats d'outils, Landlock, limitations et refus de remplacement vérifiés au HEAD.
- Cohérence roadmap / Q20 : `overwrite:true` reste refusé ; aucun élargissement de permissions proposé.
- Cohérence REPL : renvoi vers le document approuvé et ses arbitrages A-1 à A-9 ; CRLF est normalisé uniquement en mode bloc.
- Les liens Markdown internes du nouveau document et les entrées d'index sont contrôlés après rédaction ; `git diff --check` est requis avant verdict.
- Les 403 historiques sont conservés comme faits historiques et ne sont pas réécrits.

## 8. Risques et réserves

- L'API exacte d'événement et les contrats de certaines transitions ne sont pas ratifiés au niveau signature ; ne pas implémenter ces détails par supposition.
- Les logs sous `workspace/` seraient accessibles aux outils si leur chemin est connu ; le caractère caché ne constitue pas une confidentialité.
- La politique actuelle des callbacks historiques est différente de la politique nouvellement proposée pour les observers UI. Leur séparation et leurs tests sont nécessaires.
- Le build émet l'avertissement CMake CMP0135 existant ; il n'a pas été corrigé, car CMake est hors périmètre.
- La première tentative complète du harnais a buté sur le sandbox ; le résultat final a été obtenu avec autorisation pour ouvrir les sockets loopback.

## 9. Base UI recommandée

Base recommandée : `23b04f582711905c96ff6c5122046145d68e237a`, merge de la PR #15 sur `main`, parce qu'elle inclut le refus `overwrite:true` demandé par Q20 et est la référence distante vérifiée. Continuer depuis la branche documentaire ne vaut pas autorisation de UI-1. Avant cette phase, vérifier à nouveau le HEAD, le worktree et l'état des branches/PR ; aucune mise à jour ou fusion silencieuse.

## 10. Verdict

**UI-0C : VALIDÉ pour la préparation documentaire et la référence de tests.** Build réussi, CTest 7/7, harnais 118/118 cas, 407 assertions, zéro échec au SHA indiqué. Ce verdict ne valide aucune fonctionnalité UI qui n'existe pas.

**UI-1 : NON AUTORISÉE / en attente d'une autorisation distincte.** Avant lancement, le mainteneur doit ratifier les éléments ouverts de l'API événementielle et la politique de callback observer défaillant, puis autoriser explicitement UI-1. Aucun commit ou push n'a été effectué.
