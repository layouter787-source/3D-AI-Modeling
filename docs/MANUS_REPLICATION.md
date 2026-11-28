# Réplica funcional do fluxo 3D AI Modeling

## Estado analisado

Repositório de origem: `layouter787-source/3D-AI-Modeling`  
Licença: Apache-2.0  
Branch local de trabalho: `replica/manus-3d-ai`  
Base escolhida: `origin/feature/studio-levelup-extensions-theme` (`92814d2`)

A base escolhida é a linha mais recente do repositório e inclui a evolução da branch `feature/blender-native-mesh-workflow`, além de melhorias de interface, tema, ferramentas de edição, histórico de conversas e catálogo de extensões.

## Branches e papel de cada uma

- `main`: protótipo inicial. Contém viewport OpenGL ES, primitivas, transformações, seleção, undo/redo e chat com provedores externos.
- `feature/mesh-tools`: primeira consolidação do núcleo editável. Introduz `PolyMesh`, parser JSON, operações de malha, CSG, materiais/cena mais estruturados e um registo amplo de ferramentas.
- `feature/modern-ai-chat-model-picker`: melhora o chat, seleciona modelos, guarda conversas, aceita imagens de referência e adiciona o primeiro fluxo de loft para malhas contínuas.
- `feature/studio-unified-polished`: integra a base de malha e melhora a experiência do estúdio, incluindo ferramentas de escultura e organização da interface.
- `feature/blender-native-mesh-workflow`: aprofunda o fluxo técnico de modelação. Adiciona topologia poligonal preservada, inspeção de malha, operações nativas inspiradas no Blender, booleanos, loft, escultura, exportação OBJ/MTL e agente iterativo sem teto artificial fixo.
- `feature/studio-levelup-theme-tools-plugins`: acrescenta pesquisa de extensões, catálogo de integrações e tema monocromático com acento azul.
- `feature/studio-levelup-extensions-theme`: variante mais recente da linha anterior, com documentação de extensões e ligações Android para documentação externa. É a base usada nesta réplica local.

As duas branches finais (`studio-levelup-theme-tools-plugins` e `studio-levelup-extensions-theme`) divergem depois da base Blender-native. A segunda foi escolhida por ser a mais recente e conter a documentação e os pontos de entrada adicionais.

## Fluxo funcional completo

1. O utilizador escreve um pedido no painel Compose ou anexa uma imagem de referência.
2. `AiPanel` carrega as definições do provedor, modelo e chave guardada no Android Keystore.
3. `AiAgent` adiciona a mensagem ao histórico e envia ao provedor escolhido:
   - prompt de sistema especializado em modelação 3D;
   - histórico da conversa;
   - descrição JSON da cena atual;
   - definições JSON Schema das ferramentas;
   - imagem em base64 quando o provedor/modelo suporta visão.
4. O provedor (`OpenAiProvider` ou `AnthropicProvider`) normaliza a resposta para `AiTurn`, incluindo texto e chamadas de ferramentas.
5. `ToolDispatcher` valida o nome e os argumentos, limita tamanhos e valores e encaminha a operação para:
   - pesquisa/leitura de páginas públicas;
   - operações de cena e transformações;
   - geração de primitivas, lotes, presets e lofts;
   - inspeção de objetos e topologia;
   - escultura e operações de malha;
   - undo/redo.
6. Cada alteração passa pelo `Editor` C++ e entra no histórico reversível de comandos.
7. O núcleo nativo atualiza a cena e o viewport OpenGL ES; o resultado estruturado volta ao agente.
8. O agente pode inspecionar, corrigir e refinar a cena em novas rondas até receber uma resposta sem chamadas de ferramentas ou ocorrer uma falha real.
9. O painel apresenta o texto, os resultados das ferramentas e a cena atualizada.
10. A cena pode ser exportada em OBJ/MTL; o formato de projeto também é serializado pelo codec Kotlin.

## Capacidades reproduzíveis

A implementação pode reproduzir integralmente, em termos de arquitetura e comportamento observável:

- agente com chamadas de ferramentas e histórico;
- validação determinística entre o modelo e o motor 3D;
- geração combinada de primitivas e malhas loft;
- escultura radial sobre vértices;
- inspeção paginada da topologia;
- extrusão, inset, subdivisão, suavização, solidify, mirror, loop cut, triangulação, normais, weld, crease e booleanos;
- seleção, câmera orbital, pan, zoom e picking;
- undo/redo baseado em comandos;
- consulta da cena como JSON;
- pesquisa web tratada como dado não confiável;
- conversas persistentes e referências de imagem, conforme o provedor;
- armazenamento cifrado das chaves no Android Keystore;
- exportação básica OBJ/MTL;
- temas e catálogo de extensões.

## Limites que não podem ser copiados literalmente

A réplica pode reproduzir a interface de ferramentas e o ciclo de execução, mas não deve afirmar que contém o modelo interno ou a infraestrutura privada do Manus/ChatGPT. A inteligência de planeamento depende do modelo configurado pelo utilizador ou de um backend autorizado. Também não se deve prometer que o renderer atual possui UVs, texturas PBR completas, rigging, animação, importação glTF ou escultura arbitrária: o próprio prompt do agente declara essas limitações.

## Arquitetura de segurança

O modelo não escreve C++ nem altera a malha diretamente. Ele emite chamadas para todas as ferramentas declaradas e pode repeti-las sem um teto artificial de rondas, lotes ou faces. O dispatcher valida JSON, números finitos, IDs e operações permitidas; o núcleo mantém apenas guardas de validade e memória para evitar corrupção ou encerramento do processo. O editor nativo é a autoridade para modificar a cena. Conteúdo pesquisado na web deve ser tratado como dado, nunca como instrução de sistema.

As chaves de API devem permanecer no Android Keystore e nunca entrar no histórico, logs, commits ou artefactos de distribuição. A licença Apache-2.0 e os avisos de copyright devem ser preservados em qualquer distribuição derivada.

## Próxima implementação recomendada

Para transformar esta base numa réplica distribuível, a sequência técnica é:

1. adicionar ou restaurar o Gradle Wrapper;
2. compilar a branch escolhida com JDK 17, Android SDK 35, NDK e CMake 3.22.1;
3. corrigir erros nativos ou Kotlin encontrados pelo build;
4. executar testes de ferramentas, serialização, undo/redo, topologia e limites de segurança;
5. testar num dispositivo Android o viewport, gestos, chat, imagem e exportação;
6. configurar um provedor de IA autorizado e testar chamadas reais sem incluir chaves no código;
7. só depois gerar APK e decidir se o resultado deve ser publicado num repositório remoto.


## Implementado nesta fase

A branch local agora possui material PBR básico real no estado nativo da cena, com comando undo/redo, uniforms no shader OpenGL ES 3.0, ferramenta `set_material` para o agente e presets rápidos no editor. O chat foi reorganizado para uma timeline sem balões, com estados de ferramenta mais discretos e papéis explícitos.
