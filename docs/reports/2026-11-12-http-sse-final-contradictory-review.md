# Revue contradictoire finale — correctif HTTP/SSE

Date : 12 novembre 2026
Base : `fcd50afea1fcb93eef0d5573a458e667e72f25f8`
Branche examinée : `fix/http-stream-timeouts-recovered`
Worktree : `/tmp/lofiterm-http-stream-timeouts-recovered`

## Périmètre et méthode

Revue en lecture seule du diff suivi, des fichiers non suivis, des sources HTTP,
agent et métriques, des tests et du rapport
[`2026-11-12-http-sse-timeouts-and-metrics.md`](2026-11-12-http-sse-timeouts-and-metrics.md).
Le seul fichier ajouté par cette revue est le présent rapport, référencé dans
`docs/README.md`. Aucun test n'a été relancé pendant la revue. Les résultats
303 cas / 8 692 assertions et CTest 14/14 sont ceux attestés par le rapport et
les sorties de validation de la session précédente, pas une nouvelle campagne.

HEAD candidat égal à la base; le candidat testé est donc le diff local non
commité appliqué à ce HEAD. Avant ajout du présent rapport, l'inventaire était
de 23 fichiers suivis modifiés et un rapport d'implémentation non suivi. Les
modifications de production identifiées restent dans le client HTTP, l'agent
pour métriques de requête, la projection F3/F5 et la configuration; aucun diff
n'a été trouvé dans Landlock, les outils workspace, les politiques d'écriture
ou le REPL classique. Le tag `v1.0.0-technical` pointe toujours vers
`b1d194288b02b5df094953affd763776991d3e0a`. Les worktrees UI-4 et packaging
n'ont pas été modifiés. L'ancien worktree `/tmp/lofiterm-http-stream-timeouts`
reste seulement déclaré `prunable`; ses métadonnées n'ont pas été nettoyées.

## Matrice de revue

