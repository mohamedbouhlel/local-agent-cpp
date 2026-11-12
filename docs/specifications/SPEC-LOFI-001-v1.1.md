# LOFITERM — Spécification UX consolidée v1.1

**Référence :** SPEC-LOFI-001 — révision UX 1.1 de l'étude du 8 novembre 2026<br>
**Date :** 10 novembre 2026 (Europe/Paris)<br>
**Produit :** LOFITERM (dépôt technique `mohamedbouhlel/local-agent-cpp`)<br>
**Référence distante examinée :** branche `main`, commit `46d653fe9e788c6410129b7224edd33d870af593` (PR #46, 10/11/2026 14:46 UTC)<br>
**Mode :** étude et consolidation documentaire, lecture seule du code GitHub et des sources fournies<br>
**Statut :** **PROPOSITION COMPLÈTE POUR VALIDATION** — aucun changement de code, de branche, de dépôt, de tag ou de politique sandbox ; aucun test exécuté. Ce document est **externe au dépôt** et n'a pas été ajouté à `docs/reports/` ni `docs/specifications/`.

> **Note d'intégration documentaire (10/11/2026) :** cette copie conserve le
> statut et le contexte de l'étude source. La mission UX LOFITERM v1.1 autorise
> l'implémentation locale des exigences dans son périmètre; cette autorisation
> ne transforme pas les critères de qualification en résultats déjà acquis.
> Voir le rapport d'implémentation daté dans `docs/reports/`.

## 0. Objet, limites et décisions

La présente révision **regroupe** les exigences esthétiques et de non-régression de `SPEC-LOFI-001 v1.0` avec les demandes récentes : À propos, sous-menus, aide plus utile, raccourcis, souris, identité conversationnelle propre, affichage Markdown et coloration syntaxique. Elle ne lance **aucune migration vers FTXUI** ; l'interface ncursesw existante demeure la base. Pas de nouvelle roadmap ni d'empilement de sous-phases.

**Décidé par l'utilisateur :**
- L'application conserve le nom public **LOFITERM** et son design actuel ; l'expérience doit gagner en clarté et confiance sans se complexifier.
- L'appellation générique visible `Agent` doit évoluer vers un nom propre du type « Lofiterm-bot » ; le nom exact n'a pas été verrouillé. Proposition d'affichage : **LOFITERM Bot** (forme typographique), distinct du logiciel LOFITERM.
- Le périmètre d'étude intègre À propos, sous-menus, aide, raccourcis, souris, coloration syntaxique et visualisation Markdown.

**Non décidé :** renommer le binaire `agent`, les classes/espaces de noms `Agent`/`agent`, les cibles `agent_core`/`agent_tui`, le dépôt, la distribution, la licence, les politiques de sessions/cache ou la langue officielle définitive. L'implémentation reste à autoriser et qualifier.

**Codes de preuve :** **V** = code GitHub lu au SHA ; **D** = décision utilisateur ; **P** = proposition ; **R** = résultat rapporté antérieurement mais non rejoué ; **?** = indéterminé. Aucun écart non reproduit ne devient automatiquement un bogue.

## 1. Photographie technique actuelle (10/11)

| Sujet | Faits observés (V) | Conséquence pour cette étude |
|---|---|---|
| Code | C++20, `agent_core`, `agent_tui`, libcurl, nlohmann/json, ncursesw, Threads | Réutiliser l'existant, pas de refonte moteur |
| Interface | 6 vues F1 Chat, F2 Tools, F3 Activity, F4 Diagnostics, F5 Config, F6 Help | Ne pas créer un septième onglet |
| Graphisme | PR #45 fusionnée (`17f6fa1d`), thème LOFITERM intégré | L'étude historique du 08/11 disant « non intégré » est dépassée |
| Stabilisation | PR #46 fusionnée (`46d653fe`), correctifs de tests d'autorisation et d'historique | Base Git récente, sans nouveau build de notre part |
| Navigation | F1–F6, F9, F10, Tab, Escape, flèches, Page Up/Down, curseur multiligne | Conserver sans régression |
| Saisie | `Enter` crée une ligne, `F9` envoie depuis Chat ; `F10` n'annule pas magiquement le LLM | Aide fidèle aux comportements réels |
| Conversation | `TuiView::rendered_conversation_rows` préfixe l'assistant par `Agent: ` et le texte reste rendu ligne par ligne | Remplacer seulement le libellé d'affichage ; nouveau rendu Markdown |
| Souris | `mouseinterval(0)` présent ; pas de `mousemask()`/`KEY_MOUSE` identifié dans le chemin TUI examiné | Support souris à ajouter, non à déclarer présent |
| F5 | Modèle, origine endpoint, timeout, présence d'une API key (masquée), workspace indisponible dans la vue | Garder lecture seule ; ne pas inventer un test de connexion |
| F6 | Aide statique : touches de navigation, envoi, fermeture, entrée, scroll | Passer à l'aide structurée |
| Sécurité | Landlock activé après l'initialisation terminal, avant le worker ; écritures sous approbation explicite, écrasement interdit | Toute lecture de `.md` suit les mêmes frontières filesystem |
| Activité | `MetricCollector` dans `main` après PR #43, aperçu F3 ; pertes et détails partiels annoncés | Ne pas introduire une seconde collecte de métriques |
| CLI | `--ui=classic` par défaut, `--ui=tui` optionnel | Aucun nouveau markup ni contrôle terminal imposé à la sortie plain |

**Référence de version :** le tag `v1.0.0-technical` désigne un socle technique plus ancien ; ne jamais lui attribuer les fonctions présentes uniquement sur `main` plus récent. Le SHA `46d653fe` est la référence documentaire de cette étude, **pas une qualification de release**.


### Palette et contraintes visuelles préservées

Le design existant reste la référence ; ne pas substituer un thème nouveau. La table suivante reprend les **tokens proposés en SPEC-LOFI-001 v1.0**, sans prétendre que `tui_view.cpp` utilise déjà ces hexadécimaux (le code actuellement lu utilise des paires de couleurs ncurses).

| Token | Hex indicatif | Usage | Règle |
|---|---|---|---|
| `canvas` | `#141524` | Fond principal | Faible luminance |
| `surface` | `#22243D` | Onglets inactifs, saisie | Contraste suffisant avec texte |
| `border` | `#46466B` | Séparateurs | Subordonné au texte |
| `text` | `#E9E6F5` | Texte principal | Lisible sans dépendre d'une couleur d'état |
| `muted` | `#AAA6C6` | Métadonnées | Contraste à mesurer sur terminal cible |
| `lavender` | `#C9AAFF` | Marque et accents | Non réservé à une sémantique d'erreur |
| `mint` | `#9EE7C7` | Activité/indication positive | Accompagner d'un libellé |
| `peach` | `#FFC5AA` | Sélection / actions utilisateur | Couleur de focus |
| `pink` | `#F1A5C5` | Accents secondaires | Décoration, pas statut fonctionnel |

**Attention :** valeurs proposées à mesurer sur Ghostty et terminaux 256/16 couleurs ; aucune garantie de contraste n'est affirmée ici. Prévoir une palette de contraste renforcé et monochrome. En couleurs limitées, convertir en palette terminal disponible plutôt que supposer un rendu RGB.

**Règles de layout conservées :** grande largeur (environ ≥100 colonnes) = bandeau complet ; largeur plus étroite = labels abrégés ; minima déjà codés **60 colonnes × 20 lignes**. Toujours préserver les diagnostics/approbations et le composeur avant la décoration. La coloration syntaxique réutilise ces accents par famille mais doit être évaluée sur la palette ncurses réellement disponible.

## 2. Identité : logiciel et assistant

**ID-01 [P] — Marque produit :** bannière, onglets, À propos et documentation emploient « LOFITERM ».

**ID-02 [P] — Personnage conversationnel :** remplacer le préfixe `Agent: ` par `LOFITERM Bot: ` dans les réponses de l'assistant, avec éventuelle forme compacte `Bot:` si l'écran est étroit. Les messages utilisateur restent `Vous:` ou `You:` selon le catalogue de langue réellement adopté. Les notices techniques ne sont pas attribuées au bot.

**ID-03 [P] — Pas de confusion de rôle :** les textes « Agent worker », classe C++ `Agent`, outil de build, nom du modèle choisi (`LLM_MODEL`) et nom affiché du bot sont cinq notions différentes. Ne pas transformer `LOFITERM Bot` en nom de modèle ; ne pas prétendre que le logiciel possède un modèle maison.

**ID-04 [P] — Identité et erreurs :** le bot peut avoir un libellé de conversation, mais un échec `ToolFailed`, HTTP, sécurité ou streaming doit continuer à être indiqué par LOFITERM comme diagnostic technique, sans maquillage conversationnel.

**ID-05 [P] — Compatibilité :** pas de renommage immédiat de `agent`, `agent_core`, `agent_tui`, namespace, `Agent::send`, dépôt GitHub, variables d'environnement ni historiques Git.

**ID-06 [P] — À propos :** afficher nom public, version **réellement déterminée par build/tag**, commit si disponible, plateforme, lien du projet, emplacement de la documentation, licences exactes quand établies, et phrases transparentes : le serveur LLM peut être externe et le succès technique d'une réponse n'est pas une validation métier. Aucune version fabriquée, aucune URL contenant un secret.

> Recommandation d'orthographe : **LOFITERM Bot** dans l'interface, `lofiterm-bot` comme éventuel identifiant documentaire. La forme exacte reste un arbitrage cosmétique ; la distinction fonctionnelle est, elle, obligatoire.

## 3. Navigation générale et sous-menus

Conserver les six onglets sur le bandeau et le mode compact à largeur réduite. Ajouter **un niveau** de rubriques seulement dans F5 et F6. Aucun panneau flottant obligatoire ; utiliser le contenu existant, avec focus visible et retour prévisible.

```text
LOFITERM | LOCAL AI TERMINAL
F1 CHAT | F2 OUTILS | F3 ACTIVITÉ | F4 DIAGNOSTICS | F5 CONFIG | F6 AIDE

F5 CONFIG                               F6 AIDE
  • Modèle / connexion [lecture seule]    • Premiers pas
  • Sécurité / autorisations              • Clavier et souris
  • Préférences d'affichage*              • À propos / limites

F2 OUTILS
  • Historique des outils [existant]
  • Lire Markdown du workspace* [lecture seule]
```

`*` désigne une **nouvelle capacité proposée**, non présente à ce SHA. Pour F5, « Préférences d'affichage » peut commencer par un simple état d'affichage ; ne pas promettre une persistance non implémentée. `F2 Lire Markdown` n'est pas un explorateur de fichiers général.

**NAV-01 :** sélectionner une rubrique avec flèches/Tab puis Entrée ; Échap ferme d'abord la rubrique avant de revenir à Chat. Clic souris sur une rubrique = même action que sélection + Entrée, sans effet secondaire caché.

**NAV-02 :** la navigation et les changements de vues ne détruisent pas le brouillon du composeur, l'historique ou l'ancrage du scroll. Les événements outil continuent d'arriver sur le même worker pendant la lecture de l'aide.

**NAV-03 :** tous les textes doivent fonctionner à 60×20, avec possibilité de défiler et repli ASCII/monochrome. Aucun titre tronqué ne doit faire disparaître une alerte critique.

## 4. Aide : contenu utile et vérifiable

**AID-01 — Premiers pas (F6) :** trois actions maximum : (1) saisir dans Chat, (2) `F9` envoyer, (3) consulter Outils/Activité en cas de besoin. Expliquer que `Enter` insère une ligne, et que `F10` demande de quitter sans annuler forcément une requête active.

**AID-02 — Clavier/souris :** présenter exclusivement les touches implémentées, classées : navigation, saisie, lecture, sortie. Mettre en évidence le focus actif et la disponibilité conditionnelle de la souris.

**AID-03 — Sécurité et outils :** décrire brièvement `list_files`, `read_file`, `search_files`, `write_file`, les chemins limités au workspace, le refus d'écrasement et la confirmation explicite avant création. `PARTIAL` signifie détails incomplets et non succès garanti.

**AID-04 — À propos :** renvoyer au dépôt, à la version et aux limites, sans recopier toute la documentation dans la TUI.

**AID-05 — Aide contextuelle :** pied de page très court et exact selon le focus : `F9 Envoyer · Entrée Nouvelle ligne`, ou `Entrée Ouvrir · Échap Retour`, ou `PgUp/PgDn Défilement`. Priorité au diagnostic et aux confirmations avant toute aide.

**AID-06 — Langue :** les textes peuvent être définis en FR et EN, mais la bascule de langue et sa persistance restent hors périmètre minimal tant qu'elles ne sont pas décidées. Une interface partiellement traduite ne doit pas être vendue comme intégralement bilingue.

## 5. Raccourcis et conflits

| Commande | Existant ou proposé | Contrat |
|---|---|---|
| `F1` à `F6` | Existant | Ouvrir une des six vues |
| `F9` | Existant | Envoyer uniquement depuis Chat, une seule fois |
| `F10` | Existant | Demander fermeture ; signaler la requête active |
| `Tab` | Existant | Changer de zone/focus |
| `Échap` | Existant + adaptation | Fermer d'abord la sous-rubrique/visionneuse active ; sinon retour Chat |
| `↑`, `↓`, `←`, `→` | Existant + adaptation | Déplacer curseur ou sélection selon le focus, jamais de soumission implicite |
| `Page Up/Down` | Existant | Défilement de la vue / du document courant |
| `Enter` | Existant + adaptation | Nouvelle ligne dans Chat ; sélection d'une rubrique ailleurs |
| `F7`, `Ctrl+O`, `Ctrl+M` | Non retenus | Pas de nouvelle touche globale dans ce lot |

**KEY-01 :** aucune commande de rubrique ne doit intercepter le texte du composeur. **KEY-02 :** les états spéciaux d'approbation d'écriture gardent la priorité sur toutes les interactions ordinaires. **KEY-03 :** comportement stable si touches F non transmises par un terminal : navigation Tab/flèches documentée ; ne pas inventer de raccourci non supporté.

## 6. Souris — strict minimum

**MOU-01 [P] :** au démarrage TUI, demander les événements nécessaires via ncursesw `mousemask()` ; vérifier capacité du terminal ; ne pas dégrader le clavier en cas d'absence de souris.

**MOU-02 :** clic gauche sur onglet visible = changement de page ; clic dans la zone de saisie = focus du composeur ; clic sur entrée de sous-menu = sélection.

**MOU-03 :** molette vers le haut/bas = défilement du contenu actif ou de la visionneuse Markdown selon l'écran. Ne pas faire défiler le composeur à la place d'une réponse si le focus est sur le contenu.

**MOU-04 :** aucun clic ne peut constituer ou court-circuiter l'approbation d'écriture : le défi de confirmation et ses règles existantes restent exigés. Éviter toute prise en charge de copier/coller/sélection complexe ou glisser-déposer dans ce lot.

**MOU-05 :** coordonnées de clic contrôlées contre les limites écran et positions réelles après resize. Terminal non compatible = aucune action fantôme et fonctionnement clavier complet.

**MOU-06 :** restauration des modes terminal et du suivi souris à la sortie/exception ; aucune activation persistante dans Ghostty/SSH.

## 7. Rendu Markdown en conversation (F1)

**MD-01 [P] — Usage :** interpréter les réponses de **LOFITERM Bot** en Markdown de présentation dans la TUI, sans modifier le message source conservé par le moteur ni la CLI classique. Un message utilisateur peut rester en texte brut, hormis sa mise en page de sécurité.

**MD-02 — Sous-ensemble minimal :** titres `#`…`###`, paragraphes, listes simples, listes numérotées, citations, séparateurs, gras/italique, code inline et blocs délimités par triple backtick. Les liens sont représentés par un libellé et leur destination lisible ; pas d'ouverture automatique. Les tableaux Markdown ne doivent pas casser l'affichage : format simplifié ou texte brut explicite si trop large.

**MD-03 — Streaming :** analyser seulement une représentation sûre, incrémentale, avec état de clôture de bloc. Un ` ``` ` encore incomplet ne doit pas entraîner une mise en forme trompeuse ; le contenu peut rester provisoirement en texte brut puis être re-rendu à réception du fragment suivant. Si le tour échoue, marquer `PARTIAL` et conserver le contenu incomplet visible.

**MD-04 — Réversibilité :** bouton/rubrique `Texte brut` ou présentation alternative sans interprétation, pour vérifier les caractères réels et diagnostiquer un problème de rendu. Les données d'origine ne doivent jamais être détruites par le rendu.

**MD-05 — Limites :** HTML brut, SVG, images intégrées, scripts, ressources distantes, iframes, rendu web ou expressions actives sont exclus. Afficher le texte de repli ou un marqueur non exécuté.

**MD-06 — Performance :** ne pas re-parser sans borne toute la conversation à chaque fragment ; conserver budgets de taille/temps, tests de longue réponse, UTF-8 et redimensionnement. Les seuils exacts doivent être justifiés et validés, non inventés dans l'étude.

## 8. Coloration syntaxique des blocs de code

**SYN-01 [P] :** appliquer la coloration uniquement à l'intérieur des blocs ` ```lang ... ``` ` dans la réponse Markdown ou dans le document visionné. Préserver l'indentation, les espaces et les sauts de ligne ; aucun traitement syntaxique ne modifie la donnée.

**SYN-02 — Langages minimum proposés :** `cpp`/`c++`, `python`, `bash`/`sh`, `json`, `yaml` et `diff`. Langage inconnu ou absent = monospace/textuel sans coloration trompeuse. Ne pas promettre la couverture sémantique exhaustive des grammaires.

**SYN-03 — Catégories :** mots-clés, chaînes, nombres, commentaires et ponctuation/fonctions simples si détectables sûrement. Palette sobre dérivée des accents Peach Fog ; contraste testable ; chaque distinction reste lisible si `NO_COLOR` ou palette monochrome.

**SYN-04 — Ordre sûr :** parser la structure de texte en données, neutraliser les séquences terminal hostiles, calculer styles puis dessiner avec les attributs ncurses ; **ne jamais** concaténer de séquences ANSI fournies par le modèle ou des fichiers à une sortie terminal active.

**SYN-05 — Dégradation :** si parser/lexer échoue, afficher du texte brut et signaler au plus une note locale ; cela ne devient ni erreur HTTP ni échec d'outil. Sans coloration, le programme doit rester parfaitement utile.

## 9. Visionneuse Markdown locale (F2)

**VIEW-01 [P] — Objet :** une **visionneuse en lecture seule** pour fichiers `.md` du workspace déjà autorisé, accessible depuis une rubrique secondaire de F2 ; pas d'éditeur, d'explorateur système complet ni de rendu web. Elle réutilise le renderer Markdown de F1.

**VIEW-02 — Ouverture :** l'utilisateur indique explicitement un chemin **relatif au workspace**, par exemple `docs/notice.md`. Afficher le chemin relatif normalisé et l'origine workspace réelle seulement si déterminable sans divulgation sensible. Ne pas lancer un appel LLM pour ouvrir un document.

**VIEW-03 — Accès sûr :** réutiliser la logique sécurisée de lecture/validation de fichiers du projet (descripteurs et `O_NOFOLLOW`, interdiction de traversée hors workspace, liens physiques multiples selon contrat en vigueur, Landlock). N'introduire ni accès arbitraire à `/home`, ni élargissement Landlock, ni lecture par une deuxième voie moins sûre. Si la mutualisation de la lecture existante n'est pas disponible sans restructuration risquée, **reporter VIEW-01** au lieu de contourner la protection.

**VIEW-04 — Contenu :** limite de taille cohérente avec les restrictions actuelles de `read_file`, erreurs lisibles pour introuvable/autorisation/limite, document inchangé. Ni écriture ni ouverture automatique des liens/images, même si le document en contient.

**VIEW-05 — Navigation :** scroll molette ou Page Up/Down, Échap pour retour à F2 sans perte du brouillon Chat ; texte brut de secours. Les liens restent non activables par défaut.

**VIEW-06 — Séparation :** les lectures déclenchées par l'utilisateur dans la visionneuse ne doivent pas être présentées comme tool calls décidés par le modèle, ni incrémenter frauduleusement les métriques agentiques. Si leur journalisation est retenue, choisir un événement UI distinct, sans contenu sensible.

**Découpage de périmètre :** le **rendu Markdown en conversation + syntaxe** apporte le premier gain d'adoption. La **lecture de fichiers `.md`** peut être livrée dans le même lot si la réutilisation des protections est triviale ; sinon elle est **reportable** sans bloquer les autres fonctions.

## 10. Architecture cible sans refonte du moteur

```text
LLM / SSE → Agent::send → événements/deltas existants
                                  │
                          état conversation
                                  │ texte de base sécurisé
                       MarkdownPresenter [nouveau, présentation]
                           ├─ blocs/code → SyntaxHighlighter [nouveau, léger]
                           ├─ lignes et styles ncursesw
                           └─ mode Texte brut
                                  │
                      TuiView + TuiNavigation
                         ├─ Chat (bot nommé)
                         ├─ Outils / visionneuse .md (lecture sécurisée)
                         ├─ Activité / Diagnostics
                         ├─ Config / rubriques
                         └─ Aide / À propos
                          ↑                ↑
                      clavier          souris (ncursesw)

agent_core, Landlock, SSE, outils, approbation, REPL classique : conservés.
```

- **ARCH-01 :** ne pas introduire FTXUI ni un second event loop. ncursesw demeure seul propriétaire de l'écran.
- **ARCH-02 :** conserver des structures de rendu en données (`texte`, `style`, `largeur`, `ligne`), indépendantes du moteur, plutôt que d'injecter des séquences ANSI dans les strings.
- **ARCH-03 :** le moteur n'interprète pas Markdown pour modifier sa réponse ni ses tool calls ; seul le composant de présentation le fait.
- **ARCH-04 :** pas de nouveau thread, réseau, dépendance lourde ou persistance uniquement pour la coloration.
- **ARCH-05 :** conservez la logique `TerminalTextSanitizer` et les protections historiques ; la sanitisation du nouveau chemin rendu doit être vérifiée y compris sur fragments SSE et fichier externe.
- **ARCH-06 :** si la visionneuse accède au workspace, procéder à un audit du point de lecture et à un test négatif sur symlink/hardlink/concurrence. Toute impossibilité = NON INTÉGRABLE, pas de désactivation de Landlock.

**Fichiers probables à examiner à l'implémentation, sans prescription de refactor :** `src/ui/tui_view.cpp`, `include/ui/tui_view.hpp`, `src/ui/tui_navigation.cpp`, `src/ui/tui_event_loop.cpp`, `src/ui/terminal_session.cpp`, `src/ui/tui_view_data.cpp`, `src/workspace_tools.cpp`, `src/workspace_search.cpp`, `include/ui/*`, `tests/*_test.cpp`, `CMakeLists.txt`. Établir diff exact et API réutilisable avant toute modification.

## 11. Non-régression et critères d'acceptation

**Non-régression préexistante, reproduite ici pour rendre la spécification autonome :** matrice NR-01 à NR-35 issue de SPEC-LOFI-001 v1.0. Les entrées historiques restent des **cas à qualifier**, non des succès ; le collecteur et le thème sont fusionnés dans `main` depuis la rédaction initiale. Les deux intitulés F3 et l'indication du nombre de groupes CTest sont actualisés pour éviter les affirmations périmées.

| ID | Domaine / scénario | Attendu précis et oracle | Type | Gravité |
|---|---|---|---|---|
| NR-01 | CLI par défaut / `--ui=classic` | Flux texte intact, pas de séquences TUI, comportement argument inchangé | EXISTANT | Bloquant |
| NR-02 | `--ui=tui` en TTY | Six vues et navigation F1–F6 ; pas de crash | EXISTANT | Bloquant |
| NR-03 | Soumission `F9` | Un prompt soumis exactement une fois, contenu intact | EXISTANT | Bloquant |
| NR-04 | Enter en TUI | Nouvelle ligne selon contrat TUI ; absence d'envoi accidentel | EXISTANT | Bloquant |
| NR-05 | REPL `:multi` / `:end` | Un seul `Agent::send`, bloc exact avec CRLF normalisé, pas de LF final ajouté | EXISTANT | Bloquant |
| NR-06 | `exit`/EOF/ligne vide CLI | Respect exact REPL-02/07/08/09 | EXISTANT | Bloquant |
| NR-07 | SSE fragmenté et `[DONE]` | Ordre texte conservé ; aucune fin validée avant protocole complet | EXISTANT | Bloquant |
| NR-08 | `finish_reason=length` | Fin tronquée non présentée comme complète | EXISTANT | Bloquant |
| NR-09 | Coupure HTTP après deltas | Texte visible identifié comme partiel/invalide ; pas de faux succès | EXISTANT+NOUVEAU | Bloquant |
| NR-10 | Tool calls entrelacés | Appariement `tool_call_id`, nombre/ordre d'appels inchangés | EXISTANT | Bloquant |
| NR-11 | Outil en échec | Signalement clair même si le modèle répond ensuite | EXISTANT+NOUVEAU | Bloquant |
| NR-12 | `write_file` sur cible existante | Refus overwrite inchangé, aucun effet involontaire | EXISTANT | Bloquant |
| NR-13 | Workspace hors périmètre, liens | Landlock et protections nofollow/hardlink inchangés | EXISTANT | Bloquant |
| NR-14 | Données externes ANSI/OSC | Aucune commande terminal active injectée par le contenu | EXISTANT+NOUVEAU | Bloquant |
| NR-15 | Clé API, URL avec secret | Absence dans écran, erreurs et logs persistants par défaut | NOUVEAU | Bloquant |
| NR-16 | BoundedHistory 32/16384 | Évictions/rejets explicites et budgets inchangés | EXISTANT | Important |
| NR-17 | Scroll après éviction | Même contenu visible conservé quand il doit l'être ; correctif #33 non perdu | EXISTANT | Important |
| NR-18 | Statut PARTIAL | Visible après perte d'événements et sur thème clair/sombre/mono | EXISTANT+NOUVEAU | Bloquant |
| NR-19 | Resize 120×40 → 80×24 → 60×20 | Aucun débordement ni chevauchement de saisie ; brouillon intact | NOUVEAU | Bloquant |
| NR-20 | Terminal trop petit | Message/fallback défini, pas d'écriture hors écran | NOUVEAU | Important |
| NR-21 | Locale et largeur UTF-8 FR/EN | Accents, combinants et caractères larges sûrs ; menus lisibles | NOUVEAU | Important |
| NR-22 | TERM=dumb / NO_COLOR / redirection | Aucun contrôle ANSI inapproprié en sortie plain ; pas de couleurs forcées | EXISTANT+NOUVEAU | Bloquant |
| NR-23 | SSH et terminal 16/256 couleurs | Palette dégradée lisible, navigation possible | NOUVEAU | Important selon support publié |
| NR-24 | Changement de thème/langue | Ne change ni modèle ni endpoints ni permissions ; brouillon gardé | NOUVEAU | Bloquant |
| NR-25 | F3 mesures absentes ou incomplètes | `Indisponible`/`PARTIAL` si justifié, jamais de pourcentage fictif | NOUVEAU | Bloquant |
| NR-26 | F3 avec collecteur intégré | Source et unités des chiffres testables, horloge monotone | NOUVEAU conditionnel | Important |
| NR-27 | F4 état serveur | `Configuré` ≠ `Connecté`; aucun état réseau inventé | NOUVEAU | Important |
| NR-28 | F6 FR/EN | Toute touche documentée réellement active, limites exactes | NOUVEAU | Important |
| NR-29 | F10 pendant HTTP actif | Issue de fermeture fidèle, aucune fausse annulation promise | GLOBAL | Important |
| NR-30 | Charge/long transcript | Mesures RSS, fluidité et wrapping consignées ; pas de garantie sans mesure | GLOBAL | Important |
| NR-31 | Installation Linux propre | Build, exécution classic/TUI, configuration et dépendances reproduites | GLOBAL | Bloquant release |
| NR-32 | CTest au SHA de livraison | Tous groupes déclarés pour ce SHA exécutés et réussis (nombre déterminé sur le SHA de livraison, et non figé à l’ancienne base) | GLOBAL | Bloquant release |
| NR-33 | Serveur mock et LLM local réel | Échange, tool-call, échec, fin normale qualifiés avec preuves | GLOBAL | Bloquant release |
| NR-34 | Documentation | README/USAGE/aide décrivent comportement et SHA livrés, pas compteurs historiques | GLOBAL | Bloquant release |
| NR-35 | Décorations désactivées | Même capacité d'usage et états critiques sans motif, ticker ou emoji | NOUVEAU | Important |

| ID | Scénario nouveau | Résultat attendu | Gravité |
|---|---|---|---|
| UX-01 | Vue Chat, réponse assistant | Préfixe propre `LOFITERM Bot` ; message utilisateur distingué ; pas de renommage du modèle | Important |
| UX-02 | Version et À propos | Version/SHA exact ou `indisponible`, lien dépôt, limites réelles ; aucune clé affichée | Bloquant |
| UX-03 | Entrer/sortir d'une rubrique F5/F6 | Navigation réversible, Enter/Échap cohérents, brouillon conservé | Bloquant |
| UX-04 | F6 Premiers pas | Guide utilisable sans serveur LLM ni faux raccourci | Important |
| UX-05 | Raccourcis existants | F1–F6, F9, F10, Tab, Escape, flèches, PgUp/PgDn et Enter sans régression | Bloquant |
| UX-06 | Clic sur six onglets | Même écran qu'au clavier, sans clic hors zone actif | Important |
| UX-07 | Molette et focus | Défile la bonne zone ; pas d'envoi involontaire | Important |
| UX-08 | Terminal sans souris / souris désactivée | Fonctionnement 100 % clavier, restauration terminal | Bloquant |
| UX-09 | Souris + approbation écriture | Aucun clic ne remplace le défi explicite ; refus et délai restent sûrs | Bloquant |
| UX-10 | Réponse Markdown simple | Titre/liste/gras/code inline restitués lisiblement, texte source intact | Important |
| UX-11 | Code fences fragmentées SSE | Rendu provisoire puis final correct, aucun faux `COMPLETED`, pas de corruption | Bloquant |
| UX-12 | Markdown hostile avec ESC/OSC/HTML/image distante | Aucune commande terminal, requête HTTP ou ressource externe exécutée | Bloquant |
| UX-13 | Syntaxe C++, Python, Bash, JSON, YAML, diff | Styles lisibles ; fallback correct pour langage inconnu | Important |
| UX-14 | `NO_COLOR`, terminal 16 couleurs, resize 120×40→80×24→60×20 | Sémantique et navigation conservées ; aucune perte du brouillon | Bloquant |
| UX-15 | Long code/Markdown, grande conversation | Rendu et allocations bornés/mesurés ; pas de blocage UI prolongé | Important |
| UX-16 | Vue texte brut | Source initiale vérifiable, aucune suppression du contenu | Important |
| UX-17 | Ouvrir fichier `.md` sûr dans workspace | Lecture seule, sans envoi LLM, aucun effet sur métriques d'outil agentique | Important conditionnel |
| UX-18 | Chemin absolu, `..`, symlink, hardlink, fichier surdimensionné | Refus cohérent avec contrat workspace, aucune fuite hors périmètre | Bloquant si visionneuse livrée |
| UX-19 | Échec SSE après contenu Markdown affiché | `PARTIAL`/échec toujours explicite ; affichage décoratif n'efface pas l'erreur | Bloquant |
| UX-20 | Documentation et statut Git | Aide, README, USAGE, spécification, rapport et SHA sont alignés | Bloquant pour clôture |

**Règle de preuve :** SHA exact, OS/noyau, `TERM`, locale, taille de terminal, commande/harness, sortie attendue et réelle, verdict `PASS` / `FAIL` / `NOT RUN`; contrôle manuel nommé comme tel. Ne pas réutiliser 8 196 assertions ou d'anciens succès pour attester le nouvel écran Markdown. Aucun chiffre d'adoption ou de performance ne doit être inventé.

## 12. Mesures d'adoption, sans inflation fonctionnelle

Critères mesurables lors d'une petite recette utilisateurs (proposés, non résultats) :

1. **Découverte** : une personne qui ne connaît pas LOFITERM sait trouver les raccourcis et envoyer son premier message à partir de l'aide.
2. **Confiance** : elle peut identifier la version réelle, le modèle/endpoint configuré et comprendre qu'une écriture requiert son accord.
3. **Lisibilité** : elle sait lire une réponse Markdown contenant du code et distinguer réponse complète vs partielle.
4. **Accessibilité** : elle accomplit les mêmes actions sans souris.

**Test proposé :** 3–5 utilisateurs non impliqués dans le développement ; mesurer uniquement réussite/échec, temps et incompréhensions observées ; rapporter le nombre d'observations, sans prétendre démontrer une « probabilité d'adoption » statistique.

## 13. Priorisation et unique plan d'exécution produit

| Classe | Fonctions | Sortie attendue |
|---|---|---|
| **Indispensable au gain UX ciblé** | Nom du bot, À propos, aide et sous-rubriques simples, souris élémentaire, Markdown de chat, coloration de blocs, sécurité et non-régression | Nouvelle expérience fonctionnelle, compréhensible et testable |
| **Conditionnel / reportable** | Lecture directe de fichiers `.md`, si sécurisation ou coût d'intégration non trivial ; sélecteur visuel de langue ou thèmes | Pas de blocage de l'amélioration principale |
| **Exclu du lot** | Migration FTXUI, webview, navigateur de fichiers complet, rendu HTML, exécution de code, langage SQL à grammaire complète, thèmes animés, glisser-déposer, édition Markdown, nouveaux outils LLM, gestion SCCM | Aucune dérive du périmètre |

**Insertion dans l'unique roadmap de convergence existante :**

- **Définir :** rattacher cette version validée de SPEC-LOFI-001 aux travaux produit post-tag technique ; distinguer fonctions déjà présentes et proposées, nom exact du bot et contenu d'À propos ; ne pas déplacer `v1.0.0-technical`.
- **Implémenter et stabiliser :** mission intégrée UX, focalisée sur rendu/navigation/aide et sans modification du moteur ; tests UX-01…UX-20 applicables ; si visionneuse non sûre, la reporter explicitement.
- **Qualifier et livrer :** valider l'expérience sur Debian 13/Ghostty, terminal sans souris, `NO_COLOR`, resize, flux réel et mock ; rapport durable avec SHA et limites ; mettre à jour guide, aide, README et critères de packaging/release séparément.

**Règle d'arrêt :** si un scénario bloquant échoue, ne pas déclarer l'ensemble terminé. Ne pas lancer automatiquement une nouvelle sous-phase ; corriger le défaut précis ou réduire la promesse fonctionnelle après arbitrage.

## 14. Arborescence documentaire à mettre à jour dans le dépôt lors d'une exécution autorisée

| Destination | Nature du changement |
|---|---|
| `docs/specifications/` | Ajouter/réviser SPEC-LOFI-001 v1.1, en conservant l'historique v1.0 comme archive datée |
| `docs/roadmap/ROADMAP.md` | Rattacher la mission UX à la trajectoire produit existante ; aucune roadmap concurrente |
| `docs/README.md` | Indexer la spécification et le rapport durable |
| `README.md` | Présenter LOFITERM, version et raccourcis réellement livrés, dépendances effectives ; réparer toute phrase devenue historiquement fausse |
| `USAGE.md` | Aide utilisateurs : souris, saisie, Markdown, mode texte brut, accès `.md` uniquement si livré |
| `AGENTS.md` | Modifier seulement si une règle d'ingénierie/gouvernance approuvée change ; sinon aucune réécriture |
| `docs/reports/YYYY-MM-DD-...md` | Rapport durable : périmètre, SHA, vérifications, résultats, limites, verdict |

La copie externe sous `/mnt/data` ne vaut **ni commit, ni push, ni PR, ni modification de la roadmap du dépôt**. La documentation d'implémentation doit refléter le comportement réellement intégré, jamais une simple intention.

## 15. Contrôle de complétude et verdict

**Complet au niveau de l'étude** si : identité assistant/produit distincte, menus F5/F6 et aide définis, raccourcis non contradictoires, politique souris bornée, Markdown/code/document `.md` délimités, sécurité et tests couvrant les nouveaux chemins, et roadmap unique préservée.

**Non achevé au niveau du produit** tant que : cette spécification n'est pas intégrée/ratifiée dans le dépôt, que le code correspondant n'est pas développé et que la qualification au SHA effectif n'est pas exécutée et documentée.

**Verdict au 10/11/2026 :** **ÉTUDE UX v1.1 CONSOLIDÉE — PRÊTE POUR REVUE ; IMPLÉMENTATION NON EFFECTUÉE.**

---

### Sources et traçabilité

- Base documentaire utilisateur : `LOFITERM-v1.0-specification-fonctionnelle-graphique-et-convergence-2026-11-08.md` (SPEC-LOFI-001 v1.0, 327 lignes), `console-ui-architecture-requirements.md`, `repl-multiline-requirements.md`, `LOFITERM-SCCM-etude-actualisee-2026-11-10.md`.
- Dépôt courant vérifié par lecture GitHub : `https://github.com/mohamedbouhlel/local-agent-cpp`, `main` `46d653fe9e788c6410129b7224edd33d870af593` ; PR #45 (graphisme), #46 (tests).
- Points de code consultés : `src/ui/tui_view.cpp`, `src/ui/tui_navigation.cpp`, `src/ui/tui_event_loop.cpp`, `src/ui/terminal_session.cpp`, `src/ui/tui_application.cpp`, `src/ui/tui_view_data.cpp`, `CMakeLists.txt`, `README.md`, `docs/README.md`.
- Aucune modification du dépôt utilisateur, aucun test local, aucun écran généré ne prouvent l'implémentation des propositions.
