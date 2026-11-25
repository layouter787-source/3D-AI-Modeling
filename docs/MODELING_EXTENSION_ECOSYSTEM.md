# Modeling extension ecosystem — research and integration plan

This is an Android modeling application (Jetpack Compose + C++17/NDK + OpenGL ES), not a Godot game. Godot editor addons cannot be copied into this APK as plugins without their host engine and API.

## Integrated native tools (current branch)

The app's own editable PolyMesh pipeline currently exposes: Catmull–Clark subdivision, face extrusion, inset, smoothing, solidify, mirror, loop cut, triangulation, normal flipping, weld, crease by angle, Boolean union/difference/intersection, radial vertex sculpt, lofted profile mesh, paged topology inspection and undo/redo for mesh changes. AI receives these through the native tool dispatcher; these are app features, not downloaded add-ons.

## Research findings

| Project / add-on | Useful capability | License / compatibility | Decision |
|---|---|---|---|
| [Terrain3D](https://github.com/TokisanGames/Terrain3D) | Sculptable heightmap terrain, holes, texture layers, LOD, foliage | MIT; C++ Godot GDExtension, tightly integrated with Godot | Do not bundle as-is. Port standalone heightfield/brush/LOD concepts into an Android terrain module. |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | GPU cache optimization, simplification, index/vertex remapping, compression/LOD | MIT; C/C++, CMake-friendly | Strong candidate for native mesh optimize + decimate + LOD pipeline. Vendor a pinned release, retain license notice, test arm64-v8a. |
| [OpenSubdiv](https://github.com/PixarAnimationStudios/OpenSubdiv) | Production subdivision surface algorithms | Apache-2.0; C++ | Candidate for replacing/expanding current subdivision, after NDK dependency and binary-size audit. |
| [Manifold](https://github.com/elalish/manifold) | Robust manifold mesh Boolean operations | MIT; C++ | Candidate to improve current native CSG robustness. Integrate behind existing Boolean API after tests. |
| [libigl](https://github.com/libigl/libigl) | Geometry processing: remeshing, parameterization, normals and mesh analysis | Primarily MPL-2.0; some modules include third-party licenses | Select individual headers/functions only after dependency and license audit; avoid blanket import. |
| [Assimp](https://github.com/assimp/assimp) | Importer for multiple 3D asset formats | BSD-3-Clause; C++ | Candidate for OBJ/glTF/FBX-format workflow, but test Android memory and supported format subset. |
| Blender BMesh / operators | Broad modeling, topology and mesh workflow reference | Blender source is GPL; derived/distributed integration has license implications | Use as behavioral reference. A complete Blender engine port is not a small plugin integration. |
| [GrimTopo](https://extensions.blender.org/add-ons/grimtopo/) / [CurioMesh](https://github.com/CurioCrafter/CurioMesh) | Drawn quad retopology, smart fill, dense-mesh cleanup | Blender-specific add-ons; CurioMesh GPLv3 | Study algorithms/workflow; cannot execute Python Blender add-ons in this Android app. Port only with license review. |

## Planned native extension architecture

1. Extension manifest records name, version, license, Android ABI support, capabilities, dependencies, enabled state and provenance. Never load arbitrary downloaded native code from a model response.
2. Built-in tool registry maps each capability to a validated Kotlin schema and a native C++ operation. The AI may discover tools and request them, but cannot install binaries or execute arbitrary scripts.
3. Pinned native modules are vendored at reviewed versions, compiled by CMake for arm64-v8a (and x86_64 emulator where supported), with notices and reproducible builds.
4. PolyMesh remains canonical editable topology; renderer triangles are derived data. Every destructive operation validates input and enters undo history.
5. Import/export uses staged format support with memory limits, cancellation, Android SAF access, and explicit unsupported-format errors.
6. Terrain workspace should include heightfield grid, brush sculpt, smooth/flatten/raise/lower, hole mask, layer painting, tiled LOD and heightmap import; it is designed as an app-native module inspired by Terrain3D, not its GDExtension binary.
7. Tool quality gates include topology unit tests, malformed input, degenerate faces, large mesh limits, undo/redo and JNI bounds before enabling each tool to the agent.

## Current status and honesty

The extension catalog is exposed to the AI through get_modeling_extensions. The listed third-party projects are research candidates, not bundled dependencies in this commit. Existing native PolyMesh operations are integrated. Do not label an unported plugin as installed or promise full Blender parity.