# Qualification ciblée de la sécurité concurrente du workspace

**Date :** 2026-11-13
**SHA qualifié :** `bad20b404155e882971e806290be0d668b0de445`
**Branche :** `qualification/workspace-concurrency-security`
**Worktree :** `/home/oper/tmp/local-agent-cpp-workspace-concurrency`

## Périmètre et environnement

`origin/main` a été vérifié directement sur GitHub à `bad20b404155e882971e806290be0d668b0de445`, puis sa référence locale a été actualisée. Le worktree de qualification est isolé sur ce SHA. L'inventaire préalable a révélé des modifications préexistantes dans le worktree Debian packaging, trois rapports non suivis dans le checkout principal et un worktree déjà absent/prunable; aucun n'a été modifié.

Environnement : Linux x86_64, noyau `6.12.115+deb13.1-amd64`, Debian 13, GNU C++ 14.2.0, CMake 3.31.6. Build propre dans `/tmp/local-agent-cpp-workspace-concurrency-build`, suite activée par `-DAGENT_BUILD_TESTS=ON`. Les tests utilisent uniquement leurs répertoires temporaires synthétiques. Aucun fichier utilisateur ni workspace réel n'a été utilisé. Aucune mesure de performance ni expérience concurrente supplémentaire n'a été lancée.

## Matrice des garanties

| Propriété étudiée | Mécanisme dans le code | Test ou observation | Résultat réel | Limite résiduelle | Niveau de preuve |
|---|---|---|---|---|---|
| Deux créations concurrentes de même chemin, sans écrasement | Sous Linux, le writer prépare un inode anonyme `O_TMPFILE`, puis le publie avec `linkat(..., AT_EMPTY_PATH)`. Le lien de destination échoue si le nom existe déjà (`EEXIST`); le code transforme ce cas en refus d'écrasement. | `tests/workspace_tools_test.cpp`, test `creation_concurrente_sans_overwrite_ne_remplace_pas` (deux threads synchronisés, même cible). | Réussi : exactement un appel obtient le succès; le contenu final est intégralement l'une des deux valeurs synthétiques. | Test concurrent dans un processus avec threads, pas deux processus indépendants. La propriété anti-remplacement provient aussi de la primitive atomique de publication. | **Code vérifié + test exécuté**. |
| Tiers créant la cible après approbation | La prévalidation vérifie la cible, mais le writer re-vérifie le nom et `linkat` publie sans remplacer. | `tests/write_authorization_test.cpp`, `target_created_after_approval_is_not_overwritten`. | Réussi : après approbation, le test crée la cible avec `competitor`; la consommation de `PreparedWrite` échoue « already exists » et le contenu concurrent reste intact. | Le test place la création entre l'approbation et le démarrage du writer; la course exactement simultanée à `linkat` est surtout protégée par l'atomicité de cette primitive, également exercée par le test de deux threads. | **Code vérifié + test exécuté**. |
| Identité du fichier publié | Le writer relit le contenu du descripteur temporaire et vérifie la taille; après `linkat`, `verify_published_file()` compare le type régulier et le couple `(st_dev, st_ino)` du descripteur conservé avec le nom publié (`fstat`/`fstatat`, sans suivre un lien symbolique). | `tests/workspace_tools_test.cpp`, `verification_publication_detecte_substitution`. | Réussi : le helper accepte le même fichier et rejette un autre fichier, un lien symbolique et un nom absent. | Le test valide le helper avec des objets déterministes; il ne synchronise pas un processus rival précisément entre `linkat` et `fstatat`. | **Code vérifié + test unitaire exécuté**; fenêtre de course non reproduite. |
| Modification ou suppression après vérification et retour du writer | Après la comparaison d'identité, la fonction retourne le succès et ne conserve aucun verrou, lease ni descripteur partagé avec les autres processus pour protéger le nom. | Inspection de `src/workspace_write.cpp:146-148`; aucun test post-retour correspondant dans les tests ciblés. | Aucun changement après retour n'a été provoqué. Le code ne fournit pas de garantie de stabilité ultérieure du chemin ou du contenu face à un autre processus autorisé par les permissions Unix. | Un processus distinct du même UID peut généralement modifier, supprimer ou remplacer un fichier accessible dans le workspace selon les droits DAC. Ce n'est pas une preuve qu'une attaque a eu lieu ni un contournement de la publication atomique par le writer. | **Limite déduite du code et du modèle de permissions**, non reproduite. |
| Portée de Landlock face à un processus indépendant du même UID | `apply_filesystem_sandbox()` applique des règles au processus/thread appelant avec `landlock_restrict_self`; les descendants héritent de la restriction. Aucun contrôle de permissions ne lie tous les processus partageant un UID. | `tests/filesystem_sandbox_test.cpp`, `denies_filesystem_access_outside_workspace`: le processus fils sandboxé vérifie accès refusés hors workspace et accès permis dedans. | Réussi dans le processus sandboxé. Le test n'essaie pas de faire écrire un processus indépendant du même UID pendant que l'agent fonctionne. | Landlock est une barrière pour le processus restreint et ses descendants, pas une isolation d'identité utilisateur. Un autre processus du même compte, non placé sous ces règles, reste soumis aux permissions Unix ordinaires et peut accéder aux fichiers du workspace si elles le permettent. | **Code vérifié + test de confinement exécuté** pour le processus restreint; absence de confinement des processus indépendants déduite de la portée du mécanisme. |
| Durabilité après crash | Le writer écrit, relit et vérifie le fichier, puis le publie; aucun `fsync`/`fdatasync` du fichier ni `fsync` du répertoire n'est appelé. | Inspection de `src/workspace_write.cpp:115-148`; aucun crash forcé ni test de redémarrage n'a été effectué. | Aucun crash n'a été provoqué; aucune persistance après coupure n'a été observée. | Un retour de succès confirme l'écriture et la publication observées pendant l'exécution, mais ne promet pas leur durabilité après crash/panne. | **Code vérifié** pour l'absence de synchronisation explicite; conséquence après crash non testée. |

