# Interface TUI LOFITERM

🇫🇷 Français · [🇬🇧 English](../en/03-tui.md) · [🇪🇸 Español](../es/03-tui.md)

La TUI ncursesw est une interface plein écran pour terminal interactif. Elle conserve le même agent, le même transport et les mêmes politiques d'outils que la CLI classique.

## Les six vues

| Raccourci | Vue | Contenu |
|---|---|---|
| F1 | Conversation | Historique, fragments de streaming, composeur. |
| F2 | Outils | Déclarations des outils et événements d'activité réellement reçus. |
| F3 | Activité | Durées, compteurs HTTP/SSE, fragments de texte et détails outil conservés. |
| F4 | Diagnostics | Erreurs et diagnostics observés. |
| F5 | Configuration | Valeurs de configuration en lecture seule; secrets masqués. |
| F6 | Aide | Démarrage, raccourcis, À propos et limites. |

Les vues affichent des informations disponibles; elles ne fabriquent pas de progression. F3 précise que les fragments de texte ne sont pas des tokens et montre `PARTIAL` lorsque les détails d'événements sont incomplets.

## Clavier et souris

- F1–F6 sélectionnent les vues.
- Tab change le focus. Échap ferme une sous-rubrique ouverte ou revient à Conversation.
- Flèches déplacent le curseur du composeur ou la sélection du menu selon le focus.
- Entrée ajoute une ligne lorsque le composeur est focalisé; F9 soumet exactement une fois depuis Conversation.
- Page Up/Page Down font défiler la vue. F10 demande de quitter.
- `M` bascule la présentation Markdown/texte brut lorsque le focus est sur le contenu Conversation.
- Cliquer un onglet sélectionne la vue; cliquer le composeur lui donne le focus; les clics sur les rubriques F5/F6 les ouvrent ou sélectionnent. La molette défile le contenu actif.

La sélection native de texte dépend du terminal; Ghostty peut demander Maj + clic-glisser. Dans le dialogue `WRITE APPROVAL`, la saisie du défi est distincte du composeur. Aucun clic ou raccourci de navigation ne consent à une écriture.

## Markdown, code et Unicode

Les réponses utilisent un sous-ensemble Markdown (titres, listes, citations, emphase, code et liens en texte). Les liens ne sont pas ouverts, aucun HTML n'est interprété et aucune séquence ANSI fournie par le modèle n'est exécutée. `M` bascule le rendu formaté et le texte brut. Les blocs clôturés peuvent être colorés pour C++, Python, Bash, JSON, YAML et diff; les blocs incomplets restent provisoires. Le résultat visuel dépend des capacités du terminal, de sa police et de la locale. La conformité Unicode universelle n'est pas garantie.

## Petits terminaux et fermeture

Le layout prend en charge des dimensions compactes et signale lorsque le terminal est trop petit pour soumettre. Agrandissez la fenêtre avant F9 si le message l'indique. F10 demande la fermeture; un tour réseau ou outil actif peut terminer avant que l'application ferme. La restauration des modes terminal est assurée par la session ncurses RAII, mais reste dépendante du terminal.

## Capture de référence

![Conversation capturée depuis le binaire réel dans un PTY tmux](../../assets/screenshots/tui-f1-pty-capture.svg)

Cette capture sans données est générée à partir de la grille ncurses réellement émise par le binaire au SHA documenté dans le README. Elle n'est pas une capture Ghostty ni une validation physique. Les autres vues sont dans le [README](../../../README.md#tui-preview).

Voir [démarrage](01-demarrage-installation.md), [outils](04-outils-workspace.md) et [sécurité](05-securite-limites.md).
