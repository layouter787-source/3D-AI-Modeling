package com.layouter.aimodeling.ai

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import java.net.URLEncoder

/** Lista os modelos publicados pela API do provedor, nunca uma lista local de nomes inventados. */
data class AiModel(val id: String, val displayName: String = id)

object ModelCatalog {
    suspend fun load(kind: ProviderKind, apiKey: String): List<AiModel> =
        withContext(Dispatchers.IO) {
            require(apiKey.isNotBlank()) { "Introduz primeiro a chave de API." }
            when (kind) {
                ProviderKind.GEMINI -> loadGemini(apiKey)
                ProviderKind.ANTHROPIC -> loadAnthropic(apiKey, kind.baseUrl)
                ProviderKind.OPENAI, ProviderKind.NVIDIA -> loadOpenAiCompatible(apiKey, kind.baseUrl)
            }.distinctBy { it.id }.sortedBy { it.id.lowercase() }
        }

    private fun loadOpenAiCompatible(key: String, baseUrl: String): List<AiModel> {
        val response = HttpJson.get(
            "${baseUrl.trimEnd('/')}/models",
            mapOf("Authorization" to "Bearer $key")
        )
        return response.optJSONArray("data").toModels("id")
    }

    private fun loadGemini(key: String): List<AiModel> {
        val response = HttpJson.get(
            "https://generativelanguage.googleapis.com/v1beta/models?pageSize=1000",
            mapOf("x-goog-api-key" to key)
        )
        val models = response.optJSONArray("models") ?: JSONArray()
        return buildList {
            for (i in 0 until models.length()) {
                val item = models.optJSONObject(i) ?: continue
                val methods = item.optJSONArray("supportedGenerationMethods") ?: JSONArray()
                val supportsGenerate = (0 until methods.length()).any {
                    methods.optString(it) == "generateContent"
                }
                if (!supportsGenerate) continue
                val id = item.optString("name").removePrefix("models/").trim()
                if (id.isNotBlank()) add(AiModel(id, item.optString("displayName", id)))
            }
        }
    }

    private fun loadAnthropic(key: String, baseUrl: String): List<AiModel> {
        val result = mutableListOf<AiModel>()
        var cursor: String? = null
        for (page in 0 until 5) {
            val query = buildString {
                append("${baseUrl.trimEnd('/')}/models?limit=1000")
                cursor?.let { append("&after_id=").append(URLEncoder.encode(it, "UTF-8")) }
            }
            val response = HttpJson.get(
                query,
                mapOf("x-api-key" to key, "anthropic-version" to "2023-06-01")
            )
            result += response.optJSONArray("data").toModels("id", "display_name")
            if (!response.optBoolean("has_more")) break
            cursor = response.optString("last_id").takeIf { it.isNotBlank() } ?: break
        }
        return result
    }

    private fun JSONArray?.toModels(idField: String, labelField: String? = null): List<AiModel> {
        val array = this ?: JSONArray()
        return buildList {
            for (i in 0 until array.length()) {
                val item = array.optJSONObject(i) ?: continue
                val id = item.optString(idField).trim()
                if (id.isNotBlank()) add(AiModel(id, labelField?.let { item.optString(it, id) } ?: id))
            }
        }
    }
}
