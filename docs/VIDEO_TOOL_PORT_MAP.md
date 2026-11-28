# Mapa das ferramentas observadas no vídeo (MP4 anexado, 75 s)
Menus visíveis e correspondência com operações do Manus 3D:
- Mirror → mesh_operation: mirror (motor nativo)
- Soft normals → soft_normals (motor nativo)
- Combine → combine_objects (motor nativo)
- Delete → delete_object / delete_faces (motor nativo)
- Separate → detach_faces (motor nativo)
- Divide → subdivide (motor nativo)
- Smooth → smooth (motor nativo)
- Clone → clone_faces / duplicate_object (motor nativo)
- Cut loop → loop_cut (operação nativa; UX de seleção deve ser validada)
- Draw cut → connect/loop_cut (equivalência de operação; ferramenta interativa de desenho ainda requer UX)
- Select loop → seleção de loop de arestas (precisa implementar seleção/preview no viewport)
- Bevel → bevel (motor nativo; testes de topologia complexa pendentes)
- Extrude → extrude (motor nativo)
- Fill → create_face (motor nativo)
- Segment count → parâmetro de segmentos
- Vertex / Edge / Face / Object → set_select_mode + pick_component

A existência no motor ou catálogo não é prova de UI pronta: Draw Cut e Select Loop precisam de gesto, preview e confirmação, e devem ser marcadas incompletas até validação no dispositivo.
