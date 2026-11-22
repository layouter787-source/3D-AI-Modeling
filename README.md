# 3D-AI-Modeling

Aplicativo Android de modelagem 3D com assistente de IA integrado. O utilizador entra com a própria conta (ChatGPT) ou usa a própria chave de API, e a IA ajuda a modelar chamando as ferramentas do motor 3D.

## Estado
Em desenvolvimento inicial. O projeto Android já abre um viewport OpenGL ES 3.0 com um cubo e câmera orbital por toque (1 dedo roda, 2 dedos deslocam, pinça aproxima), desenhado pelo núcleo em C++.

Documentação: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) e [docs/AI_PROVIDERS.md](docs/AI_PROVIDERS.md).

## Base técnica
- Interface: Kotlin + Jetpack Compose
- Núcleo 3D: C++17 (NDK), malha, comandos com undo/redo (em construção)
- Render: OpenGL ES 3.0 (Vulkan numa fase posterior)
- IA: camada de provedores (login ChatGPT, chave de API, modelo local)

## Compilar
Abrir a pasta no Android Studio (ele gera o Gradle wrapper e instala o NDK e o CMake pedidos) e executar o módulo `app`. Também funciona com `gradle wrapper` seguido de `./gradlew assembleDebug`.

Requisitos: JDK 17, Android SDK 35, NDK e CMake 3.22.1 (instalados pelo SDK Manager).

## Licença
Apache-2.0. Ver [LICENSE](LICENSE).
