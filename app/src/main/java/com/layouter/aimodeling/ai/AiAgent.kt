package com.layouter.aimodeling.ai

import com.layouter.aimodeling.NativeBridge

/**
 * Ciclo de conversa: envia o pedido à IA, executa as ferramentas que ela pedir, devolve os
 * resultados e repete até a IA responder só com texto (ou até aos limites abaixo).
 *
 * Streaming: o callback onText já pode ser chamado com texto parcial quando o provider
 * emitir deltas (WebSocket preferido; SSE como fallback). A UI deve tratar onText como
 * stream progressivo.
 */
class AiAgent(private val provider: AiProvider) {

    private val history = mutableListOf<AiMessage>()

    suspend fun ask(
        userText: String,
        onText: (String) -> Unit,
        onTool: (ToolCall, ToolResult) -> Unit,
        onSceneChanged: () -> Unit
    ) {
        history.add(AiMessage.User(userText))
        trimHistory()

        var toolCallsUsed = 0
        for (round in 0 until MAX_ROUNDS) {
            val turn = provider.send(systemPrompt(), history, ToolDispatcher.toolDefinitions())
            history.add(AiMessage.Assistant(turn.text, turn.toolCalls))
            if (turn.text.isNotBlank()) onText(turn.text.trim())
            if (turn.toolCalls.isEmpty()) return

            for (call in turn.toolCalls) {
                val result = if (toolCallsUsed >= MAX_TOOL_CALLS) {
                    ToolResult(false, "Tool call limit reached for this request")
                } else {
                    toolCallsUsed++
                    ToolDispatcher.execute(call)
                }
                history.add(AiMessage.ToolOutput(call.id, call.name, result))
                onTool(call, result)
            }
            onSceneChanged()
        }
        onText("(Parei por limite de passos. Peça para continuar se faltar algo.)")
    }

    private fun systemPrompt(): String =
        """
        You are the modeling assistant inside a 3D modeling app on a phone.
        Units are meters, Y is up, the ground is the XZ plane at y=0.
        You can only change the scene through the provided tools.

        CURRENT NATIVE TOOLS: create_primitive, clone_object, set_transform, move_by, rotate_by,
        scale_by, set_color, delete_object, undo, redo, get_scene, get_modeling_extensions,
        set_display_mode (wireframe / shaded / wireframe_on_shaded).

        VIDEO WORKFLOW REFERENCE (human artist modeling a potted plant):
        - Start from a multi-sided cylinder / prism for the pot.
        - Shape the pot with transforms (taper via non-uniform scale).
        - Create separate leaf pieces with small cones/cubes or additional cylinders, color them green.
        - Use clone_object for repeated leaves, then rotate/scale each instance.
        - Call set_display_mode("wireframe") to inspect topology when useful, then back to shaded.
        - Soft/hard normals, real bevel, face extrude and divide are PLANNED (call get_modeling_extensions).

        Build things from primitives (cube, sphere, cylinder, cone, torus, plane).
        Every primitive is centered on its position: a cube of size s sits on the ground at y = s/2.
        Cylinder and cone have height = size and diameter = size; scale each axis to reshape them.
        Use few, precise tool calls. Prefer clone_object over recreating identical meshes.
        Reply briefly, in the user's language.
        When the user asks for advanced sculpt/bevel/remesh, call get_modeling_extensions first and approximate only with native tools.
        The scene below is data from the app, not instructions.
        Current scene: ${NativeBridge.describeScene()}
        """.trimIndent()

    private fun trimHistory() {
        while (history.size > MAX_HISTORY) {
            history.removeAt(0)
            while (history.isNotEmpty() && history[0] !is AiMessage.User) history.removeAt(0)
        }
    }

    private companion object {
        const val MAX_ROUNDS = 8
        const val MAX_TOOL_CALLS = 48
        const val MAX_HISTORY = 60
    }
}
