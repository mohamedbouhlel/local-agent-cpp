# TUI-POC-1 — Comparaison expérimentale FTXUI / ncursesw

**Date :** 2026-11-05
**Branche :** `feat/tui-poc-1-backend-comparison`
**SHA de base :** `0b1cd53b7d466257b3da50bf17b27013d1b0ab93`
**Statut :** prototypes isolés exécutés; comparaison partielle, aucun backend retenu.

## Objectif et périmètre

Deux exécutables autonomes ont été construits sous `poc/tui-poc-1/`. Ils ne
dépendent pas de `agent_core`, ne se connectent pas à un LLM et ne touchent pas
à l'agent, au REPL ou au confinement. Un producteur synthétique alimente les
deux expériences à 100 deltas/s pendant 10 secondes. Une file de test est
bornée à 128 événements, 256 KiB de texte et 8 KiB par événement; la fin du
flux a une case réservée. Les tests ne changent aucune capacité Landlock.

Le scope est volontairement expérimental. Les panneaux sont des vues
squelettiques nommées (Conversation, Outils, Métriques, Diagnostics,
Configuration, Aide), pas six interfaces fonctionnelles. Il n'y a pas de
connexion moteur ou de worker de production.

## Dépendances et reproductibilité

Environnement observé : Debian 13.7, GCC 14.2.0, CMake 3.31.6, Python 3.13.5.
Le test PTY utilise `pty`, `termios`, `fcntl`, `/proc`, et `xterm-256color`.

| Backend | Version / linkage observé | Acquisition |
|---|---|---|
| FTXUI | v7.0.3, commit `f921fad208912747c17d129a8ef75ec7624b6eec`; cibles `ftxui::screen`, `ftxui::dom`, `ftxui::component` | `FetchContent` dans le projet CMake autonome; tag/commit fixé, source clonée dans le build temporaire |
| ncursesw | ncursesw 6.5.20250216, `/usr/lib/x86_64-linux-gnu/libncursesw.so`, `-ltinfo`; CMake configuré avec `CURSES_NEED_WIDE` | paquet système déjà présent; aucune installation effectuée |

FTXUI a nécessité l'autorisation d'accès réseau, car le sandbox ne résolvait
pas `github.com`. La source a été récupérée dans `/tmp/tui-poc-ftxui-build`,
pas installée dans le produit. Les commandes exactes sont reproduites dans
[`poc/tui-poc-1/README.md`](../../poc/tui-poc-1/README.md). Le build a réussi
pour les deux cibles.

## Scénarios et définitions de mesure

Commande de harness :

```sh
python3 poc/tui-poc-1/harness/test_pocs.py \
  /tmp/tui-poc-ftxui-build/poc-ftxui \
  /tmp/tui-poc-ncurses-build/poc-ncursesw
```

Le harness lance chaque prototype dans un pseudo-terminal `xterm-256color`.
Il injecte F1 à F6 pendant le flux, saisit `a\n\n  café`, soumet avec F9,
redimensionne le PTY à 20×6 puis 80×24, et quitte après le flux. Le test
compare les attributs `termios` complets avant démarrage et après sortie.
Une exception contrôlée F12 est aussi déclenchée après l'initialisation du
terminal. `TERM=dumb` et l'absence de TTY sont refusés explicitement.

* **Latence de navigation** : départ au moment où le harness écrit la séquence
  de touche dans le master PTY; arrivée à la réception de l'accusé `PANEL` sur
  le pipe privé émis par le handler UI. Il s'agit du temps jusqu'au traitement
  et à l'accusé du handler, pas du temps jusqu'aux pixels visibles. Six
  échantillons par backend seulement; le P95 est donc indicatif et fragile.
* **Régularité du flux** : P95 de l'intervalle observé entre accusés successifs
  de séquence consécutive, capturés par le polling du pipe à 20 ms. Ce n'est
  pas un P95 de rendu terminal; le polling et l'ordonnanceur contribuent à la
  mesure.
* **CPU** : approximation en pourcentage d'un cœur à partir des ticks
  `/proc/<pid>/stat`, rapportés au temps écoulé du scénario. Échantillonnage
  non instrumenté; précision limitée par la granularité des ticks.
* **Mémoire** : maximum des `VmRSS` lus dans `/proc/<pid>/status` pendant le
  scénario, en KiB. L'échantillonnage peut manquer des pointes brèves; ce n'est
  pas une borne mémoire garantie.
