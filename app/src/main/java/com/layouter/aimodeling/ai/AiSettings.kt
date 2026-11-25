package com.layouter.aimodeling.ai

import android.content.Context

enum class ProviderKind(val label: String, val defaultModel: String, val keyHint: String) {
    OPENAI("OpenAI", "gpt-4o-mini", "sk-..."),
    ANTHROPIC("Claude", "claude-haiku-5-5", "sk-ant-...")
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
        ProviderKind.OPENAI -> OpenAiProvider(apiKey, model)
        ProviderKind.ANTHROPIC -> AnthropicProvider(apiKey, model)
    }
