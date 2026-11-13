# Sécurité et limites

🇫🇷 Français · [🇬🇧 English](../en/05-security-and-limits.md) · [🇪🇸 Español](../es/05-seguridad-y-limites.md)

## Confinement Linux

Avant le REPL, le programme active Landlock et refuse le démarrage si le noyau ou les permissions ne le permettent pas. Dans le workspace, le processus reçoit les accès nécessaires à la lecture, l'écriture, la création et la suppression prévus par le ruleset; l'exécution est refusée. Quelques chemins de certificats et de résolution système sont accessibles en lecture pour TLS/réseau. Voir la politique normative d'[`AGENTS.md`](../../../AGENTS.md).

Landlock limite **ce processus**; il ne cloisonne pas un autre processus du même UID. Un processus concurrent du même utilisateur peut modifier les fichiers du workspace. Les protections du programme ne constituent donc pas une frontière contre un compte local hostile ou un processus pair.

## Écriture avec consentement

Chaque création `write_file` nécessite une confirmation exacte par défi, après présentation du contenu complet. Refus, indisponibilité, expiration, fin d'entrée ou fermeture annulent l'opération. L'écrasement est interdit. La publication Linux utilise un fichier temporaire anonyme et une publication sans écrasement lorsque le système de fichiers le permet; il n'y a pas de repli silencieux vers une création moins sûre. L'absence de `fsync` signifie qu'une création confirmée n'est pas garantie durable après panne ou perte d'alimentation.

L'approbation n'efface pas les risques d'une décision utilisateur mal informée. Inspectez le chemin, les parents et le contenu dans la boîte de dialogue. N'approuvez pas sur la seule demande du modèle.

## Données et confidentialité

- La clé API est lue depuis l'environnement ou la configuration privée du paquet et n'est pas affichée dans F5.
- Les métriques retiennent des compteurs et des identifiants bornés; elles ne conservent pas le texte de conversation, les arguments ni les résultats d'outils comme historique de métriques.
- Le contenu transmis au serveur LLM est soumis aux règles de ce serveur et à son réseau; un modèle local ne signifie pas automatiquement absence de journalisation côté serveur.
- L'historique de conversation et le workspace source persistent respectivement en mémoire du processus et sur disque selon l'emplacement choisi; LOFITERM ne fournit pas de mémoire persistante inter-session du modèle.
- Le dépôt ne déclare pas de licence de redistribution. Le paquet Debian est une recette d'installation locale, non une autorisation de distribution publique.

## Limites opérationnelles établies

La lecture est bornée à 50 000 octets; la recherche renvoie 50 lignes, mais une longue ligne peut encore produire une grande sortie. `list_files` n'a pas de limite de nombre d'entrées dans le code actuel. Le contexte est limité en nombre de messages et tronqué par tours complets, pas selon un tokenizer ou un budget de tokens. Un tour important peut dépasser la limite de référence.

Les garanties Landlock, le refus d'écrasement et le consentement par demande réduisent le risque, mais ne prouvent ni la justesse des réponses du LLM, ni la sécurité du serveur, ni l'absence de concurrence filesystem. Consultez les [rapports datés](../../README.md#reports) pour connaître les limites de qualification et leurs SHA.
