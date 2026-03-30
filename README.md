# KeyBindManager

`KeyBindManager` is an SKSE plugin for Skyrim SE 1.5.97 that adds an in-game PrismaUI menu for binding powers, lesser powers, voice powers, and shout-like talents to keyboard or mouse hotkeys.

Version: `0.1.0`

## Features

- In-game PrismaUI menu opened with a configurable hotkey.
- Bind talents to keyboard keys or mouse buttons.
- Support for modifier combos like `Shift + Q`, `Ctrl + Mouse4`, or `Alt + R`.
- Rebind the menu hotkey with conflict protection.
- Persist bindings to `SKSE/Plugins/KeyBindManager.ini`.
- Lazy PrismaUI initialization to avoid early-load crashes.

## Current Behavior

- The default menu hotkey is `F3`.
- The menu hotkey can be changed in the UI and reset back to `F3`.
- `Esc` and pure modifier keys cannot be used as the menu hotkey.
- The menu hotkey cannot conflict with a talent's primary key.
- While binding:
  - press a normal key for a single-key bind
  - press `Shift`, `Ctrl`, or `Alt`, then a second key for a combo
  - press `Esc` to cancel

## Requirements

- Skyrim Special Edition `1.5.97`
- SKSE64
- [XMake](https://xmake.io/)
- MSVC with C++23 support
- PrismaUI runtime

## Repository Layout

- `src/` - plugin source
- `view/index.html` - PrismaUI frontend
- `vendor/PrismaUI/` - vendored PrismaUI runtime files used for local deployment
- `xmake.lua` - build and deployment config

## Build

```bat
xmake build
```

The project is configured to deploy directly into a local MO2 mod folder after a successful build.

## Local Setup

This repository currently uses hardcoded local paths inside `xmake.lua`. Before building on another machine, update these variables:

- `mod_root`
- `prisma_mod_root`
- any derived plugin or view paths if your layout is different

The current setup copies:

- `KeyBindManager.dll` into the target mod's `SKSE/Plugins`
- PrismaUI runtime files from `vendor/PrismaUI`
- the UI view from `view/index.html`

## Configuration

Runtime config is stored in:

```text
SKSE/Plugins/KeyBindManager.ini
```

The plugin stores:

- talent bindings
- display labels
- menu hotkey

## Notes

- PrismaUI is initialized on first menu use instead of during early SKSE startup.
- Old binding data with the legacy modifier format is migrated on load.
- The repository includes vendored PrismaUI runtime assets so the build can deploy a working local setup.

## License

GPL-3.0
