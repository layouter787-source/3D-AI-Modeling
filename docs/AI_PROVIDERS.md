# Provedores de IA

Interface comum `AiProvider` (em `app/src/main/java/com/layouter/aimodeling/ai/`) com três implementações planeadas:

1. **ChatGptPlanProvider**: login "Continue with ChatGPT" (OAuth 2.0 com PKCE) e pedidos à Responses API usando o plano do utilizador. Cliente registado pelo fluxo oficial de apps open source. Não reutilizar o client ID do Codex CLI.
2. **ApiKeyProvider**: chave de API do próprio utilizador (OpenAI ou Anthropic). O Claude só é suportado por esta via, porque a Anthropic não permite login de conta Claude em apps de terceiros.
3. **LocalProvider** (opcional): modelo no dispositivo.

Estado: a interface `AiProvider`, o `ToolDispatcher` e as definições das ferramentas já existem. Falta implementar os provedores e o ciclo de conversa (enviar pedido, executar as ferramentas, devolver o resultado à IA).

## Login no Android
- Abrir o navegador (Custom Tabs) com `state`, `nonce` e PKCE `S256`.
- Retorno por deep link registado.
- Trocar o código no endpoint de token, validar `state` e o ID token.
- Guardar tokens no Android Keystore. Nunca registar tokens em logs.
- Tratar consentimento recusado, limite de uso atingido e sessão revogada, mostrando uma mensagem clara e voltando ao modo manual.

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
- Conteúdo de ficheiros importados é dado, nunca instruções para a IA.
- Falta limitar o número de chamadas de ferramentas por pedido (no ciclo de conversa).
