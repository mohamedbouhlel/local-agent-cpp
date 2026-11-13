# LOFITERM — Consolidation des qualifications du résultat n° 3

**Date :** 2026-11-13
**SHA de consolidation :** `242e8a34ef36a85ffd5f775d3ef6d694857c042f`
**Branche :** `docs/jalon3-qualification-consolidation`
**Worktree :** `/home/oper/tmp/local-agent-cpp-jalon3-consolidation`

## Référence Git et intégrations

La référence directe de `origin/main` a été vérifiée à `242e8a34ef36a85ffd5f775d3ef6d694857c042f`. Le worktree isolé est à ce SHA. Le checkout principal est resté sur son HEAD local antérieur et ses trois rapports non suivis; le worktree packaging conserve ses modifications; aucun worktree existant n'a été nettoyé ou modifié.

Les commits de premier parent de `origin/main` confirment les intégrations suivantes :

| PR | Commit de merge dans `main` | Portée confirmée |
|---|---|---|
| #52 | `9202b1c8e77dfa65e7830db47b321219c1ba4a0a` | Rapport initial du contrat d'échec Agent et de l'historique, étudié au SHA `912b086e43c20be58f5ddea5f95ebdec53dcdced`. |
| #53 | `381d7dd2049bbb74653675d5460b27d94629b8c0` | Commentaire `AgentError` clarifié et test déterministe outil réussi → HTTP de suivi échoué → tour utilisateur suivant, provenant du commit candidat `97a694bcacda0fb7ea047edf7e73724761e04a7f`. |
| #54 | `bad20b404155e882971e806290be0d668b0de445` | Rapport de qualification ressources/contexte; code étudié au SHA `381d7dd2049bbb74653675d5460b27d94629b8c0`. |
| #55 | `242e8a34ef36a85ffd5f775d3ef6d694857c042f` | Rapport de qualification de la concurrence workspace; code étudié au SHA `bad20b404155e882971e806290be0d668b0de445`. |

Les quatre rapports sources sont présents dans `docs/reports/` et référencés dans `docs/README.md` :

| Rapport source | SHA étudié par le rapport |
|---|---|
| [`2026-11-13-agent-send-failure-history-contract.md`](2026-11-13-agent-send-failure-history-contract.md) | `912b086e43c20be58f5ddea5f95ebdec53dcdced` |
| [`2026-11-13-agent-error-contract-and-tool-followup-regression.md`](2026-11-13-agent-error-contract-and-tool-followup-regression.md) | Base `9202b1c8e77dfa65e7830db47b321219c1ba4a0a`; test et commentaire dans le candidat `97a694bcacda0fb7ea047edf7e73724761e04a7f` |
| [`2026-11-13-resource-and-context-boundaries-qualification.md`](2026-11-13-resource-and-context-boundaries-qualification.md) | `381d7dd2049bbb74653675d5460b27d94629b8c0` |
| [`2026-11-13-workspace-concurrency-security-qualification.md`](2026-11-13-workspace-concurrency-security-qualification.md) | `bad20b404155e882971e806290be0d668b0de445` |

La preuve d'une campagne reste attachée au SHA qu'elle indique; l'intégration ultérieure d'un rapport ou d'un test ne transforme pas ces résultats en exécution sur le SHA final `242e8a3`.

## Synthèse des qualifications

### 1. Garanties vérifiées par les tests

