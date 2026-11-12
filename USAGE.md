# Guide d'utilisation — LOFITERM (agent-cpp)

Ce guide suppose que le projet est déjà compilé. Pour l'installation, les
dépendances système et le build, voir [README.md](README.md) — ce document
détaille l'usage et la sécurité, sans reprendre les prérequis.

## 1. Vue d'ensemble

`agent-cpp` est un agent IA minimal en C++ sous Linux avec Landlock : il lit une question au clavier,
l'envoie à un serveur LLM local compatible OpenAI
(`POST /v1/chat/completions`, en streaming SSE) et affiche la réponse au fur et
à mesure. Le modèle
peut aussi appeler automatiquement quatre outils sandboxés pour lire et
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
| `LLM_CONNECT_TIMEOUT_SECONDS` | non | `10` | Durée maximale d'établissement de la connexion. Entier strictement positif. |
| `LLM_IDLE_TIMEOUT_SECONDS` | non | `60` | Durée maximale sans données de réponse. Les en-têtes et octets du corps reçus relancent ce délai. |
| `LLM_TOTAL_TIMEOUT_SECONDS` | non | `300` | Durée maximale totale d'une requête, même si le flux reste actif. Entier strictement positif. |
| `LLM_TIMEOUT_SECONDS` | non | `60` | Compatibilité : valeur de repli pour l'inactivité de l'exécutable; les anciens clients C++ gardent leur plafond global historique. |

Aucune variable n'est obligatoire. Les trois délais sont indépendants : 10 s
pour la connexion, 60 s sans données de réponse, et 300 s au total. Un flux qui
continue à transmettre des données peut ainsi dépasser 60 s, tout en restant
borné par le délai total. Les valeurs vides sont traitées comme absentes; une
valeur invalide ou non positive avertit sur `stderr` et utilise le fallback :

```
Invalid LLM_IDLE_TIMEOUT_SECONDS value, using 60 seconds.
```

`LLM_TIMEOUT_SECONDS` reste accepté comme fallback d'inactivité pour
l'exécutable. Les clients C++ utilisant seulement `AgentConfig::timeout_seconds`
ou les anciennes surcharges `HttpClient` conservent leur sémantique historique
de plafond total. Une configuration d'exécutable qui ne définit que l'ancienne
variable reçoit désormais un plafond total de 300 s; définir
`LLM_TOTAL_TIMEOUT_SECONDS` pour une autre borne.

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
  section 5). L'invite est `>`. La réponse s'affiche **progressivement**, au
  fil des fragments reçus du serveur (streaming SSE), et non d'un bloc à la
  fin.
- **Pendant un cycle d'outils** : quand le modèle décide d'appeler un outil,
  une ligne `[appel de <nom d'outil>...]` s'affiche pendant l'exécution, avant
  que la réponse finale ne continue à s'écrire. C'est un simple indicateur de
  progression (le silence serait sinon déroutant).
- **Quitter** : tapez `exit` puis Entrée. La fin de flux (`Ctrl-D`) termine
  aussi la boucle.
- **Ligne vide** : ignorée, l'invite `>` réapparaît sans appel réseau.
- L'historique de conversation est conservé d'un tour à l'autre (voir
  section 6 pour la troncature).

### Saisie multiligne

Le REPL accepte également un bloc de plusieurs lignes : la **ligne exacte**
`:multi` bascule en mode multiligne, puis la **ligne exacte** `:end`
termine le bloc et provoque un seul envoi au modèle.

```text
> :multi
... Analyse les éléments suivants :
... première ligne
... seconde ligne
... :end
```

Les espaces, lignes vides et tabulations sont conservés ; dans le bloc,
une fin de ligne CRLF est normalisée en LF, sans saut de ligne final artificiel.
Un bloc de **zéro ligne** n'est pas envoyé, mais un bloc comportant des lignes
vides est transmis. `exit` à l'intérieur du bloc est du texte ; en mode
simple, seul `exit` exact quitte le REPL. Un EOF/Ctrl-D avant `:end` abandonne
le bloc **sans envoi** et termine la boucle. La ligne `:end` en mode simple est
une entrée ordinaire. En mode multiligne, une ligne littérale égale à `:end`
ne peut pas être incluse dans le bloc (limite approuvée).