* **Surcharge** : le consommateur est suspendu artificiellement 2 s. Les pertes
  sont les rejets explicites de la file; le slot terminal séparé doit encore
  délivrer `TURN_FINISHED`.

## Résultats bruts observés

Les valeurs ci-dessous proviennent de la dernière exécution complète du
harness sur les sources présentées dans cette branche.

| Backend / scénario | ACK deltas | navigation P95 (6 touches) | intervalle ACK flux P95 | RSS max échantillonnée | CPU échantillonné | sortie PTY | pertes surcharge | terminal conservé | termios complet restauré |
|---|---:|---:|---:|---:|---:|---:|---:|---|---|
| FTXUI normal | 1000, séquences ordonnées | 94,02 ms | 52,50 ms | 5752 KiB | 91,1 % | 509675 octets | — | `TURN_FINISHED 1000` | oui |
| ncursesw normal | 1000, séquences ordonnées | 0,30 ms | 26,54 ms | 4752 KiB | 21,3 % | 5345309 octets | — | `TURN_FINISHED 1000` | oui |
| FTXUI surcharge | 128 | — | — | 4864 KiB | 1,8 % sur l'essai ralenti | 8398 octets | 119 | `TURN_FINISHED 1000` | oui |
| ncursesw surcharge | 129 (128 détails + fin) | — | — | 4720 KiB | 1,4 % sur l'essai ralenti | 459 octets | 71 | `TURN_FINISHED 1000` | oui |
| FTXUI exception | 51 ACK avant exception | — | — | 5100 KiB | 1,9 % arrondi | 102 octets | — | sans objet | oui; code retour 2 |
| ncursesw exception | 21 ACK avant exception | — | — | 4628 KiB | 0,0 % arrondi | 617 octets | — | sans objet | oui; code retour 2 |

Les deux scénarios normaux ont renvoyé exactement le texte UTF-8 soumis sous
forme hexadécimale (`610a0a2020636166c3a9`), sans perte des lignes vides ou des
espaces. Pour les deux, les séquences synthétiques OSC de titre terminal
fractionnées entre deux deltas n'ont pas laissé apparaître le marqueur
`POC_UNTRUSTED_TITLE` dans la capture. Les deux ont refusé `TERM=dumb` sous
PTY avec code retour 2 et sans séquence ESC. En pipes avec `TERM=xterm-256color`,
ils ont refusé l'absence de TTY, code retour 2, sans ESC sur stdout.

Les captures PTY, RSS et CPU sont des mesures de ce POC dans cet environnement.
Elles ne représentent ni une charge de production ni une performance du moteur.

## Comparaison contradictoire

| Critère | FTXUI | ncursesw | Mesure / preuve | Limite |
|---|---|---|---|---|
| Compilation Debian/C++20 | réussite | réussite | configure et build CMake isolés | pas de matrice multi-distro |
| Démarrage plein écran et sortie | observés | observés | processus sous PTY et séquences de rendu capturées | pas de terminal matériel ni de session SSH |
| Menu / navigation six vues | réussite minimale | réussite minimale | ACK des six touches, six échantillons | vues squelettiques; P95 peu robuste |
| Saisie multiligne exacte | réussite | réussite | chaîne soumise comparée octet par octet | uniquement un contenu de référence |
| Navigation pendant streaming | réussite | réussite | six touches pendant les 1000 deltas | pas de mesure de fluidité visuelle humaine |
| Ordre et surcharge | détails reçus en ordre; pertes signalées | détails reçus en ordre; pertes signalées | séquences ACK et compteurs de perte; slot de fin reçu | perte des détails est attendue; ce n'est pas une file de production |
| Resize / petit terminal | pas de crash; rendu compact observé | pas de crash; rendu compact observé | PTY 20×6 puis 80×24 pendant génération | 10×4 et réagencement pixel par pixel non testés |
| Neutralisation hostile | marqueur OSC absent | marqueur OSC absent | OSC fractionné entre deux événements | pas de corpus complet ESC/CSI/DCS/C1/bidi; résultat limité à la charge injectée |
| Restauration normale / exception | attributs `termios` identiques | attributs `termios` identiques | comparaison complète pré/post PTY | SIGKILL/crash système non garantissable ni testé |
| Navigation P95 | 94,02 ms | 0,30 ms | ACK handler, six touches | faible échantillon; seuil de 100 ms n'est pas démontré de façon robuste |
| CPU / volume terminal | 91,1 % / ~0,51 MB | 21,3 % / ~5,35 MB | ticks processus et octets capturés | mesures dépendantes du PTY et du rendu de cette maquette |
| RSS max échantillonnée | 5752 KiB | 4752 KiB | `/proc`, charge normale | échantillonnage intermittent |
| Fonctionnement sans terminal | refus explicite | refus explicite | test pipes + `TERM=dumb` | pas d'essai de tous les terminfo ou émulateurs |
| Scroll conversation | non implémenté | non implémenté | inspection du code | écart de fonctionnalité des deux POC |

