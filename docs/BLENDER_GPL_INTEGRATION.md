# Blender GPL integration — AI Modeling 3D

## Scope and status

AI Modeling 3D is an independent Android modeling application. Its product name and identity are **AI Modeling 3D**; it is not the Blender application and is not endorsed by the Blender Foundation.

The repository is licensed under GNU GPL version 3 or, at your option, any later version. This is compatible with Blender components released under GPL-2.0-or-later when those components are incorporated into a GPLv3-or-later combined work. Always inspect the license of each exact source file, module, asset, and dependency before copying it: Blender contains components with licenses other than GPL.

## Porting rules

For each imported or adapted Blender implementation:

1. Preserve upstream copyright and SPDX/license headers.
2. Record the exact Blender release or commit, source path, original license, and local modifications in a component ledger.
3. Keep GPL notices and provide the full license with distributed APKs.
4. Publish the complete corresponding source for the exact shipped revision, including app/native source and build scripts needed to build it.
5. Clearly mark AI Modeling 3D's changes and dates; do not claim upstream Blender authors endorse this app.
6. Do not copy Blender's protected logo or use Blender as the app's product name. A truthful textual credit is allowed.

## Feature porting roadmap

Port and validate each operation separately in the native C++ mesh engine, then expose it through the Android UI and AI tool dispatcher:

- mesh selection modes (vertex, edge, face, object)
- loop selection and loop cut
- bevel with segments and profile
- extrusion with face/region handling
- knife/draw-cut workflow
- merge/combine and separate
- subdivision/smoothing and normal operations
- mirror with clipping/merge options

A feature is not marked complete until it is implemented in the app's own mesh data model, undo/redo-safe, tested on non-trivial topology, and usable from touch UI. Blender Python add-ons are not directly executable inside this Android APK; native algorithms and UI behavior must be ported or separately reimplemented.

## Distribution

Commercial distribution, paid downloads, subscriptions, and advertising are not prohibited by GPL. Recipients must retain GPL rights and be able to obtain the corresponding source; monetization must not impose restrictions that conflict with the license.
