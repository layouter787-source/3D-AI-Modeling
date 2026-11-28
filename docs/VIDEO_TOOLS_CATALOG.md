# Catálogo de ferramentas do vídeo (humano)

Fonte: gravação de ecrã de **3D modeling: Design my model**.  
O vídeo **não** é do nosso app nem gerado por IA.

## Estado de implementação (branch feature/video-tools-and-streaming)

| UI no vídeo | Tool nativa / AI | Estado |
|-------------|------------------|--------|
| Object select | select / pick / get_scene | **OK** |
| Face / Edge / Vertex mode | set_select_mode | **OK** (estado; pick por face ainda limitado) |
| Clone | clone_object | **OK** |
| Delete | delete_object | **OK** |
| Soft normals | mesh_operation soft_normals | **OK** |
| Hard normals | mesh_operation hard_normals | **OK** |
| Divide | mesh_operation divide/subdivide | **OK** |
| Smooth | mesh_operation smooth | **OK** |
| Bevel | mesh_operation bevel (amount, segments) | **OK** (aproximação) |
| Extrude | mesh_operation extrude | **OK** |
| Mirror | mesh_operation mirror | **OK** |
| Merge close / weld | mesh_operation weld | **OK** |
| Combine | combine_objects | **OK** |
| Separate | separate_faces | **OK** |
| Wireframe | set_display_mode | **OK** |
| Free move/rotate/scale | set_transform / move_by / … | **OK** |
| Materials (cor) | set_color | **OK** (PBR completo não) |
| Create face by vertices | — | Planeado |
| Target merge / Connect | — | Planeado |
| GLB import/export | — | **Planeado** (sem dep Assimp ainda) |
| Ortho / Snap / Transparent | — | UI planeada |

## Como o AI usa

```
mesh_operation { object_id, operation: "bevel", amount: 0.05, segments: 2 }
mesh_operation { object_id, operation: "extrude", amount: 0.1, faces: [0,1,2] }
combine_objects { object_id_a, object_id_b }
set_display_mode { mode: "wireframe" }
```

Ver `get_modeling_extensions` em runtime.
