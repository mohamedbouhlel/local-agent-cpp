# HTTP/SSE : délais distincts et mesures de flux

Date : 12 novembre 2026
Branche : `fix/http-stream-timeouts-recovered`
Worktree : `/tmp/lofiterm-http-stream-timeouts-recovered`
SHA de base : `fcd50afea1fcb93eef0d5573a458e667e72f25f8`. Le candidat testé est ce SHA avec les modifications locales décrites ci-dessous; aucun commit de candidat n'existe.

## Cause et portée

Le client configurait `CURLOPT_TIMEOUT` à 60 secondes. Cette option limite la
durée totale de la requête; elle ne représente pas un délai d'inactivité. Une
génération qui continue à transmettre des données peut donc être interrompue à
60 secondes même si le serveur travaille encore. Le mock HTTP reproduit le
comportement avec un flux actif de 62 secondes.

Ce constat explique le plafond côté client. Il ne prouve pas que la session
AnythingLLM et celle de LOFITERM avaient le même payload ou les mêmes paramètres
serveur. Aucun payload/capture de requête AnythingLLM ni prompt exact de la
session d'environ 87 secondes n'était disponible pour une comparaison.

## Changements réalisés

Trois délais positifs sont désormais configurables indépendamment :

| Variable | Valeur par défaut de l'exécutable | Fonction |
| --- | ---: | --- |
| `LLM_CONNECT_TIMEOUT_SECONDS` | 10 s, ou le plus petit de `LLM_TIMEOUT_SECONDS` et 10 s | établissement de connexion |
| `LLM_IDLE_TIMEOUT_SECONDS` | 60 s | absence de données de réponse; les en-têtes et octets reçus réinitialisent le délai |
| `LLM_TOTAL_TIMEOUT_SECONDS` | 300 s | plafond mural total, même si le flux reste actif |

`LLM_TIMEOUT_SECONDS` reste accepté comme valeur de repli du délai d'inactivité
par l'exécutable. Les appels C++ historiques à `AgentConfig` et aux anciennes
surcharges `HttpClient` gardent leur plafond total historique. Pour l'exécutable,
si seule l'ancienne variable est fournie, le délai total reste le nouveau défaut
de 300 s; cette différence est explicitée dans `USAGE.md` et `AGENTS.md`.
Les valeurs sont analysées comme des entiers positifs complets : les suffixes,
espaces résiduels, valeurs nulles/négatives et dépassements sont rejetés et
déclenchent le fallback avec avertissement.

Le client libcurl distingue les erreurs de connexion, d'inactivité et de durée
totale. L'agent conserve la complétude protocolaire : `[DONE]` reste requis,
`finish_reason="length"` et les flux coupés restent des échecs, et le texte
assistant incomplet n'est pas ajouté comme réponse achevée à l'historique.

Les compteurs distinguent les octets du corps HTTP, les trames SSE `data:` et
les trames contenant les métadonnées `reasoning`/`reasoning_content`. Le texte
de raisonnement n'est pas envoyé dans les événements UI, l'historique des
métriques ou les journaux; seul le nombre de trames concernées est compté. Le
transport accumule toutefois temporairement le corps SSE brut en mémoire dans
le tampon de réponse existant, qui comprend aussi les champs de raisonnement.
Ce tampon est détruit au retour de la requête sur le chemin réussi. En cas de
statut HTTP non-2xx, le corps brut peut être inclus dans `HttpError`, comme dans
le comportement préexistant. Ces nombres ne sont pas des tokens ni un débit de
génération. Les compteurs saturent explicitement en cas de dépassement.

Le flush de la dernière trame SSE a aussi été placé dans le chemin de clôture
d'événement HTTP : une exception de callback pendant ce flush publie maintenant
un résultat HTTP échoué avant de remonter l'exception d'origine.

## Fichiers

Modifications locales (aucun commit) :

- `.env.example`, `AGENTS.md`, `USAGE.md`
- `include/agent.hpp`, `include/http_client.hpp`
- `include/ui/agent_event.hpp`, `include/ui/metrics.hpp`, `include/ui/tui_view_data.hpp`
- `src/agent.cpp`, `src/http_client.cpp`, `src/main.cpp`
- `src/ui/metrics.cpp`, `src/ui/presentation_controller.cpp`, `src/ui/tui_view.cpp`, `src/ui/tui_view_data.cpp`
- `tests/CMakeLists.txt`, `tests/agent_events_test.cpp`, `tests/http_client_test.cpp`
- `tests/mock_http_server.cpp`, `tests/mock_http_server.hpp`
- `tests/tui_navigation_test.cpp`, `tests/ui_metrics_test.cpp`, `tests/runtime_config_test.cpp`
- `src/runtime_config.hpp`
- ce rapport et son entrée dans `docs/README.md`

Les anciennes surcharges C++ restent disponibles. Aucune modification n'a été
apportée à Landlock, aux outils workspace, à l'autorisation d'écriture, au
framing SSE, ni au contenu imprimé par le REPL classique.

## Validation exécutée

Environnement observé : Debian GNU/Linux 13.7, noyau Linux 6.12.115,
GCC 14.2.0, CMake 3.31.6 et libcurl 8.14.1; build Debug avec
`AGENT_BUILD_TESTS=ON`.

- Une configuration et compilation C++20 propres ont réussi dans
  `/tmp/lofiterm-http-stream-final-clean` avant le durcissement final du
  parseur de configuration. Après ce dernier changement, le rebuild
  incrémental du programme et des tests concernés a réussi dans ce même
  répertoire.
