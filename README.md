# 3D-AI-Modeling

Aplicativo Android de modelagem 3D com assistente de IA integrado. O utilizador entra com a própria conta (ChatGPT) ou usa a própria chave de API, e a IA ajuda a modelar chamando as ferramentas do motor 3D.

## Estado
Em planeamento. Ver [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) e [docs/AI_PROVIDERS.md](docs/AI_PROVIDERS.md).

## Base técnica
- Interface: Kotlin + Jetpack Compose
- Núcleo 3D: C++17 (NDK), malha half-edge, comandos com undo/redo
- Render: OpenGL ES 3.0 (Vulkan numa fase posterior)
- IA: camada de provedores (login ChatGPT, chave de API, modelo local)

## Licença
GPL (adicionar o ficheiro LICENSE pelo GitHub: Add file > Create new file > LICENSE > escolher GPL-3.0).
