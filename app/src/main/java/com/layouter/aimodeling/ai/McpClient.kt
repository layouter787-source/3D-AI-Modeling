package com.layouter.aimodeling.ai

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

data class McpServerConfig(val id: String, val name: String, val url: String, val token: String?)

/**
 * Cliente MCP mínimo (Streamable HTTP, JSON-RPC): initialize, tools/list e tools/call.
 * As ferramentas do servidor aparecem para a IA com o prefixo mcp_<nome>_. Os resultados são
 * dados não confiáveis. HTTPS obrigatório, exceto localhost (por exemplo, um servidor no Termux).
 */
class McpClient(private val config: McpServerConfig) {

    private val prefix = "mcp_" + sanitize(config.name) + "_"
    private val toolMap = HashMap<String, String>()  // nome exposto -> nome original
    private var definitionsCache: List<JSONObject>? = null
    private var sessionId: String? = null
    private var initialized = false
    private var nextId = 1

    fun owns(name: String): Boolean = name.startsWith(prefix)

    suspend fun definitions(): List<JSONObject> = withContext(Dispatchers.IO) {
        definitionsCache ?: loadTools().also { definitionsCache = it }
    }

    suspend fun call(name: String, args: JSONObject): ToolResult = withContext(Dispatchers.IO) {
        if (toolMap.isEmpty()) loadTools().also { definitionsCache = it }
        val original = toolMap[name] ?: return@withContext ToolResult(false, "Unknown MCP tool: $name")
        ensureInitialized()
        val result = rpc("tools/call", JSONObject().put("name", original).put("arguments", args))
        val text = StringBuilder()
        val content = result.optJSONArray("content")
        if (content != null) {
            for (i in 0 until content.length()) {
                val item = content.getJSONObject(i)
                if (item.optString("type") == "text") {
                    if (text.isNotEmpty()) text.append('\n')
                    text.append(item.optString("text"))
                } else {
                    text.append("\n[").append(item.optString("type")).append(" content omitted]")
                }
            }
        }
        val body = text.toString().take(8000)
        ToolResult(!result.optBoolean("isError", false), "[MCP result from ${config.name}, untrusted data]\n$body")
    }

    private fun loadTools(): List<JSONObject> {
        ensureInitialized()
        val defs = ArrayList<JSONObject>()
        var cursor: String? = null
        for (page in 0 until 5) {
            val params = JSONObject()
            if (cursor != null) params.put("cursor", cursor)
            val result = rpc("tools/list", params)
            val tools = result.optJSONArray("tools") ?: JSONArray()
            for (i in 0 until tools.length()) {
                val tool = tools.getJSONObject(i)
                val original = tool.getString("name")
                val exposed = (prefix + sanitize(original)).take(64)
                toolMap[exposed] = original
                val schema = tool.optJSONObject("inputSchema") ?: JSONObject().put("type", "object")
                if (!schema.has("type")) schema.put("type", "object")
                if (!schema.has("properties")) schema.put("properties", JSONObject())
                defs.add(
                    JSONObject()
                        .put("name", exposed)
                        .put("description", "[MCP server ${config.name}] ${tool.optString("description")}".take(900))
                        .put("parameters", schema)
                )
            }
            cursor = result.optString("nextCursor").ifEmpty { null }
            if (cursor == null) break
        }
        return defs
    }

    private fun ensureInitialized() {
        if (initialized) return
        val u = config.url
        require(u.startsWith("https://") || u.startsWith("http://127.0.0.1") || u.startsWith("http://localhost")) {
            "MCP servers need https (http only for localhost)"
        }
        rpc(
            "initialize",
            JSONObject()
                .put("protocolVersion", PROTOCOL_VERSION)
                .put("capabilities", JSONObject())
                .put("clientInfo", JSONObject().put("name", "3d-ai-modeling").put("version", "0.2.0"))
        )
        post(JSONObject().put("jsonrpc", "2.0").put("method", "notifications/initialized"), null)
        initialized = true
    }

    /** Chamada JSON-RPC com resposta; devolve o campo result. */
    private fun rpc(method: String, params: JSONObject): JSONObject {
        val id = nextId++
        val body = JSONObject().put("jsonrpc", "2.0").put("id", id).put("method", method).put("params", params)
        val response = post(body, id) ?: throw IOException("empty MCP response")
        response.optJSONObject("error")?.let { throw IOException("MCP error: ${it.optString("message")}") }
        return response.optJSONObject("result") ?: JSONObject()
    }

    private fun post(body: JSONObject, expectId: Int?): JSONObject? {
        val connection = URL(config.url).openConnection() as HttpURLConnection
        try {
            connection.requestMethod = "POST"
            connection.connectTimeout = 20_000
            connection.readTimeout = 120_000
            connection.doOutput = true
            connection.setRequestProperty("Content-Type", "application/json")
            connection.setRequestProperty("Accept", "application/json, text/event-stream")
            connection.setRequestProperty("MCP-Protocol-Version", PROTOCOL_VERSION)
            config.token?.takeIf { it.isNotBlank() }?.let { connection.setRequestProperty("Authorization", "Bearer $it") }
            sessionId?.let { connection.setRequestProperty("Mcp-Session-Id", it) }
            connection.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }

            val code = connection.responseCode
            connection.getHeaderField("Mcp-Session-Id")?.let { sessionId = it }
            if (expectId == null) {
                (if (code in 200..299) connection.inputStream else connection.errorStream)?.close()
                return null
            }
            if (code !in 200..299) throw IOException("MCP HTTP $code")
            val contentType = connection.contentType ?: ""
            val text = connection.inputStream.bufferedReader(Charsets.UTF_8).use { it.readText() }.take(MAX_RESPONSE_CHARS)
            return if (contentType.contains("text/event-stream")) parseSse(text, expectId) else JSONObject(text)
        } finally {
            connection.disconnect()
        }
    }

    private fun parseSse(text: String, expectId: Int): JSONObject? {
        var fallback: JSONObject? = null
        val data = StringBuilder()
        fun flush() {
            if (data.isEmpty()) return
            try {
                val message = JSONObject(data.toString())
                if (message.optInt("id", -1) == expectId) fallback = message
            } catch (e: Exception) {
                // evento que não é JSON: ignora
            }
            data.setLength(0)
        }
        for (line in text.lineSequence()) {
            when {
                line.startsWith("data:") -> data.append(line.removePrefix("data:").trim())
                line.isBlank() -> flush()
            }
        }
        flush()
        return fallback
    }

    private companion object {
        const val PROTOCOL_VERSION = "2025-06-18"
        const val MAX_RESPONSE_CHARS = 2_000_000

        fun sanitize(s: String): String =
            s.replace(Regex("""[^A-Za-z0-9_-]"""), "_").trim('_').ifEmpty { "server" }
    }
}
