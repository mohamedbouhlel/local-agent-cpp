# Intégration graphique LOFITERM dans la TUI

## Référence et périmètre

- Dépôt : `mohamedbouhlel/local-agent-cpp`.
- SHA de départ : `aaf4e9042ac20669f489cb6a5ed872571981387e`.
- SHA de fin : identique. Le travail reste non commité dans le worktree
  `feat/lofiterm-graphic-tui` ; aucun commit, push, merge ou tag n'a été créé.
- Worktree : `/home/oper/projects/worktrees/local-agent-cpp-lofiterm-graphic`.
- Le tag `v1.0.0-technical` n'a pas été modifié.

Le changement porte sur la présentation ncursesw, son adaptation à la taille du
terminal et les tests de rendu PTY. Il ne modifie ni l'agent, ni HTTP/SSE, ni les
événements, ni les outils, Landlock, l'autorisation d'écriture, les calculs de
`MetricCollector`, les limites de `BoundedHistory` ou le REPL classique.

## Présentation appliquée

- Bandeau public `LOFITERM`, avec une mention `PEACH FOG` dans le format large.
- Fond noir et accents ANSI disponibles : magenta/lavande pour la marque et la
  sélection, jaune/pêche, vert/menthe, cyan pour l'état courant et texte blanc
  sur fond rouge pour `PARTIAL`. Les couleurs sont des approximations de
  terminal, pas une reproduction RGB.
- `NO_COLOR` désactive les couleurs. La sélection reste encadrée et en inverse,
  les états critiques en gras. Le contenu textuel, dont `PARTIAL`, reste
  explicite.
- Navigation F1–F6 visible, élément courant distingué, indication de focus
  `MENU`/`INPUT`/`VIEW`, titres de section et séparateurs sobres. Le rendu de
  titre utilise un trait Unicode lorsque la capacité déclarée le permet et une
  variante ASCII sinon.
- Les six vues conservent leur projection et leurs sources de données. F2
  affiche les outils/activités existants ; F3 affiche les valeurs issues du
  `MetricCollector` existant. Aucun état ou chiffre n'est fabriqué. F3 garde
  `PARTIAL` lorsque les détails d'événements sont incomplets.
- Le composeur reste dans F1. Le viewport de conversation et la hauteur passée
  au maintien de l'ancre correspondent à la nouvelle géométrie. Le curseur est
  masqué hors du composeur.
- Le rendu de chaque ligne est limité par largeur de cellules, sans couper une
  séquence UTF-8 au milieu d'un caractère. Cette protection d'affichage ne
  change pas les données conservées ni envoyées.
- Le layout complet est disponible à partir de 60 colonnes × 20 lignes ; les
  libellés de navigation sont raccourcis sous 100 colonnes ou 30 lignes. En
  dessous du minimum, la TUI montre un avertissement borné, garde le brouillon
  en mémoire et refuse F9 jusqu'à ce que le terminal soit agrandi.

## Fichiers modifiés

- `include/ui/tui_view.hpp` — expose le seuil de layout pris en charge.
- `src/ui/tui_view.cpp` — palette, styles, découpe par cellules, marque, menu,
  titres, statut et géométrie adaptative.
- `src/ui/tui_event_loop.cpp` — recalcule la hauteur du viewport et bloque une
  soumission F9 quand le composeur ne peut pas être présenté.
- `tests/tui_test.cpp` — rendu PTY aux tailles cibles, 10×4, six onglets, F2/F3,
  `PARTIAL` sans couleur, et absence d'envoi sous le seuil.
- `docs/README.md` — index du présent rapport.
- `docs/reports/2026-11-10-lofiterm-graphic-integration.md` — rapport.

## Validation exécutée

Environnement de build : GCC 14.2.0, CMake 3.31, libcurl 8.14.1 et
ncursesw détectée par CMake. Le build propre utilise nlohmann/json 3.11.3,
source FetchContent déjà téléchargée pendant cette intervention. La
configuration et la compilation finales, sans objets de build préexistants :

```sh
cmake -S . -B /tmp/local-agent-cpp-lofiterm-clean-final \
  -DAGENT_BUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_JSON=/tmp/local-agent-cpp-lofiterm-build/_deps/json-src
cmake --build /tmp/local-agent-cpp-lofiterm-clean-final -j2
```

Résultat : build complet réussi. CMake a émis son avertissement développeur
existant sur `CMP0135`/`DOWNLOAD_EXTRACT_TIMESTAMP`; aucun avertissement de
compilation lié aux changements n'a été observé.

Les tests ont été exécutés après le build propre dans
`/tmp/local-agent-cpp-lofiterm-clean-final` :

