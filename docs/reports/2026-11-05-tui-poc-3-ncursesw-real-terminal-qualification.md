# TUI-POC-3 — Qualification Ghostty et analyse du défilement

**Date :** 2026-11-05
**Branche :** `feat/tui-poc-3-ncursesw-qualification`
**Base :** `73e736bd0c04f88ecf87dadbd20f2d0f6c0c6a50`
**Statut :** prototype ncursesw qualifié fonctionnellement par observation utilisateur dans Ghostty; ergonomie du scroll corrigée et testée sous PTY; SSH et restauration exacte Ghostty non vérifiés. Aucune intégration produit.

## OBJECTIF

Enregistrer l'essai interactif Ghostty communiqué par l'utilisateur, analyser
le défilement initial du prototype, corriger le maintien du viewport lorsqu'on
consulte l'historique pendant le flux et revalider les invariants du POC.

## OBSERVATIONS MANUELLES UTILISATEUR — GHOSTTY

Les informations suivantes sont les observations fournies par l'utilisateur;
elles n'ont pas été observées directement par l'agent dans une session
interactive :

- Ghostty, `TERM=xterm-ghostty`, UTF-8, terminal interactif `/dev/pts/0`.
- ncursesw 6.5; compilation CMake réussie.
- L'utilisateur confirme l'affichage plein écran, la navigation des six vues,
  le focus, le scroll de conversation, la saisie/correction multiligne, F9,
  le redimensionnement, F10 et le retour à un shell utilisable.
- Restauration exacte des attributs `termios` avant/après dans Ghostty : non
  vérifiée. Session SSH : non vérifiée.

Ces observations valident le parcours manuel de ce prototype dans cet
environnement rapporté. Elles ne sont pas des tests automatisés et ne
qualifient ni l'ergonomie complète ni la TUI de production.

## ANALYSE DU DÉFILEMENT

### Cause établie dans le code

