# Qualification réelle HTTP/SSE au-delà de 60 secondes

Date du compte rendu : 13 novembre 2026 (Europe/Paris)
Révision qualifiée : `8421b9cbd8c8a93a1528913cb0034bed7a280d2c`
Worktree : `/home/oper/tmp/local-agent-cpp-http-real-60s` (branche documentaire locale `docs/http-sse-real-60s-qualification`, issue du SHA qualifié)
Binaire : `/home/oper/tmp/lofiterm-http-real-60s-build/agent`
SHA-256 du binaire : `e61ab5e9004ea34d9a20633e9626e6b6f50501e42828737dd29045a8f85e0c93`

## Résumé et verdict

Une requête réelle a été envoyée par l'exécutable LOFITERM compilé à la
révision indiquée, vers le serveur configuré du mainteneur. Le client a reçu
son premier delta après **73 430,975 ms**, donc il a continué à attendre au-delà
du seuil d'inactivité de 60 secondes. Il a ensuite reçu et affiché des données,
mais a terminé le tour par `incomplete model stream: missing [DONE] marker`.
La réponse n'a donc pas été acceptée comme complète.

**VERDICT : INCONCLUSIF.** Le critère de durée supérieure à 60 secondes est
établi par la latence du premier delta, mais le tour n'a pas terminé normalement
et `[DONE]` n'a pas été validé. L'observation ne permet pas d'attribuer la fin
incomplète à une régression du client, au serveur, au modèle ou à une limite de
génération. Le complément ciblé du 13 novembre a soumis une requête de
confirmation unique, mais elle n'a reçu aucun corps; elle ne confirme ni ne
réfute le comportement de génération observé ici. Aucune nouvelle génération
ou tentative de correction n'a été effectuée après cet essai.

## Référence, worktree et environnement

- `origin/main` interrogé en lecture seule a renvoyé
  `8421b9cbd8c8a93a1528913cb0034bed7a280d2c`. Le checkout principal local
  restait sur `fe681d3fb2f71f4a3aaa2b0d996df16f5896b923`; sa référence locale
  `origin/main` était également à cet ancien SHA. L'objet cible a été récupéré
  directement dans `FETCH_HEAD`, sans déplacer de branche. Le commit cible est
  le merge de la PR #50 et a pour parent `fe681d3…` et le commit documentaire
  `6fec2c8…`.
- Le worktree isolé a été créé détaché à la révision exacte; son état Git était
  propre avant ce rapport. Aucun code ni test n'a été modifié.
- OS : Debian GNU/Linux 13.7; noyau `6.12.115+deb13.1-amd64`, x86_64.
- Compilateur : GNU C++ 14.2.0; CMake 3.31.6; libcurl 8.14.1; ncursesw
  6.5.20250216.
- Exécution : interface classique de LOFITERM dans un pseudo-terminal fourni
  par l'environnement d'exécution; `TERM=dumb`, locale UTF-8. Ce n'était pas
  une recette physique Ghostty.
- L'application utilisait l'endpoint LiteLLM configuré localement. Le serveur
  était joignable en TCP et a répondu à la première requête. Une clé était
  configurée; son adresse et sa valeur n'ont pas été conservées dans ce rapport.
- Paramètres effectifs vérifiés après chargement local du `.env` : connexion
  10 s, inactivité 60 s, total 300 s. Ces valeurs n'ont pas été modifiées.
- Le workspace dédié était
  `/home/oper/tmp/lofiterm-http-real-60s-nsZQQe/workspace`; il est resté vide
  après le tour. L'instance a été fermée normalement par `exit`.

## Build et rattachement du binaire

Configuration propre, tests désactivés :

```sh
cmake -S /home/oper/tmp/local-agent-cpp-http-real-60s \
  -B /home/oper/tmp/lofiterm-http-real-60s-build -DAGENT_BUILD_TESTS=OFF
cmake --build /home/oper/tmp/lofiterm-http-real-60s-build --parallel 2
```

Les deux commandes ont réussi; `agent_core`, `agent_tui` et `agent` ont été
construits. Le `CMAKE_HOME_DIRECTORY` du cache correspond au worktree de
qualification et son HEAD était le SHA demandé. CMake a émis son avertissement
de politique FetchContent CMP0135; make a aussi signalé que
`/home/oper/tmp/GMfifo3` existait déjà, puis la compilation s'est achevée avec
succès. Aucun de ces avertissements n'a été corrigé ni supprimé.

## Configuration expurgée et lancement réel

Le `.env` existant du checkout principal a été sourcé sans `set -x`. Seuls
l'hôte/port, la présence de la clé et les délais numériques ont été affichés.
Lancement depuis le workspace temporaire :

