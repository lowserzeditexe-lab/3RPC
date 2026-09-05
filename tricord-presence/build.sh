#!/usr/bin/env bash
# build.sh — compile les 3 composants dans l'ordre et produit dist/.
#
# Prérequis : devkitARM + devkitPro (3ds-dev : libctru, citro3d, 3dstools,
# general-tools ; portlibs 3ds-mbedtls, 3ds-wslay, 3ds-jansson), plus les
# outils hôte makerom / 3gxtool / bannertool (tools/build_host_tools.sh) et
# libctrpf (CTRPluginFramework, installé ci-dessous si absent).
#
#   source tools/env.sh && ./build.sh            # build complet
#   QR_URL=https://.../installer.cia ./build.sh  # + QR code avec l'URL réelle
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[ -f "$ROOT/tools/env.sh" ] && source "$ROOT/tools/env.sh"

if [ -z "${DEVKITARM:-}" ] || [ ! -x "$DEVKITARM/bin/arm-none-eabi-gcc" ]; then
    echo "ERREUR: DEVKITARM n'est pas défini / devkitARM introuvable." >&2
    echo "Installe devkitPro (dkp-pacman -S 3ds-dev 3ds-mbedtls 3ds-wslay 3ds-jansson) puis exporte DEVKITARM." >&2
    exit 1
fi
export DEVKITPRO="${DEVKITPRO:-$(dirname "$DEVKITARM")}"

need() { command -v "$1" >/dev/null 2>&1 || { echo "ERREUR: outil '$1' introuvable (lance tools/build_host_tools.sh)" >&2; exit 1; }; }
need makerom; need 3gxtool; need bannertool; need smdhtool; need 3dsxtool

# libctrpf (CTRPluginFramework) : nécessaire au plugin .3gx
if [ ! -f "$DEVKITPRO/libctrpf/lib/libctrpf.a" ]; then
    echo "== [0/3] CTRPluginFramework (libctrpf) =="
    [ -d "$ROOT/third_party/ctrpf" ] || git clone -q --depth 1 https://gitlab.com/thepixellizeross/ctrpluginframework.git "$ROOT/third_party/ctrpf"
    # -Werror retiré : gcc 16 (devkitARM r68) signale des warnings inoffensifs
    sed -i 's/-Werror//' "$ROOT/third_party/ctrpf/Library/Makefile"
    make -C "$ROOT/third_party/ctrpf/Library" install -j"$(nproc)" >/dev/null
    # le "make install" de CTRPF n'exporte pas ses headers ctrulib (types.h, csvc.h, plgldr.h...)
    cp "$ROOT/third_party/ctrpf/Library/include/"*.h "$DEVKITPRO/libctrpf/include/"
fi

echo "== [1/3] Sysmodule (ELF -> CXI via makerom) =="
make -C "$ROOT/sysmodule"
SYSMODULE_CXI="$ROOT/sysmodule/000401300F000102.cxi"
[ -f "$SYSMODULE_CXI" ] || { echo "ERREUR: $SYSMODULE_CXI manquant" >&2; exit 1; }

echo "== [2/3] Plugin overlay (.3gx via 3gxtool) =="
make -C "$ROOT/plugin"
PLUGIN_3GX="$ROOT/plugin/tricord_overlay.3gx"
[ -f "$PLUGIN_3GX" ] || { echo "ERREUR: $PLUGIN_3GX manquant" >&2; exit 1; }

echo "== [3/3] Installeur CLI (.3dsx + .cia) =="
mkdir -p "$ROOT/installer/romfs" "$ROOT/installer/assets"
cp "$SYSMODULE_CXI" "$ROOT/installer/romfs/"
cp "$PLUGIN_3GX"    "$ROOT/installer/romfs/"
python3 "$ROOT/tools/gen_assets.py" "$ROOT/installer/assets" >/dev/null
if [ ! -f "$ROOT/installer/romfs/titles.txt" ] || [ "${REFRESH_TITLES:-0}" = "1" ]; then
    python3 "$ROOT/tools/gen_titles_db.py" "$ROOT/installer/romfs/titles.txt" || \
        echo "NOTE: titles.txt non régénéré (pas de réseau ?) — version existante conservée."
fi
make -C "$ROOT/installer" cia

mkdir -p "$ROOT/dist"
cp "$ROOT/installer/tricord-presence-installer.3dsx" "$ROOT/dist/"
cp "$ROOT/installer/tricord-presence-installer.cia"  "$ROOT/dist/"
cp "$SYSMODULE_CXI" "$PLUGIN_3GX" "$ROOT/dist/"   # aussi fournis "à la main" (copie SD manuelle)
cp "$ROOT/installer/romfs/titles.txt" "$ROOT/dist/"

echo "== QR code (dist/qr.html) =="
python3 "$ROOT/dist/gen_qr.py" ${QR_URL:+"$QR_URL"} --out "$ROOT/dist/qr.html"

echo
echo "Build terminé. Contenu de dist/ :"
ls -la "$ROOT/dist"
