# Clôture TUI-POC-1/2 et qualification ncursesw

**Date :** 2026-11-05
**Branche :** `feat/tui-poc-1-backend-comparison`
**SHA testé :** `0b1cd53b7d466257b3da50bf17b27013d1b0ab93`
**Périmètre :** prototypes autonomes sous `poc/tui-poc-1/`; aucune intégration au produit.

## Résumé

Les deux projets configurés depuis des répertoires de build propres, compilés,
et leurs corpus sanitizer ont réussi. Le harness PTY complet a réussi sur les
deux exécutables pour les scénarios normal, exception contrôlée, surcharge,
fermeture pendant activité, `TERM=dumb` et refus sans TTY. Les résultats
confirment l'ordre des 1000 événements synthétiques, l'édition multiligne,
les six vues, le scroll, le resize incluant 10×4, le signalement de surcharge
et la restauration `termios`/écran alternatif observables sous PTY.

Dans cette exécution, le P95 du traitement clavier mesuré par accusé du
handler est de **132,78 ms pour FTXUI** et **0,61 ms pour ncursesw**. Selon le
seuil expérimental de 100 ms du protocole, FTXUI échoue encore cette charge;
ncursesw la respecte. Cela justifie de garder ncursesw comme candidat
privilégié pour l'étape expérimentale suivante, sans démontrer une supériorité
générale ni autoriser son intégration au produit.

## Référence, environnement et reproductibilité

HEAD et SHA testé : `0b1cd53b7d466257b3da50bf17b27013d1b0ab93`; branche dédiée
du POC. `origin/main` après `git fetch origin main` pointait sur le même SHA.
La requête réseau séparée pour lister la branche distante a échoué sur une
erreur DNS; l'état distant de la branche POC n'a donc pas pu être confirmé par
Git dans cette exécution. La recherche GitHub des PR ouvertes pour le sujet
n'a retourné aucun résultat au moment de la consultation.

Environnement documenté par le POC-2 : Debian 13.7, GCC 14.2.0, CMake 3.31.6,
Python 3.13.5, locale `C.UTF-8`, PTY `xterm-256color`; FTXUI v7.0.3 au commit
`f921fad208912747c17d129a8ef75ec7624b6eec`; ncursesw 6.5.20250216.

Commandes exécutées :

```sh
cmake -S poc/tui-poc-1/poc-ftxui -B /tmp/tui-poc-ftxui-clean \
  -DFETCHCONTENT_SOURCE_DIR_FTXUI=/tmp/tui-poc-ftxui-build/_deps/ftxui-src
cmake --build /tmp/tui-poc-ftxui-clean -j2
cmake -S poc/tui-poc-1/poc-ncursesw -B /tmp/tui-poc-ncurses-clean
cmake --build /tmp/tui-poc-ncurses-clean -j2
/tmp/tui-poc-ftxui-clean/sanitizer-tests
/tmp/tui-poc-ncurses-clean/sanitizer-tests
python3 poc/tui-poc-1/harness/test_pocs.py \
  /tmp/tui-poc-ftxui-clean/poc-ftxui \
  /tmp/tui-poc-ncurses-clean/poc-ncursesw \
  /tmp/tui-poc-ftxui-clean/sanitizer-tests \
  /tmp/tui-poc-ncurses-clean/sanitizer-tests
```

Le build FTXUI a réutilisé le clone local épinglé au commit indiqué. Il s'agit
d'une reconfiguration/recompilation propre à partir d'une source déjà acquise,
pas d'une validation de l'acquisition réseau depuis zéro. ncursesw a été
reconfiguré depuis un répertoire de build propre avec les paquets système
présents. Les deux sorties sanitizer étaient `sanitizer corpus passed`; le
harness s'est terminé avec code 0 et toutes ses assertions.

## Résultats PTY rejoués

### Scénario normal

