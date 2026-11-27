package com.layouter.aimodeling.ai

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

/**
 * Definições das ferramentas externas. A internet e os servidores MCP ficam desligados até o
 * utilizador os ligar. Os tokens dos servidores MCP ficam cifrados no [SecretStore].
 */
class ToolSettings(private val context: Context) {

    private val prefs = context.getSharedPreferences("tool_settings", Context.MODE_PRIVATE)
    private val secrets = SecretStore(context)
    private val pluginStore = PluginStore(context)

    var webEnabled: Boolean
        get() = prefs.getBoolean("web", false)
        set(value) {
            prefs.edit().putBoolean("web", value).apply()
        }

    /** Rondas máximas por pedido; 0 = sem limite (há sempre o botão Parar). */
    var maxRounds: Int
        get() = prefs.getInt("max_rounds", 300)
        set(value) {
            prefs.edit().putInt("max_rounds", value.coerceAtLeast(0)).apply()
        }

    fun mcpServers(): List<McpServerConfig> {
        val arr = try {
            JSONArray(prefs.getString("mcp", "[]"))
        } catch (e: Exception) {
            JSONArray()
        }
        return (0 until arr.length()).map { i ->
            val o = arr.getJSONObject(i)
            val id = o.getString("id")
            McpServerConfig(id, o.getString("name"), o.getString("url"), secrets.get("mcp_token_$id"))
        }
    }

    fun addMcp(name: String, url: String, token: String) {
        require(name.isNotBlank()) { "give the server a name" }
        require(url.startsWith("https://") || url.startsWith("http://127.0.0.1") || url.startsWith("http://localhost")) {
            "use an https URL (http only for localhost)"
        }
        val arr = try {
            JSONArray(prefs.getString("mcp", "[]"))
        } catch (e: Exception) {
            JSONArray()
        }
        val id = UUID.randomUUID().toString().take(8)
        arr.put(JSONObject().put("id", id).put("name", name.trim()).put("url", url.trim()))
        prefs.edit().putString("mcp", arr.toString()).apply()
        if (token.isNotBlank()) secrets.put("mcp_token_$id", token.trim())
    }

    fun removeMcp(id: String) {
        val arr = JSONArray(prefs.getString("mcp", "[]"))
        val out = JSONArray()
        for (i in 0 until arr.length()) {
            if (arr.getJSONObject(i).getString("id") != id) out.put(arr.getJSONObject(i))
        }
        prefs.edit().putString("mcp", out.toString()).apply()
        secrets.remove("mcp_token_$id")
    }

    fun plugins(): List<Plugin> = pluginStore.loadAll()
    fun installPlugin(plugin: Plugin) = pluginStore.save(plugin)
    fun removePlugin(id: String) = pluginStore.remove(id)

    /** Passa as definições atuais para o [ToolHub]. */
    fun apply() {
        ToolHub.webEnabled = webEnabled
        ToolHub.mcpClients = mcpServers().map { McpClient(it) }
        ToolHub.plugins = plugins()
    }
}