- **Échec de tour Agent :** au SHA `912b086`, les tests déterministes cités vérifiaient la conservation des messages utilisateur, l'absence de message assistant incomplet après flux interrompu ou incomplet, et la distinction entre transport/protocole et résultat final du tour. Ces résultats étaient rapportés à **43 cas / 900 assertions** pour le groupe Agent de cette campagne; ils ne sont pas additionnés aux campagnes suivantes.
- **Outil réussi avant erreur de suivi :** au candidat `97a694b`, le test ajouté vérifie une exécution d'outil réussie une seule fois, la corrélation des événements, l'échec HTTP 500 du suivi, l'historique `user → assistant(tool_calls) → tool` conservé et la réussite du tour utilisateur suivant. Le cas isolé a réussi avec **1 cas / 110 assertions**; le groupe Agent avec **44 cas / 1 010 assertions**. Ce sont deux exécutions distinctes; le cas isolé n'est pas à additionner au groupe.
- **Ressources et contexte :** au SHA `381d7dd`, `workspace` a réussi avec **48 cas / 206 assertions**, `tui4b1` avec **10 / 6 125**, et `agent` avec **44 / 1 010**. Les probes synthétiques bornés ont vérifié `read_file` à la frontière de 50 000 octets et `search_files` à 49/50/51 correspondances; aucun épuisement mémoire n'a été constaté.
- **Concurrence et confinement :** au SHA `bad20b4`, CTest ciblé `workspace` + `sandbox` a réussi **2/2 groupes**; les exécutions ciblées rapportées sont `workspace` **48 / 206** et `sandbox` **4 / 9**. Les tests ont confirmé qu'une seule de deux créations concurrentes gagne sans écraser l'autre, qu'une cible apparue après approbation reste intacte, que le helper vérifie l'identité publiée et que le processus sandboxé respecte les règles Landlock.

Ces compteurs décrivent des commandes et builds distincts à des révisions différentes. Ils ne constituent pas une campagne agrégée, et aucun total global d'assertions n'est calculé ici.

### 2. Contradiction corrigée

Le rapport initial au SHA `912b086` relevait à juste titre une contradiction dans le commentaire `AgentError` : celui-ci laissait entendre qu'aucun élément du tour n'était conservé, alors que le message utilisateur était déjà dans l'historique et que des tool calls/résultats/effets pouvaient précéder un échec ultérieur. Le commentaire a été corrigé avant le merge #53. Au SHA final, il précise qu'une réponse assistant incomplète n'est pas validée, que le message utilisateur demeure et que les effets d'outils déjà produits ne sont pas implicitement annulés. Le test de régression #53 comble également la lacune combinée qui était explicitement ouverte dans le rapport initial.

Il ne s'agit pas d'une réécriture du rapport historique : il décrit l'état et la couverture de son SHA, puis le présent bilan rattache la correction à son commit ultérieur.

### 3. Limites structurelles établies

- `list_files` ne possède pas de plafond explicite du nombre de chemins ou du volume total produit. `search_files` plafonne les correspondances à 50, mais ne borne ni les octets par ligne ni le volume parcouru. Les mesures disponibles sont des fixtures petites et synthétiques; elles n'établissent pas un problème de performance ou un épuisement mémoire.
- `BoundedHistory` plafonne les chaînes textuelles retenues à 32 entrées et 16 384 octets par historique; cela ne borne pas le RSS global, les allocations temporaires, le transcript de conversation ni tous les objets de rendu.
- L'historique Agent cible 20 messages en supprimant les tours anciens complets lorsque cela est possible; le tour courant est préservé et peut dépasser ce nombre. Aucun tokenizer ni budget de tokens/contexte n'est intégré ou qualifié.
- La publication du fichier est sans remplacement et son identité est comparée au descripteur temporaire au moment de la vérification. Cette vérification ne protège pas le chemin contre les mutations postérieures d'un processus qui dispose des droits Unix.
- Landlock restreint le processus auquel les règles sont appliquées et ses descendants, pas un processus indépendant partageant le même UID. Aucun test d'attaque entre deux processus indépendants n'a été lancé.
- Aucun `fsync`/`fdatasync` du fichier ni `fsync` du répertoire n'est effectué par le writer; la durabilité après crash n'est pas garantie. Aucun crash n'a été simulé.

Ces points sont des limites de conception ou des risques structurels démontrés par le code. Aucun n'est présenté comme un défaut exploitable ou un épuisement effectivement reproduit.

### 4. Risques non reproduits et validations non exécutées

Le registre des réserves reste le suivant :