La source normative est la
[spécification REPL-01 à REPL-09](docs/specifications/repl-multiline-requirements.md) ;
l'implémentation se trouve dans `src/prompt_reader.cpp` et `src/main.cpp`.

## 4.1 Interface LOFITERM ncursesw

La CLI reste le mode par défaut. Lancez explicitement `./build/agent --ui=tui`
dans un terminal interactif; `--ui=classic` sélectionne la CLI. F1 à F6
sélectionnent Conversation, Outils, Activité, Diagnostics, Configuration et
Aide. Tab change le focus, Échap ferme d'abord une rubrique ouverte puis
revient à Conversation, F9 envoie depuis Conversation et F10 demande la
fermeture. Enter ajoute une ligne au composeur lorsqu'il est focalisé. Une
navigation ou un clic ne soumet jamais le texte et ne peut jamais approuver
une écriture.

Les onglets sélectionnent une vue à la souris si le terminal signale son
support. Un clic dans le composeur lui donne le focus; la molette fait défiler
le contenu actif. Toute action reste disponible au clavier.

F5 Configuration et F6 Aide ont des rubriques sélectionnables. Utilisez les
flèches puis Enter, ou cliquez la rubrique; Échap revient à la liste. La vue
À propos n'affiche une version ou un commit que s'ils sont compilés dans le
binaire; sinon elle indique qu'ils sont indisponibles. La configuration est en
lecture seule et la clé API n'est jamais affichée.

Dans Conversation, les réponses assistant sont présentées avec un sous-ensemble
Markdown. `M` bascule entre cette présentation et le texte brut lorsque le
focus est sur le contenu de Conversation. Les liens restent du texte; aucun
HTML, code ou URL n'est exécuté ou ouvert. Les blocs de code reconnus sont
colorés de manière légère pour C++, Python, Bash, JSON, YAML et diff; les
autres langages restent lisibles en style code. Un bloc clôturé n'est formaté
qu'après réception de son délimiteur fermant; un bloc encore ouvert reste
littéral pendant le streaming. `NO_COLOR` désactive les couleurs sans retirer
les libellés d'état. Le rendu exact dépend de la locale et du terminal.

Les outils ne sont **pas** invocables manuellement depuis le REPL : c'est le
LLM qui décide de les appeler pendant qu'il compose sa réponse.

`write_file` prépare et vérifie le contenu avant publication. Toute création
requiert une approbation explicite pour cette demande précise; aucun accord
n'est mémorisé pour une création ultérieure. Le contrôle intervient avant la
création des répertoires parents. Sans décision valide, aucun fichier ni
répertoire parent demandé n'est créé. Sous Linux,
une création part d'un fichier temporaire anonyme
(`O_TMPFILE`), publié atomiquement sans écrasement avec `linkat`; le système
de fichiers doit prendre en charge ce mécanisme. Un échec d'ouverture est
signalé, sans repli vers un fichier temporaire nommé. L'écrasement est
temporairement désactivé jusqu'à ce que l'accès au workspace utilise une
identité système isolée. L'écriture n'est pas garantie durable après une
coupure de courant ou un crash (pas de `fsync`).

## 5. Outils disponibles

Les quatre outils sont enregistrés au démarrage (`register_workspace_tools()`
dans `src/workspace_tools.cpp`) et exposés au modèle via le champ `tools` du
payload. Les chemins absolus, les composants `..` et les liens symboliques
sortants constatés lors de la validation sont rejetés. Cette validation et
les accès ultérieurs sont distincts : une modification concurrente des
chemins ou des liens symboliques peut contourner cette vérification. Ne pas
utiliser ce mécanisme comme frontière de sécurité face à un processus pouvant
modifier le workspace simultanément.