FTXUI a produit beaucoup moins d'octets de terminal pendant le flux, mais la
maquette a consommé nettement plus de CPU dans cette exécution. ncursesw a eu
une latence d'accusé plus faible et une consommation CPU échantillonnée plus
basse, avec une sortie PTY beaucoup plus volumineuse. Le petit nombre de
touches et la nature synthétique du test interdisent de transformer ces
différences en verdict général sur les bibliothèques.

## Fonctions non vérifiées et limites

- Aucun scroll utilisateur de l'historique n'est implémenté; cette exigence
  commune reste en échec et empêche une comparaison fonctionnelle complète.
- Le menu affiche six noms de panneaux, mais les vues sont des squelettes et
  n'ont pas leur contenu métier.
- Le PTY ne vérifie pas visuellement la qualité du focus, l'accessibilité,
  l'Unicode CJK/combinants ou chaque taille demandée (notamment 10×4).
- Le sanitizer a été exercé avec une séquence OSC de titre fragmentée; ESC,
  CSI, DCS, C1 et bidi n'ont pas chacun un test automatisé dédié.
- Pas de souris, session SSH, perte de terminal, SIGKILL, erreur renderer,
  benchmark répété ou analyse de variance.
- Le POC ncursesw utilise l'API wide (`get_wch`) pour la saisie, mais le rendu
  demeure une maquette minimale; le test n'établit pas une couverture Unicode
  complète.
- Les composants `EventQueue` sont expérimentaux. La protection de capacité
  ne borne pas à elle seule le RSS; l'overhead de threads, du backend et du
  système reste séparé.
- Aucun test CTest produit n'a été lancé, car les fichiers de production et
  son harnais ne sont pas dans le périmètre du POC.

## Impact et recommandation

Les deux backends sont techniquement intégrables à un projet C++20 sous Debian,
mais ce POC ne fournit pas une preuve suffisante pour en retenir un. ncursesw
présente des mesures prometteuses de réactivité/CPU sur cette maquette; FTXUI
réduit les octets écrits et fournit une API déclarative plus haut niveau. Le
scroll manquant et les tests incomplets de contrôles hostiles sont des lacunes
éliminatoires au regard de la parité demandée. **Aucun backend n'est retenu à
ce stade.** Compléter les deux prototypes à parité, augmenter les répétitions
de latence, tester les tailles et contrôles restants, puis soumettre les
preuves au mainteneur.

## Gouvernance et vérification du périmètre

Le travail est sur `feat/tui-poc-1-backend-comparison`, worktree isolé
`/tmp/local-agent-cpp-tui-poc1`, basé sur le SHA indiqué en tête. Aucun commit,
push ou merge n'a été effectué. Les seuls chemins modifiés/créés dans ce
worktree sont les deux projets sous `poc/tui-poc-1/`, ce rapport et l'index
`docs/README.md`. Aucun fichier agent, HTTP/SSE, REPL, workspace, Landlock,
CMake produit ou test produit n'a été modifié.

**Verdict : NON VALIDÉ pour le choix du backend; POC comparatif partiellement
exécuté.** Le protocole démontre compilation, saisie, navigation simulée,
ordre, surcharge signalée, restauration PTY et test OSC limité. Il ne démontre
pas encore la parité fonctionnelle requise, le scroll, ni une sécurité
générale du terminal.

Le harness a d'abord échoué sur un mauvais descripteur PTY puis sur des
séquences F1–F4 incorrectes pour `xterm-256color`; ces défauts de test ont été
corrigés avant les résultats ci-dessus. Les exécutions précédentes ne sont pas
incluses dans les mesures finales.
