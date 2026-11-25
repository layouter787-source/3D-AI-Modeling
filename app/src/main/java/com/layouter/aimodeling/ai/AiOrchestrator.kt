package com.layouter.aimodeling.ai

import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import java.util.concurrent.ConcurrentHashMap

/**
 * Resilient routing over providers/models that the user has actually configured.
 *
 * Candidate model IDs are discovered from each provider's authenticated model API.
 * The user's explicitly selected model is tried first; no model names are fabricated.
 */
class AiOrchestrator(
    private val credentials: Map<ProviderKind, String>,
    private val preferredProvider: ProviderKind,
    private val preferredModel: String,
    private val savedModels: Map<ProviderKind, String> = emptyMap(),
    private val modelLookup: suspend (ProviderKind, String) -> List<AiModel> = ModelCatalog::load,
    private val providerFactory: (ProviderKind, String, String) -> AiProvider = ::buildProvider,
    private val onRoute: (RouteEvent) -> Unit = {}
) {
    data class RouteEvent(
        val provider: ProviderKind,
        val model: String,
        val attempt: Int,
        val total: Int,
        val fallback: Boolean,
        val message: String
    )

    private data class Candidate(
        val provider: ProviderKind,
        val model: String,
        val key: String
    )

    private val discoveryCache = ConcurrentHashMap<ProviderKind, CachedModels>()
    private val discoveryLocks = ProviderKind.values().associateWith { Mutex() }
    private var lastSuccessful: Candidate? = null

    suspend fun send(system: String, messages: List<AiMessage>, tools: org.json.JSONArray): AiTurn {
        val candidates = buildCandidates()
        if (candidates.isEmpty()) {
            throw AiException("Nenhum provedor configurado. Adiciona pelo menos uma chave de API e escolhe um modelo.")
        }

        var lastError: Exception? = null
        val errors = mutableListOf<String>()
        var attempt = 0
        for (candidate in candidates) {
            if (candidate.provider in failedCredentials) continue
            attempt++
            val fallback = attempt > 1
            onRoute(
                RouteEvent(
                    candidate.provider, candidate.model, attempt, candidates.size,
                    fallback, if (fallback) "A tentar alternativa após falha…" else "A tentar modelo selecionado…"
                )
            )
            try {
                val turn = providerFactory(candidate.provider, candidate.key, candidate.model)
                    .send(system, messages, tools)
                lastSuccessful = candidate
                onRoute(
                    RouteEvent(candidate.provider, candidate.model, attempt, candidates.size,
                        fallback, if (fallback) "Ligação recuperada com modelo alternativo" else "Modelo respondeu")
                )
                return turn
            } catch (e: Exception) {
                if (e is kotlinx.coroutines.CancellationException) throw e
                lastError = e
                val short = "${candidate.provider.label}/${candidate.model}: ${e.message ?: e.javaClass.simpleName}"
                errors += short.take(180)
                onRoute(
                    RouteEvent(candidate.provider, candidate.model, attempt, candidates.size,
                        true, "Falhou; a procurar alternativa")
                )
                // Don't waste the entire request on more models for a credential/permission failure.
                if (e is AiException && (e.httpCode == 401 || e.httpCode == 403)) {
                    // Other models using the same invalid credential are not useful; candidates
                    // for the same provider are skipped below.
                    failedCredentials.add(candidate.provider)
                }
            }
        }

        throw AiException(
            "Todos os modelos configurados falharam. ${errors.takeLast(5).joinToString(" | ")}",
            lastError
        )
    }

    private val failedCredentials = ConcurrentHashMap.newKeySet<ProviderKind>()

    private suspend fun buildCandidates(): List<Candidate> {
        failedCredentials.clear()
        val configured = ProviderKind.values().filter { !credentials[it].isNullOrBlank() }
        if (configured.isEmpty()) return emptyList()

        val providerOrder = buildList {
            add(preferredProvider)
            lastSuccessful?.provider?.let { if (it !in this) add(it) }
            configured.forEach { if (it !in this) add(it) }
        }.filter { it in configured }

        val result = mutableListOf<Candidate>()
        for (provider in providerOrder) {
            val key = credentials[provider]?.trim().orEmpty()
            if (key.isBlank()) continue

            val selected = (if (provider == preferredProvider) preferredModel else savedModels[provider]).orEmpty().trim()
            val previousModel = lastSuccessful?.takeIf { it.provider == provider }?.model.orEmpty()

            val models = try {
                cachedModels(provider, key)
            } catch (_: Exception) {
                emptyList()
            }

            // Explicitly selected model remains usable if model listing is unavailable or
            // the service omits it from discovery.
            val ids = buildList {
                if (selected.isNotBlank()) add(selected)
                if (previousModel.isNotBlank() && previousModel !in this) add(previousModel)
                models.forEach { if (it.id.isNotBlank() && it.id !in this) add(it.id) }
            }

            // No implicit model defaults. Only user-selected or API-returned IDs are valid.
            ids.take(MAX_MODELS_PER_PROVIDER).forEach { model ->
                result += Candidate(provider, model, key)
            }
        }
        return result.distinctBy { "${it.provider.name}:${it.model}" }.take(MAX_TOTAL_ATTEMPTS)
    }

    private suspend fun cachedModels(provider: ProviderKind, key: String): List<AiModel> {
        val cached = discoveryCache[provider]
        if (cached != null && cached.keyFingerprint == key.hashCode() && System.currentTimeMillis() - cached.time < CACHE_MS) {
            return cached.models
        }
        return discoveryLocks.getValue(provider).withLock {
            val secondCheck = discoveryCache[provider]
            if (secondCheck != null && secondCheck.keyFingerprint == key.hashCode() &&
                System.currentTimeMillis() - secondCheck.time < CACHE_MS
            ) return@withLock secondCheck.models

            val fetched = modelLookup(provider, key)
            discoveryCache[provider] = CachedModels(key.hashCode(), System.currentTimeMillis(), fetched)
            fetched
        }
    }

    private data class CachedModels(val keyFingerprint: Int, val time: Long, val models: List<AiModel>)

    companion object {
        private const val MAX_MODELS_PER_PROVIDER = 4
        private const val MAX_TOTAL_ATTEMPTS = 12
        private const val CACHE_MS = 10 * 60 * 1000L
    }
}
