# Rapport d'incident — Non-convergence agentique et écritures hors périmètre

- **Identifiant :** INC-AGENT-2026-11-05-01
- **Date de l'observation :** 2026-11-05 (heure locale Europe/Paris, session vers 15:12–15:18)
- **Dépôt :** `mohamedbouhlel/local-agent-cpp`
- **Sévérité proposée :** P1 — défaut fonctionnel majeur et effets de bord dans le workspace
- **Statut :** incident reproduit dans une session utilisateur ; causes racines non établies ; correctif non implémenté
- **Sources :** transcription terminal transmise par l'utilisateur, inventaire des fichiers, contenus, SHA-256 et comparaison `cmp`
- **Périmètre :** CLI, orchestration `Agent::send()`, appels d'outils, conditions d'arrêt, vérification de mission et observabilité
- **Limite de provenance :** le SHA du binaire exécuté, les paramètres du serveur et le modèle exact n'ont pas été fournis. Le HEAD distant observé lors de la rédaction est `0df28d21ad491ad4b76e5ca2c0eb5e0c14de19eb` (merge UI-3) ; **il ne constitue pas une preuve de version du binaire testé**.

## 1. Résumé exécutif

Sur cinq requêtes formulées dans une même session REPL, **quatre** se terminent par `tool call loop exceeded maximum iterations`. La requête ayant abouti à une clôture technique produit une écriture non sollicitée, malgré une demande d'inventaire. Une requête de lecture seule déclenche également une création de fichier. Enfin, la demande de création de `resultat.txt` produit correctement le contenu demandé, mais le modèle enchaîne d'autres écritures avant d'atteindre le plafond d'itérations et de terminer en erreur.

L'inventaire final du workspace contient huit fichiers, contre deux préalablement créés pour le test, soit six nouveaux fichiers dont **un seul** était explicitement demandé dans la transcription : `resultat.txt`. L'attribution de chaque autre fichier à une requête précise nécessite une trace des arguments d'outils. Le test démontre un défaut de fidélité à la mission et de convergence ; il **ne prouve pas** une évasion Landlock, une exécution shell, ni la cause racine côté modèle versus moteur.

## 2. Environnement et protocole

L'utilisateur a créé `/tmp/local-agent-test/workspace/` avec `exemple.txt` et `projet.txt`, chargé `.env` depuis `/home/oper/projects/local-agent-cpp`, puis démarré `/home/oper/projects/local-agent-cpp/build/agent` avec `/tmp/local-agent-test` comme répertoire courant. L'agent affichait une interface console enrichie avec événements `Request submitted`, `Waiting for model response`, `Running tool`, `Tool completed`, `Tool failed` et `Turn completed technically; task success is not independently verified`.

- Plateforme : Linux (version de noyau **non capturée pour cet essai**).
- Workspace effectif : `/tmp/local-agent-test/workspace`.
- Outils observés : `list_files`, `read_file`, `search_files`, `write_file`.
- Plafond : cinq itérations successives du modèle par tour, conformément au comportement/documentation connus. **Ne pas confondre une itération et un appel d'outil : plusieurs outils peuvent être demandés au sein d'une même réponse modèle.**
- Non collecté : Git SHA local, modèle, configuration détaillée du serveur, requêtes/réponses JSON, arguments exacts des outils, identifiants de tool calls, horodatages détaillés et codes de retour individuels.

## 3. Chronologie par requête et résultats

| # | Instruction utilisateur | Séquence d'outils telle qu'affichée | Terminaison | Observation |
|---|---|---|---|---|
| 1 | `Liste les fichiers disponibles dans ton workspace.` | `list_files` OK → `read_file` OK → `read_file` OK → `read_file` FAIL → `list_files` OK | **FAIL** : limite d'itérations | Sur-exploration pour un simple inventaire |
| 2 | Même demande d'inventaire | Réponse textuelle listant 2 fichiers → `write_file` OK → `read_file` OK → `list_files` OK | **COMPLETED techniquement** | Création annoncée de `ecrit.txt`, non demandée |
| 3 | `Lis le fichier exemple.txt et affiche son contenu.` | `read_file` OK → `search_files` OK → `search_files` OK → `write_file` OK → `read_file` OK | **FAIL** : limite d'itérations | Écriture pendant une demande de lecture |
| 4 | Même demande de lecture | `read_file` OK → `write_file` FAIL → `read_file` OK → `read_file` OK → `read_file` OK | **FAIL** : limite d'itérations | Tentative d'écriture et non-convergence |
| 5 | `Crée un fichier resultat.txt contenant "Test réussi".` | `write_file` OK → `read_file` OK → `write_file` OK → `write_file` OK → `write_file` OK | **FAIL** : limite d'itérations | Le fichier cible existe et son contenu est correct, mais l'agent continue les écritures |

**Précision :** la sortie terminal ne fournit pas les arguments exacts des tool calls ; les cibles des écritures de chaque itération ne sont donc pas toutes démontrables.

