# Provedores de IA

O app usa **apenas chave de API**: não há login com conta OpenAI/ChatGPT nem com conta Claude. O utilizador escolhe o serviço, cola a chave e a ligação fica pronta, porque a base URL de cada serviço já vem predefinida. Interface comum `AiProvider` em `app/src/main/java/com/layouter/aimodeling/ai/`.

| Serviço | Base URL | Formato | Onde obter a chave |
|---|---|---|---|
| OpenAI | `https://api.openai.com/v1` | Chat Completions | platform.openai.com/api-keys |
| Claude | `https://api.anthropic.com/v1` | Messages API | console.anthropic.com |
| Google Gemini | `https://generativelanguage.googleapis.com/v1beta/openai` | Chat Completions (compatibilidade) | aistudio.google.com/apikey |
| NVIDIA NIM | `https://integrate.api.nvidia.com/v1` | Chat Completions | build.nvidia.com (chaves `nvapi-`) |

OpenAI, Gemini e NVIDIA NIM usam o mesmo `OpenAiProvider`; só muda a base URL. O Claude usa o `AnthropicProvider`. Todos os provedores estão implementados mas **não foram testados**.

Notas por serviço:
- **Gemini 3:** exige que a `thought_signature` das chamadas de ferramenta seja devolvida na mensagem seguinte; o app guarda o `extra_content` de cada chamada e reenvia-o. Ferramentas sem argumentos são enviadas sem `parameters`.
- **NVIDIA NIM:** o nível gratuito tem limite de pedidos por minuto; o modelo precisa de suportar chamadas de ferramenta.
- Os nomes de modelo predefinidos são sugestões e podem ser mudados nas definições.

## Ciclo de conversa
`AiAgent`: envia o pedido, executa as ferramentas pedidas pela IA no `ToolDispatcher`, devolve os resultados e repete até a IA responder só com texto. Limites: 6 rondas e 40 chamadas de ferramenta por pedido; histórico de 60 mensagens. O painel de chat (`AiPanel`) abre com o botão "IA".

## Chave de API
- Guardada cifrada (AES-GCM) com uma chave do Android Keystore (`SecretStore`). Nunca é registada em logs.
- Só é enviada ao serviço escolhido; o uso é cobrado na conta do utilizador nesse serviço.

## Ferramentas expostas à IA

Implementadas (`ToolDispatcher`, sobre o `Editor` em C++, todas com undo/redo):

| Ferramenta | Argumentos |
|---|---|
| `create_primitive` | `type` (cube, sphere, cylinder, cone, torus, plane), `size`, `position` |
| `set_transform` | `object_id`, `position`, `rotation_degrees`, `scale` |
| `move_by` | `object_id`, `offset` |
| `rotate_by` | `object_id`, `degrees` |
| `scale_by` | `object_id`, `factor` |
| `set_color` | `object_id`, `color` (r, g, b de 0 a 1) |
| `delete_object` | `object_id` |
| `undo` / `redo` | - |
| `get_scene` | devolve a cena em JSON |

Planeadas (precisam de edição de malha): `extrude_faces`, `bevel`, `boolean`, `subdivide`, `mirror`, seleção de faces/arestas/vértices.

## Segurança
- Todos os argumentos são validados (JSON, 3 números finitos, ids existentes). O núcleo limita tamanho, escala, cor e o número de objetos (500).
- A IA só opera sobre objetos que existem na cena.
- A descrição da cena e os resultados das ferramentas são dados, nunca instruções.
- Conteúdo de ficheiros importados será tratado como dado, nunca como instruções para a IA.
