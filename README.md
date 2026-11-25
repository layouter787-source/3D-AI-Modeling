# 3D-AI-Modeling

Aplicativo Android de modelagem 3D com assistente de IA integrado. O utilizador cola a própria chave de API (OpenAI, Claude, Google Gemini ou NVIDIA NIM) e a IA ajuda a modelar chamando as ferramentas do motor 3D. Não há login com conta: só chave de API, com a base URL de cada serviço já predefinida.

## Estado
Em desenvolvimento inicial (ainda não testado em dispositivo).

Já existe:
- Viewport OpenGL ES 3.0 com grelha e eixos, câmera orbital por toque (1 dedo roda, 2 dedos deslocam, pinça aproxima).
- Cena com vários objetos: cubo, esfera, cilindro, cone, torus e plano.
- Seleção por toque, mover, rodar, escalar, cor e apagar, com desfazer/refazer.
- Painel de chat com IA (botão "IA"): OpenAI, Claude, Gemini e NVIDIA NIM por chave de API, ciclo de conversa com chamadas de ferramentas e chave guardada cifrada.

Falta: edição de malha (extrudir, bevel, boolean), importar/exportar, testes.

Documentação: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) e [docs/AI_PROVIDERS.md](docs/AI_PROVIDERS.md).

## Base técnica
- Interface: Kotlin + Jetpack Compose
- Núcleo 3D: C++17 (NDK): malha, cena, `Editor` com histórico de comandos
- Render: OpenGL ES 3.0 (Vulkan numa fase posterior)
- IA: provedores por chave de API (OpenAI, Claude, Gemini, NVIDIA NIM)

## Compilar
Abrir a pasta no Android Studio (ele gera o Gradle wrapper e instala o NDK e o CMake pedidos) e executar o módulo `app`. Também funciona com `gradle wrapper` seguido de `./gradlew assembleDebug`.

Requisitos: JDK 17, Android SDK 35, NDK e CMake 3.22.1 (instalados pelo SDK Manager).

## Licença
Apache-2.0. Ver [LICENSE](LICENSE).
