# Interface classique et REPL

🇫🇷 Français · [🇬🇧 English](../en/02-classic-interface.md) · [🇪🇸 Español](../es/02-interfaz-clasica.md)

L'interface classique lit les lignes depuis le terminal, envoie les messages au serveur configuré et écrit la réponse progressive sur la sortie standard. Les diagnostics sont séparés sur `stderr`.

## Lancer

Depuis le répertoire contenant `build/agent` :

```sh
./build/agent --ui=classic
```

Sans option, le mode classique est sélectionné. Pour le paquet, utilisez `lofiterm --ui=classic`. Le workspace est relatif au cwd avec une compilation source; le paquet choisit le workspace XDG décrit dans le [guide de démarrage](01-demarrage-installation.md).

## Échanges simples

- Saisissez une ligne puis Entrée pour envoyer une requête.
- Une ligne vide est ignorée.
- La réponse est affichée au fil des fragments reçus.
- Tapez exactement `exit` puis Entrée pour quitter; Ctrl-D ferme également l'entrée.
- Une erreur HTTP, SSE ou agent est signalée comme échec. Les données partielles ne sont pas présentées comme une réponse finale validée.

## Messages multiligne

Utilisez les marqueurs exacts :

```text
> :multi
... Première consigne.
...
... Dernière ligne.
... :end
```

Le bloc complet est envoyé en un seul tour. Les espaces, tabulations et lignes vides sont conservés; CRLF est normalisé en LF. Aucun saut de ligne final supplémentaire n'est ajouté. Un bloc vide n'est pas envoyé. EOF avant `:end` abandonne le bloc sans soumission. `exit` dans le bloc est du contenu. La ligne exacte `:end` ne peut donc pas être incluse comme contenu.

## Appels d'outils

Le modèle peut décider d'appeler les outils workspace pendant une réponse. La CLI affiche l'indicateur d'outil puis le résultat résumé; vous ne pouvez pas invoquer un outil en entrant une commande dédiée. `write_file` suspend la réponse pour présenter une demande d'autorisation. Inspectez soigneusement chemin et contenu avant de taper le défi exact.

## Sorties et scripts

La sortie standard contient la réponse destinée à l'utilisateur; les messages d'erreur et diagnostics sont dirigés vers `stderr`. La CLI n'est pas un format JSON ni une API stable. Pour automatiser des tests, utilisez les composants C++ et les tests mock du dépôt plutôt que de parser le texte d'interface.

Voir aussi : [outils workspace](04-outils-workspace.md), [sécurité](05-securite-limites.md) et [FAQ](06-tutoriels-depannage-faq.md).
