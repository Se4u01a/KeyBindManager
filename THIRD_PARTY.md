# Third-Party Components

This repository contains third-party code and runtime files that are not part of the `KeyBindManager` MIT license unless stated otherwise.

## PrismaUI

Path:

- `vendor/PrismaUI/`

Purpose:

- Local build and deployment dependency for the in-game PrismaUI interface runtime.

Origin:

- Prisma UI - Next-Gen Web UI Framework by StarkMP
- Nexus Mods page: <https://www.nexusmods.com/skyrimspecialedition/mods/148718>

Notes:

- These files are vendored for local build and deployment convenience.
- They remain subject to their own upstream terms, permissions, notices, and embedded third-party licenses.
- The MIT license in the repository root applies to `KeyBindManager` source code and original project files, not automatically to vendored PrismaUI files.

## Embedded Third-Party Licenses Inside PrismaUI

The vendored PrismaUI runtime includes additional third-party components with their own license files, including:

- `vendor/PrismaUI/PrismaUI/inspector/External/CodeMirror/LICENSE`
- `vendor/PrismaUI/PrismaUI/inspector/External/CSSDocumentation/LICENSE`
- `vendor/PrismaUI/PrismaUI/inspector/External/Esprima/LICENSE`
- `vendor/PrismaUI/PrismaUI/inspector/External/three.js/LICENSE`

Those notices should be preserved when redistributing the vendored runtime.
