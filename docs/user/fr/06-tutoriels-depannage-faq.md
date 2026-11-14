# Tutoriels, dépannage et FAQ

🇫🇷 Français · [🇬🇧 English](../en/06-tutorials-troubleshooting-faq.md) · [🇪🇸 Español](../es/06-tutoriales-solucion-problemas-faq.md)

## Tutoriel : poser une question sur un fichier

1. Placez une copie non sensible du fichier dans le workspace effectif.
2. Demandez au modèle de lire ce chemin relatif et de résumer un point précis.
3. Vérifiez le chemin mentionné si le modèle appelle `read_file`.
4. Examinez la réponse et contrôlez-la par rapport au fichier original.

La limite de lecture est 50 000 octets; les fichiers binaires ne sont pas pris en charge par cet outil.

## Tutoriel : chercher une expression

Placez les fichiers de texte pertinents dans le workspace, puis demandez une recherche littérale en indiquant l'expression. `search_files` renvoie au plus 50 lignes correspondantes; pour un grand corpus, réduisez le workspace ou la requête.

## Tutoriel : créer un nouveau fichier

Demandez un seul fichier avec un chemin relatif et décrivez le contenu souhaité. Lorsque l'écran d'approbation apparaît, comparez la cible et chaque ligne du contenu avec votre demande. Refusez si la cible existe déjà ou si l'écran présente autre chose. N'utilisez jamais un défi antérieur.

## Dépannage

| Symptôme | Vérification |
|---|---|
| `Could not contact LLM_BASE_URL` ou échec HTTP | Vérifiez l'URL complète, le port, le serveur, le modèle et l'authentification. Ne collez pas de secret dans les journaux partagés. |
| Départ puis échec de flux | Vérifiez les journaux du serveur; un flux SSE doit se terminer par `[DONE]`, sans erreur de protocole ni réponse tronquée. |
| `write_file` indisponible en CLI | L'approbation exige stdin et stderr interactifs. Les pipes et l'absence de TTY échouent en mode fermé. |
| Fichiers introuvables | Vérifiez le workspace effectif : répertoire courant pour une compilation source, emplacement XDG pour le paquet. |
| TUI illisible ou touches manquantes | Utilisez un terminal interactif UTF-8 qui prend en charge ncursesw; essayez une taille plus grande. Les capacités souris dépendent du terminal. |
| Démarrage refusé | Vérifiez la prise en charge Landlock du noyau et les restrictions du conteneur. LOFITERM ne démarre pas en mode non confiné. |

## FAQ

**LOFITERM est-il le nom du binaire ?** Non. Le nom public est LOFITERM; une compilation des sources produit `agent`, tandis que le paquet local installe la commande `lofiterm`.

**Les outils peuvent-ils exécuter une commande shell ?** Non, aucun outil shell n'est déclaré.

**Puis-je demander d'écraser un fichier ?** Non. `overwrite:true` et les cibles déjà existantes sont refusés.

**F3 affiche-t-il les tokens par seconde ?** Non. Le programme affiche des octets, des trames SSE, des fragments texte et des durées effectivement observés, sans les assimiler à des tokens.

**Les conversations sont-elles mémorisées après fermeture ?** L'historique conversationnel réside dans le processus et n'est pas restauré au prochain lancement. Les fichiers écrits dans le workspace restent sur disque.

**Le paquet peut-il être redistribué ?**

Le code original de LOFITERM est distribué sous licence 0BSD, qui autorise sa redistribution. Le dépôt contient une recette de paquet Debian 13 amd64 qualifiée localement, mais ne publie actuellement ni release officielle ni dépôt APT public. Toute redistribution binaire doit respecter les licences et notices des dépendances tierces et faire l'objet d'une qualification adaptée.

Pour les langues et les autres guides, ouvrir [l'index documentaire](../../README.md).
