# TUI-POC-2 — Parité fonctionnelle et comparaison des backends

**Date :** 2026-11-05
**Branche :** `feat/tui-poc-1-backend-comparison`
**Worktree :** `/tmp/local-agent-cpp-tui-poc1`
**SHA de base :** `0b1cd53b7d466257b3da50bf17b27013d1b0ab93`
**Verdict :** recommandation POC en faveur de ncursesw; validation sur terminal réel et intégration produit non réalisées.

## Objectif et périmètre

Cette itération complète les prototypes isolés FTXUI et ncursesw pour comparer
le même parcours conversationnel synthétique. Il n'y a ni appel à `Agent`, ni
LLM, ni code de production modifié. Le checkout UI-4 et le worktree documentaire
TUI-0 sont restés séparés et inchangés.

## Modifications

Sous `poc/tui-poc-1/` :

- les deux prototypes présentent six vues nommées avec contenus de statut
  distincts; F1–F6 naviguent et F1 revient à Conversation;
- Tab alterne le focus entre navigation et saisie, l'état de focus est affiché;
- la conversation conserve 1000 lignes synthétiques et PageUp/PageDown fait
  défiler la fenêtre de conversation;
- la saisie est multiligne, soumise explicitement par F9; ncursesw dispose d'un
  curseur UTF-8 avec déplacement, insertion et suppression, tandis que FTXUI
  utilise son composant `Input` multiligne;
- les tailles 10×4, 20×6, 40×12 et 80×24 passent par un mode compact ou le layout
  régulier; le harness effectue 16 changements de taille après l'initialisation;
- F10 ferme pendant activité et F12 déclenche une exception contrôlée;
- le filtre terminal commun couvre les séquences ESC/CSI/OSC/DCS fragmentées,
  C0/C1, UTF-8 invalide et contrôles bidi; un corpus C++ dédié vérifie les
  octets produits avant rendu;
- les compteurs de frames sont remontés pour l'expérience; ncursesw ne redessine
  qu'après changement de vue/texte/dimensions, FTXUI compte les appels de son
  renderer.

Tous ces changements restent dans `poc/tui-poc-1/`, hors du CMake produit.

## Environnement et commandes

Environnement observé : Debian 13.7, GCC 14.2.0, CMake 3.31.6, Python 3.13.5,
PTY `xterm-256color`, locale `C.UTF-8`. FTXUI v7.0.3 est épinglé au commit
`f921fad208912747c17d129a8ef75ec7624b6eec`; ncursesw 6.5.20250216 est la
bibliothèque système effectivement liée.

Configuration et compilation propres :

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

Le build propre FTXUI réutilise la source au commit fixé récupérée lors du
POC-1; cela valide une compilation propre sans réseau, pas une récupération
hors ligne depuis zéro. ncursesw a été configuré et compilé depuis un build
vide en utilisant les paquets déjà installés.

Le harness PTY a terminé cinq scénarios PTY par backend (normal, exception,
surcharge, fermeture active, `TERM=dumb`) et deux essais sans TTY. Les deux
exécutables `sanitizer-tests` ont réussi leur corpus commun. Les tests produit
CTest n'ont pas été lancés, car rien du produit n'a été modifié.

## Contrat des mesures

Le test normal utilise les mêmes 1000 deltas, 100/s, les mêmes 240 touches de
navigation injectées toutes les 15 ms, les mêmes données de saisie et les mêmes
16 changements de taille pour les deux moteurs.

- **Latence clavier** : de l'écriture de la touche dans le master PTY à la
  réception du record `PANEL` produit dans le handler UI. P95 calculé sur les
  240 touches. C'est une latence de traitement/accusé, pas une mesure de pixels.
- **Débit accusé** : 999 intervalles séquence 0→999 mesurés à la lecture du
  pipe privé du harness; la cadence de polling et l'ordonnanceur contribuent.
- **P95 intervalle flux** : intervalles entre accusés consécutifs de deltas,
  calculés par la même méthode pour les deux backends.
- **Frames** : appels du renderer FTXUI et appels `refresh()` ncursesw après un
  changement d'état. Les points d'instrumentation diffèrent légèrement et ce
  chiffre sert à vérifier l'ordre de grandeur, pas à comparer le temps GPU.
- **CPU** : delta des ticks `/proc/<pid>/stat` divisé par le temps entre le
  premier et le dernier échantillon, échantillons toutes les 50 ms; pourcentage
  d'un cœur, granularité limitée par les ticks système.
- **RSS** : maximum `VmRSS` échantillonné toutes les 50 ms dans
  `/proc/<pid>/status`; les pointes entre échantillons peuvent manquer.
