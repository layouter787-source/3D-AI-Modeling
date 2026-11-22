# Arquitetura

## Módulos

| Módulo | Linguagem | Função |
|---|---|---|
| `app/` | Kotlin (Compose) | Interface, viewport, painel de chat da IA |
| `core/` | C++17 (NDK) | Malha half-edge, primitivas, extrusão, bevel, boolean, subdivisão, espelho, undo/redo, import/export OBJ e glTF |
| `render/` | C++ / OpenGL ES 3.0 | Desenho da cena, seleção, gizmos. Vulkan numa fase posterior |
| `ai/` | Kotlin | Provedores de IA, despachante de ferramentas, validação |
| `ads/` | Kotlin | Anúncios, num módulo separado (ver Licença) |

## Fluxo da IA
1. O utilizador escreve um pedido ("faz uma mesa com 4 pernas").
2. O provedor envia o pedido, o estado resumido da cena e a lista de ferramentas.
3. A IA devolve chamadas de ferramentas em JSON.
4. O despachante valida os argumentos (tipos, limites, IDs existentes).
5. Cada chamada vira um comando no `CommandStack` (undo/redo).
6. O viewport atualiza e o resultado volta para a IA continuar.

A IA nunca escreve a malha diretamente: só chama ferramentas do motor.

## Licença e anúncios
O projeto é GPL. SDKs de anúncios são proprietários e podem ser incompatíveis com a GPL, por isso ficam isolados em `ads/`. Decisão pendente: manter o módulo de anúncios com licença própria ou usar uma rede de anúncios compatível.

## Pendências
- Confirmar com a OpenAI se um app open source com anúncios continua elegível ao login com conta gratuita.
- Registar o cliente OAuth pelo fluxo oficial de apps open source.