Sous Linux, le processus active également un confinement filesystem Landlock
avant le REPL. Dans le workspace, il autorise la lecture des fichiers et
répertoires, l'écriture et la suppression de fichiers et la création de
fichiers réguliers/répertoires. Il refuse l'exécution de fichiers et la
création de liens symboliques ou de fichiers spéciaux. En dehors,
la lecture est autorisée sous `/etc/ssl/certs` et
`/usr/share/ca-certificates`, ainsi que sur `/etc/ssl/openssl.cnf`,
`/etc/hosts`, `/etc/resolv.conf` et `/etc/nsswitch.conf` lorsqu'ils existent.
`/etc/ssl/private` reste refusé. Les autres
accès filesystem, notamment sous `/usr`, `/lib` et `/lib64`, sont refusés après
le démarrage du processus (ses bibliothèques sont chargées avant l'application
du ruleset). Si Landlock est absent ou ne peut pas être
activé, l'agent refuse de démarrer. Sur macOS et les autres systèmes, le
binaire refuse aussi le démarrage plutôt que de fonctionner sans cette
barrière. Landlock ne confine que le processus agent, pas les autres processus
du même compte. `read_file` et `search_files` refusent les fichiers dont le
compteur de liens physiques indique plusieurs noms. Un autre processus du même
compte qui modifie le workspace simultanément reste hors de la protection de
Landlock. Landlock ne contrôle pas les accès réseau ; la connexion au serveur
LLM reste nécessaire au fonctionnement de l'agent.
Le chemin `workspace/` lui-même doit être un répertoire réel : s'il est un
lien symbolique, le démarrage est refusé. Le descripteur utilisé pour ajouter
la règle Landlock est ouvert avec `O_NOFOLLOW` et `O_DIRECTORY`.

Une fuite de `search_files` par remplacement concurrent d'un lien a été
reproduite puis corrigée pour ce scénario : la recherche ouvre les entrées
relativement à des descripteurs de répertoire et ne suit aucun lien
symbolique. `read_file` ouvre aussi les composants du chemin résolu sans
suivre de liens et lit depuis le même descripteur. `list_files` parcourt
également les répertoires par descripteurs et exclut les liens symboliques.
`write_file` utilise des descripteurs de répertoire et publie les créations
depuis un inode temporaire anonyme, sans nom temporaire substituable. Un
processus du même compte qui peut modifier le workspace reste hors de la
barrière Landlock et peut modifier les fichiers indépendamment de l'agent; le
workspace n'est donc pas une frontière de sécurité contre ce processus.

**Liens physiques — historique et état courant à distinguer :** un essai
historique a montré qu'un montage isolé du workspace sous `bwrap` ne
supprime pas l'accès à un fichier externe déjà lié physiquement au
workspace ([rapport du 4 novembre](docs/reports/2026-11-04-isolation-systeme.md)).
Ce constat ne signifie **pas** que les outils de lecture actuels acceptent
ces fichiers : `read_file` et `search_files` rejettent désormais les fichiers
dont `st_nlink != 1` (`src/workspace_tools.cpp`,
`src/workspace_search.cpp`). Cela ne garantit pas une isolation complète
contre les manipulations concurrentes d'autres processus du même compte.

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
  listés (pas les répertoires ni les liens symboliques). Pas de plafond de nombre de résultats codé
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

### write_file

- **Ce qu'il fait** : crée un fichier texte dans le workspace à partir d'un
  chemin relatif, en créant au besoin les répertoires parents manquants. Le
  fichier doit ne pas déjà exister; modifier le contenu d'un fichier existant
  n'est pas pris en charge. Retourne une confirmation du type
  `wrote 42 bytes to notes/a.txt (created new file)`.
- **Quand** : quand le modèle demande de produire un fichier (notes, code,
  rapport) dans le workspace. La demande ne suffit pas à déclencher l'écriture :
  l'utilisateur doit examiner la demande et confirmer explicitement chaque
  création.
- **Demande affichée** : workspace effectif, chemin cible relatif, taille en
  octets, répertoires parents manquants et contenu complet. Le contenu est
  représenté en ASCII réversible; les caractères de contrôle et retours à la
  ligne sont échappés pour éviter leur interprétation par le terminal. Dans la
  TUI, le contenu peut être parcouru; vérifiez-en toute la plage avant de
  décider. Sur un terminal trop petit pour garder l'inspection et le défi
  visibles, l'approbation est indisponible et la création est refusée.
- **Confirmer dans la CLI classique** : après l'affichage intégral de la
  demande, saisir exactement `yes ` suivi des 32 chiffres hexadécimaux du défi
  affiché, puis Entrée. `yes` seul, une réponse différente, une entrée déjà en
  attente, une entrée vide ou une entrée non interactive ne donne pas
  l'approbation. EOF annule la demande. Une demande expire au plus tard après
  cinq minutes.
