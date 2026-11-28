# Catálogo de ferramentas do vídeo → implementação

Fonte: gravação humana em *3D modeling: Design my model*.

## Estado (branch feature/video-tools-and-streaming)

| Ferramenta vídeo | Tool / op nativa | Estado |
|------------------|------------------|--------|
| Object select | select / pick / get_scene | OK |
| Clone | clone_object | OK |
| Delete | delete_object | OK |
| Free move/rotate/scale | set_transform, move_by, rotate_by, scale_by | OK |
| Soft normals | mesh_operation soft_normals | OK |
| Hard normals | mesh_operation hard_normals | OK |
| Divide | mesh_operation divide | OK |
| Smooth | mesh_operation smooth | OK |
| Mirror | mesh_operation mirror (segments=axis) | OK |
| Merge close vertices | mesh_operation weld | OK |
| Bevel | mesh_operation bevel (approx) | OK (aproximação) |
| Extrude | mesh_operation extrude | OK (approx) |
| Combine | combine_objects | OK |
| Separate | separate_faces | OK |
| Wireframe | set_display_mode | OK |
| Materials color | set_color | OK |
| Vertex/Edge/Face modes | set_select_mode | Parcial |
| Create face / Connect / Target merge | — | Planeado |
| GLB import/export | — | Planeado |
| PBR materials | — | Planeado |

## Uso AI

```
mesh_operation { object_id, operation: "soft_normals" }
mesh_operation { object_id, operation: "divide" }
mesh_operation { object_id, operation: "smooth", amount: 0.5, segments: 5 }
mesh_operation { object_id, operation: "bevel", amount: 0.05, segments: 2 }
mesh_operation { object_id, operation: "mirror", segments: 0 }
mesh_operation { object_id, operation: "weld", amount: 0.001 }
mesh_operation { object_id, operation: "extrude", amount: 0.1, face_indices: "0,1,2" }
combine_objects { object_id_a, object_id_b }
separate_faces { object_id, face_indices: "3,4,5" }
set_display_mode { mode: "wireframe" }
```
