# Manuel utilisateur — LOFITERM

🇫🇷 Français · [🇬🇧 English](USAGE.en.md) · [🇪🇸 Español](USAGE.es.md)

Ce manuel décrit le comportement vérifié dans le code de la branche documentaire au SHA `39646ffade1c71ece9995fde7770dcf428e45d3a`. Les essais manuels et la qualification d'autres SHA sont archivés séparément et ne sont pas présentés comme rejoués ici.

## 1. Choisir l'interface

Depuis une compilation des sources, définissez le répertoire du dépôt puis lancez depuis un workspace utilisateur dédié :

```sh
cd /chemin/vers/local-agent-cpp
repo_dir=$PWD
mkdir -p "$HOME/lofiterm-workspace"
cd "$HOME/lofiterm-workspace"
"$repo_dir/build/agent" --ui=classic
"$repo_dir/build/agent" --ui=tui
```

Sans option, le mode classique est lancé. Le binaire compilé s'appelle `agent`; LOFITERM est le nom public de l'interface.

Avec le paquet local Debian 13, la commande est `lofiterm --ui=classic` ou `lofiterm --ui=tui`. Son lanceur place le processus dans un workspace utilisateur XDG; le comportement est différent du `workspace/` relatif utilisé par une compilation source. Voir [Installation Debian 13](docs/INSTALL_DEBIAN13.md).

## 2. Configuration

Le binaire compilé depuis les sources lit l'environnement avec `std::getenv()`. Il ne lit pas `.env` lui-même.

```sh
repo_dir=$PWD
cp .env.example .env
chmod 600 .env
# Modifier .env sans partager sa clé.
set -a; . ./.env; set +a
mkdir -p "$HOME/lofiterm-workspace"
cd "$HOME/lofiterm-workspace"
"$repo_dir/build/agent" --ui=tui
```

Depuis les sources, le workspace est `workspace/` sous le répertoire courant du processus. Lancez donc l'application depuis ce répertoire utilisateur pour retrouver le même workspace.

| Variable | Valeur par défaut dans l'exécutable | Rôle |
|---|---:|---|
| `LLM_BASE_URL` | `http://localhost:8080/v1/chat/completions` | Endpoint HTTP Chat Completions compatible OpenAI. |
| `LLM_MODEL` | `local-model` | Identifiant transmis au serveur. |
| `LLM_API_KEY` | vide | Si renseignée, envoyée dans l'en-tête Bearer. |
| `LLM_CONNECT_TIMEOUT_SECONDS` | `min(LLM_TIMEOUT_SECONDS, 10)`; normalement 10 | Limite d'établissement de connexion. |
| `LLM_IDLE_TIMEOUT_SECONDS` | valeur de `LLM_TIMEOUT_SECONDS`, normalement 60 | Délai sans en-tête ni octet de réponse; une donnée reçue le relance. |
| `LLM_TOTAL_TIMEOUT_SECONDS` | `300` | Limite dure de durée totale de la requête. |
| `LLM_TIMEOUT_SECONDS` | `60` | Variable historique, fallback du délai d'inactivité dans l'exécutable. |

Les valeurs doivent être des entiers positifs. Une valeur invalide déclenche un avertissement sur `stderr` et le fallback documenté dans l'exécutable. Les délais de connexion, d'inactivité et total ont des rôles distincts. Un flux actif peut dépasser 60 secondes, mais reste borné par le délai total.