- **Confirmer dans la TUI** : saisir dans le dialogue le texte exact
  `yes <défi>` affiché, puis Entrée. Échap annule et F10 ferme l'application
  en annulant la demande. Toute réponse non exacte, dont `no`,
  ne refuse qu'après validation par Entrée; taper `n` seul laisse le dialogue
  ouvert. Le dialogue permet de parcourir le contenu avec les flèches et
  PageUp/PageDown. En dessous de 6 lignes ou 48
  colonnes, l'approbation est indisponible; agrandir le terminal ou refuser.
- **Résultat** : seul un défi courant correctement confirmé autorise le writer.
  L'écriture n'est annoncée comme réussie qu'après son exécution effective.
  Refus, annulation, expiration, fermeture, erreur d'interface ou absence de
  terminal utilisable ne créent pas le fichier. L'approbation ne contourne ni
  la validation du chemin, ni Landlock, ni le refus d'écrasement.
- **Écrasement désactivé** : le schéma de l'outil n'expose plus le paramètre
  `overwrite`. Tout appel direct avec `overwrite: true` est refusé. Si la cible
  existe déjà, l'outil renvoie au modèle :

  ```
  tool error: file already exists: <chemin> (overwriting is disabled until workspace access uses an isolated system identity)
  ```

  Le refus est temporaire, jusqu'à une architecture qui isole l'identité
  système ayant accès au workspace. Aucun fichier existant n'est modifié par
  l'outil.
- **Contrainte** : `path` et `content` sont obligatoires (`path` non vide,
  relatif, sans `..`). Si le chemin résolu est un répertoire existant,
  l'écriture est refusée (`cannot write file: path is an existing
  directory: <chemin>`). Le contenu est plafonné à **200 000 caractères** :
  au-delà, l'écriture est **entièrement rejetée** (pas de troncature, qui
  corromprait le fichier) avec l'invitation à écrire en plusieurs fois —
  `content is <N> characters, exceeding the maximum write size of 200000;
  write the file in smaller parts instead`.
- **Sandbox** : `write_file` valide le chemin et rejette les chemins absolus
  et `..`. Les parents sont ouverts sans suivre de lien. Les déplacements
  concurrents de répertoires et substitutions du temporaire restent une
  limite de sécurité.
- **Publication atomique** : le contenu est écrit dans un inode temporaire
  anonyme, puis publié sans écrasement par `linkat()`. Un chemin déjà existant
  est refusé.

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
- **Taille max d'écriture** : `max_write_chars` = **200 000 caractères** par
  appel à `write_file`. Au-delà, l'écriture est rejetée en bloc (pas de
  troncature) pour ne pas produire un fichier corrompu.

## 7. Dépannage

### Échec du confinement au démarrage

Si Landlock est indisponible, si les règles ne peuvent pas être activées ou si
la racine `workspace/` est un lien symbolique, le programme refuse de démarrer
avec `Filesystem sandbox setup failed: ...`. Vérifier les prérequis Linux
et le répertoire depuis lequel l'agent est lancé. Aucun mode de démarrage
sans confinement n'est fourni par l'application courante.

### Réponse SSE incomplète

Une réponse sans marqueur `[DONE]`, une réponse terminée avec
`finish_reason="length"` ou un appel d'outil invalide déclenche un
`AgentError`, signalé dans la CLI par `Reponse incomplete ou invalide du modele`.
Le texte éventuellement affiché progressivement avant l'erreur ne doit pas être
interprété comme une réponse validée.

### Serveur LLM local injoignable

Si `curl` ne parvient pas à joindre l'endpoint (serveur éteint, mauvaise
URL, port fermé), le message suivant est affiché sur `stderr` et le REPL
continue :

```
Impossible de contacter LLM_BASE_URL (<url configurée>): HTTP POST failed: <détail>
```

Vérifiez que le serveur tourne et que `LLM_BASE_URL` pointe vers le bon
endpoint (`/v1/chat/completions`).

Une réponse HTTP hors plage 2xx déclenche `HttpError` et est signalée
par la branche « Impossible de contacter LLM_BASE_URL ».
Une réponse JSON inattendue ou un dépassement de la limite d'itérations
d'outils peuvent déclencher le message générique :

```
Reponse du serveur inattendue: <détail>
```

Le dépassement de `max_tool_iterations` produit notamment
`tool call loop exceeded maximum iterations`.

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
