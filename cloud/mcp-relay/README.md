# Manus 3D — relay MCP próprio (Claude custom connector)
Este servidor expõe o nosso aplicativo por Streamable HTTP. O relay encaminha os pedidos MCP pelo WebSocket de saída do telefone; não instala ferramentas externas.

## Publicar
1. Node/npm e conta Cloudflare; na pasta, `npm install`.
2. `npx wrangler login` e `npm run deploy`.
3. Copia o hostname HTTPS gerado pelo deploy para as definições MCP Cloud do Android.
4. O app cria um segredo aleatório de 256 bits e apresenta `https://HOST/c/SEGREDO/mcp`.
5. Claude Web: Customize → Connectors → Add custom connector → inserir a URL.

A URL é uma credencial bearer-capability: manter privada e revogar ao regenerar. O dispositivo só inicia uma conexão WSS de saída; sem porta aberta no Android. Offline retorna 503; chamada demorada retorna 504. Para uso público, implementar autenticação do dispositivo, rate limit, política de retenção e logs sem conteúdo de tool calls. O hostname real só existe após deploy numa conta Cloudflare; este commit inclui a implementação e configuração, não finge ter criado uma infraestrutura/URL sem conta e publicação.