| Point | Fichier(s) et lignes | Constat et preuve | Gravité | Correction requise |
| --- | --- | --- | --- | --- |
| Priorité des variables | `src/main.cpp:27-63`, `.env.example:4-8`, `USAGE.md:31-54` | Chaque nouvelle variable explicite remplace son fallback. Connecte = défaut `min(legacy, 10)`, idle = legacy (60 par défaut), total = 300. `LLM_TIMEOUT_SECONDS` seul ne fixe donc plus la borne totale de l'exécutable à 60 s; les nouvelles variables explicitement présentes dans un `.env` copié ont priorité. Ce comportement est écrit dans USAGE. | Conforme, changement sémantique documenté | Commentaire `.env.example` corrigé pour décrire uniquement le fallback idle de l'exécutable. |
| Valeurs invalides | `src/runtime_config.hpp`, `src/main.cpp:36-42`, `tests/runtime_config_test.cpp` | `std::from_chars` valide toute la chaîne et rejette suffixes, espaces, valeurs non positives et overflow. Le fallback et l'avertissement de `main` sont conservés. Tests ajoutés pour valeur valide et valeurs invalides, dont `20seconds`. | Corrigé et couvert par test | Aucune. |
| Compatibilité des appelants | `include/http_client.hpp:22-39`, `src/http_client.cpp:225-268`, `include/agent.hpp:22-28`, `src/agent.cpp:375-377` | Les anciennes surcharges `long` et `AgentConfig::timeout_seconds` restent disponibles. La surcharge historique mappe le timeout sur idle et total et garde connect borné à 10 s au plus. Une `AgentConfig` sans `http_timeouts` suit aussi son ancien plafond total. Initialisations agrégées compilent, avec avertissements de champ optionnel omis. | Conforme source; risque ABI non évalué | Aucun pour les appelants source du dépôt. ABI externe non garantie/documentée. |
| Connexion | `src/http_client.cpp:178-210` | `CURLOPT_CONNECTTIMEOUT` reçoit `connect_seconds`; les trois valeurs positives sont validées avant l'appel curl. Les délais sont réellement transmis séparément. L'erreur connect est distinguée par le chemin d'erreur curl et le temps écoulé. Aucun test dédié ne force une connexion lente et ne vérifie le message connect. | Risque de couverture, pas de défaut reproduit | Ajouter éventuellement un test déterministe du diagnostic connect si le mock peut retarder l'établissement; ne bloque pas le comportement total/idle démontré. |
| Inactivité | `src/http_client.cpp:61-129`, `178-191`, `200-204` | Ce n'est pas `CURLOPT_LOW_SPEED_TIME`. `XFERINFOFUNCTION` compare l'horloge monotone depuis le dernier en-tête ou octet de réponse; une absence de données après connexion retourne 1 et est reconnue via `idle_timed_out`. Le mock qui envoie les en-têtes puis retarde le corps vérifie l'échec idle. | Conforme, démontré par test | Aucune. Limite de précision liée à la cadence d'appel du callback libcurl, sans défaut observé. |
| Durée totale et classification | `src/http_client.cpp:178-210`; tests `tests/http_client_test.cpp:204-215`, `tests/agent_events_test.cpp:376-405` | `CURLOPT_TIMEOUT` borne la durée globale. L'oracle Agent utilise maintenant idle=5 s, total=2 s et passe les événements d'échec. Le test HTTP total utilise connect=1 s et total=1 s après réception immédiate des en-têtes; il vérifie le message total. La distinction des deux échéances égales n'est pas intrinsèquement déterminable, mais ce test a déjà franchi la connexion avant le délai de corps. Aucun faux succès n'est observé. | Conforme avec couverture perfectible | Pour une preuve plus nette, utiliser dans le test HTTP un connect supérieur au total tout en confirmant la connexion, et/ou classer la phase via l'état de connexion plutôt que seulement `elapsed < total`. Pas de mauvaise classification reproduite. |
| Erreurs HTTP et callbacks | `src/http_client.cpp:75-103`, `193-220`, `225-268` | Les exceptions des callbacks restent capturées hors de la pile C libcurl puis relancées. Le statut non-2xx et le comportement d'accumulation du corps restent présents; `post()` et `post_streaming()` partagent le même chemin et les anciennes signatures sont conservées. Un test Agent couvre le callback qui échoue pendant `SseParser::flush()` et vérifie les événements terminaux. | Conforme, avec réserve de confidentialité ci-dessous | Aucun défaut de retour d'erreur démontré. |
| Corps brut / raisonnement | `src/http_client.cpp:89-99`, `137-150`, `253-268`; `src/agent.cpp:342-349` | Les champs de raisonnement ne sont ni envoyés dans `TextDelta`, ni copiés dans les compteurs ou l'historique de métriques; le test sentinelle le vérifie. Le sink accumule temporairement le corps HTTP complet dans une `std::string` pour le contrat de retour/diagnostic de `post_streaming`; il inclut les octets SSE de raisonnement jusqu'au retour. Le rapport initial est maintenant explicite sur ce tampon temporaire. Le corps brut peut être inclus dans `HttpError` non-2xx, comportement préexistant. | Limitation réelle, non persistante | La rétention mémoire temporaire est documentée. Si l'exigence interdit aussi le tampon transitoire ou l'affichage du corps d'erreur non-2xx, il faudra arbitrer un changement de contrat HTTP distinct. |
| Compteurs SSE | `src/agent.cpp:295-302`, `316-360`, `356-389`; `include/ui/agent_event.hpp:90-101`; `src/ui/metrics.cpp:255-280`; `src/ui/tui_view.cpp:765-805` | `response_bytes` compte les octets du corps, `sse_data_frames` chaque charge utile remise à `consume` (`[DONE]` inclus), et `reasoning_frames` une trame dont le delta `choices[0]` contient une des deux clés non-nulles. Saturation arithmétique signalée. F3 étiquette des trames, jamais des tokens/s. Les assertions vérifient taille du corps, 4 trames, 2 trames reasoning et absence des sentinelles dans les `TextDelta`. | Conforme aux définitions implémentées; sémantique de compte documentable | Aucune correction. Le compteur est un compte de trames contenant les clés, pas une mesure de longueur ou de volume du raisonnement. |
| Complétude SSE | `src/agent.cpp:320-340`, `375-421`; `tests/agent_events_test.cpp` (cas malformés, missing DONE, length, flush) | `[DONE]` demeure requis; les JSON malformés deviennent une erreur protocolaire générique sans contenu brut; `finish_reason=length` et flux interrompu restent en échec sans réponse assistant complète dans l'historique. Le flush final est inclus avant `HttpRequestFinished`; son exception produit un événement HTTP échoué. Ces cas sont présents dans le harnais. | Conforme, résultats historiques attestés | Aucune. |
| Tests de délais | `tests/http_client_test.cpp:155-215`, `tests/agent_events_test.cpp:376-410`, `tests/CMakeLists.txt:45-55` | Le flux 62 s (idle 5, total 70) compare le corps accumulé et le callback et ne se termine qu'après 60 s. Le cas idle attend après en-têtes; le cas total coupe un flux actif. Le timeout CTest HTTP est 80 s. Résultats consignés : harnais 303/8 692; CTest 14/14. Ils ne sont pas réexécutés dans cette revue. | Couverture solide; cas connect spécifique absent | Aucune correction bloquante. Ajouter un test connect est une amélioration ciblée facultative. |
| Rapport et documentation | `docs/reports/2026-11-12-http-sse-timeouts-and-metrics.md`, `USAGE.md:31-54`, `.env.example:7` | Le rapport initial décrit maintenant la rétention temporaire du corps HTTP et l'absence de stockage dans métriques/historique/journaux sur le chemin réussi. Les résultats restent attribués à la campagne précédente. | Conforme après correction documentaire | Aucune. |

