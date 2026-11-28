# Provedores de IA e modelos dinâmicos

O aplicativo liga-se às APIs com a chave fornecida pelo utilizador. Não simula login nem inclui modelos fixos na interface.

## Provedores integrados

| Provedor | Endpoint de conversa | Endpoint usado para descobrir modelos |
|---|---|---|
| OpenAI | Chat Completions | `GET /v1/models` |
| Claude / Anthropic | Messages API | `GET /v1/models` (com paginação) |
| Google Gemini | API compatível com OpenAI | API nativa `GET /v1beta/models`; apenas modelos que anunciam `generateContent` |
| NVIDIA NIM | Chat Completions compatível | `GET /v1/models` |

Os IDs são recebidos diretamente do serviço, apresentados no seletor e guardados por provedor. O utilizador pode também introduzir manualmente um ID publicado pelo seu serviço. Não há fallback silencioso para um modelo fictício: é necessário escolher um modelo.

A lista é a lista visível à chave/conta usada e pode incluir modelos que não suportam todas as ferramentas. A API de conversa é a autoridade final sobre disponibilidade, permissões e compatibilidade.

## Fluxo da aplicação

- `AiPanel.kt`: conversa, configurações, escolha de provedor e seletor de modelos.
- `ModelCatalog.kt`: consulta as APIs reais e normaliza os IDs.
- `HttpJson.kt`: pedidos HTTP autenticados executados em `Dispatchers.IO`.
- `AiAgent.kt`: histórico, chamadas de ferramentas e limites de segurança.
- `ToolDispatcher.kt`: valida os argumentos e executa apenas ferramentas expostas pelo motor.

## Chaves

As chaves são guardadas cifradas com AES-GCM e Android Keystore (`SecretStore`). Não são escritas em logs. A chave é enviada apenas ao endpoint do provedor selecionado. O custo e os limites são os da conta/API desse provedor.

## Ferramentas da cena

- criar primitivas (cubo, esfera, cilindro, cone, toro e plano)
- transformar, mover, rodar, escalar e colorir objetos
- apagar, desfazer e refazer
- consultar a cena
- aplicar material PBR básico por objeto (`metallic`, `roughness`, `clearcoat` e `emission`)

A IA só altera a cena através destas ferramentas. Os parâmetros são validados no Kotlin e no núcleo C++.

## Ícones e interface

A interface utiliza ícones vetoriais consistentes e escaláveis em vez de símbolos Unicode usados como botões. O estilo é compacto, escuro e de alto contraste, com conversa, composer e configurações separadas.

Referência de desenho SVG consultada: [Lucide Icons](https://lucide.dev/icons/). O projeto Android usa ícones vetoriais Compose/Android para manter renderização nativa e acessibilidade.

## Orquestrador e recuperação automática

`AiOrchestrator` coordena os provedores integrados:

1. Tenta primeiro o provedor/modelo escolhido na configuração.
2. Se a chamada falhar, tenta outros modelos anunciados pela API autenticada desse provedor.
3. Se não recuperar, muda para os outros provedores integrados que tenham uma chave guardada e modelo disponível.
4. Prioriza o último provedor/modelo que respondeu com sucesso.
5. Cacheia a descoberta por 10 minutos para não consultar os catálogos em cada mensagem.

O router não inventa nomes de modelos. O catálogo pode falhar por indisponibilidade; nesse caso, ainda pode tentar o ID selecionado manualmente, mas não fabrica alternativas.

O failover é limitado a 12 chamadas de modelo por turno (até 4 modelos por provedor integrado). Chaves com erro HTTP 401/403 são excluídas do restante desse turno. Cancelamento do utilizador não é tratado como falha de provedor.

A resposta só é processada e as ferramentas da cena só são executadas depois de uma chamada ao modelo ser concluída com sucesso. Assim, o router não repete uma operação 3D apenas porque a conexão caiu antes de uma resposta válida. Se todos os modelos falharem, o chat mostra a causa resumida e pode ser usado novamente quando a rede/API voltar.


## Capacidades adicionais do agente (branch de desenvolvimento)

### Ferramentas 3D atuais

- `create_batch`: cria até 30 malhas primitivas numa chamada, com posição, escala, rotação e cor.
- `create_asset_preset`: kits paramétricos editáveis de árvore, mesa, casa, robô, barril, escadas e candeeiro.
- `duplicate_object`: duplica mantendo transformação e cor.
- `get_object_details`: consulta propriedades completas de um objeto.
- ferramentas existentes: criar primitiva, mover, rodar, escalar, transformar, colorir, apagar, seleção, undo/redo e estado da cena.
- Tesselação procedural aumentada para esfera (48x32), cilindro/cone (48 segmentos) e toro (64x24), mantendo índices dentro do limite atual de 16 bits.

Estes recursos aumentam a densidade geométrica e permitem composições mais complexas. O viewport já possui materiais PBR básicos no estado nativo e no shader OpenGL ES, mas isso **não equivale ainda a texturas PBR completas, UVs, pintura de mapas, rigging, importação GLB ou renderização fotorealista**; essas funcionalidades exigem sistemas adicionais de textura, animação, importador e renderização.

### Pesquisa na Internet

- `web_search`: pesquisa referências e documentação pública via HTTPS.
- `read_webpage`: lê páginas públicas HTTPS, com limites de tempo/tamanho e bloqueio básico de hosts locais.
- O conteúdo externo é tratado como dados não confiáveis: não pode substituir instruções do sistema nem autorizar operações.

### Limites deliberados

A versão atual **não executa código arbitrário descarregado da Internet**, não instala extensões silenciosamente e ainda não possui um cliente MCP configurável pelo utilizador, importador de modelos GLB/glTF nem ferramenta de download/importação de assets. Para isso será necessário implementar:
1. Gestor MCP com configuração manual do servidor, descoberta `tools/list`, confirmação do utilizador e permissões por conector.
2. Download de assets 3D com licença, domínio, tamanho, formato e verificação de ficheiro.
3. Importador glTF/GLB e validação de malha antes de inserir no projeto.
4. Execução de scripts em sandbox, sem acesso direto a chaves, armazenamento privado ou rede arbitrária.

Não se deve prometer “autonomia infinita”: chamadas têm limites para evitar ciclos, custos descontrolados e alterações massivas acidentais.
