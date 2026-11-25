# Extension and Modeling Tool Research

Research snapshot: 25 November 2026. The links below are public project/documentation sources; check current upstream license, release, Android support and compatibility before copying code.

## Compatibility rule

This app is an Android Kotlin/Jetpack Compose interface with a C++17 NDK/OpenGL ES native mesh engine. Godot EditorPlugin/GDExtension addons are **not installable as drop-in plugins** in this app. We use their workflows as references and port features to our own scene/mesh API. Do not vendor code without reviewing its license and platform constraints.

## Researched projects

- **Terrain3D (Godot)** — editable terrain with clipmap/LOD, sculpting, holes, texture painting, heightmap import and foliage. The project notes platform-specific support. Port candidates: terrain mesh tile/LOD, heightmap import, terrain sculpt/paint, region streaming. Upstream: https://github.com/TokisanGames/Terrain3D ; docs: https://terrain3d.readthedocs.io/en/stable/
- **Polyforge (Godot)** — Blender-style edit workflow including transform shortcuts, extrude, inset, bevel, loop cut, merge, mirror, Boolean, proportional editing and component selection. Port candidates: vertex/edge/face selection, bevel, proportional editing, selection overlays and last-operation controls. Upstream: https://github.com/Seba313/polyforge
- **GoBuild (Godot)** — mesh editing at vertex, edge and face level, with selection and transform handles. Port candidates: native sub-element picking and manipulators. Upstream: https://github.com/marcel-b-roodt/gobuild
- **Blender BMesh** — topology data structures and low-level mesh-editing operations that underpin mesh tools. Reference, not a bundled Android plugin: https://developer.blender.org/docs/features/objects/mesh/bmesh/ and https://docs.blender.org/api/main/bmesh.html
- **Materializr CAD** — parametric CAD, solid modeling and exchange formats; relevant to a future CAD/solid workspace. Upstream: https://github.com/materializr-cad/materializr
- **Strata:3D (Android)** — native 3D viewing and mesh-format support; useful as a reference for import/viewer performance and formats. Upstream: https://github.com/damzenquis/Strata-3D

## Native features currently exposed

The app already provides a C++ editable PolyMesh layer, topology inspection, undoable mesh operations, custom loft mesh creation and vertex sculpt deformation. AI tools expose subdivision, extrude/inset by face IDs, smooth, solidify, mirror, loop cut by edge vertex IDs, triangulate, flip normals, weld, crease by angle and Boolean union/difference/intersection. Web search and public HTTPS page reading are available to the AI agent.

The in-app Extension Library is a research/shortcut panel, not a claim that external Godot addons are installed or executed. Native implementations must be tested on Android and must remain undoable where they modify the scene.

## Suggested next implementation stages

1. Vertex/edge/face selection and topology overlays.
2. Bevel/chamfer, merge/dissolve and bridge edge loops.
3. Proportional editing, snapping and transform gizmos.
4. UV coordinates, unwrap tools and texture painting.
5. Terrain workspace: heightmap import, brush sculpting, hole/paint layers and tiled LOD.
6. Import/export validation for OBJ, glTF/GLB, STL and PLY.
7. Plugin/extension SDK only after defining sandboxing, permissions, versioning and ABI boundaries.
