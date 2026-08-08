/*
 * hook_stub.S — register-preserving detour for FUN_004bfe20 (the language-WAD mount).
 *
 * That function reads its WAD-manager state through ESI (an implicit register argument the
 * caller set), so a plain C detour that used ESI as a temp before tail-calling the original
 * would corrupt it. This stub preserves ALL general-purpose registers and the flags across
 * our C helper, then tail-jumps into the MinHook trampoline (the original prologue + a jmp
 * back into the function body), so the original runs with the caller's exact machine state.
 *
 * 32-bit MinGW (i386) decorates cdecl C symbols with a leading underscore.
 */
    .text
    .globl  _det_mount
_det_mount:
    pushfl                          /* preserve EFLAGS                            */
    pushal                          /* preserve EAX..EDI (incl. ESI = implicit arg) */
    call    _mercs2_language_apply  /* force the language index (or log, in dry_run) */
    popal
    popfl
    jmp     *_g_orig_mount          /* tail into the trampoline -> original mount code */
