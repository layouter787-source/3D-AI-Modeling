# Catálogo de ferramentas do vídeo (humano)

Fonte: gravação de ecrã de **3D modeling: Design my model** (app comercial de modelagem táctil).  
O vídeo **não** é do nosso app nem gerado por IA — é um artista humano a modelar um vaso + planta suculenta.

Objetivo: mapear **todas** as ações visíveis para o nosso motor (PolyMesh nativo + AI tools) e marcar o estado de implementação.

## 1. Modos de seleção (barra inferior)

| UI no vídeo | Função | Nosso estado | Tool / op nativa |
|-------------|--------|--------------|------------------|
| Vertex (ponto) | Selecionar vértices | Planeado (topologia) | `get_mesh_topology` + seleção futura |
| Edge (linha) | Selecionar arestas | Planeado | idem |
| Face (losango) | Selecionar faces | Planeado | `get_face_shell` (futuro) |
| Object (cubo) | Selecionar objeto inteiro | **Implementado** | `select` / `pickAt` / `get_scene` |

## 2. Menu Models / Create / More

| UI no vídeo | Função | Estado | Mapeamento |
|-------------|--------|--------|------------|
| Separate | Separar faces selecionadas em novo objeto | Planeado | `detach_faces` |
| Divide | Subdividir / triangulate faces ou mesh | Planeado (subdiv simples) | `mesh_operation` "divide" / "triangulate" |
| Smooth | Suavizar malha / normais | Planeado | `mesh_operation` "smooth" |
| Soft normals | Normais suaves (média de faces adjacentes) | **Parcial** (recompute) | `soft_normals` |
| Hard normals | Normais planas por face | **Parcial** | `hard_normals` |
| Combine | Juntar objetos | Planeado | `combine_objects` |
| Clone | Duplicar objeto | **Implementado nesta branch** | `clone_object` |
| Mirror | Espelhar | Planeado | `mesh_operation` "mirror" |
| Delete | Apagar | **Implementado** | `delete_object` |
| Repair | Reparar topologia / holes | Pesquisa | — |
| Merge close vertices | Soldar vértices próximos | Planeado | `weld` / `merge_vertices` |
| Lock / Hide | Bloquear ou esconder | UI futura | — |
| Display edge length | Mostrar comprimento de arestas | UI | — |

## 3. Operações de face / vértice (menu inferior)

| UI | Função | Estado |
|----|--------|--------|
| Create face by vertices | Criar face a partir de vértices ordenados | Planeado |
| Merge | Fundir vértices selecionados | Planeado |
| Target merge | Fundir para um vértice alvo | Planeado |
| Connect | Ligar dois vértices com aresta / diagonal | Planeado |

## 4. Bevel

- Diálogo de confirmação "Bevel? This action may require some time (20+ sec)".
- Parâmetros visíveis noutros frames: Segments, Radius, Smooth toggle.
- Estado: planeado como `mesh_operation` "bevel" (pesado; precisa de topologia real).

## 5. Materiais e shading

| UI | Função | Estado |
|----|--------|--------|
| Default material / Assign | Aplicar material PBR básico | Parcial (`set_color` hoje; PBR futuro) |
| Soft / Hard normals | Já coberto acima | Parcial |
| Material verde brilhante (folhas) / bege (vaso) | Cor + roughness/metallic | Cor implementada; PBR planeado |

## 6. Basic tools / viewport

| UI | Função | Estado |
|----|--------|--------|
| Grow or convert selection | Expandir seleção / converter tipo | Planeado |
| Add reference image | Imagem de referência | UI |
| Deselect | Limpar seleção | Fácil |
| Reset axis orient | Reset gizmo | UI |
| Display selected | Isolar seleção | Planeado |
| Ortho camera | Câmera ortográfica | Planeado |
| Transparent | Transparência | Planeado |
| Free move / rotate / scale | Transform livre | **Implementado** (`set_transform`, `move_by`…) |
| Snap | Snap à grelha | Planeado (`snap_to_grid`) |

## 7. Wireframe & display

- Wireframe on/off e wireframe-on-shaded aparecem implicitamente no fluxo de inspeção de topologia.
- Estado: **implementação iniciada** (`set_display_mode` + flag no renderer).

## 8. GLB / glTF

- O app do vídeo exporta formatos comuns; Nomad Sculpt (referência de mercado) exporta glTF/GLB.
- Nosso app: **roadmap** — `import_glb` / `export_glb` via Assimp ou parser mínimo (não ainda no APK).

## 9. Sequência observada no vídeo (vaso + planta)

1. Primitiva cilíndrica / prisma de muitos lados.
2. Modelagem do vaso (forma afunilada, buraco superior).
3. Criação de folhas (faces extrudidas / novas faces triangulares/poligonais).
4. Bevel em arestas selecionadas.
5. Soft normals + material verde brilhante nas folhas.
6. Material bege no vaso.
7. Ajustes finais de câmera e inspeção.

Com as tools nativas atuais + as desta branch o AI já consegue **aproximar** o resultado com primitivas + transform + cor + clone. Topologia fina (bevel real, extrude de faces) continua no roadmap PolyMesh.

## 10. Decisão de produto

- Nunca copiar código binário ou assets do app comercial.
- Aproximar comportamentos com operações nativas documentadas.
- `get_modeling_extensions` devolve o mapa de pesquisa (Blender / Prisma3D / Nomad / ZBrush) para o AI escolher sequências válidas.
