/*
 * tricord-presence-installer — homebrew .3dsx / .cia, mode CLI uniquement
 *
 * Volontairement sans interface graphique pour cette version : juste une
 * console texte (consoleInit) qui log chaque étape et attend une validation
 * clavier avant les actions destructives (copie/écrasement de fichiers).
 *
 * Les fichiers à installer (sysmodule .cxi, plugin .3gx, base titles.txt)
 * sont embarqués dans le romfs de l'installeur (installer/romfs/, rempli par
 * build.sh). Pas d'assets visuels dans ce romfs.
 */

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

// Title ID du sysmodule (voir sysmodule/tricord_presenced.rsf). Luma3DS
// exige que le fichier s'appelle <TitleID>.cxi dans /luma/sysmodules/
// (Luma3DS sysmodules/loader/source/patcher.c, openSysmoduleCxi).
#define SYSMODULE_TID_HI 0x00040130u
#define SYSMODULE_TID_LO 0x0F000102u
#define SYSMODULE_DEST "sdmc:/luma/sysmodules/000401300F000102.cxi"
// default.3gx = plugin chargé dans tous les jeux (Luma3DS plugin loader).
// Écrase un éventuel plugin "default" déjà installé -> on prévient avant.
#define PLUGIN_DEST    "sdmc:/luma/plugins/default.3gx"
#define CONFIG_DIR     "sdmc:/3ds/tricord-presence"
#define CONFIG_FILE    CONFIG_DIR "/config.txt"
#define TITLES_FILE    CONFIG_DIR "/titles.txt"

static bool fileExists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// mkdir récursif sur sdmc: (newlib + devoptab libctru). EEXIST n'est pas
// une erreur.
static bool ensureDir(const char *path) {
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + strlen("sdmc:/"); *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
            printf("  ERREUR mkdir %s (errno %d)\n", tmp, errno);
            return false;
        }
        *p = '/';
    }
    if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
        printf("  ERREUR mkdir %s (errno %d)\n", tmp, errno);
        return false;
    }
    return true;
}

static bool copyFile(const char *src, const char *dst) {
    printf("  copie: %s\n         -> %s\n", src, dst);
    FILE *in = fopen(src, "rb");
    if (!in) { printf("  ERREUR: source introuvable (romfs)\n"); return false; }
    FILE *out = fopen(dst, "wb");
    if (!out) { printf("  ERREUR: impossible d'ecrire (errno %d)\n", errno); fclose(in); return false; }

    static char buf[64 * 1024];
    size_t total = 0, n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
        total += n;
    }
    fclose(in);
    if (fclose(out) != 0) ok = false;
    printf(ok ? "  OK (%u octets)\n" : "  ERREUR d'ecriture\n", (unsigned)total);
    return ok;
}

static bool promptYesNo(const char *question) {
    printf("%s [A = oui / B = non]\n", question);
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_A) return true;
        if (kDown & KEY_B) return false;
        gspWaitForVBlank();
    }
    return false;
}

// Saisie du token Discord via l'applet clavier système (libctru swkbd).
// Le token est écrit dans config.txt ("token=..."), jamais dans le binaire.
static bool promptToken(char *out, size_t outSize) {
    SwkbdState swkbd;
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, (int)outSize - 1);
    swkbdSetHintText(&swkbd, "Token Discord (compte utilisateur)");
    swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    swkbdSetFeatures(&swkbd, SWKBD_FIXED_WIDTH);
    swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Annuler", false);
    swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Valider", true);

    SwkbdButton btn = swkbdInputText(&swkbd, out, outSize);
    if (btn != SWKBD_BUTTON_CONFIRM) return false;
    // Nettoyage : espaces/retours parasites
    size_t len = strlen(out);
    while (len > 0 && (out[len - 1] == ' ' || out[len - 1] == '\n' || out[len - 1] == '\r')) out[--len] = '\0';
    return len > 0;
}

static bool writeConfig(const char *token) {
    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) { printf("  ERREUR: ecriture %s\n", CONFIG_FILE); return false; }
    fprintf(f, "# TriCord Presence - config\n");
    fprintf(f, "# Ce fichier contient votre token Discord : ne le partagez jamais.\n");
    fprintf(f, "token=%s\n", token);
    fclose(f);
    return true;
}

