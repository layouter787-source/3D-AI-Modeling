package com.layouter.aimodeling.ai

import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

/** POST JSON simples (bloqueante: chamar sempre fora da thread principal). */
internal object HttpJson {

    fun post(url: String, headers: Map<String, String>, body: JSONObject): JSONObject {
        val connection = URL(url).openConnection() as HttpURLConnection
        try {
            connection.requestMethod = "POST"
            connection.connectTimeout = 20_000
            connection.readTimeout = 90_000
            connection.doOutput = true
            connection.setRequestProperty("Content-Type", "application/json")
            headers.forEach { (name, value) -> connection.setRequestProperty(name, value) }
            connection.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }

            val code = connection.responseCode
            val stream = if (code in 200..299) connection.inputStream else connection.errorStream
            val text = stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }.orEmpty()
            if (code !in 200..299) throw AiException("HTTP $code: ${errorMessage(text)}")
            return JSONObject(text)
        } finally {
            connection.disconnect()
        }
    }

    private fun errorMessage(text: String): String =
        try {
            JSONObject(text).optJSONObject("error")?.optString("message")?.take(300) ?: text.take(200)
        } catch (e: Exception) {
            text.take(200)
        }
}
