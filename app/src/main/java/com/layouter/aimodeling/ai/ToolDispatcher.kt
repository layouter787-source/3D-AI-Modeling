package com.layouter.aimodeling.ai

import com.layouter.aimodeling.NativeBridge
import com.layouter.aimodeling.PrimitiveType
import org.json.JSONArray
import org.json.JSONObject

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
            "clone_object" -> {
                val id = a.getInt("object_id")
                val newId = NativeBridge.cloneObject(id)
                if (newId >= 0) ToolResult(true, "Cloned object $id -> $newId")
                else ToolResult(false, "Clone failed")
            }
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
                report(NativeBridge.moveBy(a.getInt("object_id"), o[0], o[1], o[2]), "Moved")
            }
            "rotate_by" -> {
                val d = a.vec3("degrees")
                report(NativeBridge.rotateBy(a.getInt("object_id"), d[0], d[1], d[2]), "Rotated")
            }
            "scale_by" -> {
                val f = a.vec3("factor")
                report(NativeBridge.scaleBy(a.getInt("object_id"), f[0], f[1], f[2]), "Scaled")
            }
            "set_color" -> {
                val c = a.vec3("color")
                report(NativeBridge.setColor(a.getInt("object_id"), c[0], c[1], c[2]), "Color set")
            }
            "delete_object" -> report(NativeBridge.deleteObject(a.getInt("object_id")), "Deleted")
            "mesh_operation" -> {
                val id = a.getInt("object_id")
                val op = a.getString("operation")
                val amount = a.optDouble("amount", 0.0).toFloat()
                val segments = a.optInt("segments", 1)
                val faces = a.optString("face_indices", "")
                report(
                    NativeBridge.meshOperation(id, op, amount, segments, faces),
                    "mesh_operation $op ok",
                    "mesh_operation $op failed"
                )
            }
            "combine_objects" -> {
                val aId = a.getInt("object_id_a")
                val bId = a.getInt("object_id_b")
                val result = NativeBridge.combineObjects(aId, bId)
                if (result >= 0) ToolResult(true, "Combined into object $result")
                else ToolResult(false, "Combine failed")
            }
            "separate_faces" -> {
                val id = a.getInt("object_id")
                val faces = a.optString("face_indices", "")
                val newId = NativeBridge.separateFaces(id, faces)
                if (newId >= 0) ToolResult(true, "Separated into object $newId")
                else ToolResult(false, "Separate failed (need face_indices)")
            }
            "set_display_mode" -> {
                val mode = a.getString("mode")
                report(NativeBridge.setDisplayMode(mode), "Display mode $mode")
            }
            "undo" -> report(NativeBridge.undo(), "Undone", "Nothing to undo")
            "redo" -> report(NativeBridge.redo(), "Redone", "Nothing to redo")
            "get_scene" -> ToolResult(true, NativeBridge.describeScene())
            "get_modeling_extensions" -> modelingExtensions()
            else -> ToolResult(false, "Unknown tool: ${call.name}")
        }
    }

    private fun createPrimitive(a: JSONObject): ToolResult {
        val type = PrimitiveType.fromApiName(a.getString("type"))
            ?: return ToolResult(false, "Unknown primitive type")
        val size = a.optDouble("size", 1.0).toFloat()
        require(size.isFinite() && size > 0f) { "size must be positive" }
        val position = if (a.has("position")) a.vec3("position")
        else floatArrayOf(0f, if (type == PrimitiveType.PLANE) 0f else size / 2f, 0f)
        val id = NativeBridge.addPrimitive(type.id, size, position[0], position[1], position[2])
        return if (id < 0) ToolResult(false, "Could not create object")
        else ToolResult(true, "Created object id $id")
    }

    private fun modelingExtensions(): ToolResult {
        val json = JSONObject()
            .put("native_ops", JSONArray(listOf(
                "create_primitive", "clone_object", "set_transform", "move_by", "rotate_by",
                "scale_by", "set_color", "delete_object", "mesh_operation", "combine_objects",
                "separate_faces", "set_display_mode", "undo", "redo", "get_scene"
            )))
            .put("mesh_operations", JSONArray(listOf(
                "soft_normals", "hard_normals", "divide", "subdivide", "smooth",
                "weld", "merge_vertices", "mirror", "extrude", "bevel"
            )))
            .put("video_tools", JSONArray(listOf(
                "Clone", "Delete", "Soft normals", "Hard normals", "Divide", "Smooth",
                "Mirror", "Combine", "Separate", "Bevel", "Merge close vertices",
                "Wireframe", "Free transform", "Materials (color)"
            )))
            .put("note", "Use mesh_operation with operation name from mesh_operations list.")
        return ToolResult(true, json.toString())
    }

    private fun report(ok: Boolean, okMsg: String, failMsg: String = "Failed") =
        ToolResult(ok, if (ok) okMsg else failMsg)

    private fun JSONObject.vec3(key: String): FloatArray {
        val array = getJSONArray(key)
        require(array.length() == 3) { "$key needs 3 numbers" }
        return FloatArray(3) { i ->
            val v = array.getDouble(i).toFloat()
            require(v.isFinite()) { "$key invalid" }
            v
        }
    }

    fun toolDefinitions(): JSONArray {
        fun number3(desc: String) = JSONObject()
            .put("type", "array")
            .put("items", JSONObject().put("type", "number"))
            .put("minItems", 3).put("maxItems", 3)
            .put("description", desc)

        val objectId = JSONObject().put("type", "integer").put("description", "Object id")

        fun tool(name: String, desc: String, props: JSONObject, required: List<String>) =
            JSONObject()
                .put("name", name)
                .put("description", desc)
                .put("parameters", JSONObject()
                    .put("type", "object")
                    .put("properties", props)
                    .put("required", JSONArray(required)))

        val primitives = JSONArray(PrimitiveType.values().map { it.apiName })
        val meshOps = JSONArray(listOf(
            "soft_normals", "hard_normals", "divide", "subdivide", "smooth",
            "weld", "merge_vertices", "mirror", "extrude", "bevel"
        ))

        return JSONArray()
            .put(tool("create_primitive", "Create a primitive (meters, Y up).",
                JSONObject()
                    .put("type", JSONObject().put("type", "string").put("enum", primitives))
                    .put("size", JSONObject().put("type", "number"))
                    .put("position", number3("[x,y,z]")),
                listOf("type")))
            .put(tool("clone_object", "Duplicate an object.",
                JSONObject().put("object_id", objectId), listOf("object_id")))
            .put(tool("set_transform", "Absolute transform.",
                JSONObject()
                    .put("object_id", objectId)
                    .put("position", number3("[x,y,z]"))
                    .put("rotation_degrees", number3("Euler XYZ deg"))
                    .put("scale", number3("[sx,sy,sz]")),
                listOf("object_id", "position", "rotation_degrees", "scale")))
            .put(tool("move_by", "Move by offset.",
                JSONObject().put("object_id", objectId).put("offset", number3("[dx,dy,dz]")),
                listOf("object_id", "offset")))
            .put(tool("rotate_by", "Rotate by degrees.",
                JSONObject().put("object_id", objectId).put("degrees", number3("[rx,ry,rz]")),
                listOf("object_id", "degrees")))
            .put(tool("scale_by", "Scale multiply.",
                JSONObject().put("object_id", objectId).put("factor", number3("[fx,fy,fz]")),
                listOf("object_id", "factor")))
            .put(tool("set_color", "RGB 0..1.",
                JSONObject().put("object_id", objectId).put("color", number3("[r,g,b]")),
                listOf("object_id", "color")))
            .put(tool("delete_object", "Delete object.",
                JSONObject().put("object_id", objectId), listOf("object_id")))
            .put(tool("mesh_operation",
                "Apply mesh op from the video/Blender set: soft_normals, hard_normals, divide, smooth, weld, mirror, extrude, bevel.",
                JSONObject()
                    .put("object_id", objectId)
                    .put("operation", JSONObject().put("type", "string").put("enum", meshOps))
                    .put("amount", JSONObject().put("type", "number")
                        .put("description", "Distance/factor/epsilon depending on op"))
                    .put("segments", JSONObject().put("type", "integer")
                        .put("description", "Bevel segments or smooth iterations; mirror axis 0/1/2"))
                    .put("face_indices", JSONObject().put("type", "string")
                        .put("description", "Comma-separated triangle indices for extrude/separate")),
                listOf("object_id", "operation")))
            .put(tool("combine_objects", "Merge B into A and delete B.",
                JSONObject()
                    .put("object_id_a", objectId)
                    .put("object_id_b", objectId),
                listOf("object_id_a", "object_id_b")))
            .put(tool("separate_faces", "Extract faces into a new object.",
                JSONObject()
                    .put("object_id", objectId)
                    .put("face_indices", JSONObject().put("type", "string")),
                listOf("object_id", "face_indices")))
            .put(tool("set_display_mode", "wireframe | shaded | wireframe_on_shaded",
                JSONObject().put("mode", JSONObject().put("type", "string").put("enum",
                    JSONArray(listOf("wireframe", "shaded", "wireframe_on_shaded")))),
                listOf("mode")))
            .put(tool("undo", "Undo.", JSONObject(), emptyList()))
            .put(tool("redo", "Redo.", JSONObject(), emptyList()))
            .put(tool("get_scene", "Scene JSON.", JSONObject(), emptyList()))
            .put(tool("get_modeling_extensions", "Catalog of native + video tools.",
                JSONObject(), emptyList()))
    }
}
