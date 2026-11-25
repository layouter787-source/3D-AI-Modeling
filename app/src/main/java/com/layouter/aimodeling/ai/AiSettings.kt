package com.layouter.aimodeling.ai

import android.content.Context

/** Provedores com integração efetivamente implementada no aplicativo. */
enum class ProviderKind(
    val label: String,
    val keyHint: String,
    val keySource: String,
    val baseUrl: String
) {
    OPENAI(
        "OpenAI", "sk-...", "platform.openai.com/api-keys",
        "https://api.openai.com/v1"
    ),
    ANTHROPIC(
        "Claude", "sk-ant-...", "console.anthropic.com",
        "https://api.anthropic.com/v1"
    ),
    GEMINI(
        "Gemini", "AIza...", "aistudio.google.com/apikey",
        "https://generativelanguage.googleapis.com/v1beta/openai"
    ),
    NVIDIA(
        "NVIDIA NIM", "nvapi-...", "build.nvidia.com",
        "https://integrate.api.nvidia.com/v1"
    )
}

/** Preferências de provedor/modelo. Nenhum modelo é pré-selecionado ou inventado. */
class AiSettings(context: Context) {
    private val prefs = context.getSharedPreferences("ai_settings", Context.MODE_PRIVATE)

    var kind: ProviderKind
        get() = ProviderKind.values().firstOrNull { it.name == prefs.getString("kind", null) }
            ?: ProviderKind.OPENAI
        set(value) {
            prefs.edit().putString("kind", value.name).apply()
        }

    fun model(kind: ProviderKind): String =
        prefs.getString("model_${kind.name}", null)?.trim().orEmpty()

    fun setModel(kind: ProviderKind, model: String) {
        val value = model.trim()
        if (value.isBlank()) {
            prefs.edit().remove("model_${kind.name}").apply()
        } else {
            prefs.edit().putString("model_${kind.name}", value).apply()
        }
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
