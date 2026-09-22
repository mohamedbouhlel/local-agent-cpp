# Guide d'utilisation — agent-cpp

Ce guide suppose que le projet est déjà compilé. Pour l'installation, les
dépendances système et le build, voir [README.md](README.md) — ce document
ne les duplique pas.

## 1. Vue d'ensemble

`agent-cpp` est un agent IA minimal en C++ : il lit une question au clavier,
l'envoie à un serveur LLM local compatible OpenAI
(`POST /v1/chat/completions`, non streamé) et affiche la réponse. Le modèle
peut aussi appeler automatiquement trois outils sandboxés pour lire et
explorer les fichiers du dossier `workspace/`.

## 2. Configuration

La configuration se fait par variables d'environnement, lues via
`std::getenv()` au démarrage (`load_config()` dans `src/main.cpp`). Il n'y a
aucune valeur en dur ni parsing de fichier `.env` côté C++ : c'est le shell
qui charge le fichier.

```
cp .env.example .env
```

Puis éditez `.env` si besoin.

| Variable | Obligatoire | Défaut | Rôle |
| --- | --- | --- | --- |
| `LLM_BASE_URL` | non | `http://localhost:8080/v1/chat/completions` | Endpoint OpenAI-compatible appelé en POST. |
| `LLM_MODEL` | non | `local-model` | Nom du modèle envoyé dans le champ `model` du payload. |
| `LLM_API_KEY` | non | *(vide)* | Si non vide, ajoute l'en-tête `Authorization: Bearer <clé>`. Ignoré si vide. |
| `LLM_TIMEOUT_SECONDS` | non | `60` | Timeout des requêtes HTTP, en secondes. Doit être un entier strictement positif. |

Aucune variable n'est obligatoire : sans `.env`, l'agent démarre avec les
défauts ci-dessus. Une valeur vide est traitée comme absente et remplacée par
le défaut. Si `LLM_TIMEOUT_SECONDS` n'est pas un entier positif, un
avertissement est affiché sur `stderr` et la valeur `60` est utilisée :

```
Invalid LLM_TIMEOUT_SECONDS value, using 60 seconds.
```

## 3. Lancer l'agent

Rappel des commandes (détail complet dans [README.md](README.md)) :

```
cmake -B build
cmake --build build
```

Chargez ensuite `.env` dans le shell courant, puis lancez le binaire depuis
la racine du projet :

```
set -a; source .env; set +a
./build/agent
```

`set -a` exporte automatiquement les variables définies par `.env` pour
qu'elles soient visibles par `std::getenv()`. Si vous lancez le binaire sans
charger `.env`, les valeurs par défaut du tableau ci-dessus s'appliquent.

Au démarrage, l'agent affiche :

```
agent-cpp ready. Type 'exit' to quit.
```

## 4. Utiliser le REPL

L'agent fonctionne en boucle lecture → envoi → affichage :

- **Poser une question** : tapez votre texte puis Entrée. L'agent l'envoie au
  serveur LLM et affiche la réponse (ou déclenche des appels d'outils, voir
  section 5). L'invite est `>`.
- **Quitter** : tapez `exit` puis Entrée. La fin de flux (`Ctrl-D`) termine
  aussi la boucle.
- **Ligne vide** : ignorée, l'invite `>` réapparaît sans appel réseau.
- L'historique de conversation est conservé d'un tour à l'autre (voir
  section 6 pour la troncature).

Les outils ne sont **pas** invocables manuellement depuis le REPL : c'est le
LLM qui décide de les appeler pendant qu'il compose sa réponse.

## 5. Outils disponibles

Les trois outils sont enregistrés au démarrage (`register_workspace_tools()`
dans `src/workspace_tools.cpp`) et exposés au modèle via le champ `tools` du
payload. **Tous sont strictement sandboxés dans `workspace/`** : les chemins
absolus, les `..` et toute tentative de sortie du dossier sont rejetés, donc
aucun accès en dehors de ce dossier n'est possible.

Le dossier `workspace/` est résolu relativement au répertoire de lancement
(`std::filesystem::absolute("workspace")`) et créé automatiquement s'il
n'existe pas. En lançant `./build/agent` depuis la racine du projet, il
s'agit donc de `workspace/` à la racine. Pour analyser des fichiers, il
suffit de les déposer dans ce dossier.

### list_files

- **Ce qu'il fait** : liste récursivement tous les fichiers réguliers du
  workspace, en chemins relatifs (un par ligne). Les chemins cachés — tout
  chemin dont un composant commence par `.` — sont exclus.