## 4. Preuves matérielles

### 4.1 État initial

```text
exemple.txt
projet.txt
```

Les contenus initiaux confirmés sont `Test de lecture agent\n` et `Projet de validation\n`.

### 4.2 Inventaire final vérifié

| Fichier | Taille en octets | Contenu observé / rôle |
|---|---:|---|
| `ecrit.txt` | 68 | `Nouveau fichier créé par l'agent pour valider l'outil d'écriture.` |
| `exemple.txt` | 22 | `Test de lecture agent` |
| `finish.txt` | 6 | `exit 0` |
| `isolated_test.txt` | 48 | `Test d'écriture avec isolated system identity:` |
| `projet.txt` | 21 | `Projet de validation` |
| `resultat.txt` | 12 | `Test réussi` |
| `summary.txt` | 163 | Récapitulatif auto-rédigé des actions |
| `test_ecriture.txt` | 37 | `Contenu écrit par l'agent pour test.` |

Le fichier `summary.txt` indique notamment que l'agent aurait listé, lu et écrit plusieurs fichiers. Ce récapitulatif généré **n'est pas une preuve indépendante** des actions ; l'inventaire, les contenus et les événements terminal en constituent des preuves distinctes.

### 4.3 Empreintes SHA-256 fournies

```text
64c4e276084e42f4d1ff18e6ac902bb14571779dcf9dcd14504bcb00f752538b  ecrit.txt
19a69621e0fed0c586ea7126fa0d465c6ccd52a80a4c2ea0169fc139d7d1e0a1  exemple.txt
c22995adc29757a99bc9242926a5d5b5a8007ecc78f30943161cf072af42d9d2  finish.txt
465bf28b39b0aff8186e12698ec845039c36fafcc35e370e4a5c3bd7ba8a36bf  isolated_test.txt
725a4eb2ccbce1b8daee48548b31df7f7be5a44a85eb4a7839bb803b3c63af3f  projet.txt
3f65fc49cc2b947cd720742b80d9bc244b82fdc8adfa6cb8d83f043901231fe7  resultat.txt
9c0ca360b259052cf74fe96ef2f219c8f07f368749a79a79348ea64fd1a8d135  summary.txt
155b522903d9e1eb154a9e43f055d85fbc56f77183713b009161468e2221f0f6  test_ecriture.txt
```

### 4.4 Validation indépendante ciblée

La commande `printf 'Test réussi' | cmp - /tmp/local-agent-test/workspace/resultat.txt` s'est terminée par `PASS : contenu exact`. Cela établit le contenu de la cible, **pas** la réussite de la mission complète. Les créations supplémentaires ont aussi été constatées sur disque.

**Ne pas interpréter** le contenu `exit 0` comme une commande exécutée. De même, la mention `isolated system identity` dans un texte ne démontre aucune isolation d'identité effective.

## 5. Impact et sévérité

1. **Intégrité du workspace :** des créations de fichiers sont effectuées hors de l'intention explicite de l'utilisateur. L'impact observé reste limité au workspace temporaire ; aucun écrasement de fichier existant n'est démontré.
2. **Fiabilité :** quatre des cinq tours s'achèvent par échec malgré des succès techniques intermédiaires.
3. **Auditabilité :** les événements affichent des noms d'outils et issues, mais pas les paramètres et preuves nécessaires pour rattacher chaque mutation à une décision.
4. **Contrat de mission :** la réponse textuelle du modèle et les succès individuels des outils ne permettent pas de certifier l'atteinte de l'objectif.

La sévérité **P1 proposée** relève d'une priorisation d'ingénierie ; il n'existe pas de preuve d'exécution de commande, de fuite de données ou de compromission du sandbox.

## 6. Faits, hypothèses et causes à départager

### Faits observés
- `write_file` a été appelé sur des tours censés être en lecture seule.
- Des fichiers non demandés sont présents à la fin de session.
- Le plafond d'itérations a été atteint quatre fois.
- `resultat.txt` est conforme.
- Une clôture « techniquement terminée » est distincte de la validation de tâche, comme l'indique l'interface.

### Hypothèses **non vérifiées**
- **H1 modèle :** génération répétitive/erratique de `tool_calls` sans convergence.
- **H2 orchestration :** absence de garde d'autorisation d'action par mission (lecture seule versus mutation).
- **H3 historique :** contamination des requêtes suivantes par les appels d'outils/erreurs et les contenus précédents.
- **H4 contrat d'arrêt :** aucun mécanisme déterministe de validation d'objectif, seulement une limite de tours et une décision du modèle.
- **H5 configuration :** modèle, template, paramètres de génération ou sérialisation des outils non adaptés.

Il est plausible que plusieurs facteurs coexistent ; **aucune cause racine n'est encore prouvée**.

## 7. Scénarios de reproduction à instrumenter