- Le téléchargement propre de la dépendance déclarée `nlohmann/json` a d'abord
  échoué à cause du DNS restreint dans le sandbox; après autorisation réseau,
  CMake l'a téléchargée et la compilation propre a atteint 100 %.
- CMake a signalé son avertissement de développement CMP0135. La compilation
  signale aussi `-Wmissing-field-initializers` sur des agrégats `AgentConfig`
  historiques qui n'initialisent pas le nouveau champ optionnel; aucun de ces
  avertissements n'a fait échouer le build.
- `agent_tests --filter=agent` avec loopback autorisé : **42 cas,
  890 assertions, zéro échec** avant le durcissement du parseur; après celui-ci,
  **43 cas, 900 assertions, zéro échec**.
- Harnais complet après durcissement : **304 cas, 8 702 assertions, zéro échec**
  sur les deux dernières exécutions complètes.
- CTest complet après durcissement : **14/14 groupes**, zéro échec, durée
  totale **103,52 s**. Le groupe HTTP a pris 67,37 s, Agent 3,35 s et TUI-3
  23,60 s.
- Le groupe HTTP comprend un flux mock actif de 62 secondes avec un délai
  d'inactivité de 5 s et un plafond total de 70 s. Il s'est terminé sans
  perte ni duplication, démontrant le dépassement de l'ancien plafond de
  60 secondes. Les tests couvrent aussi l'inactivité après les en-têtes, le
  plafond total, le flux interrompu, les métriques, `reasoning` non exposé,
  `[DONE]`, les trames malformées, les callbacks et les erreurs.
- Un premier lancement sandboxé du groupe Agent a échoué car le sandbox
  interdisait `socket()` loopback. Après autorisation d'accès loopback, le
  même groupe a passé 42/42 cas et 890/890 assertions. Le harnais et CTest
  complets ont ensuite été exécutés avec cette autorisation.
- Le premier CTest du nouveau build propre a échoué une fois sur l'assertion
  de message « total timeout » du cas `malformed_stream_still_obeys...` : le
  test utilisait une limite idle et une limite totale toutes deux à 1 s, ce
  qui rendait leur ordre d'expiration indéterminé. Le transport échouait bien;
  seul le type de délai attendu dans l'oracle était ambigu. Le test fixe
  désormais explicitement idle=5 s et total=2 s, tandis que le corps mock
  continue au-delà de 2 s. Il passe dans le groupe Agent ciblé, le harnais et
  le CTest final. Aucun code de production n'a été modifié pour ce constat.
- Après ajout du test du parseur, une première exécution du harnais complet a
  retourné 304 cas, 8 702 assertions et un échec. La sortie capturée était
  tronquée avant le nom du cas; sa cause n'est donc pas établie. Deux nouvelles
  exécutions complètes filtrées ont ensuite passé 304/304 et 8 702/8 702,
  tandis que CTest a passé 14/14. Ce résultat ponctuel non reproduit est
  conservé comme réserve, pas requalifié en succès rétroactif.
- Le nouveau test `agent.positive_seconds_config_accepts_only_complete_positive_integers`
  vérifie `20`, `20seconds`, `20s`, espaces, chaîne vide, zéro, valeur négative
  et dépassement numérique.
- `git diff --check` : réussi.

Un appel court au serveur LiteLLM configuré dans l'environnement du mainteneur
a renvoyé le marqueur de réponse attendu et la fin technique du tour, sans
erreur/timeout observé. Les valeurs de configuration n'ont pas été imprimées.
Cet appel n'était pas la génération de 87 secondes et ne reproduit donc pas
le cas utilisateur long.

## Limites et réserves

- La comparaison de payload avec AnythingLLM n'a pas été possible faute de
  capture de sa requête. Les différences de payload, modèle, paramètres ou
  politique serveur restent inconnues.
- La génération réelle d'environ 87 secondes n'a pas été répétée; le dépassement
  de 60 secondes est prouvé par le mock déterministe, pas par le serveur LLM.
- Aucun essai physique Ghostty n'a été effectué pour cette modification. La
  présentation des nouveaux nombres dans F3 est couverte par tests de projection,
  pas par une recette physique.
- Le plafond total par défaut de 300 secondes demeure une limite configurable;
  un flux actif plus long nécessite de définir `LLM_TOTAL_TIMEOUT_SECONDS`.
- Le compteur d'octets désigne le corps de réponse reçu par libcurl, pas les
  octets réseau incluant en-têtes/TLS. Les trames SSE comptent les événements
  `data:` remis au parseur, `[DONE]` inclus. Les trames de raisonnement ne
  mesurent ni longueur ni contenu du raisonnement. Le corps SSE brut, y compris
  les champs de raisonnement, est tamponné temporairement par `HttpClient`;
  il n'est pas conservé dans les métriques ou l'historique sur le chemin réussi.
  Un corps HTTP non-2xx peut rester présent dans le message `HttpError`.
- Les initialisations agrégées positionnelles d'`AgentConfig` dans les tests
  émettent un avertissement de champ optionnel omis. La compatibilité source
  est conservée et les tests/builds passent; le nettoyage de ces avertissements
  n'a pas été élargi à des fichiers sans rapport.

## Verdict

**PRÊT POUR REVUE.** Les délais séparés sont exercés par des tests déterministes,
le flux actif de 62 secondes dépasse l'ancien plafond sans timeout, tandis que
silence, plafond total et flux incomplets restent détectés. Les métriques
n'exposent pas le texte de raisonnement. La comparaison AnythingLLM et la
qualification physique restent explicitement non établies. Aucun commit, push,
PR ou merge n'a été effectué.
