# LOFITERM v1.0 — corrections finales et clôture

## Référence et périmètre

- Base étudiée : `17f6fa1d784fe9fcc4d03b30044ddf62356001e6` — fusion de la PR #45.
- `origin/main` observé au même SHA au début de l'intervention.
- Branche isolée : `fix/lofiterm-v1-final-corrections`.
- SHA final du code testé : identique à la base; les modifications de cette intervention ne sont pas commitées.
- Tag `v1.0.0-technical` inchangé, toujours pointé sur
  `b1d194288b02b5df094953affd763776991d3e0a`.
- Aucun code de production n'a été modifié. Les changements sont limités aux
  tests et au présent rapport/index.

## Anomalie A — observation du `no` dans le dialogue TUI

### Constat et cause

Le test `tui3.typed_no_is_retained_until_enter_without_leaking_to_composer`
vérifiait immédiatement la sortie PTY après un `read_for(100 ms)`. Cette pause
ne synchronisait pas le test avec le prochain rendu ncurses. L'assertion
`dialog_kept_typed_no` observait donc une échéance temporelle, pas l'arrivée de
l'état rendu `Input: no`.

L'exécution initiale sur la base a passé; l'échec intermittent rapporté par le
mainteneur n'a pas été reproduit dans cette campagne. La lecture de
`TuiEventLoop::handle_approval_key()` confirme que les caractères sont ajoutés
au champ d'approbation et que la décision n'est résolue que par Entrée, une
annulation explicite ou expiration. Aucun comportement de production ne ferme
le dialogue sur la première touche ordinaire.

### Correction de test

Dans `tests/tui_test.cpp`, la pause fixe a été remplacée par une attente bornée
qui lit la sortie PTY jusqu'à l'observation de `Input: no` (polls de 20 ms,
limite de sécurité de 2 s). Le test vérifie désormais aussi, avant Entrée,
qu'une seule requête HTTP a été reçue et que le fichier cible n'existe pas.
Après Entrée, il poursuit le scénario de refus, reçoit la réponse de suivi et
sonde le composeur afin de détecter une fuite de saisie.

La correction synchronise l'observation sur l'état attendu; elle ne change pas
le dialogue, le défi, les règles d'approbation ni les délais de production.
Le test lit le flux d'octets PTY et ne constitue pas un émulateur complet de
cellules terminal. Les tests d'approbation, d'expiration, de F10 et de
confirmation explicite restent également exécutés.

## Anomalie B — répétitions d'appels d'outils

### Chemin vérifié

Dans `src/agent.cpp`, chaque réponse assistant contenant des tool calls est
ajoutée à l'historique avant exécution. Pour chaque outil exécuté, le résultat
réel est ensuite ajouté comme message `tool` lié au même `tool_call_id` avant
la requête suivante. Les erreurs standard deviennent un résultat explicite
`tool error: ...`, sont signalées par `ToolFailed` et sont aussi ajoutées à
l'historique. Un refus d'écriture publie `ToolAuthorizationResolved`, ne lance
pas le writer et ajoute au transcript le résultat explicite
`write authorization denied; no file was created` (ou l'équivalent
cancelled/expired/unavailable). Il n'est pas compté comme une exécution
`ToolStarted`.

La boucle conserve son plafond existant de cinq réponses du modèle demandant
des outils. Si le modèle continue à en demander, l'agent termine le tour en
échec avec `tool call loop exceeded maximum iterations`; le plafond n'a pas
été augmenté.

### Preuve et interprétation

Le test `agent.boucle_outils_limitee_a_cinq_iterations` a été renforcé avec un
serveur mock qui répète volontairement `list_files`. Il vérifie que chaque
requête suivante contient les paires assistant/tool précédentes, leurs IDs
corrélés et le résultat exact `a.txt\n`, puis que le plafond de cinq demeure
effectif. Les tests bout en bout existants vérifient également le résultat
réussi de `read_file` et le refus de `write_file`; le test
`agent.failed_tool_is_reported_and_turn_can_complete` vérifie maintenant que
`tool error: handler failed` est renvoyé avec l'ID de l'appel.

