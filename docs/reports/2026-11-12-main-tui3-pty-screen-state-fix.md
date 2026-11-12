# LOFITERM — diagnostic de l'intermittence PTY TUI-3

**Date :** 2026-11-12

**Branche :** `main`

**SHA de base et du code applicatif :** `9a8857be962911f5b8484d87cdd3eb60b661db51`

**Build :** `/home/oper/projects/builds/lofiterm-dev-ta1wlsRh`

## Cause démontrée

Le test `tui3.typed_no_is_retained_until_enter_without_leaking_to_composer`
cessait d'attendre dès que la séquence d'octets contenant `WRITE APPROVAL`
était reçue. Sa condition ne vérifiait pas que le champ de saisie avait été
émis avant d'injecter `n` et `o`. La trace instrumentée a relevé :

| Événement | Temps depuis le début |
| --- | ---: |
| En-tête d'approbation capturé | 335 ms |
| Injection de `n` | 335 ms |
| Injection de `o` | 376 ms |
| `Input: no` visible dans la projection PTY | 436 ms |

Au moment de l'injection, le modèle d'écran ne montrait pas le champ; il a
ensuite projeté `Input: no`. Avant Entrée, une seule requête HTTP avait été
reçue et la cible était absente. Cela démontre que la synchronisation sur le
seul titre ne prouvait pas que la ligne de saisie était prête et laissait une
course possible entre le driver et le rendu. La trace détaillée des anciens
échecs n'étant pas conservée, elle ne prouve pas rétrospectivement leurs
octets exacts. Aucun défaut de production ni effet d'écriture n'a été
reproduit.

`visible_pty_text()` supprimait les commandes CSI/OSC sans en appliquer les
effets sur le curseur et les cellules. Sa projection ne pouvait donc pas
représenter fidèlement les redraws ncurses. Elle a été remplacée pour ce test
par un petit modèle d'écran PTY qui applique les positions du curseur, les
effacements et les caractères. Les séquences CSI/OSC incomplètes à la fin
d'une lecture sont conservées pour le fragment suivant. Le test unitaire
vérifie notamment un CSI réparti entre trois lectures.

## Correction

Le driver attend maintenant, sous la même échéance bornée déjà utilisée pour
la détection du dialogue, l'émission de `Input: ` par `TuiView::draw_approval`
avant d'injecter les frappes. Il n'ajoute aucun délai fixe. Après `n` puis `o`,
le modèle d'écran vérifie que les cellules affichées contiennent
`Input: no` avant Entrée.

Les assertions de sécurité restent actives : aucune seconde requête outil
avant Entrée, aucune création avant décision, aucun répertoire parent après
refus, et aucune troisième requête après le F9 de contrôle de fuite vers le
composeur. L'émission du défi exact, le traitement de l'approbation et la
logique applicative ne changent pas.

La trace de défaillance est écrite uniquement si ce scénario échoue. Elle ne
contient ni sortie PTY brute, ni chemin cible, ni défi, ni contenu proposé.
Elle se limite aux horodatages d'étapes fixes, aux indicateurs booléens, à la
longueur du champ attendu et aux paramètres numériques des CSI récents.

## Fichiers modifiés

- `tests/tui_test.cpp` : modèle d'écran PTY, synchronisation sur l'émission du
  champ et oracle de cellules après saisie.
- `docs/README.md` : référencement de ce rapport.
- `docs/reports/2026-11-12-main-tui3-pty-screen-state-fix.md` : présent rapport.

Les trois rapports non suivis déjà présents avant cette intervention sont
conservés sans modification.

## Vérifications réellement exécutées

Build incrémental du build fourni (pas une nouvelle configuration propre) :

```text
cmake --build /home/oper/projects/builds/lofiterm-dev-ta1wlsRh --parallel 2
PASS — agent_core, agent_tui, agent, tui_pty_driver et agent_tests construits.
```

Résultats sur le code final :

| Commande | Résultat |
| --- | --- |
| `agent_tests --filter=tui3` | PASS — 30 cas, 491 assertions, zéro échec |
| `agent_tests` | PASS — 295 cas, 8 604 assertions, zéro échec |
| `ctest --test-dir /home/oper/projects/builds/lofiterm-dev-ta1wlsRh --output-on-failure` | PASS — 14/14 groupes, zéro échec, 35,71 s |
| `git diff --check` | PASS |

Le harnais et CTest ont été exécutés avec l'accès loopback nécessaire aux
serveurs HTTP mock. Le harnais complet et CTest incluent chacun le scénario
`typed_no` après correction. La validation est PTY automatisée, pas un nouvel
essai physique Ghostty.

## Limites

- Le modèle d'écran est un interpréteur minimal des contrôles émis dans ce
  scénario PTY, pas une implémentation complète de xterm ni une preuve au
  niveau des pixels.
- La synchronisation initiale s'appuie sur le libellé `Input: ` émis par le
  rendu du dialogue; l'oracle de cellules vérifie ensuite le texte édité
  `Input: no` avant Entrée.
- Les anciennes sorties détaillées des échecs signalés antérieurement ne sont
  pas disponibles. La cause démontrée ici provient de la reproduction
  instrumentée actuelle, pas d'une reconstitution des anciens octets.
- Aucun code de production, politique d'approbation, Landlock, SSE ou moteur
  n'a été modifié.