| Vérification | Résultat |
| --- | --- |
| `./tests/agent_tests --filter=tui3` | 29 cas, 404 assertions, 0 échec |
| `./tests/agent_tests --filter=tui4a` | 20 cas, 169 assertions, 0 échec |
| `./tests/agent_tests --filter=tui4b1` | 10 cas, 6 125 assertions, 0 échec |
| `ctest --output-on-failure` | 14/14 groupes, 0 échec, 36,51 s |
| `./tests/agent_tests` | 280/280 cas, 8 277 assertions, 0 échec |

Le harnais et CTest ont été rejoués après les derniers changements de source.
Un premier CTest a échoué sur sept assertions attendant les anciens titres de
section ; les attentes de test ont été mises à jour pour les nouveaux libellés,
puis le groupe et CTest complet ont réussi. Aucun défaut fonctionnel n'a été
masqué.

Les tests PTY vérifient 120×40, 100×30, 80×24, 60×20, 19×59 et 10×4. À 10×4,
l'écran réduit affiche `LOFITERM`, `MIN 60x20`, `TOO SMALL` et `F10 QUIT`. Le
scénario PTY sous-minimal vérifie le message d'avertissement et qu'aucune
requête HTTP n'est émise après F9. Les tests existants TUI-3 couvrent le
composeur, le streaming, le défi d'approbation, le refus, l'approbation, F10 et
la restauration du terminal ; TUI-4B.1 couvre les historiques et le scroll FIFO.
Le diff n'altère pas leurs mécanismes.

## Recette physique Ghostty demandée

**NON EXÉCUTÉE dans cette mission.** L'environnement de l'agent n'a pas de TTY
(`tty` retourne `not a tty`), expose `TERM=dumb`, ne liste aucune application
graphique via CUA et aucun processus Ghostty n'est présent. Le binaire est
disponible à
`/tmp/local-agent-cpp-lofiterm-clean-final/agent` (ELF x86-64 Linux; il contient
le rendu LOFITERM construit dans ce worktree). Lancer ce binaire depuis le shell
de l'agent ne constituerait pas un essai Ghostty.

| Contrôle physique demandé | Résultat | Preuve |
| --- | --- | --- |
| Couleurs et contraste Peach Fog | NON EXÉCUTÉ | Pas de fenêtre Ghostty accessible |
| Titres, séparateurs, sélection et focus F1–F6 | NON EXÉCUTÉ | Pas de fenêtre Ghostty accessible |
| Six vues avec données de session réelles | NON EXÉCUTÉ | Pas de session interactive |
| F3 après un tour réel et `PARTIAL` | NON EXÉCUTÉ | `PARTIAL` est vérifié par PTY sans couleur; aucune donnée de perte réelle n'a été injectée dans une session physique |
| Composeur, accents, curseur, brouillon et F9 | NON EXÉCUTÉ | Pas de clavier/terminal physique accessible |
| Redimensionnement et layout compact | NON EXÉCUTÉ | Les tailles ont été testées par PTY seulement |
| F10 et restauration du terminal Ghostty | NON EXÉCUTÉ | Les tests de restauration antérieurs sont PTY et ne valent pas recette Ghostty |

Commande de lancement pour la recette mainteneur, depuis un Ghostty sur cette
machine; la configuration est chargée sans l'imprimer et le workspace est
isolé sous `/tmp` :

```sh
mkdir -p /tmp/lofiterm-graphic-physical
cd /tmp/lofiterm-graphic-physical
set -a
source /home/oper/projects/local-agent-cpp/.env
set +a
exec /tmp/local-agent-cpp-lofiterm-clean-final/agent --ui=tui
```

L'application utilise alors `/tmp/lofiterm-graphic-physical/workspace`. Dans
Ghostty, vérifier F1–F6, faire un tour simple puis F3, coller et éditer un
brouillon accentué, envoyer avec F9, redimensionner aux formats compact/large,
puis quitter avec F10. Vérifier que le shell est restauré. Les vues Outils et
Diagnostics reflètent uniquement les événements réellement produits; ne pas
forcer un état `PARTIAL` ni interpréter son absence comme une perte de données.
Cette procédure est préparée, non exécutée.

## Limites et contrôles non exécutés

- Aucun terminal physique Ghostty n'était disponible : cette intégration n'est
  pas qualifiée manuellement sur Ghostty. Les écrans et interactions mentionnés
  ici ont été vérifiés par PTY automatisé.
- Les tests vérifient `PARTIAL` sans couleur et son emphase textuelle, mais le
  chemin intégré déterministe `EventChannel saturé → MetricCollector incomplet
  → rendu F3` n'a pas été ajouté ni démontré par cette mission. La réserve
  historique UI-4 reste ouverte.
- Le coût CPU/RSS/latence des snapshots de métriques n'a pas été mesuré ; aucune
  optimisation n'a été faite.
- Les couleurs ANSI standard dépendent du terminal et ne garantissent pas les
  nuances hexadécimales des maquettes. L'état fonctionnel demeure lisible sans
  couleur.
