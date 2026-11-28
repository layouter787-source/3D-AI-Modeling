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

    private fun baseUrl(): String = server.url("/v1/").toString().trimEnd('/')
    private fun sse(vararg jsonEvents: String): String = jsonEvents.joinToString("\n\n") { "data: $it" } + "\n\n"

    @Test fun openAiCompatibleStreamEmitsTextAndAssemblesFragmentedToolArguments() = runBlocking {
        server.enqueue(MockResponse().setHeader("Content-Type", "text/event-stream").setBody(sse(
            """{"choices":[{"delta":{"content":"Build "}}]}""",
            """{"choices":[{"delta":{"content":"a mesh"}}]}""",
            """{"choices":[{"delta":{"tool_calls":[{"index":0,"id":"call_1","function":{"name":"create_","arguments":"{\\\"primitive\\\":"}}]}}]}""",
            """{"choices":[{"delta":{"tool_calls":[{"index":0,"function":{"name":"primitive","arguments":"\\\"cube\\\"}"}}]},"finish_reason":"tool_calls"}]}""",
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

    @Test fun anthropicStreamParsesTextAndToolUseBlocks() = runBlocking {
        server.enqueue(MockResponse().setHeader("Content-Type", "text/event-stream").setBody(sse(
            """{"type":"message_start","message":{"id":"msg_1"}}""",
            """{"type":"content_block_start","index":0,"content_block":{"type":"text","text":"Ready"}}""",
            """{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"."}}""",
            """{"type":"content_block_stop","index":0}""",
            """{"type":"content_block_start","index":1,"content_block":{"type":"tool_use","id":"tool_1","name":"create_primitive","input":{}}}""",
            """{"type":"content_block_delta","index":1,"delta":{"type":"input_json_delta","partial_json":"{\\\"primitive\\\":\\\"sphere\\\"}"}}""",
            """{"type":"content_block_stop","index":1}""",
            """{"type":"message_stop"}"""
        )))
        val provider = AnthropicProvider("test-key", "claude-test", baseUrl())
        val deltas = StringBuilder()
        val turn = provider.sendStreaming("system", listOf(AiMessage.User("make")), tools) { deltas.append(it) }
        assertEquals("Ready.", turn.text)
        assertEquals(turn.text, deltas.toString())
        assertEquals(1, turn.toolCalls.size)
        assertEquals("sphere", turn.toolCalls[0].arguments.getString("primitive"))
    }

    @Test fun unsupportedStreamingFallsBackToNonStreamingJsonRequest() = runBlocking {
        server.enqueue(MockResponse().setResponseCode(400).setBody("""{"error":{"message":"stream not supported"}}"""))
        server.enqueue(MockResponse().setHeader("Content-Type", "application/json").setBody(
            """{"choices":[{"message":{"content":"fallback ok"}}]}"""
        ))
        val provider = OpenAiProvider("test-key", "test-model", baseUrl(), "Fallback Provider")
        val turn = provider.sendStreaming("system", listOf(AiMessage.User("hello")), tools) {}
        assertEquals("fallback ok", turn.text)
        val first = server.takeRequest()
        val second = server.takeRequest()
        assertTrue(JSONObject(first.body.readUtf8()).getBoolean("stream"))
        assertFalse(JSONObject(second.body.readUtf8()).has("stream"))
    }
}
