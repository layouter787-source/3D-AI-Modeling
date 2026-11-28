package com.layouter.aimodeling.ai

import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit

/** HTTP JSON. Os métodos bloqueantes são usados apenas em Dispatchers.IO. */
internal object HttpJson {
    private val streamClient = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS)
        .readTimeout(0, TimeUnit.MILLISECONDS)
        .writeTimeout(30, TimeUnit.SECONDS)
        .retryOnConnectionFailure(true)
        .build()


    fun get(url: String, headers: Map<String, String> = emptyMap()): JSONObject =
        request(url, "GET", headers, null, 30_000)

    fun post(url: String, headers: Map<String, String>, body: JSONObject): JSONObject =
        request(url, "POST", headers, body, 90_000)

    /** OkHttp/Okio reads each flushed SSE event from the open response stream. */
    suspend fun postSse(url: String, headers: Map<String, String>, body: JSONObject, onData: (String) -> Unit) =
        withContext(Dispatchers.IO) {
            val requestBody = body.toString().toRequestBody("application/json; charset=utf-8".toMediaType())
            val request = Request.Builder().url(url).post(requestBody)
                .header("Accept", "text/event-stream")
                .header("Cache-Control", "no-cache, no-transform")
                .header("Content-Type", "application/json")
                .apply { headers.forEach { (name, value) -> header(name, value) } }
                .build()
            streamClient.newCall(request).execute().use { response ->
                if (!response.isSuccessful) {
                    val error = response.body?.string().orEmpty()
                    throw AiException("HTTP ${response.code}: ${errorMessage(error)}", httpCode = response.code)
                }
                val source = response.body?.source() ?: throw AiException("Resposta de streaming vazia")
                var data = StringBuilder()
                while (!source.exhausted()) {
                    currentCoroutineContext().ensureActive()
                    val line = source.readUtf8Line() ?: break
                    if (line.isEmpty()) {
                        if (data.isNotEmpty()) { onData(data.toString()); data = StringBuilder() }
                    } else if (line.startsWith("data:")) {
                        if (data.isNotEmpty()) data.append('\\n')
                        data.append(line.substring(5).trimStart())
                    }
                }
                if (data.isNotEmpty()) onData(data.toString())
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
