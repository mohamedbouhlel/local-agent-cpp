# Qualification technique physique — LOFITERM v1.0

## Révision et preuve

- Dépôt : `mohamedbouhlel/local-agent-cpp`.
- SHA testé : `78aab200af4709307dc2b0fffe69306d2a63f1d5`.
- `origin/main` vérifié le 2026-11-10 : même SHA; commit de merge de la PR #39.
- Machine : `versutus-ia`, Debian 13.
- Terminal réel : Ghostty. Les détails de version Ghostty, `TERM`, locale, compilateur/CMake et les sorties shell brutes n'ont pas été fournis avec les résultats.
- Les contrôles physiques et résultats sont **confirmés par le mainteneur**. Ils n'ont pas été reproduits indépendamment par cette mise à jour documentaire.
- Le checkout principal demeure sur `feat/ui-4-observable-metrics`, avec ses changements locaux préexistants; aucun fichier de ce checkout n'a été modifié.

## Résultats automatisés rapportés sur `versutus-ia`

| Vérification | Résultat | Attribution / limite |
| --- | --- | --- |
| Compilation | PASS | Compilation réussie, rapportée par le mainteneur. |
| Harnais complet | PASS — 262/262 cas | Exécuté sur la machine de recette, selon le mainteneur. |
| Assertions | PASS — 8 102, zéro échec | Compteur observé sur `versutus-ia`. |
| CTest | PASS — 13/13 groupes | Exécuté sur la machine de recette, selon le mainteneur. |

Les références DEV restent séparées : **262 cas / 8 104 assertions / CTest 13/13** rapportés sur l'environnement de développement. La machine physique a produit **8 102 assertions**, soit deux de moins. Aucun écart n'est corrigé artificiellement; le résultat physique demeure 8 102. Aucun journal de sortie détaillé ni comparaison des versions d'exécutable/test n'a été fourni pour expliquer cet écart.

### Commandes reproductibles

Les lignes ci-dessous correspondent à la procédure de compilation et de test du dépôt et permettent de rejouer la vérification au SHA qualifié. Le mainteneur a confirmé les résultats, mais n'a pas fourni l'historique exact des commandes réellement saisies; ces commandes sont donc consignées comme procédure reproductible, pas comme transcription certifiée de sa session :

```sh
git checkout 78aab200af4709307dc2b0fffe69306d2a63f1d5
cmake -S . -B build-v1 -DAGENT_BUILD_TESTS=ON
cmake --build build-v1 --parallel
./build-v1/tests/agent_tests
ctest --test-dir build-v1 --output-on-failure
```

## Recette manuelle Debian 13 / Ghostty

Les résultats suivants ont été confirmés par le mainteneur sur `versutus-ia` :

| Scénario | Résultat | Observation |
| --- | --- | --- |
| Affichage du prompt multiligne | PASS | Le texte et ses lignes sont affichés dans le composeur. |
| Navigation du curseur et redimensionnement | PASS | Confirmation utilisateur sur Ghostty. |
| Envoi F9 et réponse LLM | PASS | Le tour a été soumis et une réponse a été reçue. |
| Refus d'écriture dans le workspace isolé | PASS | Aucun fichier créé dans ce workspace après refus. |
| Approbation explicite d'une création | PASS | `preuve-accord.txt` a été créé. |
| Contenu de la création approuvée | PASS | Contenu exact vérifié : `ACCORD_V1_2026`. |
| Absence de fichiers supplémentaires dans le workspace d'approbation | PASS | Aucun autre fichier constaté. |
| F10 et retour au terminal | PASS | Fermeture et retour au terminal confirmés par l'utilisateur. |

Ces observations qualifient les parcours exercés sur cette machine, cette configuration et cette session. Elles ne constituent pas une preuve de comportement identique avec tous les serveurs, modèles ou émulateurs.

## Classification du produit et limites

### Qualification technique

**PASS pour le périmètre fonctionnel effectivement testé.** La compilation et les deux suites automatisées passent selon les résultats fournis; la recette Ghostty rapporte l'édition multiligne, l'envoi, une réponse LLM, un refus sans création, une création approuvée au contenu vérifié, l'absence de fichier supplémentaire et la fermeture.

Ce verdict est circonscrit aux scénarios exécutés. Il ne qualifie pas exhaustivement les erreurs de serveur, les timeouts, tous les prompts ou les comportements de tous les modèles. Le résultat du LLM n'est pas, à lui seul, une preuve d'action : pour la création, la preuve rapportée est le contrôle filesystem du fichier et de son contenu.

### Limitations du modèle et du service LLM

Un essai réussi ne garantit ni la qualité sémantique générale du modèle, ni l'absence de demandes répétées après un refus, ni l'usage identique d'un autre modèle/endpoint. Les événements peuvent dépendre de la réponse du serveur. Aucun benchmark ni essai multi-modèles n'est revendiqué ici.

### Compilation, packaging et installation

La compilation depuis les sources est vérifiée sur la machine indiquée. La configuration utilise CMake/FetchContent selon le dépôt. Cette preuve ne signifie pas que le projet fournit un installateur : le CMake vérifié ne déclare pas de cible système `install`, et aucun paquet Debian ou artefact de distribution n'est qualifié par cette recette. Exécution depuis le répertoire de build et installation système sont deux statuts distincts.

### Identité LOFITERM

Le nom LOFITERM désigne le futur produit, mais aucune identité graphique ou habillage de marque dédié n'est qualifié par cette recette. Le comportement testé est celui de l'agent technique avec CLI classique et TUI ncursesw.

## Réserves restantes

- L'écart entre **8 104 assertions DEV** et **8 102 assertions sur `versutus-ia`** reste inexpliqué faute de sorties détaillées/version complète de l'environnement. Les deux comptes sont maintenus séparément.
- Les versions Ghostty, compilateur et CMake, les paramètres `TERM`/locale ainsi que les commandes shell effectivement exécutées n'ont pas été enregistrés dans les informations fournies.
- Pas de qualification SSH ou d'autres terminaux dans cette recette.
- Pas de qualification d'installation système, de paquet ou d'identité graphique LOFITERM.
- Les résultats manuels sont des confirmations du mainteneur, non une reproduction indépendante.

## Verdict

**QUALIFIÉ POUR LE PÉRIMÈTRE TECHNIQUE TESTÉ sur Debian 13/Ghostty au SHA `78aab200af4709307dc2b0fffe69306d2a63f1d5`.** La conclusion s'appuie sur les résultats automatisés rapportés (262/262, 8 102 assertions, CTest 13/13) et les vérifications manuelles confirmées du composeur, du tour LLM et des effets filesystem d'approbation/refus.

Ce verdict ne signifie pas que le packaging, l'installation système, l'identité graphique LOFITERM, d'autres terminaux ou une qualification générale de la qualité du modèle sont achevés. Il ne prétend pas résoudre l'écart DEV/recette de deux assertions.
