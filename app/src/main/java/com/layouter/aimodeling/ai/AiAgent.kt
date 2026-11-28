package com.layouter.aimodeling.ai

import com.layouter.aimodeling.NativeBridge

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
                    ToolResult(false, "Tool call limit reached")
                } else {
                    toolCallsUsed++
                    ToolDispatcher.execute(call)
                }
                history.add(AiMessage.ToolOutput(call.id, call.name, result))
                onTool(call, result)
            }
            onSceneChanged()
        }
        onText("(Limite de passos. Peça para continuar.)")
    }

    private fun systemPrompt(): String =
        """
        You are the modeling assistant inside a phone 3D app. Units: meters, Y up, ground XZ at y=0.
        Only change the scene through tools.

        TOOLS:
        - create_primitive (cube/sphere/cylinder/cone/torus/plane)
        - clone_object, delete_object, set_transform, move_by, rotate_by, scale_by, set_color
        - mesh_operation: soft_normals | hard_normals | divide | smooth | weld | mirror | extrude | bevel
          amount = distance/factor/epsilon; segments = bevel segments or smooth iterations or mirror axis 0/1/2
          face_indices = optional csv of triangle indices for extrude
        - combine_objects, separate_faces
        - set_display_mode: shaded | wireframe | wireframe_on_shaded
        - set_select_mode: object | face | edge | vertex
        - undo, redo, get_scene, get_modeling_extensions

        VIDEO WORKFLOW (potted plant reference):
        1) cylinder for pot, scale non-uniformly
        2) mesh_operation divide / smooth / soft_normals on pot
        3) small cones/cubes for leaves, set_color green, clone_object + rotate
        4) bevel on edges when useful; wireframe to inspect topology
        5) set_color beige/brown on pot

        Reply briefly in the user's language. Prefer few precise tool calls.
        Current scene: ${NativeBridge.describeScene()}
        """.trimIndent()

    private fun trimHistory() {
        while (history.size > MAX_HISTORY) {
            history.removeAt(0)
            while (history.isNotEmpty() && history[0] !is AiMessage.User) history.removeAt(0)
        }
    }

    private companion object {
        const val MAX_ROUNDS = 10
        const val MAX_TOOL_CALLS = 60
        const val MAX_HISTORY = 60
    }
}
