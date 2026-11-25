# Arquitetura

## Módulos

| Módulo | Linguagem | Função |
|---|---|---|
| `app/` | Kotlin (Compose) | Interface, viewport, painel de chat da IA |
| `app/.../ai/` | Kotlin | Provedores de IA (chave de API), agente, despachante de ferramentas, armazenamento cifrado da chave |
| `app/src/main/cpp/core/` | C++17 (NDK) | Malha, primitivas, cena, `Editor` com undo/redo (futuro: extrusão, bevel, boolean, import/export) |
| `app/src/main/cpp/` (render) | C++ / OpenGL ES 3.0 | Desenho da cena, grelha, seleção. Vulkan numa fase posterior |
| `ads/` (planeado) | Kotlin | Anúncios, num módulo separado |

Quando o núcleo crescer, `core/` passa a ser uma biblioteca nativa própria.

## Fluxo da IA
1. O utilizador escreve um pedido ("faz uma mesa com 4 pernas").
2. O `AiAgent` envia o pedido, o resumo da cena e a lista de ferramentas ao provedor escolhido.
3. A IA devolve chamadas de ferramentas.
4. O `ToolDispatcher` valida os argumentos (tipos, limites, ids existentes).
5. Cada chamada vira um comando no `Editor` (undo/redo).
6. O viewport atualiza e o resultado volta para a IA continuar.

A IA nunca escreve a malha diretamente: só chama ferramentas do motor.

## Licença e anúncios
O projeto usa Apache-2.0, que não conflita com SDKs de anúncios proprietários. Mesmo assim, o código de anúncios fica isolado em `ads/`. Evitar dependências GPL/AGPL (por exemplo CGAL) e confirmar a licença de cada biblioteca antes de a incluir.
