package com.layouter.aimodeling.ai

import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.json.JSONArray
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class ProviderStreamingTest {
    private lateinit var server: MockWebServer
    private val tools = JSONArray()

    @Before fun startServer() { server = MockWebServer(); server.start() }
    @After fun stopServer() { server.shutdown() }
    private fun baseUrl() = server.url("/v1/").toString().trimEnd('/')
    private fun sse(vararg frames: String) = frames.joinToString("\n\n") { "data: $it" } + "\n\n"
    private fun openAiFrame(delta: JSONObject, finish: String? = null): String {
        val choice = JSONObject().put("delta", delta)
        if (finish != null) choice.put("finish_reason", finish)
        return JSONObject().put("choices", JSONArray().put(choice)).toString()
    }

    @Test fun openAiCompatibleStreamEmitsTokensAndReassemblesToolArguments() = runBlocking {
        val firstCall = JSONObject().put("index", 0).put("id", "call_1").put("function",
            JSONObject().put("name", "create_").put("arguments", "{\"primitive\":"))
        val secondCall = JSONObject().put("index", 0).put("function",
            JSONObject().put("name", "primitive").put("arguments", "\"cube\"}"))
        server.enqueue(MockResponse().setHeader("Content-Type", "text/event-stream").setBody(sse(
            openAiFrame(JSONObject().put("content", "Build ")),
            openAiFrame(JSONObject().put("content", "a mesh")),
            openAiFrame(JSONObject().put("tool_calls", JSONArray().put(firstCall))),
            openAiFrame(JSONObject().put("tool_calls", JSONArray().put(secondCall)), "tool_calls"),
            "[DONE]"
        )))
        val provider = OpenAiProvider("test-key", "test-model", baseUrl(), "Mock OpenAI")
        val deltas = StringBuilder()
        val turn = provider.sendStreaming("system", listOf(AiMessage.User("make")), tools) { deltas.append(it) }
        assertEquals("Build a mesh", turn.text)
        assertEquals(turn.text, deltas.toString())
        assertEquals(1, turn.toolCalls.size)
        assertEquals("create_primitive", turn.toolCalls[0].name)
        assertEquals("cube", turn.toolCalls[0].arguments.getString("primitive"))
    }

    @Test fun anthropicStreamParsesTextAndToolUse() = runBlocking {
        fun event(type: String, fields: JSONObject = JSONObject()) = fields.put("type", type).toString()
        server.enqueue(MockResponse().setHeader("Content-Type", "text/event-stream").setBody(sse(
            event("message_start", JSONObject().put("message", JSONObject().put("id", "msg_1"))),
            event("content_block_start", JSONObject().put("index", 0).put("content_block", JSONObject().put("type", "text").put("text", "Ready"))),
            event("content_block_delta", JSONObject().put("index", 0).put("delta", JSONObject().put("type", "text_delta").put("text", "."))),
            event("content_block_stop", JSONObject().put("index", 0)),
            event("content_block_start", JSONObject().put("index", 1).put("content_block", JSONObject().put("type", "tool_use").put("id", "tool_1").put("name", "create_primitive").put("input", JSONObject()))),
            event("content_block_delta", JSONObject().put("index", 1).put("delta", JSONObject().put("type", "input_json_delta").put("partial_json", "{\"primitive\":\"sphere\"}"))),
            event("content_block_stop", JSONObject().put("index", 1)),
            event("message_stop")
        )))
        val provider = AnthropicProvider("test-key", "claude-test", baseUrl())
        val deltas = StringBuilder()
        val turn = provider.sendStreaming("system", listOf(AiMessage.User("make")), tools) { deltas.append(it) }
        assertEquals("Ready.", turn.text)
        assertEquals(turn.text, deltas.toString())
        assertEquals(1, turn.toolCalls.size)
        assertEquals("sphere", turn.toolCalls[0].arguments.getString("primitive"))
    }

    @Test fun unsupportedStreamingRetriesWithJsonNotStreamFlag() = runBlocking {
        server.enqueue(MockResponse().setResponseCode(400).setBody("""{"error":{"message":"stream not supported"}}"""))
        server.enqueue(MockResponse().setHeader("Content-Type", "application/json").setBody("""{"choices":[{"message":{"content":"fallback ok"}}]}"""))
        val provider = OpenAiProvider("test-key", "test-model", baseUrl(), "Fallback Provider")
        val turn = provider.sendStreaming("system", listOf(AiMessage.User("hello")), tools) {}
        assertEquals("fallback ok", turn.text)
        assertTrue(JSONObject(server.takeRequest().body.readUtf8()).getBoolean("stream"))
        assertFalse(JSONObject(server.takeRequest().body.readUtf8()).has("stream"))
    }
}