// Lancement immédiat du sysmodule sans reboot, méthode "Plug-n-play"
// (zaksabeast/3ds-Plug-n-play, launcher/source/main.cpp) : on "vole" une
// session pm:app via l'extension kernel Luma3DS svcControlService
// (SERVICEOP_STEAL_CLIENT_SESSION, SVC 0xB0, cf Luma3DS rosalina/include/
// csvc.h) puis pm:app LaunchTitle (cmd 0x0001, libctru services/pmapp.c).
// PM demande alors le titre au loader, qui trouve /luma/sysmodules/<TID>.cxi.
// TODO(hardware): non validé sur console. Nécessite Luma3DS >= 12 avec
// "Enable loading external FIRMs and modules" activé.
extern Result svcControlService(u32 op, ...); // csvc.s
static Result launchSysmoduleNow(void) {
    Handle pm = 0;
    Result rc = svcControlService(0 /* SERVICEOP_STEAL_CLIENT_SESSION */, &pm, "pm:app");
    if (R_FAILED(rc)) return rc;

    FS_ProgramInfo info;
    memset(&info, 0, sizeof(info));
    info.programId = ((u64)SYSMODULE_TID_HI << 32) | SYSMODULE_TID_LO;
    info.mediaType = MEDIATYPE_NAND;

    u32 *cmdbuf = getThreadCommandBuffer();
    cmdbuf[0] = IPC_MakeHeader(0x0001, 5, 0); // PMAPP_LaunchTitle
    memcpy(&cmdbuf[1], &info, sizeof(info));
    cmdbuf[5] = PMLAUNCHFLAG_LOAD_DEPENDENCIES;
    rc = svcSendSyncRequest(pm);
    if (R_SUCCEEDED(rc)) rc = (Result)cmdbuf[1];
    svcCloseHandle(pm);
    return rc;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    Result romfsRc = romfsInit();

    printf("=== TriCord Presence - Installeur (CLI) ===\n\n");
    printf("Cet outil va :\n");
    printf("  1) copier le sysmodule vers /luma/sysmodules/\n");
    printf("  2) copier le plugin overlay vers /luma/plugins/\n");
    printf("  3) copier la base de titres + creer la config\n");
    printf("     (saisie du token Discord au clavier)\n\n");
    if (R_FAILED(romfsRc)) {
        printf("ERREUR: romfs indisponible (%08lX) - build incomplet ?\n", (unsigned long)romfsRc);
        goto wait_exit;
    }

    if (!promptYesNo("Continuer ?")) {
        printf("\nAnnule par l'utilisateur.\n");
        goto wait_exit;
    }

    bool ok = true;

    printf("\n[1/3] Sysmodule...\n");
    ok &= ensureDir("sdmc:/luma/sysmodules");
    ok &= copyFile("romfs:/000401300F000102.cxi", SYSMODULE_DEST);

    printf("\n[2/3] Plugin overlay...\n");
    ok &= ensureDir("sdmc:/luma/plugins");
    if (fileExists(PLUGIN_DEST)) {
        printf("  ATTENTION: %s existe deja.\n", PLUGIN_DEST);
        if (promptYesNo("  Ecraser ce plugin 'default' ?")) ok &= copyFile("romfs:/tricord_overlay.3gx", PLUGIN_DEST);
        else printf("  ignore (l'overlay ne sera pas installe).\n");
    } else {
        ok &= copyFile("romfs:/tricord_overlay.3gx", PLUGIN_DEST);
    }

    printf("\n[3/3] Configuration...\n");
    ok &= ensureDir(CONFIG_DIR);
    ok &= copyFile("romfs:/titles.txt", TITLES_FILE);

    if (fileExists(CONFIG_FILE) && !promptYesNo("  Un config.txt existe deja - remplacer le token ?")) {
        printf("  token conserve.\n");
    } else {
        static char token[256];
        printf("  Ouverture du clavier...\n");
        if (promptToken(token, sizeof(token))) {
            ok &= writeConfig(token);
            memset(token, 0, sizeof(token));
            printf("  token enregistre dans %s\n", CONFIG_FILE);
        } else {
            printf("  saisie annulee : completez %s\n  (ligne \"token=...\") a la main.\n", CONFIG_FILE);
        }
    }

    printf(ok ? "\nInstallation terminee.\n" : "\nInstallation terminee AVEC ERREURS (voir ci-dessus).\n");
    printf("\nA faire :\n");
    printf(" - Luma3DS : activer \"Enable loading external FIRMs and modules\"\n");
    printf("   (menu de config Luma, SELECT au boot).\n");
    printf(" - Rosalina (L+Bas+Select) : activer \"Plugin loader\".\n");

    if (ok && promptYesNo("\nLancer le sysmodule maintenant (sans reboot) ?")) {
        Result rc = launchSysmoduleNow();
        if (R_SUCCEEDED(rc)) printf("  sysmodule lance.\n");
        else printf("  echec du lancement (%08lX) : redemarrez la console\n  et relancez cet installeur.\n", (unsigned long)rc);
    }
    printf("\nNOTE: Luma3DS ne relance pas un sysmodule custom tout seul au\n");
    printf("boot ; relancez cet installeur apres chaque redemarrage\n");
    printf("(voir RAPPORT.md, \"Lancement au boot\").\n");

wait_exit:
    printf("\n(appuyez sur START pour quitter)\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    romfsExit();
    gfxExit();
    return 0;
}
