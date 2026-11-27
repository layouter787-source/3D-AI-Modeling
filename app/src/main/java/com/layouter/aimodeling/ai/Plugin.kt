package com.layouter.aimodeling.ai

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.net.URL
import kotlin.math.abs
import kotlin.math.ceil
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt
import kotlin.math.tan

class PluginTool(val name: String, val description: String, val parameters: JSONObject, val steps: JSONArray)

/**
 * Plugin = ficheiro JSON com ferramentas novas definidas como macros: uma sequência de chamadas
 * a ferramentas nativas do app. Não executa código, não acede à rede nem a MCP: só pode fazer
 * o que as ferramentas nativas já fazem (e tudo é reversível com desfazer).
 *
 * Nos argumentos dos passos: "$nome" é substituído pelo valor do parâmetro (qualquer tipo) e
 * "=expressão" é calculada (números, + - * / ^, parênteses, sin cos tan sqrt abs min max floor
 * ceil, pi, e nome[índice] para listas). Um passo pode ter "as": "nome" para guardar o object_id
 * criado e "repeat": {"var": "i", "from": 0, "to": "=n"} para repetir.
 */
class Plugin(
    val id: String,
    val name: String,
    val version: String,
    val author: String,
    val description: String,
    val tools: List<PluginTool>,
    val raw: String
) {
    fun owns(exposedName: String): Boolean = exposedName.startsWith("plugin_${id}_")

    fun definitions(): List<JSONObject> = tools.map { t ->
        JSONObject()
            .put("name", "plugin_${id}_${t.name}")
            .put("description", "[plugin ${name}] ${t.description}".take(900))
            .put("parameters", t.parameters)
    }

    suspend fun run(exposedName: String, args: JSONObject): ToolResult {
        val tool = tools.firstOrNull { "plugin_${id}_${it.name}" == exposedName }
            ?: return ToolResult(false, "Unknown plugin tool")
        val vars = HashMap<String, Any?>()
        val keys = args.keys()
        while (keys.hasNext()) {
            val k = keys.next()
            val v = args.get(k)
            vars[k] = if (v is Number) v.toDouble() else v
        }
        var executed = 0
        val created = ArrayList<String>()
        for (i in 0 until tool.steps.length()) {
            val step = tool.steps.getJSONObject(i)
            val repeat = step.optJSONObject("repeat")
            if (repeat == null) {
                val r = runStep(step, vars, i)
                executed++
                if (!r.ok) return ToolResult(false, "Step ${i + 1} failed: ${r.message}")
            } else {
                val variable = repeat.getString("var")
                val from = toNumber(resolve(repeat.opt("from") ?: 0, vars)).toInt()
                val to = toNumber(resolve(repeat.opt("to"), vars)).toInt()
                require(to - from <= MAX_REPEAT) { "repeat is too long" }
                for (k in from until to) {
                    vars[variable] = k.toDouble()
                    val r = runStep(step, vars, i)
                    executed++
                    if (!r.ok) return ToolResult(false, "Step ${i + 1} (repeat $k) failed: ${r.message}")
                    require(executed <= MAX_STEPS) { "too many steps" }
                }
            }
            require(executed <= MAX_STEPS) { "too many steps" }
            step.optString("as").takeIf { it.isNotEmpty() }?.let { variableName ->
                vars[variableName]?.let { created.add("$variableName=${(it as? Double)?.toInt() ?: it}") }
            }
        }
        return ToolResult(true, "Plugin tool finished ($executed steps). ${created.joinToString(", ")}".trim())
    }

    private fun runStep(step: JSONObject, vars: MutableMap<String, Any?>, index: Int): NativeResult {
        val toolName = step.getString("tool")
        val args = resolve(step.optJSONObject("args") ?: JSONObject(), vars) as JSONObject
        val result = ToolHub.runNative(toolName, args)
        val variable = step.optString("as")
        if (result.ok && variable.isNotEmpty()) {
            val id = result.data?.optInt("object_id", -1) ?: -1
            if (id >= 0) vars[variable] = id.toDouble()
        }
        return result
    }

    private fun resolve(value: Any?, vars: Map<String, Any?>): Any? = when (value) {
        is String -> when {
            value.length > 1 && value[0] == '$' ->
                vars[value.substring(1)] ?: throw IllegalArgumentException("unknown variable ${value.substring(1)}")
            value.length > 1 && value[0] == '=' -> Expr.eval(value.substring(1), vars)
            else -> value
        }
        is JSONArray -> JSONArray().also { out -> for (i in 0 until value.length()) out.put(resolve(value.get(i), vars)) }
        is JSONObject -> JSONObject().also { out ->
            val keys = value.keys()
            while (keys.hasNext()) {
                val k = keys.next()
                out.put(k, resolve(value.get(k), vars))
            }
        }
        else -> value
    }

    private fun toNumber(v: Any?): Double = (v as? Number)?.toDouble() ?: throw IllegalArgumentException("expected a number")

    companion object {
        private const val MAX_STEPS = 500
        private const val MAX_REPEAT = 200

        private val idRe = Regex("""[a-z0-9-]{1,32}""")
        private val toolRe = Regex("""[a-z0-9_]{1,32}""")

        /** Lê e valida um plugin. Lança IllegalArgumentException se algo estiver errado. */
        fun parse(text: String): Plugin {
            require(text.length <= 200_000) { "plugin file is too big" }
            val root = JSONObject(text)
            require(root.optInt("schema") == 1) { "unsupported plugin schema (expected 1)" }
            val id = root.getString("id")
            require(idRe.matches(id)) { "plugin id must match [a-z0-9-]{1,32}" }
            val tools = ArrayList<PluginTool>()
            val arr = root.getJSONArray("tools")
            require(arr.length() in 1..30) { "a plugin needs 1 to 30 tools" }
            for (i in 0 until arr.length()) {
                val t = arr.getJSONObject(i)
                val toolName = t.getString("name")
                require(toolRe.matches(toolName)) { "tool names must match [a-z0-9_]{1,32}" }
                val steps = t.getJSONArray("steps")
                require(steps.length() in 1..100) { "a tool needs 1 to 100 steps" }
                for (s in 0 until steps.length()) {
                    val step = steps.getJSONObject(s)
                    val callee = step.getString("tool")
                    require(ToolHub.isNativeTool(callee)) { "step uses an unknown or non-native tool: $callee" }
                }
                val params = t.optJSONObject("parameters")
                    ?: JSONObject().put("type", "object").put("properties", JSONObject())
                tools.add(PluginTool(toolName, t.optString("description").take(400), params, steps))
            }
            return Plugin(
                id = id,
                name = root.optString("name", id).take(60),
                version = root.optString("version", "0").take(20),
                author = root.optString("author", "unknown").take(60),
                description = root.optString("description").take(400),
                tools = tools,
                raw = text
            )
        }

        /** Aceita github.com/dono/repo (opcionalmente /tree/ramo) ou um URL raw.githubusercontent.com. */
        fun rawUrlFor(input: String): String {
            val u = URL(input.trim())
            require(u.protocol == "https") { "use an https URL" }
            return when (u.host) {
                "raw.githubusercontent.com" -> input.trim()
                "github.com" -> {
                    val parts = u.path.trim('/').split('/')
                    require(parts.size >= 2) { "expected github.com/owner/repo" }
                    val owner = parts[0]
                    val repo = parts[1].removeSuffix(".git")
                    val branch = if (parts.size >= 4 && parts[2] == "tree") parts[3] else "HEAD"
                    "https://raw.githubusercontent.com/$owner/$repo/$branch/plugin.json"
                }
                else -> throw IllegalArgumentException("only github.com and raw.githubusercontent.com are allowed")
            }
        }

        /** Descarrega e valida um plugin do GitHub (bloqueante). Nada é instalado ainda. */
        fun fetchFromGithub(input: String): Plugin = parse(WebFetch.download(rawUrlFor(input), 220_000))
    }
}

