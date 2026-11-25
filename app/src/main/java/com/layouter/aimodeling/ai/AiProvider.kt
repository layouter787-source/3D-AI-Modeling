package com.layouter.aimodeling.ai

import org.json.JSONArray
import org.json.JSONObject

/** Chamada de ferramenta pedida pela IA. */
data class ToolCall(val id: String, val name: String, val arguments: JSONObject)

/** Resultado devolvido à IA depois de executar uma ferramenta. */
data class ToolResult(val ok: Boolean, val message: String) {
    fun toJson(): JSONObject = JSONObject().put("ok", ok).put("message", message)
}

/** Histórico da conversa, em formato neutro. Cada provedor converte para o seu formato. */
sealed class AiMessage {
    data class User(val text: String) : AiMessage()
    data class Assistant(val text: String, val toolCalls: List<ToolCall>) : AiMessage()
    data class ToolOutput(val callId: String, val name: String, val result: ToolResult) : AiMessage()
}

/** Uma resposta da IA: texto para mostrar e/ou ferramentas a executar. */
data class AiTurn(val text: String, val toolCalls: List<ToolCall>)

class AiException(message: String) : Exception(message)

/**
 * Provedor de IA (chave de API OpenAI/Anthropic, login ChatGPT, modelo local).
 * Converte as ferramentas neutras de [ToolDispatcher.toolDefinitions] para o formato do seu serviço.
 */
interface AiProvider {
    val displayName: String
    suspend fun send(system: String, messages: List<AiMessage>, tools: JSONArray): AiTurn
}
