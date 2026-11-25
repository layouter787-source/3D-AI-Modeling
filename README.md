# 3D-AI-Modeling

Aplicativo Android de modelagem 3D com assistente de IA integrado. A interface é Kotlin + Jetpack Compose; o núcleo de cena e geometria é C++17/NDK com renderização OpenGL ES 3.0.

## O que já existe

- Viewport 3D com grelha, eixos e câmera orbital por toque.
- Objetos básicos: cubo, esfera, cilindro, cone, toro, plano e pirâmide.
- Seleção, transformações, cor, remoção e histórico undo/redo.
- Guardar e abrir projetos editáveis `.3dproj` (JSON versionado, com validação e limite de 500 objetos).
- Exportar geometria transformada para OBJ com biblioteca MTL separada para manter as cores.
- Duplicar objetos pela interface e pelo assistente de IA.
- Assistente de IA que pode chamar ferramentas limitadas para modificar a cena.
- OpenAI, Claude/Anthropic, Google Gemini e NVIDIA NIM.
- Chaves cifradas no Android Keystore.
- Chat renovado com composer compacto, mensagens separadas, ícones vetoriais e configuração em painel próprio.
- Descoberta de modelos em tempo real a partir da API do provedor: não existem modelos predefinidos no seletor.

## Modelos por API

Abre o assistente → Configurações → escolhe o provedor → introduz a chave → **Consultar modelos** → escolhe um ID devolvido pelo serviço → Guardar.

A disponibilidade depende da chave, região, conta e permissões do provedor. O app mostra os modelos anunciados pela API; o próprio endpoint de conversa confirma se o modelo suporta as ferramentas usadas pelo editor.

## Interface e extensões

- Tema claro branco/preto e tema escuro preto/branco, com azul como cor de destaque; escolha guardada no dispositivo.
- Catálogo de capacidades nativas e projetos de extensão pesquisados, acessível à IA pela ferramenta get_modeling_extensions.
- Pesquisa na web e leitura de documentação pública por ferramentas HTTPS com validação de destinos, redirecionamentos e limites de tamanho.
- Extensões de terceiros são portadas e compiladas nativamente; addons Godot/Blender não são carregados diretamente no APK.
- [Pesquisa e plano de integração do ecossistema](docs/MODELING_EXTENSION_ECOSYSTEM.md).

## Base técnica

- Android: Kotlin, Jetpack Compose, Material 3
- Ícones: vetoriais Compose/Android
- Motor: C++17, Android NDK
- Renderização: OpenGL ES 3.0
- APIs de IA: HTTP JSON, chamadas de ferramentas

## Compilar

Abrir no Android Studio ou executar:

```bash
gradle wrapper
./gradlew assembleDebug
```

Requisitos: JDK 17, Android SDK 35, NDK e CMake 3.22.1.

O GitHub Actions compila um APK debug em cada push. Ver a aba Actions do repositório para descarregar o artefacto.

## Documentação

- [Arquitetura](docs/ARCHITECTURE.md)
- [Formato de projeto](docs/PROJECT_FORMAT.md)
- [Provedores e modelos](docs/AI_PROVIDERS.md)

## Licença

Apache-2.0.
