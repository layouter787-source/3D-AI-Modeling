# Modeling extension ecosystem

Este documento descreve o que está **instalado de verdade** no APK e o que é apenas catálogo de pesquisa para o AI aproximar workflows profissionais.

## Operações nativas atuais (branch feature/video-tools-and-streaming)

- Primitivas: cube, sphere, cylinder, cone, torus, plane
- Transform: set / move_by / rotate_by / scale_by
- Cor (RGB)
- Clone object
- Delete, undo, redo
- get_scene / describeScene
- Seleção por id / pick

## Roadmap PolyMesh (próximas iterações)

Catmull-Clark, extrude, inset, bevel, smooth, solidify, mirror, loop cut, triangulate/divide, soft/hard normals, weld, boolean, detach/combine, sculpt_mesh, vertex paint, loft.

## Catálogo de pesquisa (exposto via get_modeling_extensions)

O AI deve chamar `get_modeling_extensions` quando o utilizador mencionar Blender, ZBrush, Nomad, Prisma3D, sculpt brushes ou retopo. O JSON devolvido lista:

- Brushes de sculpt e a aproximação nativa possível
- Operações de mesh-edit e estado (nativo / planeado)
- Deformações de referência (não nativas)
- Remesh / retopo de referência

**Regra de ouro:** nunca afirmar que um plugin de terceiros está instalado. Aproximar com as tools listadas em `toolDefinitions()`.

## Streaming da resposta da IA

- Preferência: WebSocket bidirecional (cancelamento, tool approval mid-stream).
- Fallback: SSE.
- WebTransport quando o backend e o cliente Android suportarem QUIC de forma estável.
- O `AiAgent` já entrega texto via callback `onText`; a UI deve consumir deltas quando o provider passar a emitir tokens.

## Wireframe & GLB

- Wireframe: flag de display + draw em linhas (implementação incremental no renderer).
- GLB: import/export planeado; formatos atuais de exportação limitados.