- **Quand** : quand le modèle a besoin de savoir quels fichiers existent
  avant de lire ou chercher.
- **Contrainte** : aucun argument. Seuls les fichiers réguliers sont
  listés (pas les répertoires). Pas de plafond de nombre de résultats codé
  pour `list_files` lui-même.

### read_file

- **Ce qu'il fait** : lit un fichier texte du workspace par chemin relatif et
  renvoie son contenu.
- **Quand** : quand le modèle veut inspecter le contenu d'un fichier repéré
  via `list_files` ou `search_files`.
- **Contrainte** : argument `path` obligatoire (chaîne non vide, relative,
  sans `..`). Le fichier doit exister et être un fichier régulier. Les
  fichiers binaires sont rejetés (détection d'un octet NUL dans les 4096
  premiers octets). La lecture est plafonnée à **50 000 caractères** ; au
  delà, le contenu est tronqué et suivi du marqueur :

  ```
  [truncated: file exceeds 50000 characters]
  ```

### search_files

- **Ce qu'il fait** : recherche une chaîne **littérale** (pas de regex) dans
  les fichiers texte du workspace, ligne par ligne, et renvoie les
  occurrences au format `chemin:numéro_de_ligne:contenu`. La recherche est
  sensible à la casse.
- **Quand** : quand le modèle cherche où un symbole, un mot-clé ou un texte
  apparaît sans savoir dans quel fichier.
- **Contrainte** : argument `query` obligatoire (chaîne non vide). Les
  fichiers cachés et binaires sont ignorés. Le nombre de résultats est
  plafonné à **50** ; si la limite est atteinte, la sortie se termine par :

  ```
  [truncated: reached 50 search results]
  ```

## 6. Limites connues

- **Historique borné** : l'historique est limité à `max_history_messages`
  = **20 messages** (constante dans `src/agent.cpp`). La troncature retire
  les tours les plus anciens, mais toujours **par tours complets** : un tour
  commence sur un message `role=user` et inclut ses appels d'outils et leurs
  résultats, afin de ne jamais casser l'appairage `assistant (tool_calls)`
  → `tool (tool_call_id)` attendu par l'API.
- **Tour surdimensionné non tronqué** : `trim_history()` ne retire jamais le
  dernier tour utilisateur, même s'il dépasse à lui seul les 20 messages. Un
  échange qui enchaîne beaucoup de `tool_calls` peut donc **temporairement
  dépasser** ce cap. C'est un compromis assumé, pas un bug ; c'est aussi plus
  probable si `max_tool_iterations` est augmenté (voir ci-dessous).
- **Plafond d'itérations d'outils** : `max_tool_iterations` = **5** appels
  successifs au modèle par échange utilisateur (constante dans
  `src/agent.cpp`). Si le modèle réclame encore des outils après la 5e
  itération, l'agent lève une erreur `tool call loop exceeded maximum
  iterations`, affichée dans le REPL (voir section 7).
- **Taille max de lecture** : `max_read_chars` = **50 000 caractères** par
  appel à `read_file`.
- **Plafond de résultats de recherche** : `max_search_results` = **50**
  occurrences par appel à `search_files`.

## 7. Dépannage

### Serveur LLM local injoignable

Si `curl` ne parvient pas à joindre l'endpoint (serveur éteint, mauvaise
URL, port fermé), le message suivant est affiché sur `stderr` et le REPL
continue :

```
Impossible de contacter LLM_BASE_URL (<url configurée>): HTTP POST failed: <détail>
```

Vérifiez que le serveur tourne et que `LLM_BASE_URL` pointe vers le bon
endpoint (`/v1/chat/completions`).

Une réponse HTTP hors plage 2xx, ou une réponse dont le JSON est
inattendu, produit un message générique :

```
Reponse du serveur inattendue: <détail>
```

Le même message générique apparaît si la boucle d'outils dépasse
`max_tool_iterations` (`tool call loop exceeded maximum iterations`).

### Fichier binaire demandé via read_file

`read_file` rejette les fichiers binaires (octet NUL dans les 4096 premiers
octets). L'erreur n'interrompt pas le REPL : elle est renvoyée **au modèle**
comme résultat d'outil, sous la forme :

```
tool error: cannot read binary file: <chemin relatif>
```

Le modèle peut alors le signaler dans sa réponse. Les autres erreurs
d'outil (fichier inexistant, chemin absolu, `..`, sortie du sandbox)
suivent le même mécanisme et apparaissent préfixées par `tool error: ` —
par exemple `tool error: file does not exist` ou
`tool error: absolute workspace paths are not allowed`.