/** Guarda os plugins instalados em ficheiros dentro do armazenamento privado do app. */
class PluginStore(context: Context) {
    private val dir = File(context.filesDir, "plugins").also { it.mkdirs() }

    fun loadAll(): List<Plugin> =
        (dir.listFiles { f -> f.extension == "json" } ?: emptyArray()).mapNotNull { f ->
            try {
                Plugin.parse(f.readText())
            } catch (e: Exception) {
                null
            }
        }.sortedBy { it.id }

    fun save(plugin: Plugin) {
        File(dir, plugin.id + ".json").writeText(plugin.raw)
    }

    fun remove(id: String) {
        File(dir, "$id.json").delete()
    }
}

/** Avaliador de expressões numéricas para os plugins. */
object Expr {
    fun eval(text: String, vars: Map<String, Any?>): Double {
        val p = ExprParser(text, vars)
        val v = p.expression()
        p.end()
        require(v.isFinite()) { "expression is not a finite number" }
        return v
    }
}

private class ExprParser(private val s: String, private val vars: Map<String, Any?>) {
    private var i = 0

    private fun ws() {
        while (i < s.length && s[i] == ' ') i++
    }

    fun end() {
        ws()
        require(i == s.length) { "unexpected text in expression" }
    }

    fun expression(): Double {
        var v = term()
        while (true) {
            ws()
            if (i < s.length && (s[i] == '+' || s[i] == '-')) {
                val op = s[i++]
                val r = term()
                v = if (op == '+') v + r else v - r
            } else {
                return v
            }
        }
    }