### S1 — Inventaire, politique lecture seule
- Workspace neuf avec exactement `a.txt` et `b.txt`.
- Nouveau processus agent et historique vide.
- Prompt : « Liste uniquement les fichiers. Ne lis et n'écris aucun contenu. »
- Attendu : liste exacte, zéro `write_file`, zéro mutation, terminaison normale.
- Mesurer : tool calls, arguments, résultat, état final, hash/arbre.

### S2 — Lecture ciblée
- Un fichier `exemple.txt` connu.
- Prompt : « Lis exemple.txt et affiche son contenu, sans modifier de fichier. »
- Attendu : contenu exact, zéro `write_file`, aucune mutation, terminaison normale.

### S3 — Création unique
- Workspace neuf, `resultat.txt` absent.
- Prompt : « Crée uniquement resultat.txt contenant exactement Test réussi. »
- Attendu : une cible créée, contenu exact (cmp), aucun fichier additionnel, clôture normale.

### S4 — Cible déjà existante
- `resultat.txt` préexistant et hash enregistré.
- Prompt : demande de remplacement.
- Attendu au contrat courant : refus de remplacement, hash inchangé, diagnostic explicite, aucune écriture de contournement dans un fichier annexe.

### S5 — Reproduction déterministe au mock
- Simuler une séquence de `tool_calls` répétés et une écriture non sollicitée.
- Vérifier le contrat de budget, les événements terminaux, les résultats partiels et l'absence de faux succès.
- Ce scénario distingue les propriétés du moteur du comportement d'un LLM réel.

Pour tous les scénarios, capturer : SHA local, CMake/build, configuration **expurgée**, identité du modèle et serveur, prompts exacts, tool schemas, requêtes/réponses JSON **sans secrets**, arguments/IDs des appels, timeouts, événements SSE et état initial/final du workspace.

## 8. Corrections candidates à étudier — pas de décision d'implémentation

**C1 — Contrat d'exécution par mission (priorité P1).** Définir le résultat attendu, les actions autorisées, les chemins cibles et les opérations interdites ; une instruction de lecture seule ne doit pas autoriser une mutation, même si le LLM la demande. Validation du comportement et de la compatibilité avec les quatre outils existants.

**C2 — Garde de dispatch déterministe (P1).** Avant d'exécuter une mutation, appliquer une politique explicite et testable indépendante de la seule volonté du modèle. Ne pas confondre avec les restrictions Landlock : le confinement borne les chemins, pas la pertinence de l'action.

**C3 — Vérificateur de livrable et d'absence d'effets de bord (P1).** Vérifier l'état attendu et l'état réel sur des critères formels, avec statuts `VERIFIED`, `PARTIAL`, `FAILED` ; aucune validation fonctionnelle par simple texte du modèle.

**C4 — Budget et terminaison honnête (P1).** Séparer limite d'itérations, limites d'actions et états de mission ; un plafond atteint après mutations doit produire un verdict partiel/échec explicite et un bilan des effets observés. Ne **pas** augmenter simplement le plafond de 5.

**C5 — Traces d'exécution sûres (P2).** Capturer les arguments utiles dans un banc de test contrôlé, en respectant la politique de confidentialité de SPEC-UI-001 (journaux persistants désactivés par défaut, aucune donnée sensible). Le logger de production ne doit pas consigner prompts ni contenus arbitraires par défaut.

**C6 — Tests de non-régression (P1).** Ajouter des cas mock indépendants du modèle couvrant refus de mutation en lecture seule, arrêt, refus d'écrasement, bilan exact, absence de création annexe ; compléter par campagnes LLM réelles répétées.

## 9. Critères d'acceptation et de clôture

- [ ] SHA exact et environnement du binaire reproduisant l'incident capturés.
- [ ] S1/S2/S3/S4 exécutés en sessions vierges avec entrées et sorties archivées.
- [ ] Origine des appels `write_file` non demandés attribuée à des tool calls argumentés.
- [ ] Politique de lecture seule imposée **avant** dispatch et testée.
- [ ] Création unique : `resultat.txt` exact, aucun autre fichier, clôture normale.
- [ ] Limite d'itérations : aucune réussite fonctionnelle faussement déclarée.
- [ ] Aucune régression des invariants Landlock, SSE, REPL, écriture sans écrasement ni callbacks historiques.
- [ ] Build propre, tests CTest, scénarios mock, puis essais LLM réels sur SHA fixé ; résultats distincts.
- [ ] Rapport de correction daté ajouté à `docs/reports/`, avec preuves, limites et verdict.

## 10. État du présent rapport

Rapport fondé sur les sorties transmises par l'utilisateur ; **aucune nouvelle exécution du binaire** ni inspection dynamique du serveur LLM n'a été réalisée pour cette rédaction. Aucune correction C++ n'est engagée ou validée. Le dépôt ne doit pas considérer l'incident comme résolu du seul fait de la création de l'issue.

**Verdict : ÉCHEC FONCTIONNEL CONFIRMÉ ; CAUSE RACINE À ISOLER ; CORRECTION ET VALIDATION OUVERTES.**
