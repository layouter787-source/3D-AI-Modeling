package com.layouter.aimodeling.ai

import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

/** HTTP JSON. Os métodos bloqueantes são usados apenas em Dispatchers.IO. */
internal object HttpJson {

    fun get(url: String, headers: Map<String, String> = emptyMap()): JSONObject =
        request(url, "GET", headers, null, 30_000)

    fun post(url: String, headers: Map<String, String>, body: JSONObject): JSONObject =
        request(url, "POST", headers, body, 90_000)

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
            if (body != null) {
                connection.doOutput = true
                connection.setRequestProperty("Content-Type", "application/json")
                connection.outputStream.use {
                    it.write(body.toString().toByteArray(Charsets.UTF_8))
                }
            }
            headers.forEach { (name, value) -> connection.setRequestProperty(name, value) }

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