- **Octets PTY** : total lu du master PTY, teardown inclus après drainage de la
  sortie du processus.
- **Restitution terminal** : égalité complète des tableaux `termios` pré/post
  et présence d'entrée/sortie écran alternatif sur la capture PTY.

## Résultats finaux observés

| Mesure, scénario normal | FTXUI | ncursesw |
|---|---:|---:|
| ACK des deltas | 1000, séquences exactes et ordonnées | 1000, séquences exactes et ordonnées |
| touches traitées | 240 | 240 |
| P95 handler clavier | 127,38 ms | 0,42 ms |
| débit d'ACK séquence 0→999 | 103,6/s | 98,5/s |
| P95 intervalle d'ACK du flux | 16,95 ms | 22,10 ms |
| frames/refresh | 577 | 586 |
| changements de taille observés | 17 (initialisation comprise) | 17 (initialisation comprise) |
| RSS max échantillonnée | 4788 KiB | 4840 KiB |
| CPU échantillonné | 19,8 % | 4,8 % |
| octets PTY lus | 1 271 324 | 40 186 |
| alt-screen entrée/sortie | oui / oui | oui / oui |
| `termios` complet restauré | oui | oui |

Le harness a vérifié le texte soumis après édition du curseur :
`aXb\n\n  café` (égalité octet par octet). Les tests ont navigué dans les six
vues, alterné le focus, défilé la conversation dans les deux directions,
retourné à Conversation et utilisé les flèches dans le texte multiligne.
Les resize ont été émis 16 fois et les records ont confirmé les tailles
10×4 et 40×12; le processus a terminé normalement.

Sous surcharge avec pause consommateur de 2 s :

| Backend | détails ACK | rejets signalés | résultat terminal reçu | RSS max | CPU échantillonné |
|---|---:|---:|---|---:|---:|
| FTXUI | 128 | 119 | `TURN_FINISHED 1000` | 4788 KiB | 1,1 % |
| ncursesw | 129 (128 détails + fin) | 71 | `TURN_FINISHED 1000` | 4752 KiB | 0,4 % |

Les rejets diffèrent parce que l'ordonnancement et le rythme du consommateur
diffèrent; les deux signalent l'overflow et livrent le statut final réservé.
Les statuts terminaux HTTP/tool individuels ne sont pas modélisés ici.

Les deux exceptions contrôlées ont terminé avec code 2, après activation du
plein écran, et ont restauré `termios` et l'écran alternatif. Les deux
fermetures actives ont terminé avec code 0 et restauré le terminal; le
producteur synthétique s'est arrêté avant les 1000 deltas. `TERM=dumb` sous PTY
et l'absence de TTY ont été refusés, sans ANSI sur stdout dans le cas pipe.

Les corpus sanitizer exécutés pour les deux builds couvrent CSI de mouvement
de curseur/effacement, OSC titre et OSC 8, DCS, fragments découpés octet par
octet, contrôles C0/C1 bruts et UTF-8, bidi, UTF-8 invalide et texte Unicode
valide. Les séquences de commande et leurs paramètres ont été supprimés; les
textes ordinaires adjacents sont conservés. Le flux synthétique du programme
a aussi passé ses fragments `SAFE_HEX` et assertions sans octet ESC/C0/C1,
bidi ou séquence UTF-8 invalide.

## Comparaison et critères éliminatoires

