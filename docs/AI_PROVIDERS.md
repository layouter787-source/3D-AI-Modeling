# Provedores de IA

Interface comum `AiProvider` (em `app/src/main/java/com/layouter/aimodeling/ai/`).

## Estado

| Provedor | Estado |
|---|---|
| `OpenAiProvider` (chave de API do utilizador, Chat Completions) | implementado, não testado |
| `AnthropicProvider` (chave de API do utilizador, Messages API) | implementado, não testado |
| `ChatGptPlanProvider` (login "Continue with ChatGPT", OAuth/PKCE) | planeado: depende do registo do cliente na OpenAI |
| `LocalProvider` (modelo no dispositivo) | planeado |

O Claude só é suportado por chave de API, porque a Anthropic não permite login de conta Claude em apps de terceiros.

O ciclo de conversa está no `AiAgent`: envia o pedido, executa as ferramentas pedidas pela IA no `ToolDispatcher`, devolve os resultados e repete até a IA responder só com texto. Limites: 6 rondas e 40 chamadas de ferramenta por pedido; histórico de 60 mensagens. O painel de chat (`AiPanel`) usa o botão "IA" no topo do ecrã.

## Chave de API
- Guardada cifrada (AES-GCM) com uma chave do Android Keystore (`SecretStore`). Nunca é registada em logs.
- Só é enviada ao serviço escolhido; o uso é cobrado na conta de API do utilizador.
- O nome do modelo é editável nas definições, porque os modelos mudam com o tempo.

## Login ChatGPT (planeado)
- Abrir o navegador (Custom Tabs) com `state`, `nonce` e PKCE `S256`.
- Retorno por deep link registado.
- Trocar o código no endpoint de token, validar `state` e o ID token.
- Guardar tokens no Android Keystore. Não reutilizar o client ID do Codex CLI: usar o registo oficial.
- Tratar consentimento recusado, limite de uso atingido e sessão revogada, voltando ao modo manual.

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
