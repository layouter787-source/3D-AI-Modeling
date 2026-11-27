package com.layouter.aimodeling.ai

import com.layouter.aimodeling.NativeBridge
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject

/** Resultado cru de uma ferramenta nativa. */
data class NativeResult(val ok: Boolean, val message: String, val data: JSONObject?)

/**
 * Junta todas as fontes de ferramentas que o agente pode usar:
 *  - nativas (motor 3D em C++): sempre ligadas;
 *  - internet (web_fetch, import_obj_url): só se o utilizador ligar;
 *  - servidores MCP que o utilizador adicionou;
 *  - plugins (macros em JSON que só chamam ferramentas nativas; não executam código).
 */
object ToolHub {

    private val nativeDefs: JSONArray by lazy { JSONArray(NativeBridge.toolDefinitions()) }

    private val nativeNames: Set<String> by lazy {
        (0 until nativeDefs.length()).map { nativeDefs.getJSONObject(it).getString("name") }.toSet()
    }

    @Volatile var webEnabled: Boolean = false
    @Volatile var mcpClients: List<McpClient> = emptyList()
    @Volatile var plugins: List<Plugin> = emptyList()

    fun isNativeTool(name: String): Boolean = name in nativeNames

    fun nativeToolCount(): Int = nativeNames.size

    suspend fun definitions(): JSONArray = withContext(Dispatchers.IO) {
        val out = JSONArray()
        for (i in 0 until nativeDefs.length()) out.put(nativeDefs.getJSONObject(i))
        if (webEnabled) {
            out.put(WebFetch.fetchDefinition())
            out.put(WebFetch.importDefinition())
        }
        for (client in mcpClients) {
            try {
                for (def in client.definitions()) out.put(def)
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                // servidor indisponível: as ferramentas dele não entram nesta ronda
            }
        }
        for (plugin in plugins) {
            for (def in plugin.definitions()) out.put(def)
        }
        out
    }

    suspend fun execute(call: ToolCall): ToolResult = withContext(Dispatchers.Default) {
        try {
            route(call)
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            ToolResult(false, "Tool error: ${e.message}")
        }
    }

    private suspend fun route(call: ToolCall): ToolResult {
        val name = call.name
        return when {
            name in nativeNames -> runNative(name, call.arguments).toToolResult()
            name == WebFetch.FETCH && webEnabled -> WebFetch.fetch(call.arguments.getString("url"))
            name == WebFetch.IMPORT_OBJ && webEnabled -> WebFetch.importObj(call.arguments).toToolResult()
            name.startsWith("mcp_") -> {
                val client = mcpClients.firstOrNull { it.owns(name) } ?: return ToolResult(false, "Unknown MCP tool: $name")
                client.call(name, call.arguments)
            }
            name.startsWith("plugin_") -> {
                val plugin = plugins.firstOrNull { it.owns(name) } ?: return ToolResult(false, "Unknown plugin tool: $name")
                plugin.run(name, call.arguments)
            }
            else -> ToolResult(false, "Unknown tool: $name")
        }
    }

    /** Chama uma ferramenta nativa. Bloqueante, mas rápida; o núcleo usa um mutex. */
    fun runNative(name: String, args: JSONObject): NativeResult {
        val raw = JSONObject(NativeBridge.callTool(name, args.toString()))
        return NativeResult(raw.optBoolean("ok"), raw.optString("message"), raw.optJSONObject("data"))
    }

    private fun NativeResult.toToolResult(): ToolResult {
        val text = buildString {
            append(message)
            data?.let {
                append(' ')
                append(it.toString())
            }
        }
        return ToolResult(ok, text.take(MAX_RESULT_CHARS))
    }

    private const val MAX_RESULT_CHARS = 7000
}
