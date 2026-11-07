# Après TUI-4A — réconciliation de la roadmap et prochain incrément

**Date :** 2026-11-07
**Référence de code :** `main` au SHA `51ae20867357b1b873657585b70d813f7f328194`
**PR #30 :** fusionnée
**Nature :** analyse de l'arbre fusionné et proposition; aucune modification produit.

## Résumé de l'état réel

L'arbre `main` contient maintenant la TUI ncursesw de production, son worker asynchrone, la conversation et les six vues accessibles par navigation. La qualification automatisée rapportée est 214 cas/1 345 assertions, 12/12 CTest; le mainteneur rapporte également 20/20 contrôles manuels Ghostty. Les essais manuels ne sont pas reproduits indépendamment ici.

SPEC-UI-001 v2.0 place le shell/navigation/vues dans TUI-3, la conversation/compositeur dans TUI-4 et métriques/diagnostics dans TUI-5. La roadmap G avait sa propre proposition TUI-0 à TUI-8 et regroupait les six vues dans TUI-3. L'exécution réelle a livré l'infrastructure/conversation en TUI-3 puis navigation et six vues dans TUI-4A. Cette différence est documentée; les numéros historiques et la spécification normative ne sont pas modifiés.

## Matrice de traçabilité

| Fonctionnalité | Statut réel | SHA ou source de preuve | Limitation | Jalon restant |
|---|---|---|---|---|
| Backend ncursesw POC | POC réalisé avant produit | Rapports TUI-POC-1 à 3 dans l'arbre | Ghostty/SSH et différents terminaux ne sont pas universellement qualifiés | Aucun POC supplémentaire requis par les faits actuels |
| Worker et transport événementiel asynchrone | Fusionnés; la TUI utilise `AgentWorker` et `EventChannel` | `include/ui/agent_worker.hpp`, `src/ui/agent_worker.cpp` au SHA | Pas d'annulation de `Agent::send`; fermeture peut attendre; file bornée ne borne pas les objets déjà consommés | Résilience selon besoin/autorisation |
| Session ncursesw, restauration, TUI conversationnelle | Fusionnée | `src/ui/terminal_session.cpp`, `src/ui/tui_application.cpp`, `src/ui/tui_event_loop.cpp` | Qualification manuelle rapportée pour Ghostty seulement | Déjà livré |
| Six vues/navigation TUI-4A | Fusionnées | `TuiNavigation`, `TuiView`, `TuiViewData` au SHA; PR #30 | Vues Outils/Diagnostics sont des projections compactes | Enrichissement éventuel, à décider |
| Conversation, saisie, focus et scroll | Fonctionnels et tests PTY rapportés; Ghostty 20/20 rapporté | Rapports TUI-4A; SHA qualifié | Pas de plafond observé pour transcript et brouillon | Arbitrage séparé si rétention nécessaire |
| Vue Outils | Schémas déclarés et nom/statut/durée d'événements réels | `make_tui_view_data`, `TuiEventLoop::handle_event` | Pas d'arguments ni de résultats; vecteur d'activité non borné | Proposition: rétention bornée avec compte d'entrées omises |
| Vue Diagnostics | Catégories d'échec et avertissements/incohérences réellement ajoutés par l'UI | `TuiEventLoop`, `TuiView::add_diagnostic` | Pas de trace complète avec corrélation/horodatage; vecteur non borné | Même proposition de rétention, sans log persistant |
| Vue Configuration | Modèle, origine endpoint, timeout, présence de clé et outils déclarés | `TuiViewData` / `TuiApplication` | Clé masquée; workspace non transmis; pas d'édition | Pas de besoin manquant démontré |
| Vue Métriques | État indisponible explicite | `TuiView::rendered_page_rows`; aucun fichier MetricCollector dans l'arbre Git du SHA | Aucun agrégat de métriques connecté | Intégration séparée, selon TUI-5 de la spec |
| Journaux JSONL | Non intégrés | Aucun logger runtime TUI présent dans l'arbre | Canal, stockage, confinement et rétention non décidés | Futur jalon distinct si ratifié |

## Données et limites de conservation constatées

Les événements fournissent `RequestStarted`, débuts/fins HTTP, fragments de texte transitoires, noms d'outils, durées, catégories d'échec et bilan technique. Ils ne portent pas les arguments/résultats d'outils dans ce contrat. `TuiViewData` lit directement `AgentConfig` et `ToolRegistry::schemas_json()`; l'endpoint est projeté vers son origine et la clé est réduite à un booléen configuré/non configuré.

Dans le code fusionné, `messages_`, `tool_activity_` et `diagnostics_` sont des vecteurs alimentés sans plafond explicite; `input_` est une chaîne également sans plafond constaté. Les limites de `EventChannel` ne s'appliquent qu'à la file de transfert et ne constituent pas une limite RSS ou une limite des collections de la vue. Aucune mesure mémoire de session longue n'est disponible ici. C'est une propriété de code observée, pas un défaut quantifié.

La vue Métriques ne consomme pas les compteurs de `PresentationState` comme s'ils étaient un `MetricCollector`. Aucun nombre de deltas n'est présenté comme tokens; pas de tokens/s, durée estimée, pourcentage ou ETA fabriqués. Il n'y a pas de journalisation persistante de prompts, réponses, arguments ou résultats.

## Options de prochain incrément

### Option A — Proposition recommandée : rétention de l'activité Outils/Diagnostics

