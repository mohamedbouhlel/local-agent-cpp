# Outils workspace

🇫🇷 Français · [🇬🇧 English](../en/04-workspace-tools.md) · [🇪🇸 Español](../es/04-herramientas-workspace.md)

L'agent déclare quatre fonctions au serveur LLM. Le modèle décide de les appeler au cours d'un tour; elles ne sont pas des commandes shell disponibles dans le REPL.

| Outil | Usage | Limites visibles dans le code |
|---|---|---|
| `list_files` | Lister récursivement les fichiers non cachés. | Pas de plafond d'entrées codé pour la sortie. Gardez un workspace ciblé. |
| `read_file` | Lire un fichier texte par chemin relatif. | Au plus 50 000 octets lus; fichiers binaires et avec plusieurs liens physiques refusés. |
| `search_files` | Chercher une chaîne littérale dans les fichiers texte. | Au plus 50 lignes correspondantes; aucune limite de taille pour une ligne individuelle. |
| `write_file` | Créer un nouveau fichier texte et ses parents manquants. | Contenu plafonné à 200 000 octets; consentement explicite à chaque appel; cible existante et `overwrite:true` refusés. |

Les chemins doivent être relatifs au workspace. Les chemins absolus et les composants `..` sont refusés. Le parcours ignore les noms cachés et évite les liens symboliques; la concurrence d'un autre processus du même compte reste une limite (voir [sécurité](05-securite-limites.md)).

## Autoriser une création

La demande présente le workspace effectif, la cible relative, le nombre d'octets, les parents à créer et le contenu intégral échappé de manière réversible. Dans la CLI, la réponse doit être exactement `yes <défi affiché>` suivie d'Entrée. Dans la TUI, le dialogue affiche la même demande et attend le défi complet.

Avant d'approuver :

1. Vérifiez que le chemin et le workspace sont ceux attendus.
2. Lisez le contenu présenté, y compris les caractères de contrôle affichés sous forme échappée.
3. N'approuvez que la demande exacte. Chaque appel reçoit un défi différent.
4. Refusez ou annulez en cas de doute. Une absence de décision, EOF ou expiration échoue en mode fermé.

Le writer ne crée les répertoires parents qu'après approbation. Une approbation autorise une seule création; elle ne permet ni une autre cible, ni un autre contenu, ni un écrasement.

## Exemple de demande sûre

Une consigne au modèle peut nommer un chemin relatif précis, limiter les fichiers attendus et préciser le contenu. Cela aide à contrôler le résultat mais ne remplace pas l'inspection : le modèle peut interpréter une instruction de façon inattendue. Refusez toute demande qui dépasse le périmètre annoncé.

L'historique d'activité visible dans F2 et les métriques F3 ne sont pas des journaux complets garantis; une perte d'événements peut rendre les détails partiels. Ils n'affichent pas les arguments ou résultats sensibles comme historique des outils.