## Vérifications exécutées

```text
cmake -S . -B /tmp/local-agent-cpp-workspace-concurrency-build -DAGENT_BUILD_TESTS=ON  PASS
cmake --build /tmp/local-agent-cpp-workspace-concurrency-build -j2                 PASS
ctest --test-dir /tmp/local-agent-cpp-workspace-concurrency-build --output-on-failure -R '^(workspace|sandbox)$'
  2/2 groupes PASS
agent_tests --filter=workspace : 48 cas, 206 assertions, zéro échec
agent_tests --filter=sandbox   : 4 cas, 9 assertions, zéro échec
```

Les tests directement pertinents `creation_concurrente_sans_overwrite_ne_remplace_pas`, `target_created_after_approval_is_not_overwritten` et `verification_publication_detecte_substitution` ont passé dans le groupe `workspace`. Les quatre tests `sandbox` ont passé, y compris le contrôle Landlock dans un processus restreint. Le CTest intégral, les tests des autres groupes, un test avec deux processus indépendants du même UID et un test de crash n'ont pas été exécutés; ils ne sont pas nécessaires pour affirmer les limites structurelles indiquées ici.

La configuration CMake a émis son avertissement développeur existant `CMP0135` sur `DOWNLOAD_EXTRACT_TIMESTAMP`; la compilation a réussi. Aucun benchmark n'a été réalisé et aucune mesure de ressources n'est revendiquée.

## Lecture de sécurité et limites

- **Garantie vérifiée :** le chemin normal de création refuse le remplacement; la publication par lien est sans remplacement atomique. La cible apparue après approbation reste intacte.
- **Garantie vérifiée dans son périmètre :** l'identité publiée est comparée à celle du descripteur temporaire au moment de la vérification.
- **Limite de concurrence :** la vérification est ponctuelle. Elle n'empêche pas une mutation ou suppression ultérieure par un processus qui dispose des permissions du workspace. Aucune exploitation ni reproduction d'une course post-retour n'est rapportée.
- **Limite du sandbox :** Landlock restreint le processus auquel il est appliqué et sa descendance, pas les autres processus du même UID.
- **Durabilité :** absence d'appels de synchronisation explicite; pas de garantie après crash. Ce point n'a pas été testé par simulation de panne.

Aucun défaut exploitable n'a été démontré pendant cette qualification. Les tests fonctionnels ne constituent pas une preuve de sécurité totale contre les processus du même compte ni contre les pannes système.

## Verdict

**PASS** — les garanties ciblées effectivement testées (création concurrente sans remplacement, cible créée après approbation, vérification d'identité et confinement du processus Landlock) sont conformes au comportement attendu; aucune anomalie n'a été reproduite.

Ce verdict conserve les limites structurelles : absence de protection contre un processus indépendant du même UID, absence de garantie de stabilité du fichier après le retour du writer et absence de garantie de durabilité après crash. Ces limites sont établies par la portée du code et ses primitives, mais n'ont pas été qualifiées comme attaques ou pertes effectivement observées.
