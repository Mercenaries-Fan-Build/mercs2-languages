# mercs2-language

A **Shipment** that adds a novel, player-selectable language to *Mercenaries 2: World in Flames* —
one the game never shipped — without touching the executable or any base WAD.

It is the shipment-native expression of "Route C" from the Localization design: the game has no
in-game language selector and picks its language at boot from OS-locale, so a small native plugin
overrides that choice, and the translation ships as a new language WAD.

## How it works

| Piece | Contribution | What it does |
|---|---|---|
| `src/mercs2_language.asi` | `native_hook` | Forces the language index at the language-WAD mount (`FUN_004bfe20`), so the game boots into our language regardless of OS-locale. Built against the **m2 SDK**. |
| `src/mercs2_language.ini` | `place_file` (`scripts`) | The plugin's config, read beside the module. |
| `src/polski.strings` | `add_language` | The novel language's base WAD (stringdb + fonts). |

The plugin repoints an unused name-table slot (`allcaps`, index 7) to the new language string in
DllMain, then — because the game writes the locale-derived index *after* our DllMain — defers the
index force to a detour that fires at mount time. Full reverse-engineering and rationale:
[`language_asi_hook_contract.md`](../notes-on-the-released-game/docs/reverse_engineer/language_asi_hook_contract.md).

## ⚠ Requirements and defaults

- `manifest.yaml` is manifest **format 2**, read by `qm` 4.0.0 (Quartermaster release `v0.17.0`).
- **`add_language`** is the one kind that places a WAD in `data/`: a novel language needs a new
  base `.\Data\<name>.wad` (a missing one is a hard `exit(1)`).
- The plugin imports `m2-sdk.dll`, so the Shipment requires the **m2-sdk** Shipment
  (`load.requires: { shipment: m2-sdk, version: "^0.4" }`, i.e. `>=0.4.0, <0.5.0`), which places
  `m2-sdk.dll` in the game root.
- The plugin ships **`enabled = false`, `dry_run = true`** — inert until configured and until the
  live verification spike confirms the hook fires at the mount frame.

## Layout

```
mercs2_language/
  manifest.yaml            the Shipment
  sdk/                     m2 SDK (git submodule)
  plugin/
    mercs2_language.c      DllMain, config, slot repoint, hook install
    hook_stub.S            register-preserving detour (the mount fn reads ESI implicitly)
    Makefile               include ../sdk/sdk.mk
  src/
    mercs2_language.asi    built plugin (shipment input)
    mercs2_language.ini    config (place_file)
    polski.strings         translation (add_language)
```

## Build

```sh
git submodule update --init --recursive     # if not already
make -C plugin                              # builds sdk + ../src/mercs2_language.asi
qm lint .                                   # hermetic, no game install
qm build . --out build                      # needs the retail WADs
```

`make -C plugin` needs the `i686-w64-mingw32-gcc` cross-compiler. When `sdk/build/libm2-sdk.dll.a`
is absent it first runs `make -C sdk build`, which at m2 SDK v0.4.0 builds `m2-sdk.dll` from the
`mercs2_mod_runtime` crate and also needs `rustup`, `curl` and `shasum` (see `sdk/README.md`).

## Selecting the language

Modkit's language selector writes `mercs2_language.ini` (`enabled`, `index`, `name`) and installs
the Shipment. There is no in-game selector — this plugin *is* the selection mechanism.
