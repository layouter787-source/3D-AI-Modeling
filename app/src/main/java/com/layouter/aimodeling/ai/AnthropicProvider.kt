package com.layouter.aimodeling.ai

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

/** Claude (Anthropic) com a chave de API do próprio utilizador (Messages API com ferramentas). */
class AnthropicProvider(
    private val apiKey: String,
    private val model: String,
    private val baseUrl: String = "https://api.anthropic.com/v1"
) : AiProvider {

    override val displayName = "Anthropic (chave de API)"

    override suspend fun send(system: String, messages: List<AiMessage>, tools: JSONArray): AiTurn =
        withContext(Dispatchers.IO) {
            val msgs = JSONArray()

            // Resultados de ferramentas seguidos vão todos na mesma mensagem de utilizador.
            var pendingResults: JSONArray? = null
            fun flushResults() {
                val results = pendingResults
                if (results != null) {
                    msgs.put(JSONObject().put("role", "user").put("content", results))
                    pendingResults = null
                }
            }

            for (m in messages) {
                when (m) {
                    is AiMessage.ToolOutput -> {
                        val results = pendingResults ?: JSONArray().also { pendingResults = it }
                        results.put(
                            JSONObject()
                                .put("type", "tool_result")
                                .put("tool_use_id", m.callId)
                                .put("content", m.result.toJson().toString())
                                .put("is_error", !m.result.ok)
                        )
                    }
                    is AiMessage.User -> {
                        flushResults()
                        msgs.put(JSONObject().put("role", "user").put("content", m.text))
                    }
                    is AiMessage.Assistant -> {
                        flushResults()
                        val blocks = JSONArray()
                        if (m.text.isNotEmpty()) {
                            blocks.put(JSONObject().put("type", "text").put("text", m.text))
                        }
                        for (call in m.toolCalls) {
                            blocks.put(
                                JSONObject()
                                    .put("type", "tool_use")
                                    .put("id", call.id)
                                    .put("name", call.name)
                                    .put("input", call.arguments)
                            )
                        }
                        if (blocks.length() > 0) {
                            msgs.put(JSONObject().put("role", "assistant").put("content", blocks))
                        }
                    }
                }
            }
            flushResults()

            val toolDefs = JSONArray()
            for (i in 0 until tools.length()) {
                val t = tools.getJSONObject(i)
                toolDefs.put(
                    JSONObject()
                        .put("name", t.getString("name"))
                        .put("description", t.getString("description"))
                        .put("input_schema", t.getJSONObject("parameters"))
                )
            }

            val body = JSONObject()
                .put("model", model)
                .put("max_tokens", 2048)
                .put("system", system)
                .put("messages", msgs)
                .put("tools", toolDefs)

            val response = HttpJson.post(
                url = "$baseUrl/messages",
                headers = mapOf("x-api-key" to apiKey, "anthropic-version" to "2023-06-01"),
                body = body
            )

            val text = StringBuilder()
            val calls = ArrayList<ToolCall>()
            val content = response.getJSONArray("content")
            for (i in 0 until content.length()) {
                val block = content.getJSONObject(i)
                when (block.optString("type")) {
                    "text" -> text.append(block.optString("text"))
                    "tool_use" -> calls.add(
                        ToolCall(
                            block.getString("id"),
                            block.getString("name"),
                            block.optJSONObject("input") ?: JSONObject()
                        )
                    )
                }
            }
            AiTurn(text.toString(), calls)
        }
}
