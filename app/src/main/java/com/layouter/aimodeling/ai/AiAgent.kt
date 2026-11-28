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

        NATIVE TOOLS:
        - create_primitive, clone_object, set_transform, move_by, rotate_by, scale_by, set_color
        - delete_object, undo, redo, get_scene
        - set_display_mode (shaded | wireframe | wireframe_on_shaded)
        - mesh_operation with operations:
          soft_normals, hard_normals, divide, subdivide, triangulate, smooth,
          weld, merge_vertices, mirror, extrude, bevel
        - combine_objects, separate_faces
        - set_select_mode (object|face|edge|vertex), set_selected_faces
        - get_modeling_extensions

        VIDEO WORKFLOW (potted plant reference):
        1. create cylinder for pot; scale non-uniformly to taper
        2. mesh_operation divide / soft_normals / bevel as needed
        3. create leaf pieces (cone/cube), set_color green, clone_object + rotate
        4. set_display_mode wireframe to inspect, then shaded
        5. combine_objects when you want a single mesh

        mesh_operation tips:
        - soft_normals / hard_normals: amount ignored
        - divide/subdivide: one midpoint level
        - smooth: amount=factor (0-1), segments=iterations
        - bevel: amount=radius-ish, segments=1..4
        - extrude: amount=distance; optional faces[] triangle indices
        - mirror: segments = axis (0=X,1=Y,2=Z)
        - weld: amount=epsilon

        Reply briefly in the user's language.
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
        const val MAX_TOOL_CALLS = 56
        const val MAX_HISTORY = 60
    }
}
