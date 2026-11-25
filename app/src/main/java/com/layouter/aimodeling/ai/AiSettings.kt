package com.layouter.aimodeling.ai

import android.content.Context

/**
 * Serviços suportados. O utilizador só cola a chave de API: a base URL já vem preenchida.
 * O nome do modelo é só uma sugestão e pode ser mudado nas definições, porque os modelos mudam.
 */
enum class ProviderKind(
    val label: String,
    val defaultModel: String,
    val keyHint: String,
    val keySource: String,
    val baseUrl: String
) {
    OPENAI(
        "OpenAI", "gpt-5.4-mini", "sk-...", "platform.openai.com/api-keys",
        "https://api.openai.com/v1"
    ),
    ANTHROPIC(
        "Claude", "claude-haiku-5-5", "sk-ant-...", "console.anthropic.com",
        "https://api.anthropic.com/v1"
    ),
    GEMINI(
        "Gemini", "gemini-3.8-flash", "AIza...", "aistudio.google.com/apikey",
        "https://generativelanguage.googleapis.com/v1beta/openai"
    ),
    NVIDIA(
        "NVIDIA NIM", "nvidia/nemotron-3-super-120b-a12b", "nvapi-...", "build.nvidia.com",
        "https://integrate.api.nvidia.com/v1"
    )
}

/** Definições não secretas (provedor e modelo). A chave de API fica no [SecretStore]. */
class AiSettings(context: Context) {

    private val prefs = context.getSharedPreferences("ai_settings", Context.MODE_PRIVATE)

    var kind: ProviderKind
        get() = ProviderKind.values().firstOrNull { it.name == prefs.getString("kind", null) }
            ?: ProviderKind.OPENAI
        set(value) {
            prefs.edit().putString("kind", value.name).apply()
        }

    fun model(kind: ProviderKind): String =
        prefs.getString("model_${kind.name}", null)?.takeIf { it.isNotBlank() } ?: kind.defaultModel

    fun setModel(kind: ProviderKind, model: String) {
        prefs.edit().putString("model_${kind.name}", model).apply()
    }

    companion object {
        fun keyName(kind: ProviderKind) = "api_key_${kind.name}"
    }
}

fun buildProvider(kind: ProviderKind, apiKey: String, model: String): AiProvider =
    when (kind) {
        ProviderKind.OPENAI -> OpenAiProvider(apiKey, model, kind.baseUrl, "OpenAI")
        ProviderKind.ANTHROPIC -> AnthropicProvider(apiKey, model, kind.baseUrl)
        ProviderKind.GEMINI ->
            OpenAiProvider(apiKey, model, kind.baseUrl, "Gemini", omitEmptyParameters = true)
        ProviderKind.NVIDIA -> OpenAiProvider(apiKey, model, kind.baseUrl, "NVIDIA NIM")
    }