## Résultats antérieurs examinés

Le rapport d'implémentation consigne un build propre, le harnais **303 cas /
8 692 assertions**, CTest **14/14** (103,56 s), et le flux mock actif de 62 s.
La correction de l'oracle Agent donne idle=5 s / total=2 s. Ces nombres sont
cohérents avec les sources/tests examinés. Ils n'ont pas été rejoués pour cette
revue. Le premier CTest propre avait échoué sur l'ancien oracle où idle et
total étaient tous deux à 1 s; le rapport décrit le changement de configuration
du test et la réussite ultérieure, sans masquer cet échec initial.

Le `git diff --check` et le lien du rapport initial avaient été vérifiés dans
la campagne précédente. Aucune commande de test ni modification de production
n'a été exécutée pendant la présente revue.

## Anomalies et décision

- **Défaut bloquant démontré : aucun** dans les sources parcourues.
- **Risques réels non reproduits :** suffixe accepté après nombre par
  `std::stol`; diagnostic connect/total inféré par horloge plutôt que par état
  explicite; test de connexion dédiée absent.
- **Limitation :** tampon HTTP garde temporairement le corps SSE brut en mémoire
  et le corps d'erreur non-2xx peut être inclus dans `HttpError`; aucun stockage
  durable ou texte de raisonnement dans les métriques n'a été constaté.
- **Améliorations facultatives :** test déterministe de connexion lente et
  commentaire `.env.example` plus précis.

## Verdict de la revue initiale

**GO AVEC RÉSERVES** à la revue initiale. Le candidat est techniquement cohérent pour la correction
du plafond total de 60 secondes : le délai d'inactivité est un détecteur
temporel explicite, le plafond total reste dur, les flux incomplets restent en
échec, et les mesures ne sont pas confondues avec des tokens. Avant PR, corriger
la formulation trop absolue sur la rétention du raisonnement et idéalement
resserrer le parseur de valeurs entières. Aucun changement de code n'avait été
effectué à ce stade.

## Clôture après corrections ciblées

Le parseur accepte maintenant uniquement une chaîne entière positive complète;
`.env.example` décrit le fallback legacy du délai idle de l'exécutable sans
référence erronée aux appelants C++. Le rapport d'implémentation précise que
le corps SSE brut, y compris les champs de raisonnement, reste temporairement
en mémoire dans le sink HTTP et qu'un corps non-2xx peut être inclus dans
l'exception, sans conservation dans les métriques ou l'historique de
conversation sur le chemin réussi.

Validation après ces changements :

- Build incrémental C++20 sur le répertoire propre configuré : réussi.
- `agent_tests --filter=agent` : **43 cas, 900 assertions, zéro échec**.
- Harnais complet : **304 cas, 8 702 assertions, zéro échec** sur les deux
  dernières exécutions complètes.
- CTest : **14/14 groupes PASS**, zéro échec, 103,52 s.
- `git diff --check` et liens du rapport : à confirmer à la clôture finale.

Une exécution du harnais complet entre les deux passes a retourné une assertion
en échec, mais la sortie capturée était tronquée avant le nom du cas. Elle ne
s'est pas reproduite dans les deux répétitions complètes suivantes ni dans
CTest; sa cause reste inconnue. Ce constat est conservé comme réserve.

Les réserves restantes sont la classification connect/total inférée du temps
écoulé, l'absence d'un test dédié de connexion lente, l'absence de comparaison
de payload AnythingLLM, l'absence de nouvelle recette Ghostty, et cette
assertion isolée non identifiée.

## Verdict final

**GO AVEC RÉSERVES — PRÊT À ÊTRE COMMITÉ ET PROPOSÉ EN PR.** Le défaut de parsing
des entiers est corrigé et testé; les deux dernières exécutions du harnais et
CTest passent. La première exécution complète après le changement a eu un
échec isolé non identifié, non reproduit ensuite; il doit rester visible dans
la description de la PR. Aucun commit, push, PR ou merge n'a été effectué.