    private fun term(): Double {
        var v = unary()
        while (true) {
            ws()
            if (i < s.length && (s[i] == '*' || s[i] == '/')) {
                val op = s[i++]
                val r = unary()
                if (op == '*') {
                    v *= r
                } else {
                    require(r != 0.0) { "division by zero" }
                    v /= r
                }
            } else {
                return v
            }
        }
    }

    private fun unary(): Double {
        ws()
        if (i < s.length && s[i] == '-') {
            i++
            return -unary()
        }
        if (i < s.length && s[i] == '+') {
            i++
            return unary()
        }
        return power()
    }

    private fun power(): Double {
        val base = atom()
        ws()
        if (i < s.length && s[i] == '^') {
            i++
            return base.pow(unary())
        }
        return base
    }

    private fun atom(): Double {
        ws()
        require(i < s.length) { "unexpected end of expression" }
        val c = s[i]
        if (c == '(') {
            i++
            val v = expression()
            ws()
            require(i < s.length && s[i] == ')') { "missing )" }
            i++
            return v
        }
        if (c.isDigit() || c == '.') {
            val start = i
            while (i < s.length && (s[i].isDigit() || s[i] == '.')) i++
            return s.substring(start, i).toDouble()
        }
        if (c.isLetter() || c == '_') {
            val start = i
            while (i < s.length && (s[i].isLetterOrDigit() || s[i] == '_')) i++
            val name = s.substring(start, i)
            ws()
            if (i < s.length && s[i] == '(') {
                i++
                val args = ArrayList<Double>()
                ws()
                if (i < s.length && s[i] != ')') {
                    args.add(expression())
                    ws()
                    while (i < s.length && s[i] == ',') {
                        i++
                        args.add(expression())
                        ws()
                    }
                }
                require(i < s.length && s[i] == ')') { "missing )" }
                i++
                return call(name, args)
            }
            if (i < s.length && s[i] == '[') {
                i++
                val index = expression().toInt()
                ws()
                require(i < s.length && s[i] == ']') { "missing ]" }
                i++
                val list = vars[name] as? JSONArray ?: throw IllegalArgumentException("$name is not a list")
                return list.getDouble(index)
            }
            if (name == "pi") return Math.PI
            val v = vars[name]
            return (v as? Number)?.toDouble() ?: throw IllegalArgumentException("unknown variable $name")
        }
        throw IllegalArgumentException("unexpected character '$c'")
    }

    private fun call(name: String, a: List<Double>): Double {
        fun need(n: Int) = require(a.size == n) { "$name needs $n argument(s)" }
        return when (name) {
            "sin" -> { need(1); sin(a[0]) }
            "cos" -> { need(1); cos(a[0]) }
            "tan" -> { need(1); tan(a[0]) }
            "sqrt" -> { need(1); sqrt(a[0]) }
            "abs" -> { need(1); abs(a[0]) }
            "floor" -> { need(1); floor(a[0]) }
            "ceil" -> { need(1); ceil(a[0]) }
            "min" -> { need(2); min(a[0], a[1]) }
            "max" -> { need(2); max(a[0], a[1]) }
            else -> throw IllegalArgumentException("unknown function $name")
        }
    }
}
