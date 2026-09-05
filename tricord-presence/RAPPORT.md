# RAPPORT — TriCord Presence (complétion du squelette)

Date : juin 2026. Environnement de build : conteneur Linux aarch64 (Debian 12),
devkitARM r68 / gcc 16.1.0 / libctru 2.7.0 (image Docker officielle
`devkitpro/devkitarm`, extraite manuellement car `pkg.devkitpro.org` et
`apt.devkitpro.org` répondent 403 depuis ce réseau — voir §Environnement).

**Aucun test sur console ni sur Citra/Azahar n'a été possible ici** (pas
d'émulateur exécutable dans le conteneur). Tout ce qui touche à l'OS 3DS
est donc marqué `TODO(hardware)` dans le code là où une validation réelle
manque, avec la source utilisée pour chaque choix.

## 0. Clarification sur le test "Gateway Discord" (correction d'une formulation trompeuse)

La version précédente de ce rapport parlait d'un client Gateway « exécuté avec
succès contre la vraie gateway ». **C'était trompeur.** Les faits :

- **Aucun token Discord valide n'a été utilisé ni disponible.** Le test hôte
  (`tools/host_gateway_test/`) a été lancé avec la chaîne arbitraire
  `token=FAKE.TOKEN.FOR_PROTOCOL_TEST` (et `FAKE.TOKEN` par l'agent de test),
  écrite dans `/tmp/cfg` (hors projet, volatile). Aucun compte n'a été créé,
  aucun token trouvé ou généré. Aucun fichier du projet ne contient cette
  chaîne ; seul `test_reports/iteration_1.json` la mentionne en clair.
- **Ce qui s'est réellement passé** : TCP + TLS 1.2 (SNI) établis vers
  `gateway.discord.gg:443` → upgrade WebSocket accepté (`101`) → trame HELLO
  (op 10, `heartbeat_interval=41250`) reçue et parsée → IDENTIFY (op 2)
  envoyé → **Discord a fermé la connexion avec le code 4004 (Authentication
  failed)**. L'IDENTIFY a donc été **rejeté**.
- **Validé** : couche réseau (TLS + WS), réception/parsing de HELLO,
  sérialisation d'IDENTIFY, réception/interprétation d'une trame CLOSE et
  des codes fatals, backoff de reconnexion, et (depuis cette révision) la
  vérification du certificat serveur.
- **Non validé** : authentification, READY, heartbeat/HEARTBEAT_ACK, RESUME,
  INVALID_SESSION, UPDATE PRESENCE (op 3), scanner READY volumineux (testé
  uniquement sur un JSON synthétique). Tout cela n'a jamais été échangé avec
  la Gateway.

Sortie brute du test (relancé, `TRICORD_CONFIG=/tmp/cfg ./gateway_test 12`) :
```
scanner streaming: OK (chunks 1..64)
[gateway] connexion à gateway.discord.gg
[gateway] HELLO: heartbeat_interval=41250 ms
[gateway] IDENTIFY envoyé
[gateway] close reçu, code 4004
[gateway] Authentification refusée (4004) : token invalide -> arrêt
[gateway] thread gateway terminé (fatal=1)
exit=0
```

Ce qui a **été validé réellement** dans l'ensemble du projet :
- compilation sans erreur des 3 composants (`./build.sh`) → `.cxi`, `.3gx`,
  `.3dsx`, `.cia`, `dist/qr.html` ;
- structure des binaires vérifiée avec `ctrtool` (exheader du sysmodule :
  Title ID, services, SVC, type mémoire ; CIA : exefs `.code/banner/icon/logo`
  + romfs) ;
- couche TLS + WebSocket + HELLO/IDENTIFY/CLOSE du client Gateway, sur hôte
  Linux, dans les limites décrites en §0 ci-dessus ;
- vérification TLS : avec le bundle CA embarqué, le handshake vers
  `gateway.discord.gg` réussit et un hôte auto-signé
  (`self-signed.badssl.com`) est refusé (`-0x2700`, "not correctly signed by
  the trusted CA").

---

## 1. Environnement installé

| Élément | État | Détail |
|---|---|---|
| devkitARM + libctru + citro3d + 3dstools + general-tools | ✅ | image Docker `devkitpro/devkitarm` (arm64), layers extraites dans `/opt/dkp_root/opt/devkitpro`. `tools/env.sh` exporte `DEVKITPRO/DEVKITARM/CTRULIB/PORTLIBS`. |
| Port mbedtls 3DS | ✅ | déjà packagé par devkitPro : `3ds-mbedtls 2.28.8` (portlibs). Fournit `mbedtls_hardware_poll`. |
| Port wslay 3DS | ✅ | déjà packagé par devkitPro : `3ds-wslay 1.1.1`. Pas besoin de source tierce. |
| jansson (JSON) | ✅ | `3ds-jansson 2.13` (portlibs). |
| makerom / ctrtool | ✅ | compilés depuis `3DSGuy/Project_CTR` (`tools/build_host_tools.sh`). |
| bannertool | ✅ | dépôt Steveice10 disparu → miroir `Epicpkmn11/bannertool` + patch `buildtools/make_base` pour aarch64. |
| 3gxtool | ✅ | `Nanquitas/3gxtool`, recompilé contre la yaml-cpp système (celle embarquée est x86). |
| CTRPluginFramework (libctrpf) | ✅ | `gitlab.com/thepixellizeross/ctrpluginframework`, `-Werror` retiré (gcc 16 génère des warnings inoffensifs) ; headers annexes (`types.h`, `csvc.h`, `plgldr.h`…) copiés dans `libctrpf/include` car `make install` ne les exporte pas. |
| Sleepy Discord | ❌ non utilisé | inutile : la pile wslay + mbedtls est disponible directement en portlibs ; réécrire un client C minimal (≈600 lignes) était plus léger que porter la lib C++ complète dans un sysmodule. |
| Citra / Azahar | ❌ | non exécutable dans le conteneur. |

Blocage réseau documenté : `apt.devkitpro.org` / `pkg.devkitpro.org` → HTTP 403
(Cloudflare). Contournement : Docker Hub (`registry-1.docker.io`) accessible.
Sur une machine normale, `dkp-pacman -S 3ds-dev 3ds-mbedtls 3ds-wslay
3ds-jansson` suffit et `tools/env.sh` détecte `/opt/devkitpro`.

## 2. Ce qui a été implémenté (par point du cahier des charges)

### 2.1 `sysmodule/source/apt_monitor.c` — Title ID du jeu actif ✅ (non testé HW)
- **Erreur du squelette corrigée** : `0x0001` est `APT:GetLockHandle`, pas
  `GetAppletManInfo`. Codes vérifiés sur 3dbrew :
  - `APT:GetAppletManInfo` = `0x00050040`, réponse `[2] AppletPos, [3]
    Requested AppID, [4] HOME Menu AppID, [5] Current AppID`
    (https://www.3dbrew.org/wiki/APT:GetAppletManInfo).
  - `APT:GetAppletInfo` = `0x00060040`, réponse `[2-3] u64 TitleID, [4]
    MediaType, [5] Registered, [6] Loaded, [7] Attr`, erreur `0xC880CFFA`
    si l'AppID n'est pas enregistré (https://www.3dbrew.org/wiki/APT:GetAppletInfo).
- `aptMonitorGetActiveTitleId` = `GetAppletInfo(0x300)` : l'Application au
  premier plan est toujours enregistrée sous l'AppID `0x300` (libctru
  `APPID_APPLICATION`, 3dbrew NS_and_APT_Services#AppIDs). Ni AM ni pm:app
  ne sont nécessaires (`PMDBG_GetCurrentAppInfo` n'existe que dans un fork
  "Luma3DS-3GX", pas dans libctru 2.7 ni Luma mainline — vérifié dans les
  headers).
- Sessions APT ouvertes/fermées à chaque appel, ordre `APT:S, APT:A, APT:U`
  : copie du comportement de libctru `aptSendCommand()`
  (libctru/source/services/apt.c) car NS limite les sessions APT.
- `TODO(hardware)` restant : NS pourrait refuser `GetAppletInfo` à un
  process non enregistré comme applet. Plan B documenté dans le code :
  `svcGetProcessList` + `svcGetProcessInfo(h, 0x10001)` (extension kernel
  Luma, utilisée par `rosalina/source/errdisp.c`), qui demanderait d'ajouter
  ces SVC au `.rsf`.

### 2.2 `sysmodule/source/discord_gateway.c` — Gateway ✅ code complet, ⚠️ testé sur hôte jusqu'à IDENTIFY/4004 seulement (cf §0)
- TLS : mbedtls 2.28, TLS 1.2 min, SNI, **`MBEDTLS_SSL_VERIFY_REQUIRED`** avec
  bundle de racines embarqué (`discord_ca_bundle.h`, généré par
  `tools/gen_ca_bundle.py` depuis le magasin ca-certificates : GTS Root
  R1-R4, GlobalSign Root CA, ISRG Root X1, DigiCert Global Root CA/G2,
  Baltimore CyberTrust — chaîne observée en juin 2026 : discord.gg ← WE1 ←
  GTS Root R4 ← GlobalSign Root CA) + contrôle du nom d'hôte. Testé sur hôte
  (accepté : gateway.discord.gg ; refusé : self-signed.badssl.com).
  `TODO(hardware)` : mbedtls compare les dates de validité à `time()` (RTC
  3DS) ; une horloge très fausse fait échouer la connexion (motif loggé).
- WebSocket : wslay en mode événementiel non-bloquant (`poll()` + sockets
  `O_NONBLOCK`, callbacks `mbedtls_ssl_read/write`).
- Protocole : HELLO → IDENTIFY (token lu depuis `config.txt`, propriétés
  os/browser/device, **sans `intents`** car token utilisateur) ; heartbeat
  avec jitter initial, détection zombie (pas d'ACK → RESUME) ; READY →
  `session_id` + `resume_gateway_url` ; RESUME (op 6) ; RECONNECT (op 7) ;
  INVALID_SESSION (op 9, attente 3 s puis re-IDENTIFY si non reprenable) ;
  codes de fermeture 4004 / 4010-4014 fatals ; backoff 5 s → 60 s.
- Update Presence (op 3) : en jeu `{"since":null,"activities":[{"name":"<jeu>",
  "type":0}],"status":"online","afk":false}` ; au menu HOME **`status:
  "idle"` + statut personnalisé** `{"name":"Custom Status","type":4,
  "state":"Sur le menu HOME"}` (type 4 = seul texte libre affiché pour un
  compte utilisateur ; il **remplace le statut perso de l'utilisateur** tant
  que le sysmodule tourne) ; coalescé et limité à 1 envoi / 15 s.
- **Point deviné / à surveiller** : le READY d'un compte utilisateur peut
  peser plusieurs MiB, impossible à bufferiser dans 3 MiB de heap. wslay est
  configuré en `no_buffering` : un message est accumulé jusqu'à 96 KiB, au-
  delà il est scanné à la volée (machine à états JSON qui suit la profondeur
  pour ne lire `session_id`/`resume_gateway_url` qu'au niveau de `d`, car le
  READY contient aussi `"sessions":[{"session_id":…}]`). Testé unitairement
  sur hôte, pas contre un vrai READY (aucun token disponible).
- Thread réseau dédié (pile 64 KiB), attente de `ACU_GetStatus == 3` avant la
  première connexion, `NDMU_EnterExclusiveState(INFRASTRUCTURE)` comme
  Rosalina `minisoc.c` pour garder le WiFi en fond. En veille (couvercle
  fermé), la 3DS coupe le WiFi : la session sera reprise (RESUME) au réveil.
- Journal : `sdmc:/3ds/tricord-presence/log.txt` (tronqué à 256 KiB).

### 2.3 `sysmodule/tricord_presenced.rsf` + `build.sh`/Makefile ✅
- RSF modelé sur `pnp_sys/pnp.rsf` de **zaksabeast/3ds-Plug-n-play** (le seul
  sysmodule homebrew "custom" documenté comme chargé par Luma ≥ 12 depuis
  `/luma/sysmodules/<TID>.cxi`) et `rosalina.rsf` de Luma3DS.
  - `UniqueId 0xF0001`, `Category Base` → **Title ID `000401300F000102`**,
    fichier `/luma/sysmodules/000401300F000102.cxi` (nommage imposé par
    `openSysmoduleCxi()` dans Luma3DS `sysmodules/loader/source/patcher.c`).
    Le squelette copiait `tricord_presenced.cxi` : corrigé.
  - Services : `APT:S/A/U`, `fs:USER`, `soc:U`, `ndm:u`, `ac:u` ; FS :
    `DirectSdmc` ; SVC : liste de Plug-n-play (inclut CreatePort /
    AcceptSession / ReplyAndReceive / CreateThread / CreateMemoryBlock).
  - **Choix deviné** : `MemoryType System` + `ResourceLimitCategory
    sysapplet` (Plug-n-play) plutôt que `Base`/`Other` (Rosalina). Si
    l'allocation heap échoue sur o3DS, changer ces deux lignes.
  - Core 1 (`IdealProcessor 1`, `AffinityMask 2`), priorité 28.
- Makefile : ELF `-specs=3dsx.specs` puis `makerom -f ncch -rsf … -nocodepadding
  -o 000401300F000102.cxi -elf …` (commande identique au Makefile de
  Rosalina).
- `main.c` : surcharges libctru nécessaires à un sysmodule : `__appInit`
  (srv + fs + `archiveMountSdmc`, **pas** d'`aptInit`/`hidInit`),
  `__ctru_heap_size = 3 MiB`, `__ctru_linear_heap_size = 0` (symboles weak
  de libctru, vérifié avec `nm`).

### 2.4 `plugin/` — overlay réel via CTRPluginFramework ✅ (non testé HW)
- Plutôt que ré-écrire un hook GSP, le plugin utilise **CTRPF**, le framework
  de tous les `.3gx` existants : `OSD::Run(cb)` fait appeler `cb(Screen)`
  juste avant chaque swap de framebuffer du jeu (hook MITM posé par
  `OSDImpl` sur la routine GSP du jeu, `Library/source/CTRPluginFrameworkImpl/
  Graphics/OSDImpl.cpp`). `Screen::Draw/DrawRect` écrivent dans le framebuffer.
- `main.c` → `main.cpp` et `overlay_draw.c` → `overlay_draw.cpp` (CTRPF est
  C++) ; `presence_client.c` reste en C pur ; API C conservée (`extern "C"`).
- Toast "Vous jouez a <jeu>" 4 s, glissement 250 ms, écran du haut,
  texte ASCII-isé (police 6x10 de CTRPF). Poll IPC 1×/s via
  `PluginMenu::Callback`.
- `plugin/Makefile` : chaîne réelle = ELF lié avec `3gx.ld` (copié du
  `TestPlugin` CTRPF, code à `0x07000100`) + `3gxtool -s <elf> <plgInfo>
  <3gx>`. Fichier `tricord_overlay.plgInfo` (cible : tous les jeux).
- `TODO(hardware)` : jeux stéréo 3D (CTRPF ne dessine que le framebuffer
  gauche), "wide mode" 800 px, jeux qui ne passent pas par la routine GSP
  standard.

### 2.5 IPC sysmodule ↔ plugin ✅ (non testé HW)
- Port **global nommé** `presence:d` (`svcCreatePort` avec nom, 10 car. ≤ 11)
  — et non un service `srv:` — parce que le plugin vit dans le process du jeu
  dont l'exheader ne peut pas connaître notre service ; `svcConnectToPort`
  n'est soumis à aucune ACL de service. C'est le mécanisme de `hb:ldr` /
  `err:f` de Luma3DS (`rosalina/source/errdisp.c`, `service_manager.c`).
- Serveur : thread dédié, boucle `svcReplyAndReceive` réduite de
  `service_manager.c` (gestion `0xC920181A` = session fermée, `0xD900182F` =
  commande invalide).
- Protocole (dans `presence_state.h`, partagé) : cmd `0x0001` GetState →
  `IPC_MakeHeader(1, 20, 0)` : result, kind, title_id (2 mots), nom 64 o.
  Tout dans le command buffer TLS, aucun descripteur de traduction.
- Client : retry toutes les 5 s si le sysmodule n'est pas (encore) lancé,
  reconnexion si la session est invalidée.

### 2.6 `title_db.c` ✅
- `tools/gen_titles_db.py` télécharge `hax0kartik/3dsdb` (jsons GB/US/JP/KR/TW
  — la base dont 3DS-RPC utilise une version modifiée), filtre les
  applications (`00040000`/`00040002`), dédoublonne, retire ™/®/©, tronque à
  63 octets UTF-8 → `titles.txt` trié (4206 titres, 224 KiB).
- Chargé au boot depuis `sdmc:/3ds/tricord-presence/titles.txt` (copié par
  l'installeur), recherche dichotomique, fallback `Title %016llX`.

### 2.7 `installer/source/main.c` ✅
- `ensureDir` (mkdir récursif, EEXIST toléré), `copyFile` (fread/fwrite 64 KiB)
  depuis le romfs (`installer/romfs/` : `.cxi`, `.3gx`, `titles.txt` — pas
  d'asset visuel), détection d'un `default.3gx` existant avant écrasement.
- Saisie du token via `SwkbdState` (mode mot de passe, validation non vide) →
  `config.txt` : `token=…`. Jamais de token dans le binaire.
- Bonus (méthode Plug-n-play, `launcher/source/main.cpp`) : proposition de
  lancer le sysmodule immédiatement via `svcControlService(STEAL_CLIENT_SESSION,
  "pm:app")` (extension Luma, `csvc.s` repris de Luma3DS) puis
  `pm:app LaunchTitle` (cmd `0x00010140`, libctru `pmapp.c`). Non testé HW.

### 2.8 Distribution `.cia` + QR ✅
- `installer/tricord-presence-installer.rsf` : application classique
  (modèle `pnp_launcher.rsf`), Title ID `000400000F000200`, `DirectSdmc`.
- Commandes exactes (Makefile, cible `cia`) :
  ```
  bannertool makebanner -i assets/banner.png -a assets/silence.wav -o build/banner.bnr
  bannertool makesmdh -s "TriCord Presence Installer" -l "CLI installer (no GUI yet)" -p "TriCord" -i assets/icon.png -o build/icon.icn
  makerom -f cia -o tricord-presence-installer.cia -elf tricord-presence-installer.elf \
          -rsf tricord-presence-installer.rsf -target t -exefslogo \
          -icon build/icon.icn -banner build/banner.bnr -DAPP_ROMFS=romfs
  ```
  Icône/bannière : aplats unis générés par `tools/gen_assets.py` (fichiers
  techniquement obligatoires pour le HOME menu, volontairement non "soignés").
- Hébergement : à faire côté projet. Recommandé : GitHub Releases, URL stable
  `https://github.com/<user>/<repo>/releases/latest/download/tricord-presence-installer.cia`
  (FBI "Remote Install" accepte une redirection 302 vers l'asset).
- `dist/qr.html` : page autonome, QR en SVG inline, aucun texte. **Contient
  pour l'instant l'URL placeholder** `https://github.com/USER/REPO/...` —
  régénérer avec `python3 dist/gen_qr.py <URL> ` (ou `QR_URL=<URL> ./build.sh`).

## 3. Ce qui reste en `TODO(hardware)` (impossible à valider sans console)

| # | Point | Fichier | Risque |
|---|---|---|---|
| 1 | **Lancement au boot** : Luma3DS mainline ne lance jamais de lui-même un sysmodule custom ; `/luma/sysmodules/<TID>.cxi` n'est utilisé que quand PM lance ce TID. Options : (a) relancer l'installeur (bouton "lancer maintenant") après chaque boot — implémenté ; (b) injecter le TID dans la liste de dépendances d'un sysmodule démarré au boot via `/luma/titles/<tid>/exheader.bin` (dépend de la version de firmware, non fait) ; (c) fork "Luma3DS-autorun". | `sysmodule/source/main.c`, `installer/source/main.c` | élevé |
| 2 | `APT:GetAppletInfo` accepté depuis un process non-applet | `apt_monitor.c` | moyen |
| 3 | `MemoryType System`/`sysapplet` vs `Base`/`Other` | `tricord_presenced.rsf` | moyen |
| 4 | `svcConnectToPort("presence:d")` depuis un jeu, cohabitation avec le plugin loader | `presence_client.c` | faible |
| 5 | Rendu OSD : 3D stéréo, wide mode, jeux à pipeline GSP atypique | `overlay_draw.cpp` | moyen |
| 6 | Validité des certificats dépend de l'horloge RTC de la console ; rotation possible des racines Discord (regénérer le bundle) | `discord_gateway.c` | faible |
| 7 | `Sec-WebSocket-Accept` non vérifié | `discord_gateway.c` | faible |
| 8 | Comportement réel du READY utilisateur en streaming (taille, ordre des clés) | `discord_gateway.c` | moyen |
| 9 | `svcControlService` + `pm:app LaunchTitle` depuis l'installeur (CIA ; depuis le .3dsx sous HBL, dépend des droits accordés par `hb:ldr`) | `installer/source/main.c` | moyen |
| 10 | Empreinte mémoire réelle (heap 3 MiB : SOC 1 MiB + TLS + titres) | `main.c` | faible |

## 4. Points devinés (comportement non documenté)
- Un jeu **suspendu derrière le HOME menu** est toujours considéré "en jeu"
  (comme Discord desktop tant que le process existe).
- Fermeture WebSocket avec le code `4000` (et non `1000`) quand on veut pouvoir
  RESUME : Discord invalide la session sur 1000/1001.
- Priorité/pile des threads (IPC 8 KiB, réseau 64 KiB, priorité 0x3F).
- Intervalle de poll APT = 1 s ; toast 4 s.
- Statut "idle" au menu HOME rendu via un statut personnalisé (type 4) : le
  rendu exact côté client Discord pour un compte utilisateur n'a pas pu être
  observé (pas de token).

## 5. Arborescence ajoutée / modifiée
```
build.sh                       chaîne complète (libctrpf → cxi → 3gx → 3dsx/cia → dist/qr.html)
sysmodule/tricord_presenced.rsf, source/log.[ch]        nouveaux
plugin/3gx.ld, tricord_overlay.plgInfo, source/*.cpp    nouveaux / renommés (.c→.cpp)
installer/tricord-presence-installer.rsf, source/csvc.s, assets/ (générés), romfs/titles.txt
tools/env.sh, build_host_tools.sh, gen_assets.py, gen_titles_db.py, gen_ca_bundle.py, host_gateway_test/
sysmodule/source/discord_ca_bundle.h   généré (racines CA)
dist/gen_qr.py, qr.html, *.cia, *.3dsx, *.cxi, *.3gx, titles.txt
```
`third_party/` (dépôts de référence clonés : Luma3DS, CTRPF, Project_CTR,
3gxtool, bannertool) et `tools/bin/` sont ignorés par git et recréés par les
scripts.
