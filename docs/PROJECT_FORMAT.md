# Formato de projeto .3dproj

O formato atual é JSON UTF-8 com a identificação `3d-ai-modeling-project` e versão inteira `1`.

## Dados persistidos

- Tipo de primitiva: cube, sphere, cylinder, cone, torus ou plane.
- Tamanho base da primitiva.
- Posição XYZ em unidades do editor (Y é o eixo vertical).
- Rotação Euler XYZ em graus.
- Escala XYZ.
- Cor RGB normalizada (0 a 1).
- ID do objeto selecionado, quando existe.

As malhas são reconstruídas no motor C++ a partir do tipo e do tamanho da primitiva. Isto mantém o projeto editável e reduz o tamanho do ficheiro. O formato ainda não guarda malhas arbitrárias importadas, UVs, materiais PBR, texturas, câmeras ou animações.

## Validação e limites

O leitor verifica a identificação e a versão, limita o ficheiro a 2 MB e a cena a 500 objetos, valida tipos, vetores, escala, tamanho e canais de cor antes de limpar ou reconstruir a cena. O histórico Undo/Redo é reiniciado depois do carregamento, para não misturar o histórico anterior com o novo projeto.

## Diferença entre projeto e exportação

- `.3dproj`: dados editáveis específicos desta aplicação.
- `.obj`: geometria triangulada com vértices, normais, faces e transformações aplicadas; não guarda os parâmetros originais das primitivas nem materiais PBR.
