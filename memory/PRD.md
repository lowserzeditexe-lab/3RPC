# PRD — TriCord Presence (homebrew 3DS, complétion de squelette)

## Problème d'origine
Compléter (sans réécrire) le squelette `tricord-presence` : sysmodule Luma3DS (détection jeu actif + Gateway Discord),
plugin .3gx (overlay "Vous jouez à <jeu>"), installeur CLI (.3dsx + .cia), QR code FBI, rapport.
Contraintes : installeur CLI pur, TODO(hardware) conservés si non validables, sources documentées, aucun token en dur,
ToS Discord (token utilisateur) assumé.

## Choix utilisateur
- URL .cia : placeholder + `dist/gen_qr.py <url>`
- Base titres : `titles.txt` sur SD généré depuis 3dsdb par script Python
- Blocages env : documenter et continuer
- Rapport : `RAPPORT.md` (français) ; priorité : chaîne de build complète

## Architecture / emplacement
`/app/tricord-presence/` (le template React/FastAPI de /app est inutilisé).
Toolchain : devkitARM extrait de l'image Docker devkitpro/devkitarm → /opt/dkp_root/opt/devkitpro (hors /app : à
réinstaller via tools/env.sh + tools/build_host_tools.sh + build.sh si le pod est recréé).

## Implémenté (juin 2026)
- sysmodule : apt_monitor (APT 0x0005/0x0006), discord_gateway (mbedtls+wslay+jansson, testé sur hôte contre la vraie
  gateway), ipc_server (port global presence:d, thread), title_db, log, rsf, Makefile → 000401300F000102.cxi
- plugin : CTRPluginFramework OSD, Makefile 3gxtool, 3gx.ld, plgInfo → tricord_overlay.3gx
- installer : copyFile/ensureDir/swkbd token/romfs, lancement pm:app (Plug-n-play), rsf, cible cia → .3dsx + .cia
- tools : env.sh, build_host_tools.sh, gen_assets.py, gen_titles_db.py, host_gateway_test/
- dist : gen_qr.py, qr.html (placeholder), artefacts
- RAPPORT.md, README.md, docs/ARCHITECTURE.md mis à jour
- Tests : test_reports/iteration_1.json — 7/7 OK (build, clean rebuild, ctrtool, gateway hôte, QR, titles, statique)

## Backlog / TODO(hardware)
P0 : validation sur console (APT depuis sysmodule, MemoryType, IPC, OSD), lancement au boot (exheader deps ou autorun)
P1 : vérification TLS (CA), Sec-WebSocket-Accept, README utilisateur final, URL réelle du .cia + QR
P2 : UI graphique de l'installeur, icône/bannière, désinstallation

## Itération 2 (juin 2026)
- RAPPORT.md §0 : clarification — test gateway hôte = faux token, IDENTIFY rejeté 4004 ; seuls TLS/WS/HELLO/CLOSE validés
- TLS : VERIFY_REQUIRED + bundle CA embarqué (tools/gen_ca_bundle.py → discord_ca_bundle.h), testé accepté/refusé sur hôte
- Presence idle : status "idle" + custom status type 4 "Sur le menu HOME"
- QR : placeholder conservé (pas de repo GitHub)
- Auto-boot exheader.bin : mécanisme + risque expliqués à l'utilisateur, EN ATTENTE de sa validation avant code