Le scénario physique de répétition n'a pas été reproduit avec le serveur LLM
réel pendant cette intervention. Les preuves démontrent que le programme
reçoit, conserve et renvoie les résultats; le mock démontre aussi qu'un
modèle qui répète volontairement les appels atteint le plafond prévu. Elles
ne permettent pas d'attribuer exclusivement au modèle local les répétitions
rapportées sur une session réelle, faute de trace correspondante du modèle.
Pour cette configuration, utiliser un modèle exposé par LiteLLM dont la prise
en charge des tool calls est explicitement documentée et vérifier son
identifiant/configuration de routage côté serveur. Le payload et la
configuration actuels n'exposent pas de paramètre `temperature`; aucun réglage
de ce type n'est proposé ici.

## Validation exécutée

Environnement : Debian GNU/Linux 13.7 (trixie), noyau Linux
`6.12.115+deb13.1-amd64`, GCC/G++ 14.2.0, CMake 3.31.6, libcurl 8.14.1,
ncursesw 6.5. nlohmann/json 3.11.3 a été fourni au build par le cache
FetchContent local; aucune dépendance produit n'a été ajoutée.

Configuration et build propres, puis rebuild des fichiers de test modifiés :

```sh
cmake -S . -B /tmp/local-agent-cpp-lofiterm-v1-final \
  -DAGENT_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-lofiterm-build/_deps/json-src
cmake --build /tmp/local-agent-cpp-lofiterm-v1-final -j2
cmake --build /tmp/local-agent-cpp-lofiterm-v1-final -j2
```

Résultats observés :

| Vérification | Résultat |
| --- | --- |
| Build propre C++20 | PASS — exécutable et cibles de test compilés |
| `./tests/agent_tests --filter=tui3` | PASS — 29 cas, 406 assertions, 0 échec |
| CTest TUI-3 répété (`--repeat until-fail:5`) | PASS — 5/5 groupes, 0 échec, 117,94 s |
| Suite complète `./tests/agent_tests` | PASS — 280/280 cas, 8 346 assertions, 0 échec |
| `ctest --output-on-failure` | PASS — 14/14 groupes, 0 échec, 35,99 s |
| `git diff --check` | PASS |

La répétition TUI-3 couvre le groupe complet de 29 tests à chaque passage; elle
ne constitue pas une preuve générale d'absence de course. La suite automatise
les échanges via serveur HTTP mock et PTY; aucun serveur LiteLLM ni terminal
Ghostty réel n'a été utilisé ici.

## Sécurité, fichiers et état final

Aucune logique d'approbation, `PreparedWrite`, politique de non-écrasement,
Landlock, outil workspace, code SSE, moteur, TUI de production ou limite
d'itérations n'a été modifié. Le test confirme le refus avant Entrée sans
requête de suivi ni création; les scénarios existants de confirmation exacte,
expiration et fermeture restent passants.

Fichiers modifiés dans le worktree :

- `tests/tui_test.cpp`
- `tests/agent_streaming_test.cpp`
- `tests/agent_events_test.cpp`
- `docs/README.md`
- `docs/reports/2026-11-10-lofiterm-v1-final-corrections.md`

Le checkout principal est resté propre au SHA de référence. Le stash UI-4, les
anciens worktrees et le tag technique n'ont pas été modifiés. Aucun commit,
push, PR ou merge n'a été créé.

## Verdict

**GO pour revue finale.** Le défaut intermittent rapporté n'a pas été
reproduit; sa cause vérifiable est une synchronisation temporelle fragile dans
l'assertion PTY, remplacée par une attente conditionnelle bornée. La protection
d'approbation est conservée. Les résultats d'outils sont présents dans les
requêtes suivantes et la répétition est bornée sans changement du plafond.
La cause précise des répétitions observées avec le LLM réel reste indéterminée
tant qu'une trace de cette session n'est pas disponible; aucun défaut logiciel
reproductible ne subsiste dans le périmètre vérifié.
