package com.layouter.aimodeling.ai

import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive

/** HTTP JSON. Os métodos bloqueantes são usados apenas em Dispatchers.IO. */
internal object HttpJson {

    fun get(url: String, headers: Map<String, String> = emptyMap()): JSONObject =
        request(url, "GET", headers, null, 30_000)

    fun post(url: String, headers: Map<String, String>, body: JSONObject): JSONObject =
        request(url, "POST", headers, body, 90_000)

    /** Consumes server-sent events incrementally; it never buffers the complete response. */
    suspend fun postSse(url: String, headers: Map<String, String>, body: JSONObject, onData: (String) -> Unit) =
        withContext(Dispatchers.IO) {
            val connection = URL(url).openConnection() as HttpURLConnection
            try {
                connection.requestMethod = "POST"
                connection.connectTimeout = 20_000
                connection.readTimeout = 120_000
                connection.setRequestProperty("Accept", "text/event-stream")
                connection.setRequestProperty("Cache-Control", "no-cache")
                connection.setRequestProperty("Content-Type", "application/json")
                headers.forEach { (name, value) -> connection.setRequestProperty(name, value) }
                connection.doOutput = true
                connection.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }
                val code = connection.responseCode
                if (code !in 200..299) {
                    val error = connection.errorStream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }.orEmpty()
                    throw AiException("HTTP $code: ${errorMessage(error)}", httpCode = code)
                }
                val reader = connection.inputStream.bufferedReader(Charsets.UTF_8)
                var data = StringBuilder()
                while (true) {
                    currentCoroutineContext().ensureActive()
                    val line = reader.readLine() ?: break
                    if (line.isEmpty()) {
                        if (data.isNotEmpty()) { onData(data.toString()); data = StringBuilder() }
                    } else if (line.startsWith("data:")) {
                        if (data.isNotEmpty()) data.append('\\n')
                        data.append(line.substring(5).trimStart())
                    }
                }
                if (data.isNotEmpty()) onData(data.toString())
            } finally {
                connection.disconnect()
            }
        }

    private fun request(
        url: String,
        method: String,
        headers: Map<String, String>,
        body: JSONObject?,
        readTimeout: Int
    ): JSONObject {
        val connection = URL(url).openConnection() as HttpURLConnection
        try {
            connection.requestMethod = method
            connection.connectTimeout = 20_000
            connection.readTimeout = readTimeout
            connection.setRequestProperty("Accept", "application/json")
            headers.forEach { (name, value) -> connection.setRequestProperty(name, value) }
            if (body != null) {
                connection.setRequestProperty("Content-Type", "application/json")
                connection.doOutput = true
                connection.outputStream.use {
                    it.write(body.toString().toByteArray(Charsets.UTF_8))
                }
            }

            val code = connection.responseCode
            val stream = if (code in 200..299) connection.inputStream else connection.errorStream
            val text = stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }.orEmpty()
            if (code !in 200..299) throw AiException("HTTP $code: ${errorMessage(text)}", httpCode = code)
            return JSONObject(text)
        } finally {
            connection.disconnect()
        }
    }

    private fun errorMessage(text: String): String =
        try {
            JSONObject(text).optJSONObject("error")?.optString("message")?.take(300)
                ?: text.take(200)
        } catch (e: Exception) {
            text.take(200)
        }
}
