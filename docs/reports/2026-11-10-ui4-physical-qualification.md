# UI-4 — qualification physique Debian 13 / Ghostty

**Révision examinée :** `6641789f7574fbcc42c51488b30f5e413e68e63c`\
**Branche :** `main`\
**Worktree documentaire :** `/tmp/local-agent-cpp-ui4-physical-docs`\
**Date du rapport :** 2026-11-10

## Portée et provenance des preuves

Le SHA distant `origin/main` a été vérifié à `6641789f7574fbcc42c51488b30f5e413e68e63c`. C'est le merge de la PR #43; le commit de code UI-4 `7625ee01f2d387857d7e537ccdc329cd9fed69e3` en est le second parent. Le tag `v1.0.0-technical` reste attaché au commit `b1d194288b02b5df094953affd763776991d3e0a`.

Le mainteneur confirme une recette physique sur `versutus-ia`, Debian 13, avec Ghostty, et déclare **dix contrôles manuels PASS**. Il indique aussi que des captures F1, F2 et F3 montrent les tours, les métriques HTTP et l'exécution de `read_file`, et rapporte les vérifications de refus et d'approbation. Les fichiers image et la liste détaillée des dix contrôles ne sont pas présents dans ce worktree; ces éléments sont donc consignés comme résultats et preuves visuelles **rapportés par le mainteneur**, non comme des artefacts inspectés indépendamment.

Les résultats automatisés mentionnés ci-dessous proviennent du rapport d'intégration UI-4 et de la description de PR #43. Ils ont été obtenus sur le candidat avant fusion, basé sur `4efc75ff71bebbed62f96359691f140086aa76f5`; ils n'ont pas été relancés sur le SHA de merge et ne constituent pas des essais physiques.

## Résultats automatisés antérieurs — candidat pré-fusion

| Contrôle | Statut | Preuve et limite |
|---|---|---|
| Build C++20 propre | PASS rapporté | GCC 14.2.0, libcurl 8.14.1, ncursesw détecté; build du candidat avant fusion. Avertissement CMake CMP0135 rapporté. |
| Tests UI-4 ciblés | PASS rapporté | 13 cas / 91 assertions, zéro échec, sur le candidat. |
| Harnais complet | PASS rapporté | 275 cas / 8 196 assertions, zéro échec, sur le candidat. |
| CTest | PASS rapporté | 14/14 groupes sur le candidat. |
| F3 et streaming PTY/mock | PASS rapporté | Test PTY consulte les métriques pendant un flux simulé; ce n'est pas une observation Ghostty ni une charge de production. |
| Saturation EventChannel → PARTIAL visible en F3 | NON PROUVÉ | La saturation du worker et la projection PARTIAL sont couvertes séparément; la revue UI-4 signale l'absence d'un test bout en bout. |
| Performance des snapshots en flux dense | NON PROUVÉ | Aucune mesure de latence, CPU ou RSS sous charge soutenue n'est consignée. |

Le premier lancement sandboxé de certains tests loopback a échoué sur `socket() failed`; le rapport indique que les suites ont ensuite réussi avec accès loopback autorisé.

## Qualification physique demandée

L'environnement ci-dessous est celui annoncé dans la mission, pas une collecte indépendante de la machine :

| Élément | Statut | Résultat disponible |
|---|---|---|
| Environnement `versutus-ia`, Debian 13, Ghostty | PASS rapporté | Déclaré par le mainteneur; versions précises Ghostty, `TERM` et locale non fournies ni collectées indépendamment. |
| Contrôles manuels de la recette | PASS rapporté, 10/10 | Le mainteneur déclare dix contrôles réussis; leur checklist individuelle n'a pas été jointe, donc aucune étape additionnelle n'est inférée ici. |
| Vues F1, F2 et F3 | PASS rapporté avec captures décrites | Les captures sont dites confirmer les tours et métriques HTTP en F1/F2/F3. Les fichiers image ne sont pas accessibles pour inspection dans ce dossier. |
| Métriques et tours observés dans F3 | PASS rapporté avec capture décrite | Capture signalée par le mainteneur; aucune valeur numérique transcrite dans le rapport. |
| Exécution de `read_file` et affichage associé | PASS rapporté avec captures décrites | Les captures F1/F2/F3 sont décrites collectivement comme confirmant les tours, les métriques HTTP et l'exécution de lecture; la correspondance précise entre capture et donnée n'est pas fournie. Aucun contenu de fichier n'est reproduit. |
| Vérifications de refus et d'approbation | PASS rapporté | Le mainteneur confirme les deux catégories de parcours; noms de fichiers, détails du défi et traces filesystem non fournis ici. |
| Sémantique des octets HTTP | PASS du contrat logiciel; pas une mesure de débit | Le code les définit comme octets du corps reçus. Ils ne représentent ni tokens ni débit de tokens. |
| Indication PARTIAL après perte d'événements | NON PROUVÉ | Aucun scénario de perte exécuté ou résultat visuel fourni; lacune de test bout en bout déjà connue. |
| Absence de fuite de prompt/réponse/arguments dans les métriques | PASS au niveau du contrat testé, pas une preuve physique | Le collecteur ne conserve pas ces contenus et les tests antérieurs vérifient certaines projections; les captures décrites ne constituent pas une inspection exhaustive de la mémoire. |
| Réactivité sous flux soutenu et coût des copies | NON PROUVÉ | Pas de mesure physique ni profil de charge. |
| Défaut UI-4 reproduit | Aucun FAIL rapporté | L'absence de compte rendu ne démontre pas l'absence de défaut. |

## Évaluation

Le SHA établit le code fusionné. Les dix PASS manuels, les captures F1/F2/F3 et les parcours de refus/approbation sont des observations explicitement attribuées au mainteneur. Les captures ne sont pas intégrées au rapport et les contrôles ne sont pas détaillés individuellement; la qualification se limite donc au périmètre déclaré, sans prétendre à une reproduction indépendante. Aucun défaut physique n'est rapporté comme reproduit.

Les deux réserves de revue restent ouvertes :

1. Test de bout en bout de la saturation réelle d'`EventChannel` jusqu'à l'indicateur `PARTIAL` de F3.
2. Mesure du coût des copies de snapshots et de la reprojection des lignes F3 sous flux soutenu.

Les tailles bornées du collecteur ne constituent pas une borne de RSS globale. Les compteurs sont des observations techniques côté client; les fragments SSE ne sont pas des tokens et un tour techniquement terminé ne prouve pas la réussite fonctionnelle.

## Verdict

**GO — UI-4 qualifié physiquement sur le périmètre déclaré par le mainteneur, avec réserves.** Dix contrôles manuels sont déclarés PASS; les vues F1/F2/F3, les métriques HTTP, `read_file`, le refus et l'approbation sont rapportés, avec captures F1/F2/F3 mentionnées. Cette qualification n'est pas une reproduction indépendante et ne s'étend pas aux scénarios non observés.

Restent explicitement **NON PROUVÉS** : le chemin de bout en bout saturation réelle d'`EventChannel` → `PARTIAL` visible dans F3 et le coût des snapshots sous charge soutenue. Les tests automatisés historiques ne couvrent pas cette preuve intégrée et aucune mesure de performance n'est disponible. Aucun débit de tokens n'est mesuré ou déduit des octets HTTP.

Cette consolidation documentaire n'a modifié aucun code et n'a relancé aucun test.
