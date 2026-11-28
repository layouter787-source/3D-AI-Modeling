# Plano de implementação — réplica funcional 3D AI

## Objetivo
Evoluir a base Android para um estúdio de modelação assistida por IA com ferramentas nativas reais, materiais PBR básicos e interfaces profissionais.

## Entregas desta fase
- Material PBR básico no núcleo nativo: metallic, roughness, clearcoat e emissão.
- Ferramenta de material exposta ao agente e refletida na descrição da cena.
- Chat sem balões, com timeline editorial, estados de ferramentas e composer profissional.
- Editor com chrome de estúdio, inspector visual e ações mais claras para viewport/cena.
- Documentação do que é realmente implementado e das limitações técnicas.

## Estrutura
- `app/src/main/cpp/core/`: cena, materiais e comandos undo/redo.
- `app/src/main/cpp/native-lib.cpp`: shaders OpenGL ES e ponte JNI.
- `app/src/main/java/.../ai/`: agente, ferramentas e schemas.
- `app/src/main/java/.../AiPanel.kt`: conversa e configuração do agente.
- `app/src/main/java/.../EditorScreen.kt`: viewport e chrome do estúdio.

## Critérios de qualidade
A IA só poderá chamar ferramentas declaradas e validadas. Nenhuma UI deve prometer textura/rigging/importação que ainda não exista. Alterações de cena entram no undo/redo e a interface deve permanecer legível em tema escuro e claro.
