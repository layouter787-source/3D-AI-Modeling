# Provedores de IA

Interface comum `AiProvider` com três implementações:

1. **ChatGptPlanProvider**: login "Continue with ChatGPT" (OAuth 2.0 com PKCE) e pedidos à Responses API usando o plano do utilizador. Cliente registado pelo fluxo oficial de apps open source. Não reutilizar o client ID do Codex CLI.
2. **ApiKeyProvider**: chave de API do próprio utilizador (OpenAI ou Anthropic). O Claude só é suportado por esta via, porque a Anthropic não permite login de conta Claude em apps de terceiros.
3. **LocalProvider** (opcional): modelo no dispositivo.

## Login no Android
- Abrir o navegador (Custom Tabs) com `state`, `nonce` e PKCE `S256`.
- Retorno por deep link registado.
- Trocar o código no endpoint de token, validar `state` e o ID token.
- Guardar tokens no Android Keystore. Nunca registar tokens em logs.
- Tratar consentimento recusado, limite de uso atingido e sessão revogada, mostrando uma mensagem clara e voltando ao modo manual.

## Ferramentas expostas à IA (esboço)

| Ferramenta | Argumentos principais |
|---|---|
| `create_primitive` | `type` (cube, sphere, cylinder, plane, cone, torus), `size`, `position` |
| `transform` | `object_id`, `translate`, `rotate`, `scale` |
| `select` | `object_id`, `faces`/`edges`/`verts` |
| `extrude_faces` | `object_id`, `faces`, `distance` |
| `bevel` | `object_id`, `edges`, `width`, `segments` |
| `boolean` | `a`, `b`, `op` (union, difference, intersect) |
| `subdivide` | `object_id`, `levels` |
| `mirror` | `object_id`, `axis` |
| `set_material` | `object_id`, `color`, `roughness`, `metallic` |
| `delete` | `object_id` |
| `undo` / `redo` | - |

## Segurança
- Validar todos os argumentos e limitar contagem de polígonos e de chamadas por pedido.
- A IA só opera sobre objetos que existem na cena.
- Conteúdo de ficheiros importados é dado, nunca instruções para a IA.