| Mesure | FTXUI | ncursesw |
|---|---:|---:|
| Deltas ACK et ordre | 1000, ordonnés | 1000, ordonnés |
| Touches de navigation | 240 | 240 |
| P95 handler clavier | 132,78 ms | 0,61 ms |
| Débit d'ACK synthétiques | 103,2/s | 98,5/s |
| P95 intervalle ACK du flux | 17,10 ms | 22,43 ms |
| Frames/refresh instrumentés | 579 | 579 |
| Changements de taille observés | 17 | 17 |
| RSS maximum échantillonnée | 4704 KiB | 4828 KiB |
| CPU échantillonné, pourcentage d'un cœur | 27,4 % | 6,4 % |
| Octets PTY lus, teardown inclus | 1 280 433 | 40 096 |
| Écran alternatif entrée/sortie | oui/oui | oui/oui |
| `termios` pré/post identique | oui | oui |

### Scénarios complémentaires

| Scénario | FTXUI | ncursesw |
|---|---|---|
| Exception contrôlée | code 2; 51 ACK; écran et `termios` restaurés | code 2; 19 ACK; écran et `termios` restaurés |
| Surcharge avec pause consommateur | 128 ACK, 119 rejets signalés; `TURN_FINISHED 1000`; restauration OK | 129 ACK (128 détails et fin), 71 rejets signalés; `TURN_FINISHED 1000`; restauration OK |
| Fermeture pendant activité | code 0; 70 ACK; restauration OK; aucun `TURN_FINISHED` attendu après fermeture anticipée | code 0; 69 ACK; restauration OK; aucun `TURN_FINISHED` attendu après fermeture anticipée |
| `TERM=dumb` | refus code 2, 36 octets PTY, aucune séquence écran | refus code 2, 50 octets PTY, aucune séquence écran |
| Absence de TTY | refus explicite, aucun ANSI sur stdout | refus explicite, aucun ANSI sur stdout |

Les mesures de latence vont de l'écriture d'une touche dans le master PTY à
l'accusé `PANEL` transmis par le handler. Elles ne mesurent pas le moment où
les pixels sont visibles. Le polling du harness, l'ordonnanceur, la charge
synthétique et l'instrumentation contribuent aux mesures de débit et
d'intervalles. CPU et RSS sont échantillonnés depuis `/proc` toutes les 50 ms;
les pointes plus brèves peuvent manquer. Les compteurs de frames/refresh ont
des points d'instrumentation propres à chaque backend et ne mesurent pas le
temps GPU.

Par rapport au POC-2 (127,38 ms / 0,42 ms de P95 clavier; 19,8 % / 4,8 % CPU;
4788 / 4840 KiB RSS; 1 271 324 / 40 186 octets PTY), les écarts rejoués sont
de +5,40 ms pour FTXUI et +0,19 ms pour ncursesw sur le P95, de +7,6 et +1,6
points de pourcentage CPU, de −84 et −12 KiB RSS et de +9 109 et −90 octets
PTY respectivement. Le protocole mesure une seule exécution ici; ces écarts
ne permettent pas d'inférer une tendance statistique. Les différences
d'ordonnancement expliquent aussi les rejets de surcharge distincts.

## Revue de comparabilité et décision

Les deux prototypes utilisent le même harness, la même charge de 1000 deltas
à 100/s, les mêmes 240 touches, la même saisie (`aXb\n\n  café` après édition),
les mêmes 16 changements de dimensions, et le même corpus sanitizer. L'ordre
des séquences est vérifié. Le protocole PTY est donc comparable pour les
scénarios rejoués, avec les réserves propres aux compteurs de frames et aux
mesures échantillonnées décrites ci-dessus.

Les deux corpus sanitizer ont réussi sur CSI de déplacement/effacement, OSC
de titre et liens OSC 8, DCS, séquences fragmentées, C0/C1, UTF-8 invalide,
Unicode valide et contrôles bidi. Le harness a aussi confirmé l'absence de
séquences dangereuses dans les octets de données vérifiés. Cela ne constitue
pas une preuve de sécurité générale des bibliothèques ou de tout terminal.

