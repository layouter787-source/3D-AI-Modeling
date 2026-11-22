package com.layouter.aimodeling.ai

import com.layouter.aimodeling.NativeBridge
import com.layouter.aimodeling.PrimitiveType
import org.json.JSONArray
import org.json.JSONObject

/**
 * Executa as ferramentas pedidas pela IA no motor 3D.
 * Valida tudo: a IA só consegue chamar estas funções, com estes argumentos, e cada alteração
 * entra no histórico (undo/redo) do núcleo.
 */
object ToolDispatcher {

    fun execute(call: ToolCall): ToolResult =
        try {
            dispatch(call)
        } catch (e: Exception) {
            ToolResult(false, "Invalid arguments: ${e.message}")
        }

    private fun dispatch(call: ToolCall): ToolResult {
        val a = call.arguments
        return when (call.name) {
            "create_primitive" -> createPrimitive(a)
            "set_transform" -> {
                val p = a.vec3("position")
                val r = a.vec3("rotation_degrees")
                val s = a.vec3("scale")
                report(
                    NativeBridge.setTransform(
                        a.getInt("object_id"),
                        p[0], p[1], p[2], r[0], r[1], r[2], s[0], s[1], s[2]
                    ),
                    "Transform updated"
                )
            }
            "move_by" -> {
                val o = a.vec3("offset")
                report(NativeBridge.moveBy(a.getInt("object_id"), o[0], o[1], o[2]), "Object moved")
            }
            "rotate_by" -> {
                val d = a.vec3("degrees")
                report(NativeBridge.rotateBy(a.getInt("object_id"), d[0], d[1], d[2]), "Object rotated")
            }
            "scale_by" -> {
                val f = a.vec3("factor")
                report(NativeBridge.scaleBy(a.getInt("object_id"), f[0], f[1], f[2]), "Object scaled")
            }
            "set_color" -> {
                val c = a.vec3("color")
                report(NativeBridge.setColor(a.getInt("object_id"), c[0], c[1], c[2]), "Color changed")
            }
            "delete_object" -> report(NativeBridge.deleteObject(a.getInt("object_id")), "Object deleted")
            "undo" -> report(NativeBridge.undo(), "Undone", "Nothing to undo")
            "redo" -> report(NativeBridge.redo(), "Redone", "Nothing to redo")
            "get_scene" -> ToolResult(true, NativeBridge.describeScene())
            else -> ToolResult(false, "Unknown tool: ${call.name}")
        }
    }

    private fun createPrimitive(a: JSONObject): ToolResult {
        val type = PrimitiveType.fromApiName(a.getString("type"))
            ?: return ToolResult(false, "Unknown primitive type")
        val size = a.optDouble("size", 1.0).toFloat()
        require(size.isFinite() && size > 0f) { "size must be a positive number" }
        val position = if (a.has("position")) {
            a.vec3("position")
        } else {
            floatArrayOf(0f, if (type == PrimitiveType.PLANE) 0f else size / 2f, 0f)
        }
        val id = NativeBridge.addPrimitive(type.id, size, position[0], position[1], position[2])
        return if (id < 0) {
            ToolResult(false, "Could not create the object (object limit reached or invalid values)")
        } else {
            ToolResult(true, "Created object with id $id")
        }
    }

    private fun report(
        ok: Boolean,
        okMessage: String,
        failMessage: String = "Object not found or invalid values"
    ) = ToolResult(ok, if (ok) okMessage else failMessage)

    private fun JSONObject.vec3(key: String): FloatArray {
        val array = getJSONArray(key)
        require(array.length() == 3) { "$key needs exactly 3 numbers" }
        return FloatArray(3) { i ->
            val value = array.getDouble(i).toFloat()
            require(value.isFinite()) { "$key has an invalid value" }
            value
        }
    }

    /** Definições neutras (JSON Schema). Cada provedor adapta ao formato do seu serviço. */
    fun toolDefinitions(): JSONArray {
        fun number3(description: String) = JSONObject()
            .put("type", "array")
            .put("items", JSONObject().put("type", "number"))
            .put("minItems", 3)
            .put("maxItems", 3)
            .put("description", description)

        val objectId = JSONObject()
            .put("type", "integer")
            .put("description", "Object id (see get_scene)")

        fun tool(name: String, description: String, properties: JSONObject, required: List<String>) =
            JSONObject()
                .put("name", name)
                .put("description", description)
                .put(
                    "parameters",
                    JSONObject()
                        .put("type", "object")
                        .put("properties", properties)
                        .put("required", JSONArray(required))
                )

        val primitiveTypes = JSONArray(PrimitiveType.values().map { it.apiName })

        return JSONArray()
            .put(
                tool(
                    "create_primitive",
                    "Create a primitive. Units are meters; Y is up. Default position puts it on the ground.",
                    JSONObject()
                        .put("type", JSONObject().put("type", "string").put("enum", primitiveTypes))
                        .put("size", JSONObject().put("type", "number").put("description", "Overall size, default 1"))
                        .put("position", number3("World position [x, y, z]")),
                    listOf("type")
                )
            )
            .put(
                tool(
                    "set_transform",
                    "Set the absolute transform of an object.",
                    JSONObject()
                        .put("object_id", objectId)
                        .put("position", number3("[x, y, z]"))
                        .put("rotation_degrees", number3("Euler XYZ in degrees"))
                        .put("scale", number3("[sx, sy, sz]")),
                    listOf("object_id", "position", "rotation_degrees", "scale")
                )
            )
            .put(
                tool(
                    "move_by", "Move an object by an offset.",
                    JSONObject().put("object_id", objectId).put("offset", number3("[dx, dy, dz]")),
                    listOf("object_id", "offset")
                )
            )
            .put(
                tool(
                    "rotate_by", "Rotate an object by Euler angles in degrees.",
                    JSONObject().put("object_id", objectId).put("degrees", number3("[rx, ry, rz]")),
                    listOf("object_id", "degrees")
                )
            )
            .put(
                tool(
                    "scale_by", "Multiply an object's scale per axis.",
                    JSONObject().put("object_id", objectId).put("factor", number3("[fx, fy, fz]")),
                    listOf("object_id", "factor")
                )
            )
            .put(
                tool(
                    "set_color", "Set the color, each channel from 0 to 1.",
                    JSONObject().put("object_id", objectId).put("color", number3("[r, g, b]")),
                    listOf("object_id", "color")
                )
            )
            .put(tool("delete_object", "Delete an object.", JSONObject().put("object_id", objectId), listOf("object_id")))
            .put(tool("undo", "Undo the last change.", JSONObject(), emptyList()))
            .put(tool("redo", "Redo the last undone change.", JSONObject(), emptyList()))
            .put(tool("get_scene", "Return the current scene as JSON.", JSONObject(), emptyList()))
    }
}
