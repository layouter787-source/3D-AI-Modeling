package com.layouter.aimodeling.ai

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.HttpURLConnection
import java.net.URI
import java.net.URLEncoder
import java.net.URL
import java.net.InetAddress
import java.io.ByteArrayOutputStream

/**
 * Internet tools for public documentation and research. HTTPS only, bounded time/size,
 * no localhost/private IPs, and redirects are revalidated. Web content is always untrusted.
 */
object InternetTools {
    suspend fun search(query: String): String = withContext(Dispatchers.IO) {
        require(query.isNotBlank() && query.length <= 300) { "Search query must be 1..300 characters" }
        val url = "https://html.duckduckgo.com/html/?q=" + URLEncoder.encode(query, "UTF-8")
        val html = request(url)
        val links = Regex("""<a[^>]*class="[^"]*result__a[^"]*"[^>]*href="([^"]+)"[^>]*>(.*?)</a>""", RegexOption.IGNORE_CASE)
        val snippets = Regex("""<a[^>]*class="[^"]*result__snippet[^"]*"[^>]*>(.*?)</a>|<div[^>]*class="[^"]*result__snippet[^"]*"[^>]*>(.*?)</div>""", RegexOption.IGNORE_CASE)
        val titles = links.findAll(html).take(8).map { it.groupValues[2].stripHtml() + " | " + it.groupValues[1] }.toList()
        val desc = snippets.findAll(html).take(8).map { (it.groups[1]?.value ?: it.groups[2]?.value.orEmpty()).stripHtml() }.toList()
        if (titles.isEmpty()) "Search completed but no parsable results. Try a more specific query." else
            titles.mapIndexed { i, t -> (i + 1).toString() + ". " + t + (desc.getOrNull(i)?.let { "\n   " + it } ?: "") }.joinToString("\n")
    }

    suspend fun readPage(url: String): String = withContext(Dispatchers.IO) {
        val text = request(url).stripHtml()
        text.take(12000).ifBlank { "The page returned no readable text." }
    }

    private fun request(initial: String): String {
        var current = initial
        repeat(4) {
            val uri = URI(current)
            require(uri.scheme == "https" && !uri.host.isNullOrBlank()) { "Only HTTPS public websites are allowed" }
            val host = uri.host.lowercase()
            require(!host.equals("localhost") && !host.endsWith(".localhost") && !host.endsWith(".local") &&
                !host.matches(Regex("""[0-9.]+""")) && !host.contains(":")) { "Private/local hosts are blocked" }
            val resolved = InetAddress.getAllByName(host)
            require(resolved.isNotEmpty() && resolved.all { address ->
                val b = address.address.map { it.toInt() and 0xff }
                val v4Private = b.size == 4 && (
                    b[0] == 10 || b[0] == 127 || (b[0] == 169 && b[1] == 254) ||
                    (b[0] == 172 && b[1] in 16..31) || (b[0] == 192 && b[1] == 168) ||
                    b[0] == 0 || b[0] >= 224
                )
                val v6Local = b.size == 16 && (
                    address.isLoopbackAddress || address.isLinkLocalAddress ||
                    address.isSiteLocalAddress || (b[0] and 0xfe) == 0xfc || b[0] == 0xff
                )
                !address.isAnyLocalAddress && !address.isLoopbackAddress &&
                    !address.isLinkLocalAddress && !address.isSiteLocalAddress &&
                    !address.isMulticastAddress && !v4Private && !v6Local
            }) { "Private or reserved network destinations are blocked" }
            val connection = URL(current).openConnection() as HttpURLConnection
            connection.instanceFollowRedirects = false
            connection.connectTimeout = 10000
            connection.readTimeout = 12000
            connection.setRequestProperty("User-Agent", "3D-AI-Modeling-Android/1.0")
            connection.setRequestProperty("Accept", "text/html,text/plain,application/json")
            val code = connection.responseCode
            if (code in 300..399) {
                val location = connection.getHeaderField("Location") ?: error("Redirect without Location")
                current = URI(current).resolve(location).toString()
                connection.disconnect()
                return@repeat
            }
            require(code in 200..299) { "Web request failed with HTTP " + code }
            val input = connection.inputStream
            val out = ByteArrayOutputStream()
            val buffer = ByteArray(8192)
            while (true) {
                val n = input.read(buffer)
                if (n < 0) break
                require(out.size() + n <= 700000) { "Web page exceeds 700 KB safety limit" }
                out.write(buffer, 0, n)
            }
            connection.disconnect()
            return out.toString("UTF-8")
        }
        error("Too many redirects or redirect validation failed")
    }

    private fun String.stripHtml(): String = this
        .replace(Regex("""(?is)<(script|style|svg|noscript).*?</\\1>"""), " ")
        .replace(Regex("""(?s)<[^>]+>"""), " ")
        .replace("&amp;", "&").replace("&quot;", "\"").replace("&#39;", "'")
        .replace("&lt;", "<").replace("&gt;", ">")
        .replace(Regex("""\\s+"""), " ").trim()
}