- Ces essais ne prouvent pas une conformité Unicode universelle. Le rendu
  respecte `wcwidth` avec la locale active ; glyphes combinés ou emoji peuvent
  varier selon la police et le terminal.
- Les protections ANSI/OSC restent confiées au sanitizer existant. La présente
  modification n'ajoute pas une nouvelle campagne exhaustive de séquences
  hostiles à la couche écran.

## Verdict

**PARTIEL — intégration graphique implémentée et validée par build propre,
harnais complet, CTest et PTY.** Les six vues gardent leurs sources réelles,
les données et contrats existants restent hors du diff métier, le mode
monochrome conserve les indications critiques et les terminaux sous-minimaux
ne peuvent pas envoyer un brouillon invisible. Une validation physique Ghostty
reste à faire avant d'affirmer l'identité visuelle qualifiée sur le terminal de
référence. La chaîne saturation vers F3 et la performance des snapshots restent
également non vérifiées.

## Addendum — recette physique Ghostty rapportée par le mainteneur

Cet addendum enregistre les observations communiquées par le mainteneur après
la recette physique sous Debian 13 et Ghostty. Elles n'ont pas été reproduites
par l'agent. Les captures mentionnées par le mainteneur constituent leur
provenance visuelle; aucun fichier image correspondant n'est présent dans ce
worktree ni dans `/home/oper/tmp`, et elles ne sont donc pas archivées dans le
dépôt.

### Contrôles confirmés par le mainteneur

| Contrôle | Résultat rapporté | Source / portée |
| --- | --- | --- |
| F1 — conversation réelle avec LiteLLM | PASS | Observation du mainteneur dans Ghostty; réponses affichées correctement |
| F2 — outils | PASS | Outils déclarés et état d'activité cohérent |
| F3 — activité | PASS | `MetricCollector` et mesures réelles visibles |
| F4 — diagnostics | PASS | Diagnostics affichés |
| F5 — configuration | PASS | Configuration visible, clé API masquée |
| F6 — aide | PASS | Aide et raccourcis présents |
| Navigation F1–F6 | PASS | Confirmée par le mainteneur |
| Composeur multiligne, F9 et accents | PASS | Saisie et envoi confirmés par le mainteneur |
| Redimensionnement | PASS | Confirmé par le mainteneur |
| Terminal plus petit que 60×20 | PASS | Message de taille minimale affiché et brouillon préservé |
| F10 et restauration du terminal | PASS | Fermeture et retour au terminal confirmés par le mainteneur |

### Interruption volontaire du flux SSE

Pendant une génération réelle, le mainteneur a changé le LLM sur le serveur,
ce qui a interrompu le flux avant `[DONE]`. Les observations rapportées sont :

- F1 a marqué la réponse `[incomplete]` et affiché
  `Turn failed; partial response is not complete.`
- F3 a indiqué `Technical outcome: failed`.
- F4 a affiché une erreur de protocole.
- Aucun faux succès n'a été observé.

Mesures rapportées pour le tour 5 :

| Mesure | Valeur rapportée |
| --- | ---: |
| Durée du tour | 22 470,520 ms |
| Délai jusqu'au premier delta | 388,272 ms |
| Requêtes HTTP démarrées / terminées | 1 / 1 |
| Corps reçu | 144 931 octets |
| Fragments de texte observés | 54 |
| Appels d'outils | 0 |

Ces chiffres décrivent ce tour et cette interruption particulière; ils ne
constituent ni des garanties générales de performance ni une mesure de débit
de tokens. Cette interruption SSE ne démontre pas une saturation de
`EventChannel`.

### Qualification et réserves

**Qualification physique : GO sur le périmètre rapporté ci-dessus.** Cette
conclusion s'appuie sur les contrôles déclarés PASS par le mainteneur et les
captures qu'il a communiquées; l'agent n'a pas exécuté la recette Ghostty.
Les résultats automatisés consignés plus haut dans ce rapport — 280 cas,
8 277 assertions et CTest 14/14 — restent les résultats historiques de la
campagne sur le candidat; ils n'ont pas été relancés pendant la présente
clôture documentaire.

Les réserves suivantes restent ouvertes :

1. La chaîne intégrée
   `EventChannel saturé → événements perdus → MetricCollector incomplet → F3 PARTIAL`
   n'a pas été démontrée de bout en bout. Le scénario SSE interrompu décrit
   ci-dessus ne la remplace pas.
2. Le coût CPU, RSS et latence des snapshots de `MetricCollector` n'a pas été
   mesuré.
3. La conformité Unicode universelle et le rendu sur d'autres terminaux que
   Ghostty ne sont pas qualifiés.

La qualification physique ne modifie pas la portée des tests automatisés et
ne ferme pas ces réserves.
