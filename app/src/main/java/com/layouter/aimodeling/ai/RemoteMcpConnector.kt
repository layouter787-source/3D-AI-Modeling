package com.layouter.aimodeling.ai

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.atomic.AtomicInteger
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Remote MCP bridge compatible with CodeAssist/Claude-style mcpServers JSON.
 * Only remote HTTP transports are supported on Android; stdio requires a local process host.
 * Configuration (including authorization headers) is encrypted with the app's Keystore SecretStore.
 */
object RemoteMcpConnector {
    private lateinit var appContext: Context
    private val requestIds = AtomicInteger(1)
    private val toolRoutes = LinkedHashMap<String, Route>()
    private val sessions = LinkedHashMap<String, String>()
    private val initializedEndpoints = HashSet<String>()
    private var cachedDefinitions = JSONArray()
    private var lastRefreshMs = 0L

    private data class Server(val name: String, val endpoint: String, val headers: Map<String, String>)
    private data class Route(val server: Server, val originalName: String, val schema: JSONObject)

    fun initialize(context: Context) {
        appContext = context.applicationContext
    }

    fun saveConfiguration(json: String) {
        require(::appContext.isInitialized) { "MCP connector has not been initialized" }
        val parsed = parseServers(json)
        require(parsed.isNotEmpty()) { "No valid remote MCP servers found in mcpServers" }
        SecretStore(appContext).put(CONFIG_KEY, json)
        synchronized(toolRoutes) {
            toolRoutes.clear()
            sessions.clear()
            initializedEndpoints.clear()
            cachedDefinitions = JSONArray()
            lastRefreshMs = 0L
        }
    }

    fun configuration(): String =
        if (::appContext.isInitialized) SecretStore(appContext).get(CONFIG_KEY).orEmpty() else ""

    fun clearConfiguration() {
        if (::appContext.isInitialized) SecretStore(appContext).remove(CONFIG_KEY)
        synchronized(toolRoutes) {
            toolRoutes.clear()
            cachedDefinitions = JSONArray()
            lastRefreshMs = 0L
        }
    }

    /** Tool declarations are cached briefly; MCP tools/list is refreshed after configuration changes. */
    suspend fun toolDefinitions(): JSONArray = withContext(Dispatchers.IO) {
        synchronized(toolRoutes) {
            if (lastRefreshMs > 0L && System.currentTimeMillis() - lastRefreshMs < CACHE_MS) {
                return@withContext JSONArray(cachedDefinitions.toString())
            }
        }
        val merged = JSONArray()
        val routes = LinkedHashMap<String, Route>()
        parseServers(configuration()).forEach { server ->
            runCatching {
                initializeSession(server)
                val response = rpc(server, "tools/list", JSONObject())
                val tools = response.optJSONArray("tools") ?: JSONArray()
                for (index in 0 until tools.length()) {
                    val tool = tools.optJSONObject(index) ?: continue
                    val original = tool.optString("name").trim()
                    if (original.isEmpty()) continue
                    val exposed = "mcp__" + safeName(server.name) + "__" + safeName(original)
                    val input = tool.optJSONObject("inputSchema") ?: JSONObject()
                        .put("type", "object").put("properties", JSONObject())
                    val schema = JSONObject()
                        .put("name", exposed)
                        .put("description", "[MCP ${server.name}] " + tool.optString("description", original))
                        .put("parameters", input)
                    merged.put(schema)
                    routes[exposed] = Route(server, original, input)
                }
            }
        }
        synchronized(toolRoutes) {
            toolRoutes.clear()
            toolRoutes.putAll(routes)
            cachedDefinitions = JSONArray(merged.toString())
            lastRefreshMs = System.currentTimeMillis()
        }
        merged
    }

    suspend fun execute(exposedName: String, arguments: JSONObject): ToolResult? = withContext(Dispatchers.IO) {
        val route = synchronized(toolRoutes) { toolRoutes[exposedName] } ?: return@withContext null
        initializeSession(route.server)
        val response = rpc(route.server, "tools/call", JSONObject()
            .put("name", route.originalName)
            .put("arguments", JSONObject(arguments.toString())))
        val content = response.optJSONArray("content") ?: JSONArray()
        val parts = ArrayList<String>()
        for (i in 0 until content.length()) {
            val part = content.optJSONObject(i) ?: continue
            if (part.optString("type") == "text") parts.add(part.optString("text"))
            else if (part.has("data")) parts.add("[MCP content: " + part.optString("mimeType") + "]")
        }
        if (response.optBoolean("isError")) ToolResult(false, "MCP tool returned error: " + parts.joinToString("\n"))
        else ToolResult(true, if (parts.isNotEmpty()) parts.joinToString("\n") else response.toString())
    }

