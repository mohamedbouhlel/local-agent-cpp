# P3 — Sort de `HttpClient::post()` (rapport de préparation)

> Itération de **préparation et d'analyse uniquement**. **Aucun changement de
> production, aucun changement de test, aucune dépendance ajoutée.** **Aucun
> commit** (le dépôt reste propre). Ce rapport rassemble les preuves nécessaires
> à un arbitrage éclairé entre **A. conserver et tester** et **B. supprimer**.

## 1. OBJECTIF ET PÉRIMÈTRE

L'item **P3** de `docs/roadmap/ROADMAP.md:144-146` demande de décider du sort de
`HttpClient::post()` : « soit le couvrir par un test (il reste une API publique
utile), soit le retirer » (dette `§B.1`). Ce rapport établit les faits, compare
les deux options et formule une recommandation. **La décision n'est pas prise
ici** : elle appartient au mainteneur.

**Hors périmètre** : toute modification de `src/`, `include/`, `tests/`,
`CMakeLists.txt` ; toute suppression de code ; toute réécriture de la roadmap.

## 2. ÉTAT INITIAL ET PREUVES

| Élément | Valeur observée |
|---|---|
| Répertoire | `/home/oper/projects/local-agent-cpp` |
| Branche | `main` |
| HEAD | `51b7d2c3bd2684c32521e11b59ba833dd85bebae` (`test: add end-to-end SSE mock scenarios`) |
| Nombre de commits | **31** |
| Working tree | **propre** (`git status --short` vide) |
| Suite de tests | **66 cas / 264 assertions**, 5 groupes CTest (`sse`, `workspace`, `http`, `agent`, `mock`), **0 échec** |
| Tags / releases | **aucun** (`git tag` vide) |
| Répertoires `examples/` / `tools/` | **absents** (aucun consommateur externe versionné) |
| Remote | aucun |

## 3. USAGES ET CONTRAT OBSERVÉS

### 3.1 Déclaration et contrat

`include/http_client.hpp:16-19` :

```cpp
std::string post(const std::string& url,
                 const Headers& headers,
                 const std::string& body,
                 long timeout_seconds) const;
```

**Contrat observable** (`src/http_client.cpp:138-146`) : POST JSON, renvoie le
**corps complet accumulé** (chaîne vide si le corps est vide), statut quelconque
traité par `perform_request()`.

**Erreurs** (communes, `src/http_client.cpp:121-131`) :

| Situation | Exception |
|---|---|
| Échec curl (connexion, troncature, timeout) | `HttpError("HTTP POST failed: " + détail)` |
| Statut non-2xx | `HttpError("HTTP POST failed with status <N>: " + corps)` |

### 3.2 Différence avec `post_streaming()`

`post()` et `post_streaming()` **partagent** `perform_request()`
(`src/http_client.cpp:84-134`) : même setup curl, même `CURLOPT_TIMEOUT`, mêmes
erreurs, même valeur de retour (corps accumulé). La **seule** différence est le
callback d'écriture :

* `post()` → `write_callback` (`:51-56`) : **accumule seulement** ;
* `post_streaming()` → `streaming_write_callback` (`:63-72`) : accumule **et**
  appelle `on_data(chunk)` si ce callback est non vide.

**Conséquence factuelle** : `post(u,h,b,t)` est **strictement équivalent** à
`post_streaming(u,h,b,t, {} /* callback vide */)` — le callback vide est ignoré
(`:68`). `post()` est donc un **raccourci de commodité** d'une API déjà testée,
et non un chemin de code distinct.

### 3.3 Origine et évolution

| Commit | Événement |
|---|---|
| `5b40cdd feat(http): add minimal libcurl POST wrapper` | **introduction** de `post()` (wrapper libcurl minimal) |
| `e0b1771 feat(http): add streaming POST with chunk callback` | **extraction** du corps dans `perform_request()` (numstat `56/10` sur `src/http_client.cpp`) ; `post()` **conservé à l'identique fonctionnellement** |

Conservé explicitement lors du passage au streaming (cf.
`docs/history/PROJECT_HISTORY.md:174-175`).

