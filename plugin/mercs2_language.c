/*
 * mercs2_language.asi — force the game's language index to a chosen (possibly novel) slot.
 *
 * Built against the m2 SDK (m2_hook / m2_ini / m2_log). The reverse-engineering and the
 * load-early/act-late rationale live in:
 *   notes-on-the-released-game/docs/reverse_engineer/language_asi_hook_contract.md
 *
 * Shape: DllMain reads the INI beside the module, optionally repoints an unused name-table
 * slot to a novel language string (static work, safe early), then detours the language-WAD
 * mount (FUN_004bfe20). The detour forces *DAT_01176018 to our slot immediately before the
 * mount sprintf — after the game's (invisible) locale write, so it is not clobbered; the
 * stringdb key and font switch inherit the same index. The detour is a naked asm stub
 * (hook_stub.S) because the mount fn reads its state through ESI (an implicit register arg),
 * so every GP register must pass through untouched.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>

#include "m2.h"

/* --- Binary-specific addresses: cracked/DRM-free retail EXE, image base 0x00400000. --- */
#define VA_LANG_INDEX_PTR  0x01176018u  /* -> language struct; *(int*)ptr (offset 0) = index */
#define VA_NAME_TABLE      0x00CF281Cu  /* char*[9] language names, indexed by the index      */
#define VA_MOUNT_LANG_WAD  0x004BFE20u  /* opens .\Data\<name>.wad — the detour target        */

/* The engine's language-stringdb lifecycle manager. `__cdecl f(mode, flag)`, self-contained
 * (loads the request-queue @0x01175CDC, resolver table @0x01176630, and language index @0x01176018
 * from globals — the caller passes only mode/flag). mode 0 = enqueue-ACQUIRE the current language's
 * stringdb (hashes name-table[index] and pushes a RequestAsset onto the streaming queue); mode 1 =
 * activate the already-resident copy. A novel language's stringdb ships only in a *patch* WAD, so
 * the engine's own boot-time acquire can fire before that patch is in _Library[] and miss; re-acquiring
 * HERE — at the language-WAD mount, by which point shell-patch/vz-patch are mounted — lets the queued
 * RequestAsset find the novel row (reverse _Library[] search) and stream it resident. This is why the
 * novel stringdb showed as raw [0x..] hashes: never resident, not misplaced. See
 * notes-on-the-released-game/memory/novel-language-stringdb-residency.md. */
#define VA_LANG_STRINGDB_ACQUIRE 0x004B87A0u
typedef void (__cdecl *lang_stringdb_fn)(int mode, int flag);

/* --- Config (from mercs2_language.ini) --- */
static int  g_enabled = 0;
static int  g_dry_run = 1;              /* default safe: log, don't apply */
static int  g_index   = 7;             /* default: the unused 'allcaps' slot */
static char g_name[64] = {0};          /* if set: repoint slot[index] -> this (static lifetime) */
static int  g_acquire = 1;             /* re-acquire the language stringdb at mount (residency fix) */

static void on_kv(void *ud, const char *key, const char *value) {
    (void)ud;
    if      (!_stricmp(key, "enabled")) g_enabled = m2_ini_bool(value);
    else if (!_stricmp(key, "dry_run")) g_dry_run = m2_ini_bool(value);
    else if (!_stricmp(key, "index"))   g_index   = m2_ini_int(value, g_index);
    else if (!_stricmp(key, "acquire")) g_acquire = m2_ini_bool(value);
    else if (!_stricmp(key, "name")) {
        strncpy(g_name, value, sizeof(g_name) - 1);
        g_name[sizeof(g_name) - 1] = '\0';
    }
    /* 'script' is advisory (Modkit/font handling); the runtime does not need it. */
}

/* Called by the asm detour, inside pushad/popad. Writes the index through the global
 * pointer, guarded — the struct is allocated before mount (FUN_00630b20), but guard anyway. */
void mercs2_language_apply(void) {
    int **pp = (int **)VA_LANG_INDEX_PTR;
    if (g_dry_run) {
        m2_logf("dry_run: mount reached; would force *0x%08X -> %d%s", VA_LANG_INDEX_PTR, g_index,
                g_acquire ? " + re-acquire stringdb" : "");
        return;
    }
    if (*pp) (*pp)[0] = g_index;
    /* Index is now our slot; re-enqueue the acquire so the queued RequestAsset finds the novel
     * language's stringdb (now that shell-patch/vz-patch are mounted) and streams it resident. */
    if (g_acquire) ((lang_stringdb_fn)VA_LANG_STRINGDB_ACQUIRE)(0, 1);
}

/* Defined in hook_stub.S; the MinHook trampoline it tail-jumps to. */
extern void det_mount(void);
void (*g_orig_mount)(void) = 0;

/* Path of mercs2_language.ini beside this module. */
static void ini_path(char *out, size_t n) {
    char mod[MAX_PATH];
    DWORD k = GetModuleFileNameA(M2_SELF_MODULE, mod, (DWORD)sizeof(mod));
    if (k == 0 || k >= sizeof(mod)) { out[0] = '\0'; return; }
    while (k && mod[k - 1] != '\\' && mod[k - 1] != '/') k--;
    mod[k] = '\0';
    _snprintf(out, n, "%smercs2_language.ini", mod);
    out[n - 1] = '\0';
}

/* Point name-table slot[index] at our novel-language string. .rdata-safe via VirtualProtect. */
static void repoint_slot(void) {
    void *slot = (void *)(VA_NAME_TABLE + (DWORD)g_index * 4u);
    DWORD old;
    if (VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old)) {
        *(const char **)slot = g_name;   /* g_name is static — safe to keep pointed at */
        VirtualProtect(slot, sizeof(void *), old, &old);
    } else {
        m2_logf("VirtualProtect failed for slot %d @0x%08X", g_index,
                (unsigned)(VA_NAME_TABLE + (DWORD)g_index * 4u));
    }
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    if (!m2_abi_ok()) return FALSE;      /* header/DLL ABI guard — refuse rather than misbehave */
    m2_log_init(h);

    {
        char path[MAX_PATH];
        ini_path(path, sizeof(path));
        m2_ini_parse(path, on_kv, NULL);
    }

    if (!g_enabled) { m2_logf("disabled (enabled=false); no-op"); return TRUE; }

    if (g_name[0]) {
        if (g_dry_run) m2_logf("dry_run: would repoint slot[%d] -> \"%s\"", g_index, g_name);
        else { repoint_slot(); m2_logf("repointed slot[%d] -> \"%s\"", g_index, g_name); }
    }

    m2_hook_init();
    if (m2_hook_attach((void *)VA_MOUNT_LANG_WAD, (void *)det_mount, (void **)&g_orig_mount))
        m2_logf("hook armed @0x%08X (index=%d, dry_run=%d, acquire=%d)",
                VA_MOUNT_LANG_WAD, g_index, g_dry_run, g_acquire);
    else
        m2_logf("FAILED to attach hook @0x%08X", VA_MOUNT_LANG_WAD);

    return TRUE;
}
