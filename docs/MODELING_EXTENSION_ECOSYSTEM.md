# Modeling extension ecosystem — research and integration plan

This is an Android modeling application (Jetpack Compose + C++17/NDK + OpenGL ES), not a Godot game. Godot editor addons cannot be copied into this APK as plugins without their host engine and API.

## Integrated native tools (current branch)

The app's own editable PolyMesh pipeline currently exposes: Catmull–Clark subdivision, face extrusion, inset, bevel, smoothing, solidify, mirror, loop cut, triangulation, normal flipping, weld, crease by angle, Boolean union/difference/intersection, radial vertex sculpt, lofted profile mesh, paged topology inspection, face shell / detach / combine, and undo/redo for mesh changes. AI receives these through the native tool dispatcher; these are app features, not downloaded add-ons.

## Industry tool research (Blender, Prisma3D and peers) — 2026

The agent catalog (`get_modeling_extensions`) now includes reference maps so the AI can approximate professional workflows with native operations.

### Sculpt brushes (common across Blender Sculpt, ZBrush, Nomad, Prisma3D, Mesh Morpher)

| Brush | Effect | Typical use | Native approximation |
|---|---|---|---|
| Draw / Standard | Push/pull along normals | Primary forms | `sculpt_mesh` offset along normal |
| Clay / Clay Strips | Accumulative material | Organic buildup | Repeated outward `sculpt_mesh` + `smooth` |
| Inflate | Swell volume | Rounder shapes | Radial outward `sculpt_mesh` |
| Grab / Move / Snake Hook | Area move with falloff | Proportions, silhouette | Large-radius directional `sculpt_mesh` |
| Smooth / Relax | Average surface | Cleanup | `mesh_operation` smooth |
| Flatten / Scrape | Planarize / remove highs | Hard-surface | `sculpt_mesh` toward plane |
| Pinch | Sharpen toward stroke | Edges | Narrow `sculpt_mesh` + crease |
| Crease | Sharp grooves | Folds, seams | Inward narrow sculpt + crease_by_angle |
| Fill | Raise depressions | Leveling | Outward sculpt into cavities |
| Elastic / Pose | Topology-preserving deform | Large form changes | Large sculpt + smooth |
| Mask / Face Sets | Isolate regions | Targeted edits | `get_face_shell` + `detach_faces` |

### Mesh edit tools

Native: extrude, inset, bevel, loop_cut, connect, subdivide, solidify, mirror, boolean_*, weld, merge_vertices, target_merge, triangulate/divide, flip/soft/hard normals, crease_by_angle, detach_faces, combine_objects, draw_face/create_face, clone_faces.

Prisma3D also exposes Extrude, Knife, mesh creator and digital sculpting on mobile — mapped to the same native set plus `sculpt_mesh`.

### Deformations (reference only unless noted)

Lattice/FFD, Simple Deform (bend/twist/taper), Curve deform, Shrinkwrap/Conform, Displace, Cast/Wave: **not native**. Approximate with multi-stroke `sculpt_mesh` and `smooth`. Smooth modifier **is** available.

### Remesh / retopology (research)

Voxel Remesh, QuadriFlow / Quad Remesher, ZBrush DynaMesh/Sculptris, Multires stacks, manual Poly Build / Contours: **candidates**. Approximate dense work with higher `detail` primitives + subdivide + sculpt; manual retopo via topology inspection + draw_face/loop_cut/connect.

### Software comparison (short)

- **Blender**: full free suite (sculpt multires/dyntopo, edit mode, modifiers, Geometry Nodes, retopo).
- **Prisma3D**: mobile modeling + rigging + animation (Extrude, Knife, sculpt, OBJ/FBX/glTF).
- **ZBrush**: high-poly organic standard (DynaMesh, deep brushes, PolyPaint).
- **Nomad Sculpt**: tablet sculpt, voxel booleans, multires.
- **3DCoat**: voxel + surface + AUTOPO retopo.
- **Mesh Morpher / Mush3D**: GPU sculpt on animated caches, node graphs.

## Research findings (libraries)

| Project / add-on | Useful capability | License / compatibility | Decision |
|---|---|---|---|
| [Terrain3D](https://github.com/TokisanGames/Terrain3D) | Sculptable heightmap terrain, holes, texture layers, LOD, foliage | MIT; C++ Godot GDExtension | Port concepts only into Android terrain module |
| [meshoptimizer](https://github.com/zeux/meshoptimizer) | GPU cache optimization, simplification, LOD | MIT; C/C++ | Strong candidate for optimize + decimate + LOD |
| [OpenSubdiv](https://github.com/PixarAnimationStudios/OpenSubdiv) | Production subdivision | Apache-2.0 | Candidate after NDK/size audit |
| [Manifold](https://github.com/elalish/manifold) | Robust manifold Booleans | MIT | Candidate behind existing Boolean API |
| [libigl](https://github.com/libigl/libigl) | Remeshing, parameterization, analysis | MPL-2.0 + exceptions | Select modules after license audit |
| [Assimp](https://github.com/assimp/assimp) | Multi-format import | BSD-3-Clause | Candidate for expanded import |
| Blender BMesh / operators | Topology reference | GPL | Behavioral reference only |
| GrimTopo / CurioMesh | Drawn retopo | Blender add-ons | Study algorithms; no Python execution in APK |

## Planned native extension architecture

1. Extension manifest records name, version, license, Android ABI support, capabilities, dependencies, enabled state and provenance. Never load arbitrary downloaded native code from a model response.
2. Built-in tool registry maps each capability to a validated Kotlin schema and a native C++ operation. The AI may discover tools and request them, but cannot install binaries or execute arbitrary scripts.
3. Pinned native modules are vendored at reviewed versions, compiled by CMake for arm64-v8a (and x86_64 emulator where supported), with notices and reproducible builds.
4. PolyMesh remains canonical editable topology; renderer triangles are derived data. Every destructive operation validates input and enters undo history.
5. Import/export uses staged format support with memory limits, cancellation, Android SAF access, and explicit unsupported-format errors.
6. Terrain workspace should include heightfield grid, brush sculpt, smooth/flatten/raise/lower, hole mask, layer painting, tiled LOD and heightmap import; it is designed as an app-native module inspired by Terrain3D, not its GDExtension binary.
7. Tool quality gates include topology unit tests, malformed input, degenerate faces, large mesh limits, undo/redo and JNI bounds before enabling each tool to the agent.

## Current status and honesty

The extension catalog is exposed to the AI through `get_modeling_extensions`. It now contains full research maps for sculpt brushes, mesh-edit tools, deformations, remesh/retopo and major DCC apps (Blender, Prisma3D, ZBrush, Nomad, 3DCoat, Mesh Morpher). Third-party projects remain research candidates, not bundled dependencies. Existing native PolyMesh operations are integrated. Do not label an unported plugin as installed or promise full Blender parity; approximate with documented native sequences instead.