## État Git et verdict

HEAD reste `9a8857be962911f5b8484d87cdd3eb60b661db51` sur `main`. Aucun commit,
push, merge, nettoyage ou changement fonctionnel n'a été effectué. Les
changements suivis se limitent au test PTY et à son index/rapport; les trois
fichiers non suivis préexistants sont préservés.

**VERDICT : PRÊT POUR REVUE.** La cause reproductible était une synchronisation
insuffisante du driver PTY; la correction attend l'émission du champ puis
vérifie son rendu édité. Les assertions d'absence d'effet d'écriture restent
actives et les suites demandées passent sur le build indiqué.

## Revue complémentaire des prédicats PTY

- Date : 2026-11-12T16:33:27+01:00
- SHA de base : 9a8857be962911f5b8484d87cdd3eb60b661db51
- Build : /home/oper/projects/builds/lofiterm-dev-ta1wlsRh
- Verdict : ECHEC

### Ajustements

- Champ Input détecté sur les cellules réelles, sans suppression des espaces de fin.
- Valeur no comparée par égalité exacte.
- Synchronisation des frappes sur le champ visible dans le modèle PTY.
- Test négatif contre Input: not.

### Résultats observés

```text

================ RESUME ================
cas executes        : 30 / 295
assertions          : 494
assertions en echec : 1
RESULTAT: ECHECS

================ RESUME ================
cas executes        : 295 / 295
assertions          : 8607
assertions en echec : 1
RESULTAT: ECHECS

Total Test time (real) =  38.18 sec

The following tests FAILED:
	  9 - tui3 (Failed)
Errors while running CTest
```

### Journaux

- /tmp/lofiterm-pty-review-A1j8FRlQ/build.log
- /tmp/lofiterm-pty-review-A1j8FRlQ/tui3.log
- /tmp/lofiterm-pty-review-A1j8FRlQ/harness.log
- /tmp/lofiterm-pty-review-A1j8FRlQ/ctest.log

Aucun changement de production, commit, push ou merge.

## Diagnostic complémentaire — champ PTY

- Date : 2026-11-12T16:43:10+01:00
- Commande : agent_tests --filter=tui3
- Code retour : 1
- Journal : /tmp/lofiterm-tui3-mask-j2lkHY.log
- Modification : masque des caractères du champ, diagnostic uniquement

```text
[PTY typed-no failure trace] screen_rows=24 cols=80 approval_header_visible=no approval_prompt_ready=yes Input_emitted_before_n=yes Input_emitted_before_o=yes model_input_visible_before_n=no input_screen="Input: <5 chars; mask=?n?o?>" raw_input_marker=yes printable_bytes=1224 screen_summary=" cursor=23,0" requests_before_enter=1 target_absent_before_enter=yes
    FAIL /home/oper/projects/local-agent-cpp/tests/tui_test.cpp:628  run.dialog_kept_typed_no
assertions          : 494
assertions en echec : 1
RESULTAT: ECHECS
```

Verdict : diagnostic; correction fonctionnelle non encore validée.

## Correction complémentaire — séquences ESC de charset

- Date : 2026-11-12T16:54:35+01:00
- SHA de base : 9a8857be962911f5b8484d87cdd3eb60b661db51
- Étape finale : Terminé
- Verdict : PASS
- Journaux : /tmp/lofiterm-pty-charset-saLplobj

Correction : prise en charge des désignations
de jeu de caractères ESC ( B et ESC ) 0,
y compris lorsqu'elles sont fragmentées.

### Résultats
```text
--- tui3.log ---
cas executes        : 31 / 296
assertions          : 498
assertions en echec : 0
RESULTAT: OK
--- harness.log ---
cas executes        : 296 / 296
assertions          : 8611
assertions en echec : 0
RESULTAT: OK
--- ctest.log ---
100% tests passed, 0 tests failed out of 14
```

Aucune modification du code de production.
Aucun commit, push ou merge.

## Correction complémentaire — séquences ESC de charset

- Date : 2026-11-12T18:01:38+01:00
- SHA de base : 9a8857be962911f5b8484d87cdd3eb60b661db51
- Étape finale : Correction source
- Verdict : ECHEC
- Journaux : /tmp/lofiterm-pty-charset-FGVOCbAK

Correction : prise en charge des désignations
de jeu de caractères ESC ( B et ESC ) 0,
y compris lorsqu'elles sont fragmentées.

### Résultats
```text
```

Aucune modification du code de production.
Aucun commit, push ou merge.

## Synthèse finale de qualification

La correction des séquences de désignation de jeu de caractères
ISO-2022 dans le modèle PTY a été appliquée et qualifiée.

- TUI-3 : PASS — 31 cas, 498 assertions, zéro échec.
- Harnais complet : PASS — 296 cas, 8 611 assertions, zéro échec.
- CTest : PASS — 14/14 groupes.
- Code de production : inchangé.
- Rapport de qualification : conservé.

La réexécution ultérieure du script s'est arrêtée avant compilation
et tests, car le test de régression existait déjà. Son verdict
ECHEC concerne uniquement cette réexécution non idempotente.
Il ne remet pas en cause les résultats PASS précédemment obtenus.

VERDICT FINAL : QUALIFICATION AUTOMATISÉE VALIDÉE.
