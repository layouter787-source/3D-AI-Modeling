package com.layouter.aimodeling.ai

import org.json.JSONArray
import org.json.JSONObject

/** Chamada de ferramenta pedida pela IA. */
data class ToolCall(val id: String, val name: String, val arguments: JSONObject)

/** Resultado devolvido à IA depois de executar uma ferramenta. */
data class ToolResult(val ok: Boolean, val message: String) {
    fun toJson(): JSONObject = JSONObject().put("ok", ok).put("message", message)
}

data class AiMessage(val role: String, val content: String)

/** Uma resposta da IA: texto para mostrar e/ou ferramentas a executar. */
data class AiTurn(val text: String, val toolCalls: List<ToolCall>)

/**
 * Provedor de IA (login ChatGPT, chave de API OpenAI/Anthropic, modelo local).
 * Cada implementação converte as ferramentas neutras de [ToolDispatcher.toolDefinitions]
 * para o formato do seu serviço.
 */
interface AiProvider {
    val displayName: String
    suspend fun send(messages: List<AiMessage>, tools: JSONArray): AiTurn
}