**Paquet Debian :** sa configuration utilisateur est un fichier `NAME=VALUE`, sans syntaxe shell, expansion ni guillemets, par défaut `~/.config/lofiterm/config`. Le fichier d'exemple installé expose `LLM_BASE_URL`, `LLM_MODEL`, `LLM_API_KEY` et `LLM_TIMEOUT_SECONDS`. Les variables de l'environnement prévalent. Voir le [guide configuration](docs/user/fr/01-demarrage-installation.md#configuration-du-serveur-llm).

## 3. Interface classique et REPL

Le mode classique affiche l'invite `>`, envoie une ligne non vide après Entrée et montre la réponse progressivement. `exit` exact quitte le REPL; Ctrl-D termine également l'entrée. Une ligne vide ne déclenche pas de requête.

Pour envoyer un bloc :

```text
> :multi
... Résume les fichiers de notes.
... Conserve les noms de fichiers dans la réponse.
... :end
```

`:multi` et `:end` doivent constituer la ligne entière. Le bloc est transmis une seule fois; lignes vides, espaces et tabulations sont conservés, avec CRLF normalisé en LF. Aucun saut de ligne final artificiel n'est ajouté. Un bloc sans lignes n'est pas envoyé; un bloc contenant des lignes vides l'est. EOF avant `:end` abandonne le bloc sans envoi. Dans le bloc, `exit` est du texte.

## 4. TUI LOFITERM

Lancez la TUI depuis un terminal interactif. Les six vues sont sélectionnées par F1–F6 :

| Touche | Vue | Informations |
|---|---|---|
| F1 | Conversation | Transcript, streaming et composeur multiligne. |
| F2 | Outils | Outils déclarés et activité réellement observée. |
| F3 | Activité | Mesures HTTP, SSE, fragments de texte et outils du tour courant/dernier. |
| F4 | Diagnostics | Erreurs et diagnostics observés. |
| F5 | Configuration | Valeurs disponibles en lecture seule; clé API masquée. |
| F6 | Aide | Démarrage, clavier, limites et informations de version disponibles. |

Raccourcis : F9 envoie une seule fois depuis Conversation; F10 demande la fermeture. Tab change le focus; Échap revient à Conversation ou ferme une rubrique; flèches et Page Up/Page Down naviguent selon le focus. Entrée insère une nouvelle ligne lorsque le composeur est focalisé. `M` bascule Markdown/texte brut lorsque le contenu de Conversation est focalisé. Les clics sur onglets/rubriques et la molette sont disponibles si le terminal émet ces événements. Sélectionner du texte peut nécessiter Maj + glisser selon le terminal.

La vue F3 n'affiche pas un débit de tokens. `Text fragments` désigne des fragments de delta, pas des tokens; `SSE data frames` et octets HTTP sont d'autres compteurs distincts. `PARTIAL` indique que des détails d'événements sont incomplets; ce n'est pas une preuve de réussite de la tâche.

## 5. Outils workspace et approbation

Le modèle peut appeler les outils suivants :

- `list_files` : énumère récursivement les fichiers non cachés; la sortie n'a pas de plafond de nombre d'entrées dans le code actuel.
- `read_file` : lit un fichier texte relatif au workspace, jusqu'à 50 000 octets; les fichiers binaires et à plusieurs liens physiques sont refusés.
- `search_files` : recherche une chaîne littérale et renvoie au plus 50 lignes correspondantes. Une ligne individuelle n'a pas de plafond de longueur. Les fichiers cachés, spéciaux, symboliques et à plusieurs liens physiques sont ignorés/refusés selon le parcours.
- `write_file` : crée un fichier absent après approbation explicite. Le contenu est limité à 200 000 octets; l'écrasement est interdit.

Les outils sont appelés par le modèle dans une conversation; ils ne sont pas des commandes manuelles de la CLI. Les opérations de lecture ne demandent pas d'approbation.

Avant une création, l'interface montre le workspace, le chemin relatif, la taille, les répertoires parents manquants et le contenu intégral sous une forme réversible échappée. Dans la CLI, la confirmation complète est `yes <défi courant>` puis Entrée; toute autre ligne refuse. Dans la TUI, saisissez le même défi dans le dialogue. Chaque défi est propre à la demande. Ne confirmez que si le chemin et le contenu correspondent à votre intention. Refus, EOF, annulation, expiration ou indisponibilité n'autorisent aucune écriture. Les répertoires parents ne sont créés qu'après accord.

## 6. Confinement et limites

Sous Linux, Landlock est activé avant l'interface. Sans support ou permission suffisante, le démarrage échoue. L'accès du processus est limité au workspace et aux chemins système de lecture nécessaires documentés dans [AGENTS.md](AGENTS.md). Cette barrière n'isole pas le processus des autres processus du même utilisateur. Une modification concurrente du workspace par le même UID reste hors de la garantie Landlock. L'écriture ne promet pas la durabilité après crash (pas de `fsync`).

Le contexte est en mémoire pendant le processus. L'historique retire les anciens tours complets au-delà de sa limite de référence de 20 messages, sans résumé; un tour actif volumineux peut dépasser cette limite. Aucune mémoire persistante inter-session ni cache applicatif durable n'est fournie.

## 7. Erreurs et dépannage

- **Connexion impossible :** vérifier `LLM_BASE_URL`, la disponibilité du serveur et les délais. Le code affiche une erreur de transport; ne copiez pas de clé API dans un ticket.
- **Réponse coupée ou SSE incomplet :** l'absence de `[DONE]`, un JSON SSE invalide ou `finish_reason=length` échoue le tour; le texte provisoire peut rester marqué incomplet, sans faux message assistant final validé.
- **Landlock refusé :** vérifier le noyau et les politiques d'exécution du conteneur; l'application ne bascule pas en mode non confiné.
- **Outil sans résultat attendu :** vérifier le chemin relatif et le workspace effectif. Depuis les sources, le workspace dépend du répertoire de lancement; avec le paquet, il est dans les données XDG.
- **Création refusée :** ce résultat signifie qu'aucun accord valide n'a été donné; il ne faut pas répéter une ancienne confirmation. Un nouveau défi est émis pour une nouvelle demande.

Voir [Dépannage et FAQ](docs/user/fr/06-tutoriels-depannage-faq.md). Pour les guides par langue, consulter [l'index documentaire](docs/README.md).