| Critère | FTXUI | ncursesw | Preuve | Limite / verdict |
|---|---|---|---|---|
| Compilation C++20 propre, Debian | réussi | réussi | builds vides; commit FTXUI fixé | FTXUI réutilise le clone exact local |
| Six vues, navigation, focus et retour Conversation | réussi | réussi | 240 ACK F1–F6; Tab; records focus | vues de statut synthétiques, pas métier |
| Conversation longue et scroll | réussi | réussi | 1000 lignes; PageUp/PageDown et records `SCROLL` | capture humaine non évaluée |
| Saisie multiligne/cursor | réussi | réussi | égalité exacte après insertion et mouvement horizontal/vertical | cas de texte court unique |
| Streaming pendant navigation | réussi | réussi | 1000 deltas; ordre séquentiel exact | flux synthétique |
| Resize répétitif et 10×4 | réussi | réussi | 16 mutations, 17 états dimensionnels; `LAYOUT tiny 10x4` | aucun écran matériel/SSH |
| Corps hostile terminal | réussi | réussi | corpus sanitizer commun et assertions des bytes filtrés | ne prouve pas l'absence de défaut inconnu de bibliothèque |
| Sortie normale et exception | réussi | réussi | alt-screen entrée/sortie et `termios` identique | SIGKILL/crash OS non testés |
| Queue saturée / statut terminal | réussi avec pertes explicites | réussi avec pertes explicites | compteurs et `TURN_FINISHED` | paramètres expérimentaux seulement |
| Latence clavier P95, seuil candidat 100 ms | **échec: 127,38 ms** | **réussite: 0,42 ms** | 240 touches, même injection/horloge | latence handler, pas rendu affiché |
| CPU / octets PTY | 19,8 % / 1,27 MB | 4,8 % / 40,2 KB | mêmes 1000 deltas et ~même nombre de frames | écarts propres à cette maquette et ce terminal |
| Dépendance / maintenance | API déclarative, dépendance à construire/fixer | API système, édition/layout davantage manuels | CMake et diff inspectés | aucun coût de maintenance à long terme mesuré |
| Intégration future `AgentEvent` | `PostEvent` est un pont possible | boucle UI peut drainer une queue avec polling | POC synthétique seulement | ni adaptateur moteur ni concurrence produit implémentés |
| CLI classique | indépendante, pas de changement produit | indépendante, pas de changement produit | aucun fichier de production modifié | sélection CLI/TUI reste une décision ultérieure |

Le volume PTY est environ 31,6 fois plus grand avec FTXUI malgré un nombre de
frames proche. **Observation :** les captures montrent 1,27 MB contre 40,2 KB.
**Interprétation probable, non isolée par un microbenchmark :** le chemin de
rendu FTXUI réémet davantage de contenu pour chaque frame, tandis que
`refresh()` ncursesw profite davantage de l'optimisation différentielle de
l'écran virtuel. Le nombre d'octets ne prouve pas seul la cause; des captures
du contenu diff/escape par frame seraient nécessaires pour l'attribuer
complètement.

Le seuil expérimental P95 de 100 ms est dépassé par FTXUI dans le stress de
240 touches à 66,7/s, donc FTXUI échoue au critère candidat dans cette charge.
ncursesw le respecte largement et présente une consommation CPU et un volume
PTY inférieurs dans cette maquette. Ce seuil doit rester limité au protocole
POC et ne constitue pas une garantie produit.

## Essais non réalisés / risques

- Pas de terminal physique, session SSH, test humain d'accessibilité ou
  compatibilité avec plusieurs émulateurs/terminfo.
- Pas de mesure fiable du délai jusqu'aux pixels; seules la latence handler,
  les frames et la sortie PTY sont mesurées.
- Les données et outils des panneaux sont simulés; aucun `AgentEvent` réel,
  worker produit ou LLM n'a été utilisé.
- Pas de SIGINT/SIGTERM explicite, SIGKILL, perte de terminal, souris,
  intégration CTest produit ou analyse statistique multi-runs.
- La charge de surcharge n'est pas une garantie RSS maximale; seul le RSS
  échantillonné est fourni.
- La sortie de FTXUI reste importante et sa P95 dépasse le seuil choisi; un
  usage de cette bibliothèque exigerait une nouvelle optimisation et mesure.
- ncursesw laisse davantage de gestion de saisie, layout et tests au projet;
  cela accroît la responsabilité de maintenance même si la maquette est plus
  légère en mesures.

## Recommandation

**Pour la suite du POC, retenir ncursesw comme backend unique candidat.** C'est
le seul des deux à respecter le seuil clavier annoncé sous la charge mesurée;
il conserve la parité testée, restaure écran et terminal, et consomme moins de
CPU et d'octets PTY dans cette expérience. Ce choix ne vaut pas intégration
dans le produit, ne modifie pas la gouvernance des dépendances et ne remplace
pas une revue d'ergonomie sur terminal réel. FTXUI est écarté pour la charge
actuelle, sauf nouvel essai après correction mesurée de son retard et de son
volume de rendu.

## Git et confinement du périmètre

Base `0b1cd53b7d466257b3da50bf17b27013d1b0ab93`; branche
`feat/tui-poc-1-backend-comparison`; worktree isolé. Aucun commit, push ou merge.
Les changements se limitent à `poc/tui-poc-1/`, ce rapport et l'index
`docs/README.md`. Pas de changement à Agent, HttpClient, SSE,
PresentationState, MetricCollector, ConsoleRenderer, REPL, Landlock, workspace
tools ou CMake produit. Le worktree UI-4 n'a pas été modifié.

**Verdict : VALIDÉ pour la comparaison de ce POC et recommandation ncursesw
comme candidat de la phase suivante; NON VALIDÉ pour l'intégration produit.**