```sh
set +x
set -a
source /home/oper/projects/local-agent-cpp/.env
set +a
/home/oper/tmp/lofiterm-http-real-60s-build/agent --ui=classic
```

Un premier lancement sandboxé a échoué immédiatement à ouvrir la connexion
(`Could not connect ... after 0 ms`); il n'a reçu aucun flux et n'est pas
compté comme l'essai réel. Le même binaire et la même configuration ont ensuite
été exécutés avec l'accès réseau autorisé. Une vérification TCP préalable a
réussi, puis la requête réelle a atteint l'application serveur.

Le prompt était une demande non sensible d'essai rédactionnel d'environ
2 800 mots sur le rôle des bibliothèques publiques dans la résilience des
communautés, sans outils, fichier ni donnée privée. Le texte de réponse et le
flux brut n'ont pas été enregistrés dans le rapport.

Commandes et opérations réellement exécutées, avec secrets exclus :

```sh
git ls-remote --heads origin main
git fetch --no-tags origin 8421b9cbd8c8a93a1528913cb0034bed7a280d2c
git worktree add --detach /home/oper/tmp/local-agent-cpp-http-real-60s \
  8421b9cbd8c8a93a1528913cb0034bed7a280d2c
cmake -S /home/oper/tmp/local-agent-cpp-http-real-60s \
  -B /home/oper/tmp/lofiterm-http-real-60s-build -DAGENT_BUILD_TESTS=OFF
cmake --build /home/oper/tmp/lofiterm-http-real-60s-build --parallel 2
mkdir -p workspace
set +x
set -a
source /home/oper/projects/local-agent-cpp/.env
set +a
/home/oper/tmp/lofiterm-http-real-60s-build/agent --ui=classic
```

`git ls-remote` a d'abord échoué dans le sandbox, puis a réussi avec accès
réseau et a donné le SHA indiqué. Le fetch ciblé a placé l'objet dans
`FETCH_HEAD` sans mettre à jour `origin/main`. Le contrôle de reachability était
un `socket.connect()` Python vers l'hôte et le port extraits du `LLM_BASE_URL`
chargé, avec délai de 4 s; il n'a affiché aucun secret. Le premier lancement
de l'application dans le sandbox a échoué immédiatement faute d'accès réseau;
le lancement expurgé ci-dessus est celui de l'unique génération réelle.

## Observations et mesures

| Mesure | Observation | Preuve / limite |
|---|---|---|
| Réseau | Connexion au serveur réel; l'application a reçu un corps de réponse. | Contrôle TCP réussi et requête exécutée par `agent`; aucun mock ou client alternatif. |
| Délai avant premier delta | `73 430,975 ms`. | Métrique `First delta` affichée par LOFITERM; elle prouve que le client n'a pas interrompu cette requête au seuil idle de 60 s. Ce n'est pas le TTFT serveur. |
| Progression | Le premier texte est apparu après 60 s; du texte a ensuite continué à être reçu/affiché. | Sortie PTY de l'application; la capture de sortie est tronquée et ne constitue pas une archive du texte. |
| Octets du corps HTTP | `498 749` octets reçus. | Valeur visible dans le récapitulatif partiel du client; ce n'est ni une mesure complète des octets réseau ni une quantité de tokens. |
| Statut HTTP | **NON OBSERVABLE dans la sortie conservée.** | Le récapitulatif terminal a été tronqué; aucun code de statut n'est consigné ici. |
| Trames SSE / fragments texte | **NON OBSERVABLE dans la sortie conservée.** | Le récapitulatif des compteurs a été tronqué. Aucun nombre n'est reconstitué. |
| Métadonnées de raisonnement | Aucun contenu n'a été consigné. Le nombre de trames concernées n'est pas récupérable. | Pas d'inspection ni d'enregistrement du corps SSE brut. |
| `[DONE]` | Pas observé directement dans un flux brut; le client a explicitement échoué avec `missing [DONE] marker`. | La complétude n'a pas été validée. Le marqueur n'est pas présenté comme directement capturé. |
| Fin du tour | Échec protocolaire explicite; sortie signalée comme potentiellement incomplète. | Message de l'application : `Reponse incomplete ou invalide du modele: incomplete model stream: missing [DONE] marker`, puis `Streamed model output may be incomplete; it is not a completed response.` |
| Délai total | Paramètre effectif 300 s; aucune erreur de timeout total n'a été observée. | Durée totale exacte non récupérable, car la partie centrale du récapitulatif PTY a été tronquée. Le transport est revenu à l'invite avec une erreur de complétude. |
| Effets d'outils / fichiers | Aucun effet d'écriture observé; workspace vide après fermeture. | Le prompt excluait les outils; inspection du workspace dédié après le tour. |