| Réserve | État de preuve | Impact sur la clôture du périmètre qualifié |
|---|---|---|
| Très grand listing ou très longue ligne/recherche provoquant une pression mémoire importante | **Risque non reproduit**; volumes mesurés faibles et bornés; RSS non mesuré. | Limite connue; aucun bug d'épuisement établi. |
| Modification/remplacement par un autre processus après la vérification de publication | **Non testé comme course**; absence de verrou après retour établie par le code; permissions du même UID pertinentes. | Protection ponctuelle démontrée; aucune isolation contre les autres processus du compte n'est revendiquée. |
| Perte d'écriture après crash/panne | **Non exécuté**; absence de synchronisation explicite vérifiée dans le code. | Pas de garantie de durabilité à promettre; aucun cas de perte réelle observé. |
| Dépassement pratique de fenêtre de contexte par message/tour volumineux | **Non mesuré**; aucune comptabilité tokenizer ou profil serveur disponible. | Le seuil de messages ne vaut pas budget tokens; aucune limite de contexte exacte ne peut être affirmée. |
| Tests complets sur le SHA final `242e8a3` | **Non exécutés** dans ces qualifications ciblées; résultats rattachés aux SHA candidats indiqués. | La consolidation documentaire n'affirme pas un nouveau résultat de build/CTest sur le merge final. |
| Test LLM réel, mesure RSS, crash forcé et concurrent indépendant même UID | **Non exécutés**. | Hors des preuves acquises; ne sont pas requis pour transformer les limites ci-dessus en constats reproductibles. |

Le CTest complet n'a pas été exécuté dans les rapports de qualification ciblée qui le précisent comme hors périmètre. Les tentatives de mocks loopback bloquées par le sandbox, lorsqu'elles ont existé, ne sont pas comptées comme tests réussis; les relances autorisées et réussies sont décrites dans chaque rapport. Aucun résultat de validation non exécuté n'est repris comme PASS.

### 5. Décisions architecturales ouvertes

Sans trancher à la place du mainteneur :

1. Faut-il ajouter des plafonds de nombre et de volume à `list_files`, et des plafonds de ligne/octets parcourus à `search_files` ? Les mesures actuelles ne justifient pas une optimisation ou un plafond optimal.
2. Le produit doit-il imposer une politique supplémentaire contre les processus indépendants du même UID, au-delà de Landlock et des droits DAC existants ?
3. Une garantie de durabilité après panne fait-elle partie du contrat produit de `write_file` ? L'implémenter aurait des conséquences de synchronisation/performance et n'est pas décidé ici.
4. Un budget de contexte dépendant d'un tokenizer ou d'un profil modèle est-il requis ? Le nombre de messages existant n'est pas une approximation vérifiée des tokens.

Aucune décision ne rouvre le résultat n° 2, la qualification HTTP/SSE réelle, ni le chantier SCCM.

## Vérification documentaire de cette consolidation

Le rapport est indexé dans `docs/README.md`. Les quatre rapports ont été lus intégralement dans le worktree au SHA de consolidation; leurs révisions, campagnes et réserves sont citées sans fusionner leurs résultats. `git diff --check`, la vérification des liens relatifs, de l'index et un contrôle de motifs de secrets sont effectués pour ce diff documentaire. Aucun build ni test n'est relancé pour cette consolidation.

## Verdict

**CLÔTURABLE** pour le **périmètre de qualification** du résultat n° 3 : les preuves demandées sur le contrat d'échec/historique Agent, la combinaison outil réussi puis échec HTTP, les limites workspace/historique/contexte et les garanties ciblées de concurrence/Landlock sont référencées à leurs SHA et leurs campagnes; la contradiction `AgentError` est corrigée dans #53; les limites non testées restent au registre.

Ce verdict ne signifie ni que toutes les limites structurelles sont résolues, ni que toutes les suites ont été rejouées sur `242e8a3`, ni que les risques non reproduits sont impossibles. Il clôt la qualification documentaire ciblée telle que définie par le résultat n° 3, avec les réserves ci-dessus maintenues.