    private fun parseServers(raw: String): List<Server> {
        if (raw.isBlank()) return emptyList()
        val root = JSONObject(raw)
        val servers = root.optJSONObject("mcpServers") ?: root.optJSONObject("servers") ?: return emptyList()
        val result = ArrayList<Server>()
        val keys = servers.keys()
        while (keys.hasNext()) {
            val name = keys.next()
            val entry = servers.optJSONObject(name) ?: continue
            if (entry.optBoolean("disabled")) continue
            val type = entry.optString("type", "http").lowercase()
            if (type == "stdio") continue // Android has no stdio child-process transport.
            val endpoint = entry.optString("httpUrl").ifBlank { entry.optString("url") }.trim()
            if (endpoint.isBlank()) continue
            val url = URL(endpoint)
            require(url.protocol == "https" || (url.protocol == "http" && url.host in setOf("127.0.0.1", "localhost", "::1"))) {
                "MCP remote endpoint must use HTTPS (HTTP is allowed only for localhost development)"
            }
            val headerObject = entry.optJSONObject("headers") ?: JSONObject()
            val headers = LinkedHashMap<String, String>()
            val headerKeys = headerObject.keys()
            while (headerKeys.hasNext()) {
                val key = headerKeys.next()
                val value = headerObject.optString(key)
                if (key.isNotBlank() && value.isNotBlank()) headers[key] = value
            }
            result.add(Server(name, endpoint, headers))
        }
        return result
    }

    private fun initializeSession(server: Server) {
        synchronized(initializedEndpoints) {
            if (server.endpoint in initializedEndpoints) return
        }
        val init = JSONObject()
            .put("protocolVersion", "2025-03-26")
            .put("capabilities", JSONObject())
            .put("clientInfo", JSONObject().put("name", "Manus-3D-AI-Android").put("version", "0.1"))
        rpc(server, "initialize", init)
        synchronized(initializedEndpoints) { initializedEndpoints.add(server.endpoint) }
        notifyInitialized(server)
    }

    private fun notifyInitialized(server: Server) {
        val connection = open(server)
        try {
            connection.requestMethod = "POST"
            connection.setRequestProperty("Content-Type", "application/json")
            connection.setRequestProperty("Accept", "application/json, text/event-stream")
            connection.setRequestProperty("MCP-Protocol-Version", "2025-03-26")
            connection.doOutput = true
            connection.outputStream.use {
                it.write(JSONObject().put("jsonrpc", "2.0")
                    .put("method", "notifications/initialized").toString().toByteArray(Charsets.UTF_8))
            }
            connection.inputStream?.close()
        } catch (_: Exception) {
            // Some compatible servers close notification requests with HTTP 202 and no body.
        } finally {
            connection.disconnect()
        }
    }

    private fun rpc(server: Server, method: String, params: JSONObject): JSONObject {
        val connection = open(server)
        try {
            connection.requestMethod = "POST"
            connection.setRequestProperty("Content-Type", "application/json")
            connection.setRequestProperty("Accept", "application/json, text/event-stream")
            connection.setRequestProperty("MCP-Protocol-Version", "2025-03-26")
            connection.doOutput = true
            val id = requestIds.getAndIncrement()
            val payload = JSONObject().put("jsonrpc", "2.0").put("id", id)
                .put("method", method).put("params", params)
            connection.outputStream.use { it.write(payload.toString().toByteArray(Charsets.UTF_8)) }
            val status = connection.responseCode
            val stream = if (status in 200..299) connection.inputStream else connection.errorStream
            val body = stream?.bufferedReader(Charsets.UTF_8)?.use { reader ->
                val raw = reader.readText()
                if (raw.contains("data:")) {
                    raw.lineSequence().filter { it.trimStart().startsWith("data:") }
                        .map { it.trimStart().removePrefix("data:").trim() }.lastOrNull().orEmpty()
                } else raw
            }.orEmpty()
            if (status !in 200..299) error("MCP HTTP $status: " + body.take(700))
            val sessionId = connection.getHeaderField("Mcp-Session-Id")
            if (!sessionId.isNullOrBlank()) synchronized(sessions) { sessions[server.endpoint] = sessionId }
            val response = JSONObject(body)
            if (response.has("error")) error("MCP ${response.optJSONObject("error")}")
            return response.optJSONObject("result") ?: error("MCP response missing result for $method")
        } finally {
            connection.disconnect()
        }
    }

    private fun open(server: Server): HttpURLConnection =
        (URL(server.endpoint).openConnection() as HttpURLConnection).apply {
            connectTimeout = 12_000
            readTimeout = 35_000
            instanceFollowRedirects = false
            server.headers.forEach { (key, value) -> setRequestProperty(key, value) }
            synchronized(sessions) { sessions[server.endpoint] }?.let { setRequestProperty("Mcp-Session-Id", it) }
        }

    private fun safeName(value: String): String =
        value.lowercase().replace(Regex("[^a-z0-9_]"), "_").take(28).ifBlank { "remote" }

    private const val CONFIG_KEY = "remote_mcp_configuration"
    private const val CACHE_MS = 45_000L
}