Le producteur synthétique est lancé au démarrage dans
`poc/tui-poc-1/poc-ncursesw/main.cpp`. Il produit les séquences `0..999` à
raison d'un delta toutes les 10 ms, soit 100 deltas/s pendant environ 10 s.
Ce flux est une animation de démonstration propre au POC; il ne vient pas d'un
LLM. Cela explique les fragments `delta-xxx` et les nombreux changements
visuels constatés au démarrage. Le rendu final affichant la queue du flux
(notamment `delta-969` à `delta-999`, selon l'observation utilisateur) est
cohérent avec cette production continue. L'utilisateur estime avoir vu au
moins une cinquantaine de mouvements avant stabilisation; ce compte est une
observation visuelle, pas un compteur instrumenté.

Le viewport utilise `visible_lines(transcript, scroll, body_height)` dans
`common/ui_helpers.hpp`. Avant le correctif, `scroll` démarrait à zéro et
restait à zéro pendant le flux : zéro désigne la fin du transcript; chaque
nouveau delta déplaçait donc la plage visible vers les lignes ajoutées. Après
`PageUp`, l'ancien code ajoutait 8 à `scroll`, mais ne l'augmentait pas avec
les nouvelles lignes. La plage calculée continuait alors d'avancer pendant
que l'utilisateur consultait l'historique. Ce défaut de maintien du viewport
est démontré par les formules du code; aucune attribution à ncursesw n'est
justifiée.

Le rendu est conditionné par `dirty`. `refresh()` est appelé uniquement dans
ce bloc. Le polling clavier (`timeout(20)`) se réveille périodiquement, mais
ne rend pas à lui seul l'écran. Un contenu filtré non vide, une touche ou un
resize marque l'écran comme sale; les événements de texte actuellement
disponibles sont consommés en lot et ne provoquent qu'un rendu à la prochaine
itération. Le POC précédent a rapporté 579 rafraîchissements pour le scénario
normal; le rejeu UI-POC-3 ci-dessous en a compté 605 pendant un scénario qui
comprenait aussi 240 touches, resize et scroll. Ces totaux ne mesurent pas une
cadence isolée du flux. L'essai idle ajouté rend une seule fois au démarrage
et zéro fois pendant l'attente sans événement.

### Correction POC et comportement démontré

Le prototype maintient désormais `follow_bottom` et un offset du bas :

- au démarrage, il suit la fin;
- `PageUp` désactive le suivi;
- chaque nouvelle ligne reçue augmente l'offset d'autant, gardant fixe la
  plage historique affichée;
- `PageDown` rapproche le viewport de la fin; quand l'offset atteint zéro,
  le suivi du bas est réactivé;
- les événements de texte restent concaténés au transcript dans l'ordre;
  leur fréquence d'arrivée ne force pas un rafraîchissement par delta.

Le harness expose des plages de viewport pour le test. Il vérifie qu'au moins
deux rendus durant le flux gardent exactement les mêmes bornes après le scroll
manuel, puis que les rendus après retour à zéro restent ancrés à la fin.
L'essai `--no-stream` de 350 ms vérifie `FRAMES 1` : un seul rendu initial et
aucun redraw à vide.

## CONTRAT UX PROPOSÉ POUR LA TUI PRODUIT

À ratifier avant l'implémentation :

1. Aucun flux synthétique ou contenu animé au démarrage.
2. Aucun défilement en l'absence d'un nouvel événement visible ou d'une
   action utilisateur.
3. Suivre la fin seulement si l'utilisateur la consultait déjà.
4. Suspendre le suivi après un scroll vers l'historique; préserver sa plage
   lors des nouveaux deltas.
5. Réactiver le suivi lorsque l'utilisateur revient explicitement en bas
   (ou selon un geste de navigation équivalent qui sera spécifié).
6. Garder l'ordre et le contenu des deltas dans l'état conversationnel,
   indépendamment du regroupement des redraws.
7. Cadencer le rendu pour conserver la réactivité clavier et éviter un redraw
   par delta; le critère précis reste à mesurer avec le worker/UI définitifs.
8. Ne jamais déplacer le viewport lors de la lecture d'un ancien message.

Le POC démontre seulement le mécanisme local de viewport. La file événementielle
expérimentale est bornée et peut signaler des pertes sous surcharge. La
conservation de tous les deltas reçus dans l'état final doit donc être conçue
avec une politique de mémoire/overflow cohérente; ce POC ne résout pas ce
contrat de production.

## MODIFICATIONS

- `poc/tui-poc-1/poc-ncursesw/main.cpp` : suivi du bas, offset préservant la
  position historique, mesure des bornes de viewport pour le harness,
  injection sans flux et sortie idle de test.
- `poc/tui-poc-1/harness/test_pocs.py` : scénario PTY de scroll pendant flux,
  vérification du maintien des bornes, du retour au suivi du bas et de
  l'absence de redraw idle.
- `docs/reports/2026-11-05-tui-poc-3-ncursesw-real-terminal-qualification.md` :
  remplacement du rapport bloqué initial par les observations Ghostty,
  l'analyse source, les résultats rejoués et les limites.
- `docs/README.md` : index du rapport.

Aucun composant du produit n'a été modifié : Agent, HTTP/SSE, REPL,
PresentationState, MetricCollector, renderers, CMake principal, Landlock,
workspace tools et politique `overwrite:true` restent hors du diff.

## TESTS ET MESURES REJOUÉS

Build neuf depuis ce worktree :

```sh
cmake -S poc/tui-poc-1/poc-ncursesw -B /tmp/tui-poc3-ncurses-clean
cmake --build /tmp/tui-poc3-ncurses-clean -j2
/tmp/tui-poc3-ncurses-clean/sanitizer-tests
python3 poc/tui-poc-1/harness/test_pocs.py \
  /tmp/tui-poc-ftxui-clean/poc-ftxui \
  /tmp/tui-poc3-ncurses-clean/poc-ncursesw \
  /tmp/tui-poc-ftxui-clean/sanitizer-tests \
  /tmp/tui-poc3-ncurses-clean/sanitizer-tests
```

Le premier essai de configuration avait échoué avant compilation parce que le
cache `/tmp/tui-poc-ncurses-clean` référençait l'autre worktree. Un nouveau
répertoire propre POC-3 a été créé; configuration et build ont alors réussi.
Le sanitizer a affiché `sanitizer corpus passed`. Le harness complet s'est
terminé sans assertion en échec.

Mesures du scénario normal lors du dernier rejeu du harness :

| Mesure | FTXUI (non modifié; comparaison) | ncursesw |
|---|---:|---:|
| Deltas ACK / ordre | 1000 / ordonnés | 1000 / ordonnés |
| Touches traitées | 240 | 240 |
| P95 handler clavier | 132,18 ms | 0,60 ms |
| Débit ACK | 103,3/s | 98,2/s |
| P95 intervalle ACK | 17,00 ms | 23,92 ms |
| Frames/refresh comptés | 578 | 605 |
| Changements de taille observés | 17 | 17 |
| RSS maximum échantillonnée | 4724 KiB | 4876 KiB |
| CPU échantillonné (% d'un cœur) | 19,4 % | 10,3 % |
| Octets PTY capturés | 1 279 045 | 37 156 |
| Écran alternatif entrée/sortie | oui/oui | oui/oui |
| `termios` PTY avant/après identique | oui | oui |

Autres résultats ncursesw : exception contrôlée code 2, 19 ACK, restauration
PTY oui; surcharge 129 ACK (128 détails + fin), 70 rejets signalés et
`TURN_FINISHED 1000`; fermeture active code 0 et restauration PTY oui;
`TERM=dumb` refusé sans séquence écran; exécution sans TTY refusée sans ANSI
sur stdout; scénario idle `FRAMES 1`, code 0 et restauration PTY oui. Le
scénario normal a inclus resize répété jusqu'à 10×4, saisie multiligne, six
vues et sanitizer, avec toutes les assertions du harness satisfaites.

Le P95 est calculé entre écriture d'une touche au master PTY et record ACK du
handler; il ne mesure pas le moment où les pixels sont visibles. RSS et CPU
sont échantillonnés via `/proc`; la mesure CPU a une granularité limitée. Le
volume PTY et les compteurs de frames sont spécifiques à la maquette et à
l'émulateur PTY. Ces résultats ne sont pas des mesures Ghostty.

## LIMITES ET RISQUES

- Les observations manuelles Ghostty ci-dessus sont fournies par l'utilisateur
  et non reproduites par l'agent.
- La sélection/copie de texte, le comportement hostile détaillé et l'ergonomie
  des six vues ne sont pas confirmés séparément par l'observation fournie.
- Restauration exacte `termios` sous Ghostty et SSH restent non vérifiés.
- Aucun test SSH, aucune mesure utilisateur de latence, aucun test de longues
  sessions répétées en Ghostty n'a été exécuté par l'agent.
- Aucun CPU, RSS, volume terminal ou latence Ghostty n'a été mesuré.
- Les tests PTY automatisés ne remplacent pas les observations manuelles.
- Le démarrage du POC lance toujours son producteur synthétique par défaut;
  cette animation sert au test. Elle doit disparaître de la TUI produit.
- La correction préserve des lignes logiques; elle ne constitue pas une
  validation du scroll avec wrapping complexe, redimensionnements pendant une
  consultation historique ou un transcript de taille illimitée.
- La file de test peut perdre des détails sous surcharge; conservation sans
  perte avec mémoire bornée reste un arbitrage d'architecture ouvert.

## VERDICT

**Prototype ncursesw : retenu comme backend candidat pour la prochaine étape
de conception**, conformément à la décision du mainteneur. L'observation
utilisateur confirme un parcours fonctionnel dans Ghostty, et le défaut de
viewport identifié a reçu une correction minimale couverte par le harness.
Le prototype n'est pas une TUI de production : l'ergonomie finale, SSH, la
restauration exacte Ghostty et le contrat de conservation sous surcharge
restent à vérifier. **Aucune intégration produit n'est commencée.**
