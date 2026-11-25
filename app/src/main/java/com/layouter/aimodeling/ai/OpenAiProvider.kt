package com.layouter.aimodeling.ai

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

/** OpenAI com a chave de API do próprio utilizador (Chat Completions com chamadas de função). */
class OpenAiProvider(
    private val apiKey: String,
    private val model: String,
    private val baseUrl: String = "https://api.openai.com/v1"
) : AiProvider {

    override val displayName = "OpenAI (chave de API)"

    override suspend fun send(system: String, messages: List<AiMessage>, tools: JSONArray): AiTurn =
        withContext(Dispatchers.IO) {
            val msgs = JSONArray()
            msgs.put(JSONObject().put("role", "system").put("content", system))
            for (m in messages) {
                when (m) {
                    is AiMessage.User ->
                        msgs.put(JSONObject().put("role", "user").put("content", m.text))
                    is AiMessage.Assistant -> {
                        val o = JSONObject().put("role", "assistant")
                        o.put("content", if (m.text.isEmpty()) JSONObject.NULL else m.text)
                        if (m.toolCalls.isNotEmpty()) {
                            val calls = JSONArray()
                            for (call in m.toolCalls) {
                                calls.put(
                                    JSONObject()
                                        .put("id", call.id)
                                        .put("type", "function")
                                        .put(
                                            "function",
                                            JSONObject()
                                                .put("name", call.name)
                                                .put("arguments", call.arguments.toString())
                                        )
                                )
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
                toolDefs.put(JSONObject().put("type", "function").put("function", tools.getJSONObject(i)))
            }

            val body = JSONObject()
                .put("model", model)
                .put("messages", msgs)
                .put("tools", toolDefs)

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
                    calls.add(ToolCall(c.getString("id"), fn.getString("name"), args))
                }
            }
            AiTurn(text, calls)
        }
}