## Complément ciblé — distinction des états et recherche des sorties

Date du complément : 13 novembre 2026. Le rapport initial et l'index sont
restés présents. La recherche limitée aux répertoires du build, du worktree et
du workspace de qualification n'a trouvé aucun journal de session, sortie
PTY ou capture sauvegardée contenant l'événement `HttpRequestFinished` ou le
récapitulatif complet. Les seules valeurs détaillées encore disponibles sont
celles déjà consignées plus haut; la sortie terminal capturée lors de l'appel
était tronquée.

Le chemin de code au SHA qualifié permet toutefois d'établir la séquence
suivante, sans prétendre que chaque événement a été conservé dans une trace :

| Étape | Conclusion vérifiable | Preuve et limite |
|---|---|---|
| Transport / HTTP | `HttpClient::post_streaming()` est revenu normalement; le chemin `HttpRequestFinished(completed)` a été atteint avant l'erreur `missing [DONE]`. Le retour normal de `HttpClient` implique que libcurl a terminé sans erreur et que le statut HTTP était dans la plage 2xx. | `src/agent.cpp:385-408` publie l'événement de fin après le retour du client; `src/agent.cpp:413-414` vérifie ensuite `saw_done`. `src/http_client.cpp:200-220` lève sur erreur curl et sur statut hors 2xx avant le retour normal. L'événement a donc été soumis par le code, mais sa ligne de sortie/capture n'est pas disponible. Le code HTTP numérique n'est pas exposé dans `HttpRequestFinished` et reste inconnu. |
| Activité SSE | Le corps de réponse a apporté des données; les octets du corps observés sont 498 749. | `response_bytes` est compté pendant le callback et l'application a ensuite rendu du texte. Le nombre exact de trames `data:` n'est pas récupérable. Les octets de corps ne donnent pas le nombre de trames. |
| Fragments texte | Au moins un fragment texte a été rendu; le premier delta est mesuré à 73 430,975 ms. | Le message client `Receiving model stream; first delta observed...` et le texte visible l'établissent. Le nombre exact de `TextDelta` n'est pas présent dans les sorties conservées. |
| Complétude `[DONE]` | Le parseur/agent n'a pas reconnu le marqueur de fin; la réponse n'a pas été validée. | Après la fin HTTP normale et le flush du parseur, `src/agent.cpp:413-414` lève si `saw_done` reste faux. Cela établit l'état vu par le client, pas si le serveur a omis les octets du marqueur, si un intermédiaire les a retirés, ou si le découpage/framing les a empêchés d'être reconnus. Le flux brut n'a pas été capturé et ne doit pas être reconstitué. |

Le défaut immédiat observé est donc une **fin HTTP 2xx sans `[DONE]` accepté par
le client**, après activité SSE et rendu de texte. Le transport s'est terminé;
la réponse applicative, elle, est restée incomplète. Le statut HTTP exact, le
nombre de trames SSE et de fragments texte ne sont pas établis par une sortie
persistée. La raison pour laquelle le marqueur n'a pas été reçu/reconnu n'est
pas identifiable à partir des preuves existantes.

## Essai unique de confirmation ciblée — exécuté le 13 novembre

Une seule requête réelle de confirmation a été soumise au binaire indiqué plus
haut, au SHA qualifié, avec les paramètres existants et un workspace temporaire
vide. Le prompt était non sensible et n'invitait pas à appeler des outils. La
sortie standard, qui contient la réponse générée, a été redirigée vers
`/dev/null`. Seules des lignes de compteurs techniques autorisées par liste
blanche ont été laissées passer depuis la sortie de diagnostic; le texte du
prompt, les réponses, le raisonnement, les secrets et les trames SSE brutes
n'ont pas été capturés ni enregistrés. L'écho du terminal était désactivé et
rétabli à la sortie.

Mesures expurgées effectivement conservées :

