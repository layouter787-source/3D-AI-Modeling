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
        sendInternal(system, messages, tools, null)

    override suspend fun sendStreaming(system: String, messages: List<AiMessage>, tools: JSONArray, onTextDelta: (String) -> Unit): AiTurn =
        sendInternal(system, messages, tools, onTextDelta)

    private suspend fun sendInternal(system: String, messages: List<AiMessage>, tools: JSONArray, onTextDelta: ((String) -> Unit)?): AiTurn =
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
                        if (m.imageBase64.isNullOrBlank()) {
                            msgs.put(JSONObject().put("role", "user").put("content", m.text))
                        } else {
                            val blocks = JSONArray()
                            blocks.put(JSONObject().put("type", "text").put("text", m.text.ifBlank { "Analyze this reference image and follow my instruction." }))
                            blocks.put(JSONObject().put("type", "image").put("source",
                                JSONObject().put("type", "base64").put("media_type", m.imageMimeType).put("data", m.imageBase64)))
                            msgs.put(JSONObject().put("role", "user").put("content", blocks))
                        }
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

            if (onTextDelta != null) {
                body.put("stream", true)
                val text = StringBuilder()
                data class ToolPart(var id: String = "", var name: String = "", var input: JSONObject = JSONObject(), val json: StringBuilder = StringBuilder())
                val toolsByIndex = sortedMapOf<Int, ToolPart>()
                var messageStopped = false
                try {
                    HttpJson.postSse("$baseUrl/messages", mapOf("x-api-key" to apiKey, "anthropic-version" to "2023-06-01"), body) { raw ->
                        val event = runCatching { JSONObject(raw) }.getOrNull() ?: return@postSse
                        when (event.optString("type")) {
                            "message_stop" -> messageStopped = true
                            "content_block_start" -> {
                                val block = event.optJSONObject("content_block") ?: return@postSse
                                val index = event.optInt("index")
                                if (block.optString("type") == "tool_use") {
                                    toolsByIndex[index] = ToolPart(block.optString("id"), block.optString("name"), block.optJSONObject("input") ?: JSONObject())
                                } else if (block.optString("type") == "text") {
                                    val initial = block.optString("text", "")
                                    if (initial.isNotEmpty()) { text.append(initial); onTextDelta(initial) }
                                }
                            }
                            "content_block_delta" -> {
                                val delta = event.optJSONObject("delta") ?: return@postSse
                                when (delta.optString("type")) {
                                    "text_delta" -> {
                                        val token = delta.optString("text", "")
                                        if (token.isNotEmpty()) { text.append(token); onTextDelta(token) }
                                    }
                                    "input_json_delta" -> toolsByIndex[event.optInt("index")]?.json?.append(delta.optString("partial_json", ""))
                                }
                            }
                            "content_block_stop" -> {
                                val part = toolsByIndex[event.optInt("index")]
                                if (part != null && part.json.isNotEmpty()) part.input = runCatching { JSONObject(part.json.toString()) }.getOrElse { JSONObject() }
                            }
                        }
                    }
                    if (!messageStopped) throw AiException("Stream Anthropic terminou sem message_stop")
                    val calls = toolsByIndex.values.mapIndexed { index, part ->
                        if (part.name.isBlank()) throw AiException("Tool use recebido sem nome")
                        val parsedInput = if (part.json.isNotEmpty()) {
                            try { JSONObject(part.json.toString()) }
                            catch (e: Exception) { throw AiException("JSON inválido nos argumentos de tool use", cause = e) }
                        } else part.input
                        ToolCall(part.id.ifBlank { "anthropic_tool_$index" }, part.name, parsedInput)
                    }
                    return@withContext AiTurn(text.toString(), calls)
                } catch (error: AiException) {
                    if (error.httpCode !in setOf(400, 404, 405, 406, 415, 422, 501) || text.isNotEmpty()) throw error
                    body.remove("stream")
                }
            }
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