| Critère | FTXUI | ncursesw | Preuve | Limite |
|---|---|---|---|---|
| Build propre C++20 | réussi | réussi | commandes ci-dessus | FTXUI source locale épinglée, pas de fetch depuis zéro |
| Parité de navigation, saisie, scroll | réussie dans le scénario POC | réussie dans le scénario POC | assertions du harness | vues synthétiques; un seul contenu soumis |
| Flux et ordre | 1000 ACK ordonnés | 1000 ACK ordonnés | records séquencés | producteur simulé, aucun moteur |
| Resize et 10×4 | réussis selon assertions | réussis selon assertions | 17 états de dimension | pas de validation visuelle par personne |
| Sanitizer | corpus passé | corpus passé | deux binaires sanitizer | le corpus ne prouve pas l'absence de défaut inconnu |
| Restauration normale/exception | observée sous PTY | observée sous PTY | termios et écran alternatif | pas de crash OS, SIGKILL ou perte de terminal |
| P95 handler clavier, cible expérimentale 100 ms | échec: 132,78 ms | réussite: 0,61 ms | 240 échantillons de touches | traitement/ACK, pas délai d'affichage; un run |
| CPU / volume PTY | 27,4 % / 1,28 MB | 6,4 % / 40,1 KB | mêmes scénarios synthétiques | mesure échantillonnée et spécifique à cette maquette |
| Terminal Linux réel et SSH | non vérifiés | non vérifiés | aucun terminal interactif accessible dans cette session | aucun substitut PTY présenté comme preuve équivalente |

**Recommandation :** garder ncursesw comme candidat privilégié pour la suite
du POC, car il respecte la cible de latence handler du protocole sous cette
charge, avec un CPU et un volume PTY mesurés inférieurs lors de cette
exécution. Il n'est pas démontré supérieur de façon générale à FTXUI; les
résultats sont limités à ces prototypes, cet hôte, ce terminal émulé et ce
harness. Cette recommandation n'autorise aucune dépendance ni intégration dans
le produit.

## Qualification terminal complémentaire

Les essais suivants sont **NON VÉRIFIÉS** : terminal Linux interactif réel,
session SSH interactive, redimensionnement manuel pendant le flux, sélection
et copie de texte, longue saisie multiligne manuelle, ergonomie des six vues,
compatibilité avec plusieurs émulateurs/terminfo, locales différentes, perte
de terminal, SIGINT/SIGTERM et fermeture humaine pendant activité. Le PTY
automatisé vérifie les tailles et la restauration mesurable, mais ne remplace
pas ces essais. SIGKILL n'est pas présenté comme restaurable.

## Périmètre et invariants

Le travail reste isolé sous `poc/tui-poc-1/` avec les documents de comparaison.
Aucun fichier de production, CMake produit, test produit, Agent, HTTP/SSE,
REPL, workspace tools, Landlock ou politique `write_file` n'a été modifié. Le
checkout UI-4 observé séparément conserve ses modifications locales; aucun
fichier de ce checkout n'a été touché par cette itération. La CLI classique
reste hors du prototype et n'est pas modifiée.

## Git et verdict

`origin/main` a été récupéré et était au SHA de base lors du contrôle. La
vérification Git directe de la branche distante a échoué par résolution
DNS; la recherche GitHub de PR ouvertes sur le sujet n'a retourné aucune PR.
L'état final de publication est consigné par la réponse de la commande de
push/PR, et non présumé.

**Verdict : VALIDÉ pour la revalidation comparative du POC sur PTY et la
recommandation de ncursesw comme candidat de POC. NON VÉRIFIÉ pour les essais
sur terminal réel/SSH. NON AUTORISÉ pour l'intégration produit.**
