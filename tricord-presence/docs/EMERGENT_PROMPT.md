# Prompt pour Emergent (Fable 5) — finalisation du projet TriCord Presence

Copie-colle (et adapte si besoin) le bloc ci-dessous dans Emergent.

---

Je te fournis le squelette d'un projet homebrew Nintendo 3DS (CFW
Luma3DS) appelé "TriCord Presence", en 3 composants distincts. Ton
rôle est de COMPLÉTER le code là où des commentaires `TODO(hardware)`
l'indiquent, PAS de tout réécrire depuis zéro — respecte l'architecture
et les fichiers fournis.

## Environnement à installer d'abord

- devkitARM + devkitPro complet, groupe `3ds-dev` (via `dkp-pacman`)
- Packages : `3ds-libctru`, `3ds-citro3d`, `general-tools`, `3dstools`,
  `bannertool`, `makerom`
- Port `mbedtls` pour 3ds (TLS) et port `wslay` pour 3ds (WebSocket) —
  si aucun port officiel n'existe déjà pour devkitARM, cherche des
  sources tierces déjà utilisées par des homebrews Discord existants
  (ex: le projet "Sleepy Discord" / "yourWaifu/sleepy-discord" compile
  déjà pour 3DS et peut servir de référence ou être réutilisé tel quel)
- `3gxtool` (https://github.com/Nanquitas/3gxtool) pour compiler le
  plugin Luma3DS `.3gx`

## Contraintes à respecter impérativement

1. **L'installeur reste en mode CLI pur pour cette version** : pas
   d'interface graphique, pas de romfs avec assets visuels, juste la
   console texte `consoleInit` déjà en place dans
   `installer/source/main.c`. N'ajoute pas de banner/icône soignée ni
   de menu graphique — ce sera fait dans une itération future.
2. Ne supprime aucun commentaire `TODO(hardware)` sans l'avoir
   réellement résolu et testé — si tu ne peux pas valider un point sur
   hardware/émulateur, laisse le TODO en place plutôt que de deviner
   silencieusement une implémentation qui a l'air correcte.
3. Le sysmodule et le plugin touchent à l'OS bas niveau de la 3DS
   (exheader, IPC custom, hook GSP) : documente clairement, pour
   chaque point incertain que tu résous, la source que tu as utilisée
   (3dbrew.org, code d'un projet homebrew existant, etc.) pour qu'on
   puisse le revérifier.
4. Ne code jamais de token/secret en dur dans le code — le token
   Discord doit venir du fichier de config créé par l'installeur
   (`sdmc:/3ds/tricord-presence/config.txt`).
5. Le risque ToS Discord (connexion permanente avec un token
   utilisateur, pas un bot) est un choix déjà assumé — pas la peine de
   proposer une alternative "bot officiel", juste implémenter proprement.

## Ce qu'il reste à finaliser (par priorité)

1. `sysmodule/source/apt_monitor.c` : implémenter
   `aptMonitorGetActiveTitleId` (Title ID du jeu au premier plan) —
   vérifier les codes de commande exacts sur 3dbrew.org (services APT,
   AM, éventuellement pm:app).
2. `sysmodule/source/discord_gateway.c` : brancher une vraie connexion
   TLS+WebSocket vers `wss://gateway.discord.gg`, gérer HELLO/IDENTIFY/
   HEARTBEAT/RESUME, et implémenter le payload Update Presence (op 3).
3. `sysmodule/` : écrire le fichier `.rsf` (exheader) manquant avec les
   droits de service nécessaires, et compléter `build.sh` pour l'appel
   `makerom` (converti .elf en .cxi).
4. `plugin/source/overlay_draw.c` : implémenter le hook GSP réel pour
   dessiner le texte à l'écran par-dessus le jeu hôte. Inspire-toi de
   plugins `.3gx` open-source existants qui font déjà de l'overlay
   texte plutôt que de partir de zéro.
5. `plugin/Makefile` : brancher la vraie chaîne de compilation 3gxtool.
6. `sysmodule/source/ipc_server.c` et `plugin/source/presence_client.c` :
   implémenter la requête/réponse IPC réelle entre les deux (actuellement
   stubs qui ne transportent pas de vraies données).
7. `sysmodule/source/title_db.c` : charger une vraie base Title ID →
   nom de jeu (s'inspirer de la base utilisée par le projet 3DS-RPC,
   qui utilise une version modifiée de 3dsdb).
8. `installer/source/main.c` : implémenter réellement `copyFile` et
   `ensureDir` (actuellement stubs qui ne font que logger), et la
   saisie du token Discord au clavier (SwkbdState de ctrulib) plutôt
   que de laisser l'utilisateur éditer le fichier config à la main.

## Livrable attendu

- Le même arbre de fichiers, complété (pas de refonte de structure)
- `build.sh` qui compile les 3 composants sans erreur avec devkitARM installé
- Un rapport texte listant : ce qui a été implémenté, ce qui reste en
  TODO faute de pouvoir être validé sans hardware/Citra réel, et tout
  point où tu as dû deviner un comportement non documenté.