- **Objectif :** borner les deux collections d'activité tout en indiquant explicitement les anciennes entrées écartées.
- **Prérequis :** arbitrage sur plafonds en nombre d'entrées et/ou en octets, ordre d'éviction et wording du compteur.
- **Sources :** données déjà observées dans `ToolStarted`, `ToolSucceeded`, `ToolFailed`, `RequestFailed`, résultat worker et anomalies de présentation.
- **Modifications possibles :** composants de modèle/vue; tests; aucune modification de `Agent::send`, HTTP/SSE, Landlock ou outils.
- **Risques :** indicateur d'éviction trompeur s'il ne compte pas précisément; plafonds arbitraires; ne borne pas transcript ni saisie.
- **Tests :** avant/au-dessus du seuil, ordre des survivants, compte d'évictions exact, chaînes contrôlées, scrolling et navigation.
- **Critères :** seuls les plafonds décidés sont garantis; aucune omission silencieuse; les valeurs restent des événements effectivement reçus.

#### Profils de plafond candidats — à arbitrer

Les deux historiques auraient des limites configurées séparément, même si le mainteneur choisit le même profil pour chacun. Les valeurs ci-dessous sont des options de discussion, pas des valeurs calibrées :

| Profil | Entrées max par historique | Octets utiles max par historique | Argument disponible | Réserve |
|---|---:|---:|---|---|
| Compact | 32 | 16 KiB | Le test TUI-4A existant ajoute 20 diagnostics pour valider le scroll; 32 garde ce scénario et 12 entrées de marge | Cette fixture ne représente pas une distribution de sessions réelles |
| Intermédiaire | 128 | 64 KiB | Donne quatre fois le nombre du profil compact; 128 est aussi le nombre par défaut d'événements en transit dans `EventChannel` | La capacité d'une file de transit ne démontre pas le besoin de rétention visible |
| Long | 512 | 256 KiB | Donne seize fois le scénario de 32 entrées et conserve davantage de contexte de session | Aucun historique utilisateur ni mesure mémoire ne justifie ce volume; coût supérieur |

Les octets visent la somme des tailles `std::string::size()` conservées, pas le RSS : allocations, capacité réservée et métadonnées d'allocateur ne sont pas exactement couvertes par cette somme. Les limites en entrées et en octets doivent s'appliquer toutes deux, indépendamment à Outils et Diagnostics. Les tests devraient inclure une entrée unique dépassant le budget, une chaîne vide, dépassement simultané des deux seuils et vérification du compteur FIFO.

Pour préserver le statut technique du tour actif, proposition d'une petite synthèse fixe distincte de ces historiques : état courant et verdict terminal de tour/dernière catégorie d'erreur, remplacés aux transitions observées. Cela ne conserve pas tous les détails d'erreurs d'un tour si le ring FIFO les évince; le compteur d'évictions doit signaler cette perte. La formulation produit et la priorité entre statut actif et navigation restent à ratifier.

Au vu des seuls tests existants, **32/16 KiB est le plus petit profil qui couvre le scénario TUI-4A de 20 diagnostics**, tandis que 128/64 KiB et 512/256 KiB sont des alternatives de confort sans preuve d'optimalité. Aucune consommation réelle ou durée de session n'a été mesurée.

Cette option ne fournit pas de garantie de mémoire totale bornée. Une politique pour le transcript/compositeur est distincte et requiert une décision UX, car elle peut masquer des messages ou limiter la saisie.

### Option B — Intégration du collecteur, indépendante de l'enrichissement des vues

- **Objectif :** alimenter le panneau Métriques avec des mesures définies et disponibles.
- **Prérequis :** code `MetricCollector` examiné et intégré à la branche principale via un flux autorisé; contrat des champs et de l'indisponibilité approuvé.
- **Sources potentielles :** métadonnées `AgentEvent`, horloge monotone, octets HTTP observés. Les tokens ne sont pas disponibles par simple comptage des fragments.
- **Risques :** provenance des changements locaux UI-4, stabilité/sémantique des mesures, confusion entre succès technique et fonctionnel.
- **Tests :** horloge déterministe, plusieurs requêtes/outils, erreurs, premier delta absent, aucune valeur par défaut zéro ou estimation.
- **Critères :** chaque valeur publiée a une unité/source; métrique absente reste indisponible; aucun contenu sensible.

Cette option correspond aux sujets de TUI-5 dans SPEC-UI-001; elle n'est pas incluse dans l'option A et ne doit pas être récupérée implicitement depuis le checkout UI-4 dirty.

### Option C — Enrichissement des Diagnostics

- **Objectif :** rendre plus consultables les événements techniques déjà accessibles, avec corrélation minimale lorsque les données existent.
- **Prérequis :** décider si l'UI peut conserver les identifiants/horodatages et quelle redaction appliquer.
- **Risques :** transformer un panneau consultatif en journal, accumulation mémoire, fuite de données.
- **Tests :** chaque entrée correspond à un événement reçu, ordre/corrélation, redaction, limites de conservation.
- **Critères :** aucune inférence, aucune donnée brute du prompt/outils, omissions signalées.

Option C peut être regroupée avec A uniquement si le mainteneur approuve un contrat commun de conservation. Aucun choix n'est ratifié par ce rapport.

## Verdict

TUI-4A est intégrée et qualifiée dans les limites rapportées. Le plus petit incrément recommandé est une politique explicite de rétention pour les deux listes d'activité auxiliaires, mais ses plafonds et son libellé d'éviction nécessitent l'arbitrage du mainteneur. L'intégration MetricCollector est séparée et dépend d'un code examiné et fusionné; le checkout local UI-4 n'est pas une preuve d'intégration. La roadmap G et SPEC v2.0 restent des traces historiques jusqu'à une ratification documentaire distincte.