### 3.4 Appelants internes (production vs tests)

| Contexte | Appelants |
|---|---|
| **Production** (`src/`, `include/`) | **aucun** pour `post()` — seul `post_streaming()` est utilisé par `Agent` (`src/agent.cpp:209`) |
| **Tests** | **4** tests dédiés : `tests/http_client_test.cpp:23,35,84,96` (succès, non-2xx, échec curl, réponse tronquée) |
| **Tests (outil de support)** | **3** appels comme *client* dans `tests/mock_http_server_test.cpp:105,125,146` (ces tests valident **le mock**, pas `post()`) |

**Constat clé — la prémisse « non testée » est périmée.** La roadmap
(`ROADMAP.md:40`, « code mort applicatif ») et `OPEN_QUESTIONS.md` Q3 (« API
publique **non testée** »)ont été rédigés **avant** l'intégration de la suite
P1.1. Aujourd'hui, `post()` est **couvert par 4 cas de test** (groupe `http`),
tous **verts**. Le volet « tester » de l'option A est donc **déjà satisfait** ;
seul le volet « conserver ou retirer » reste ouvert.

### 3.5 Contraintes de compatibilité et documentation

* **README.md / USAGE.md / AGENTS.md** : ne mentionnent **pas** `HttpClient` ni
  `post()` (aucune promesse d'API documentée) ⇒ aucune contrainte écrite.
* **`agent_core`** est une bibliothèque **statique** avec `include/` **PUBLIC**
  (`AGENTS.md:45-47`) : ses en-têtes sont, de fait, une surface d'API pour tout
  consommateur externe, même s'il n'en existe aucun dans le dépôt.
* **Aucun tag / release** : pas de contrat de versionnement formel à respecter.
* **Aucun appelant hors dépôt connu** (pas de `examples/`, pas de remote).
* La règle projet impose : « **Ne pas déduire qu'une API est inutile uniquement
  parce qu'elle n'a pas d'appelant interne** ».

## 4. COMPARAISON FACTUELLE DES OPTIONS

### A. Conserver (et considérer le test comme déjà acquis)

| Critère | Évaluation |
|---|---|
| Coût de maintenance | **faible** : ~8 lignes (`:138-146`) déléguant à `perform_request()` partagé ; aucune duplication de logique |
| Couverture de test | **déjà acquise** : 4 cas (`http_client_test.cpp`), 0 échec |
| Valeur d'API | fournit le chemin **non-streamé** (endpoints non SSE, diagnostics, appels ponctuels) ; évite au consommateur de fournir un callback vide |
| Compatibilité | **aucun changement** : rien ne casse ; aucune action requise |
| Conforme aux règles | **oui** (« ne pas déduire l'inutilité d'une API de l'absence d'appelant interne ») |
| Risque | quasi nul (code stable, inchangé depuis `e0b1771`) |

### B. Supprimer

| Critère | Évaluation |
|---|---|
| Bénéfice | supprime **8 lignes** de production et réduit la surface d'API ; résout formellement la dette `§B.1` |
| Coût | modification **cassante** de l'en-tête PUBLIC d'`agent_core` ; suppression de 4 cas de test ; **réécriture** des 3 appels de `mock_http_server_test.cpp` (qui utilisent `post()` comme client de commodité) — donc un changement de **tests**, pas seulement de production |
| Justification disponible | **faible** : l'argument « code mort » ne repose que sur l'**absence d'appelant interne**, ce que la règle projet interdit d'utiliser seul comme critère ; la fonctionnalité reste accessible via `post_streaming` |
| Conformité aux règles | **tend à s'y opposer** (« ne pas déduire l'inutilité… ») |
| Récupérabilité | triviale (le corps est déjà dans l'historique Git), mais l'API disparaît pour tout consommateur |

### Synthèse des écarts

| Question | Réponse factuelle |
|---|---|
| `post()` est-il « code mort » ? | **Mort au sens applicatif** (aucun appelant en production) mais **vivant et couvert** en test. |
| Est-il « non testé » ? | **Non** : 4 cas verts. La prémisse de Q3/`§B.1` a été **invalidée par P1.1**. |
| Est-il redondant ? | **Oui fonctionnellement** (`≡ post_streaming(…, {})`), mais **moins verbeux** pour un usage non-streamé. |
| Sa suppression casse-t-elle quelque chose **dans le dépôt** ? | **Oui** : 4 tests + 3 usages dans les tests du mock. |
| Existe-t-il un consommateur externe ? | **Aucun connu** (pas de remote, pas de tags, pas d'`examples/`). |

## 5. RECOMMANDATION ARGUMENTÉE

**Recommandation : option A — conserver `post()`, en considérant le volet
« tester » comme déjà satisfait par la suite P1.1.**

Arguments :

1. **La prémisse de la dette est périmée** : `post()` est **déjà testé** (4 cas,
   0 échec). L'option A ne demande donc **aucun travail de test**
   supplémentaire pour être remplie.
2. **Coût marginal nul** : `post()` ne duplique rien — il délègue à
   `perform_request()`, partagé avec `post_streaming()`. Le retirer ne
   simplifierait pas la logique, seulement la surface.
3. **La suppression est plus coûteuse que la conservation** : elle touche
   l'en-tête **public**, supprime des tests verts et oblige à réécrire
   `mock_http_server_test.cpp` (changement de **tests**, contraire au périmètre
   « production only » souvent visé).
4. **Règle projet explicite** : l'absence d'appelant interne ne suffit pas à
   conclure à l'inutilité d'une API.
5. **Aucune contrainte de compatibilité documentée** ne s'y oppose (pas de
   promesse d'API dans README/USAGE/AGENTS) — mais aucune ne justifie non plus
   une suppression, faute de bénéfice démontré.

**Ce qui ferait basculer vers B** (critères objectifs à réunir) : décision
explicite de **figer la surface publique** d'`agent_core` au seul chemin
streamé (p. ex. avant une v1/tag), **et** acceptation d'un changement cassant
assorti de la mise à jour des tests. En l'absence de ces deux éléments, B reste
une suppression non motivée.

**Action documentaire suggérée (hors périmètre de cette itération)** : une fois
la décision prise, mettre à jour `ROADMAP.md` (`§B.1`, P3),
`OPEN_QUESTIONS.md` Q3 et `PROJECT_CONTEXT.md:130` pour retirer l'affirmation
« non testée » — désormais fausse — et acter le choix retenu.

## 6. DÉCISION RESTANT À PRENDRE

L'arbitrage **A vs B** n'est **pas** tranché ici. Éléments nécessaires à la
décision, tous disponibles :

| Entrée de décision | Fournie ? |
|---|---|
| Contrat, erreurs et différence avec `post_streaming()` | **oui** (§3.1-3.2) |
| Couverture de test réelle | **oui** (§3.4 : 4 cas verts) |
| Inventaire des appelants (production / tests) | **oui** (§3.4) |
| Contraintes de compatibilité / versionnement | **oui** (§3.5 : aucune formelle) |
| Coût respectif de A et B | **oui** (§4) |
| Position de la règle projet | **oui** (§4-§5) |

Il ne manque **aucune** information factuelle pour trancher ; le choix relève
d'une **préférence de mainteneur** (élargir ou restreindre la surface publique),
non d'une donnée manquante.

## 7. VERDICT

### PRÉPARÉ

Toutes les preuves nécessaires à un arbitrage éclairé sont réunies et
reproductibles. **Recommandation : A (conserver)**, le volet « tester » étant
déjà satisfait ; la décision finale reste au mainteneur, avec les critères de
bascule vers B explicités (§5). Aucun changement de production ni de test n'a
été effectué ; aucun commit.

### Réserve

L'affirmation « API publique non testée » présente dans `ROADMAP.md:40` et
`OPEN_QUESTIONS.md` Q3 est **factuellement périmée** depuis P1.1 ; sa mise à jour
est recommandée mais **hors du périmètre** de cette itération de préparation.