| État | Observation | Portée |
|---|---|---|
| Transport HTTP côté client | `HttpRequestFinished` a rapporté `failed_or_aborted`, durée `10 011,317 ms`, corps `0` octet. | La ligne ne donne pas de code HTTP; aucun statut numérique n'est affirmé. La tentative n'a fourni aucune réponse HTTP exploitable dans les compteurs. |
| Activité SSE | `0` trame SSE, dont `0` trame avec métadonnée de raisonnement. | Aucun corps n'a été compté; aucune activité SSE n'est établie pour cette tentative. |
| Fragments texte | `0`; premier delta indisponible. | Aucun texte de réponse n'a été reçu par le client selon les compteurs filtrés. |
| Résultat du tour | `failed`, durée `10 013,059 ms`, une requête HTTP, zéro outil démarré, zéro fragment texte, zéro octet de corps. Erreur résumée `redacted_other`. | Le détail de l'erreur a été volontairement expurgé; sa classe précise n'est donc pas connue et ne doit pas être déduite. |
| `[DONE]` | Non observé et non validé. | Sans corps ni activité SSE, cette tentative ne permet pas d'évaluer la présence du marqueur dans une génération. |
| Workspace | Vide après la fermeture. | Aucun fichier ni sous-répertoire n'a été créé par cet essai. |

La durée d'environ dix secondes est proche du délai de connexion configuré à
10 s, mais le diagnostic détaillé a été masqué. Cette proximité ne suffit pas à
identifier la cause : **un timeout de connexion n'est pas démontré**. L'essai établit seulement que la requête de confirmation
n'a produit aucun corps ni événement de génération exploitable. Il ne constitue
pas une deuxième génération réelle et ne remplace pas le résultat antérieur à
73 430,975 ms.

La durée totale n'est pas publiée comme une mesure exacte. Le premier delta à
73,431 s établit néanmoins que l'attente et le flux ont franchi le seuil de
60 s. Le récapitulatif observé ne rapporte pas de timeout de durée totale; il
rapporte à la place un EOF de transport suivi du contrôle de complétude SSE.

## Résultats selon les critères

- **Durée réelle strictement supérieure à 60 s : PASS** — le premier delta
  client est mesuré à 73 430,975 ms.
- **Le flux continue sans interruption au seuil de 60 s : PASS pour cette
  requête** — aucun timeout idle à 60 s; des données sont arrivées après le
  premier delta. Cela ne qualifie pas tous les débits ou toutes les générations.
- **Fin normale et réponse complète : FAIL pour cet essai** — le client a
  rejeté le tour faute de `[DONE]`.
- **Respect prouvé du délai total : PARTIEL** — aucune erreur de timeout total
  n'est rapportée et l'application est revenue à l'invite, mais la durée finale
  exacte n'a pas été conservée.

Le verdict global reste **INCONCLUSIF**, suivant le contrat de mission : les
conditions nécessaires à `PASS` ne sont pas toutes satisfaites, mais cette
exécution unique ne démontre pas une régression client reproductible. Le client
a correctement refusé de présenter le flux comme complet. La cause de la
terminaison sans marqueur reste inconnue; les limites éventuelles de sortie du
serveur/modèle ne sont pas démontrées. La cause de l'absence de `[DONE]` reste
**non identifiée**, le statut HTTP numérique reste **non observé**, et la
qualification complète d'une génération réelle supérieure à 60 secondes reste
**non acquise**.

## Vérifications non exécutées et limites

- La requête de confirmation n'a pas démarré de génération observable : aucun
  corps, aucune trame SSE, aucun fragment texte et aucun premier delta. Aucun
  nouvel essai n'a été lancé.
- Pas de capture ou d'inspection du SSE brut; statut HTTP, nombre de trames et
  fragments exacts n'ont pas pu être récupérés de la sortie PTY tronquée.
- Pas de recette physique Ghostty; la session était un PTY avec `TERM=dumb`.
- Pas de CTest ni de harnais : la mission qualifie un parcours réel ciblé et
  demandait de ne pas élargir la campagne. Les tests automatisés historiques
  restent attribués à leurs SHA antérieurs.
- Pas de comparaison avec AnythingLLM; payload, paramètres et conditions
  identiques ne sont pas disponibles dans cette exécution.
- Le tour incomplet n'a pas été diagnostiqué côté serveur; aucun changement
  serveur, modèle, configuration ou code n'a été effectué.

## État Git à la clôture

Le worktree est sur la branche documentaire locale
`docs/http-sse-real-60s-qualification`, créée au SHA qualifié
`8421b9cbd8c8a93a1528913cb0034bed7a280d2c`; `main` n'a pas été déplacée. Les
seuls changements de fichiers sont ce rapport et son entrée dans
`docs/README.md`. Aucun code ni test n'a changé après le build. Le checkout
principal reste au SHA local `fe681d3…`, avec ses rapports non suivis
préexistants; sa référence locale `origin/main` n'a pas été déplacée. Les
worktrees historiques n'ont pas été nettoyés ni modifiés. Au moment des essais,
aucun commit, push, PR, merge ou tag n'avait été créé ou modifié; la présente
mission autorise uniquement le commit local de ces deux documents.
