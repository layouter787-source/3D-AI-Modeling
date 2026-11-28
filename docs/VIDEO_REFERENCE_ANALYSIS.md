# Análise do vídeo de referência de modelação Android

Ficheiro analisado: `_android_3dmodeling_tutorial(MP4).mp4`.

## Observação geral

O vídeo mostra um fluxo de modelação poligonal num dispositivo Android, com menus inferiores, edição direta no viewport, gestos de órbita e zoom e operações aplicadas sobre uma planta e um vaso. Não há pessoas visíveis no vídeo.

## Ferramentas observadas

- Criação de primitivas: torus, pipe, plane, sphere, pyramid, cone, cylinder e cube.
- Modos de seleção: objeto, vértice, aresta e face.
- Edição: Extrude, Draw Face, Select Loop, Connect, Merge, Bevel, Smooth e Loop Cut.
- Organização de geometria: Clone, Mirror, Combine, Separate e Divide.
- Transformação: gizmos de translação e rotação por eixos X, Y e Z.
- Viewport: grid, eixos, seleção destacada, Snap, câmara ortográfica, órbita e zoom.
- Materiais: slots com pré-visualização, cor, brilho e transparência.
- Menus avançados visíveis: Texture Paint, Vertex Paint, UV Editor e Light.
- Operações pesadas apresentam estado de progresso durante a execução.

## Correspondência com a branch atual

Já existem no núcleo atual: primitivas, loft, escultura radial, extrude, inset, subdivisão, smooth, solidify, mirror, loop cut, triangulate, flip normals, weld, crease, booleanos, undo/redo, materiais PBR básicos, exportação OBJ/MTL e iluminação solar/ambiente.

Ainda não existem como sistemas completos: seleção persistente de vértices/arestas/faces no viewport, gizmos subelementares, Draw Face, bevel topológico dedicado, Combine/Separate por componentes, UV Editor, Texture Paint, Vertex Paint, slots de textura, transparência real, rigging e exportação GLB/glTF.

## Prioridade técnica

1. Criar um estado de seleção de subelementos com vértices, arestas e faces, incluindo raycast e hitboxes adequadas ao toque.
2. Adicionar gizmos de transformação para subelementos e feedback visual de seleção.
3. Implementar bevel topológico dedicado, seleção de loops, connect e operações de separação/combine.
4. Criar UVs e slots de textura antes de iniciar Texture Paint e mapas PBR.
5. Implementar exportação glTF/GLB depois de existir um modelo de materiais e texturas consistente.
6. Adicionar operações assíncronas com estado de progresso para subdivisão, smooth, booleanos e exportações grandes.

## Limitações da análise

O vídeo não demonstra o formato de exportação, a implementação interna do motor, física, rigging, animação ou suporte efetivo a GLB/FBX. A presença de menus no vídeo não prova que todos os sistemas estejam ativos naquele aplicativo.
