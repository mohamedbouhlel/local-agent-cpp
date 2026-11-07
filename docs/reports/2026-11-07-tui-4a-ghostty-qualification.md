# TUI-4A — Qualification manuelle Ghostty

**Date du rapport :** 2026-11-07
**SHA qualifié :** `51ae20867357b1b873657585b70d813f7f328194`
**PR d'intégration :** #30, fusionnée dans `main` à ce SHA
**Statut de preuve :** résultats manuels rapportés par le mainteneur; non reproduits indépendamment par l'auteur de ce rapport.

## Environnement rapporté

- Debian 13.
- Ghostty, `TERM=xterm-ghostty`.
- TUI ncursesw intégrée au produit à `51ae20867357b1b873657585b70d813f7f328194`.
- Génération avec le moteur LLM réel configuré par le mainteneur.

## Résultats manuels

Le mainteneur déclare **20 contrôles sur 20 réussis**. Les observations transmises incluent :

- La navigation entre les vues pendant que le streaming continue.
- L'envoi d'un message par F9 et la fermeture volontaire par F10.
- La conservation du brouillon, du curseur et du scroll à travers les changements de vue.
- L'absence de duplication observée pendant streaming/navigation.
- La réception progressive d'une réponse du moteur LLM réel pendant la consultation d'une autre vue.
- La restauration du terminal à la fermeture.

La liste détaillée des vingt manipulations n'a pas été fournie dans cette mission; ce rapport n'invente donc pas vingt scénarios distincts. Les éléments ci-dessus sont un résumé des confirmations reçues, pas des observations refaites indépendamment.

## Résultats automatisés rapportés

Pour le build de qualification au SHA indiqué, le mainteneur rapporte :

- Harnais complet : **214 cas, 1 345 assertions, zéro échec**.
- CTest : **12/12 groupes réussis**.
- TUI-4A : **15 cas, 99 assertions**.
- TUI-3 : **13 cas, 50 assertions**.

Ces nombres sont des résultats rapportés dans le contexte de qualification. Aucun build, CTest ou harnais n'a été exécuté à nouveau pendant cette clôture documentaire.

## Limites

- SSH n'a pas été qualifié.
- Les autres terminaux et émulateurs ne sont pas couverts par cette observation Ghostty.
- La conformité Unicode universelle, notamment les variantes de largeur d'emoji et le regroupement de tous les graphèmes, n'est pas établie.
- La qualification concerne TUI-4A au SHA mentionné; elle ne valide pas les futurs panneaux enrichis, la rétention en session longue ni TUI-4B/4C.
- Aucun résultat ne valide un accomplissement fonctionnel déclaré par le LLM; le test réel établit la réponse progressive et le comportement de l'interface rapportés.

## Verdict

**Qualification Ghostty réussie sur le périmètre TUI-4A testé, selon le rapport du mainteneur : 20/20 contrôles.** Il s'agit d'une confirmation manuelle rapportée, pas d'une reproduction indépendante ni d'une garantie générale sur les terminaux.
