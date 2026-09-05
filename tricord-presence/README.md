# TriCord Presence

Extension "Rich Presence" pour TriCord (client Discord non-officiel 3DS),
composée de 3 briques distinctes :

```
tricord-presence/
├── sysmodule/   → daemon résident (Luma3DS, /luma/sysmodules/000401300F000102.cxi)
│                  détecte l'état console (idle / en jeu) via APT et pousse
│                  les mises à jour de présence sur la Gateway Discord (TLS+WS)
├── plugin/      → plugin Luma3DS (.3gx, CTRPluginFramework), injecté dans le
│                  jeu en cours, affiche l'overlay "Vous jouez a <jeu>"
├── installer/   → homebrew CLI (.3dsx + .cia) : copie les fichiers sur la SD,
│                  base de titres, saisie du token Discord au clavier
├── tools/       → env.sh, build des outils hôte, générateurs (titres, assets)
├── dist/        → artefacts + gen_qr.py / qr.html (QR "Remote Install" FBI)
├── docs/        → notes d'architecture
├── RAPPORT.md   → ce qui est fait / TODO(hardware) / points devinés + sources
└── build.sh     → compile les 3 composants dans l'ordre
```

## ⚠️ État

Le code compile de bout en bout et le client Gateway Discord a été validé
sur hôte Linux contre la vraie gateway, mais **rien n'a été exécuté sur
console ni sur Citra**. Les zones `// TODO(hardware)` listent ce qui reste
à valider (voir `RAPPORT.md`). Testez d'abord sur une console de test.

## Prérequis de build

- devkitARM + devkitPro : `dkp-pacman -S 3ds-dev 3ds-mbedtls 3ds-wslay 3ds-jansson`
- Outils hôte : `tools/build_host_tools.sh` (makerom, ctrtool, 3gxtool,
  bannertool ; nécessite `libyaml-cpp-dev`)
- CTRPluginFramework : installé automatiquement par `build.sh` si absent
- Python 3 (+ `pip install qrcode` pour le QR)

## Build

```bash
source tools/env.sh
./build.sh                                   # → dist/
QR_URL=https://…/tricord-presence-installer.cia ./build.sh   # + QR avec l'URL réelle
```

## Installation utilisateur final

1. Luma3DS ≥ 12 : activer "Enable loading external FIRMs and modules"
   (SELECT au boot) ; Rosalina : activer "Plugin loader".
2. Installer `dist/tricord-presence-installer.cia` (FBI / QR `dist/qr.html`)
   ou lancer le `.3dsx` via Homebrew Launcher.
3. Suivre l'installeur (copie, saisie du token Discord, lancement du
   sysmodule).
4. Luma3DS ne relance pas un sysmodule custom au boot : relancer l'installeur
   après chaque redémarrage (voir RAPPORT.md §3 pour les alternatives).

Fichiers écrits sur la SD : `/luma/sysmodules/000401300F000102.cxi`,
`/luma/plugins/default.3gx`, `/3ds/tricord-presence/{config.txt,titles.txt,log.txt}`.
