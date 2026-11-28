package com.layouter.aimodeling.ai

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

/**
 * Qualquer serviço compatível com a API Chat Completions da OpenAI, com chamadas de função:
 * OpenAI, Google Gemini (endpoint de compatibilidade) e NVIDIA NIM. Muda só a base URL.
 */
class OpenAiProvider(
    private val apiKey: String,
    private val model: String,
    private val baseUrl: String = "https://api.openai.com/v1",
    override val displayName: String = "OpenAI",
    // O Gemini não aceita `parameters` vazio em ferramentas sem argumentos.
    private val omitEmptyParameters: Boolean = false
) : AiProvider {

    override suspend fun send(system: String, messages: List<AiMessage>, tools: JSONArray): AiTurn =
        sendInternal(system, messages, tools, null)

    override suspend fun sendStreaming(system: String, messages: List<AiMessage>, tools: JSONArray, onTextDelta: (String) -> Unit): AiTurn =
        sendInternal(system, messages, tools, onTextDelta)

    private suspend fun sendInternal(system: String, messages: List<AiMessage>, tools: JSONArray, onTextDelta: ((String) -> Unit)?): AiTurn =
        withContext(Dispatchers.IO) {
            val msgs = JSONArray()
            msgs.put(JSONObject().put("role", "system").put("content", system))
            for (m in messages) {
                when (m) {
                    is AiMessage.User -> {
                        if (m.imageBase64.isNullOrBlank()) {
                            msgs.put(JSONObject().put("role", "user").put("content", m.text))
                        } else {
                            val parts = JSONArray()
                            parts.put(JSONObject().put("type", "text").put("text", m.text.ifBlank { "Analyze this attached reference image and follow my instruction." }))
                            parts.put(JSONObject().put("type", "image_url").put("image_url",
                                JSONObject().put("url", "data:${m.imageMimeType};base64,${m.imageBase64}")))
                            msgs.put(JSONObject().put("role", "user").put("content", parts))
                        }
                    }
                    is AiMessage.Assistant -> {
                        val o = JSONObject().put("role", "assistant").put("content", m.text)
                        if (m.toolCalls.isNotEmpty()) {
                            val calls = JSONArray()
                            for (call in m.toolCalls) {
                                val entry = JSONObject()
                                    .put("id", call.id)
                                    .put("type", "function")
                                    .put(
                                        "function",
                                        JSONObject()
                                            .put("name", call.name)
                                            .put("arguments", call.arguments.toString())
                                    )
                                // Ex.: thought_signature do Gemini 3, que tem de ser devolvida.
                                call.extra?.let { entry.put("extra_content", it) }
                                calls.put(entry)
                            }
                            o.put("tool_calls", calls)
                        }
                        msgs.put(o)
                    }
                    is AiMessage.ToolOutput ->
                        msgs.put(
                            JSONObject()
                                .put("role", "tool")
                                .put("tool_call_id", m.callId)
                                .put("content", m.result.toJson().toString())
                        )
                }
            }

            val toolDefs = JSONArray()
            for (i in 0 until tools.length()) {
                val definition = JSONObject(tools.getJSONObject(i).toString())
                if (omitEmptyParameters) {
                    val properties = definition.optJSONObject("parameters")?.optJSONObject("properties")
                    if (properties == null || properties.length() == 0) definition.remove("parameters")
                }
                toolDefs.put(JSONObject().put("type", "function").put("function", definition))
            }

            val body = JSONObject()
                .put("model", model)
                .put("messages", msgs)
                .put("tools", toolDefs)

            if (onTextDelta != null) {
                body.put("stream", true)
                val text = StringBuilder()
                data class ToolPart(var id: String = "", var name: String = "", val args: StringBuilder = StringBuilder())
                val toolParts = sortedMapOf<Int, ToolPart>()
                var streamCompleted = false
                var finishReasonSeen = false
                try {
                    HttpJson.postSse("$baseUrl/chat/completions", mapOf("Authorization" to "Bearer $apiKey"), body) { raw ->
                        if (raw == "[DONE]") { streamCompleted = true; return@postSse }
                        val event = runCatching { JSONObject(raw) }.getOrNull() ?: return@postSse
                        val choice = event.optJSONArray("choices")?.optJSONObject(0) ?: return@postSse
                        if (!choice.isNull("finish_reason")) finishReasonSeen = true
                        val delta = choice.optJSONObject("delta") ?: return@postSse
                        val token = delta.optString("content", "")
                        if (token.isNotEmpty()) { text.append(token); onTextDelta(token) }
                        val chunks = delta.optJSONArray("tool_calls") ?: return@postSse
                        for (i in 0 until chunks.length()) {
                            val part = chunks.optJSONObject(i) ?: continue
                            val target = toolParts.getOrPut(part.optInt("index", i)) { ToolPart() }
                            target.id += part.optString("id", "")
                            val fn = part.optJSONObject("function")
                            if (fn != null) {
                                target.name += fn.optString("name", "")
                                target.args.append(fn.optString("arguments", ""))
                            }
                        }
                    }
                    if (!streamCompleted && !finishReasonSeen) throw AiException("Stream terminou sem evento final")
                    val calls = toolParts.values.mapIndexed { index, part ->
                        if (part.name.isBlank()) throw AiException("Tool call recebido sem nome")
                        val args = try { JSONObject(part.args.toString().ifBlank { "{}" }) }
                            catch (e: Exception) { throw AiException("JSON inválido nos argumentos da ferramenta", cause = e) }
                        ToolCall(part.id.ifBlank { "stream_call_$index" }, part.name, args)
                    }
                    return@withContext AiTurn(text.toString(), calls)
                } catch (error: AiException) {
                    if (error.httpCode !in setOf(400, 404, 405, 406, 415, 422, 501) || text.isNotEmpty()) throw error
                    body.remove("stream")
                }
            }
            val response = HttpJson.post(
                url = "$baseUrl/chat/completions",
                headers = mapOf("Authorization" to "Bearer $apiKey"),
                body = body
            )

            val message = response.getJSONArray("choices").getJSONObject(0).getJSONObject("message")
            val text = if (message.isNull("content")) "" else message.optString("content", "")
            val calls = ArrayList<ToolCall>()
            val toolCalls = message.optJSONArray("tool_calls")
            if (toolCalls != null) {
                for (i in 0 until toolCalls.length()) {
                    val c = toolCalls.getJSONObject(i)
                    val fn = c.getJSONObject("function")
                    val args = try {
                        JSONObject(fn.optString("arguments", "{}").ifBlank { "{}" })
                    } catch (e: Exception) {
                        JSONObject()
                    }
                    val id = c.optString("id").ifBlank { "call_$i" }
                    calls.add(ToolCall(id, fn.getString("name"), args, c.optJSONObject("extra_content")))
                }
            }
            AiTurn(text, calls)
        }
}
