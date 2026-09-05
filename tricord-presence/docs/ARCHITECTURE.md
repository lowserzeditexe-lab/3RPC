# Architecture — récapitulatif des décisions prises en discussion

## Découpage en 3 composants

1. **Sysmodule `tricord_presenced`** (résident, chargé au boot par Luma3DS)
   - Détecte idle (menu HOME) vs en-jeu via APT/AM
   - Maintient la connexion websocket Gateway Discord
   - Envoie les mises à jour de présence (opcode 3)
   - Expose l'état courant à un port IPC local (`presence:d`) pour le plugin

2. **Plugin Luma3DS `.3gx`** (injecté dans le process du jeu, slot "default")
   - Interroge le sysmodule via IPC
   - Affiche un HUD "Vous jouez à `<jeu>`" par hook GSP/GPU
   - Sans état persistant : peut être rechargé à chaque changement de jeu sans problème

3. **Installeur `.3dsx`** (grand public, **version CLI sans UI graphique pour l'instant**)
   - Copie le sysmodule compilé vers `/luma/sysmodules/`
   - Copie le plugin vers `/luma/plugins/default.3gx`
   - Crée la config (token Discord)
   - Prévient l'utilisateur qu'un reboot est nécessaire + que le
     "Plugin loader" Rosalina doit être activé

## Pourquoi cette architecture (contexte de la discussion)

- Un plugin `.3gx` seul ne peut pas héberger la connexion Discord
  persistante : il vit et meurt avec le process du jeu.
- TriCord lui-même est un client Discord "complet" (connexion Gateway
  directe via un token utilisateur), pas un simple relais RPC façon
  PC — donc pas besoin d'IPC vers un client desktop, l'Update Presence
  se fait directement sur la connexion déjà ouverte.
- Contrainte ToS Discord assumée consciemment : token utilisateur en
  connexion permanente = usage de type "self-bot", risque à connaître
  avant publication.

## État après complétion (juin 2026) — voir RAPPORT.md pour le détail

| Point | Résolution |
|---|---|
| Exheader du sysmodule | `sysmodule/tricord_presenced.rsf` (modèle Plug-n-play + Rosalina), TID `000401300F000102`, services APT:S/A/U, fs:USER, soc:U, ndm:u, ac:u |
| Title ID actif | `APT:GetAppletManInfo` (0x0005) + `APT:GetAppletInfo` (0x0006, AppID 0x300), codes vérifiés sur 3dbrew |
| Hook overlay GSP | CTRPluginFramework `OSD::Run` (hook MITM GSP fourni par la lib) |
| Chaîne 3gx | `plugin/Makefile` : ELF + `3gx.ld` puis `3gxtool -s` |
| Gateway Discord | mbedtls + wslay + jansson (portlibs devkitPro), thread dédié, testé sur hôte |
| IPC | port global `presence:d` (svcCreatePort nommé), thread `svcReplyAndReceive` |
| Lancement au boot | **non résolu** : Luma mainline ne lance pas un sysmodule custom seul ; l'installeur le lance via `pm:app` (méthode Plug-n-play) |

## Distribution

- `.3dsx` (Homebrew Launcher) et `.cia` (FBI, QR `dist/qr.html`) de
  l'installeur, qui embarque sysmodule + plugin + base de titres en romfs.
- L'option "Plugin loader" de Rosalina et "Enable loading external FIRMs and
  modules" de Luma doivent être activées manuellement par l'utilisateur —
  instructions affichées par l'installeur.
