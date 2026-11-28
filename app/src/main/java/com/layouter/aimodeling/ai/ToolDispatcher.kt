package com.layouter.aimodeling.ai

import com.layouter.aimodeling.NativeBridge
import com.layouter.aimodeling.PrimitiveType
import org.json.JSONArray
import org.json.JSONObject

object ToolDispatcher {

    fun execute(call: ToolCall): ToolResult =
        try { dispatch(call) } catch (e: Exception) {
            ToolResult(false, "Invalid arguments: ${e.message}")
        }

    private fun dispatch(call: ToolCall): ToolResult {
        val a = call.arguments
        return when (call.name) {
            "create_primitive" -> createPrimitive(a)
            "clone_object" -> {
                val newId = NativeBridge.cloneObject(a.getInt("object_id"))
                if (newId >= 0) ToolResult(true, "Cloned -> $newId") else ToolResult(false, "Clone failed")
            }
            "set_transform" -> {
                val p = a.vec3("position"); val r = a.vec3("rotation_degrees"); val s = a.vec3("scale")
                report(NativeBridge.setTransform(a.getInt("object_id"), p[0], p[1], p[2], r[0], r[1], r[2], s[0], s[1], s[2]), "Transform updated")
            }
            "move_by" -> { val o = a.vec3("offset"); report(NativeBridge.moveBy(a.getInt("object_id"), o[0], o[1], o[2]), "Moved") }
            "rotate_by" -> { val d = a.vec3("degrees"); report(NativeBridge.rotateBy(a.getInt("object_id"), d[0], d[1], d[2]), "Rotated") }
            "scale_by" -> { val f = a.vec3("factor"); report(NativeBridge.scaleBy(a.getInt("object_id"), f[0], f[1], f[2]), "Scaled") }
            "set_color" -> {
                val c = a.vec3("color")
                report(NativeBridge.setColor(a.getInt("object_id"), c[0], c[1], c[2]), "Color set")
            }
            "set_pbr" -> {
                val c = a.vec3("color")
                val metallic = a.optDouble("metallic", 0.0).toFloat()
                val roughness = a.optDouble("roughness", 0.5).toFloat()
                report(NativeBridge.setPbr(a.getInt("object_id"), c[0], c[1], c[2], metallic, roughness), "PBR set")
            }
            "delete_object" -> report(NativeBridge.deleteObject(a.getInt("object_id")), "Deleted")
            "mesh_operation" -> {
                val id = a.getInt("object_id")
                val op = a.getString("operation")
                val amount = a.optDouble("amount", 0.0).toFloat()
                val segments = a.optInt("segments", 1)
                val faces = parseFaces(a.optString("face_indices", ""))
                report(NativeBridge.meshOp(id, op, amount, segments, faces), "mesh_operation $op ok", "mesh_operation $op failed")
            }
            "combine_objects" -> {
                val result = NativeBridge.combineObjects(a.getInt("object_id_a"), a.getInt("object_id_b"))
                if (result >= 0) ToolResult(true, "Combined into $result") else ToolResult(false, "Combine failed")
            }
            "separate_faces" -> {
                val faces = parseFaces(a.optString("face_indices", ""))
                val newId = NativeBridge.separateFaces(a.getInt("object_id"), faces)
                if (newId >= 0) ToolResult(true, "Separated -> $newId") else ToolResult(false, "Separate failed")
            }
            "export_glb" -> {
                val path = a.getString("path")
                report(NativeBridge.exportGlb(a.getInt("object_id"), path), "Exported GLB to $path")
            }
            "import_glb" -> {
                val path = a.getString("path")
                val id = NativeBridge.importGlb(path)
                if (id >= 0) ToolResult(true, "Imported GLB as object $id") else ToolResult(false, "Import failed")
            }
            "set_display_mode" -> report(NativeBridge.setDisplayMode(a.getString("mode")), "Display mode set")
            "set_select_mode" -> report(NativeBridge.setSelectMode(a.getString("mode")), "Select mode set")
            "undo" -> report(NativeBridge.undo(), "Undone", "Nothing to undo")
            "redo" -> report(NativeBridge.redo(), "Redone", "Nothing to redo")
            "get_scene" -> ToolResult(true, NativeBridge.describeScene())
            "get_modeling_extensions" -> modelingExtensions()
            else -> ToolResult(false, "Unknown tool: ${call.name}")
        }
    }

    private fun parseFaces(csv: String): IntArray? {
        if (csv.isBlank()) return null
        val list = csv.split(',').mapNotNull { it.trim().toIntOrNull() }
        return if (list.isEmpty()) null else list.toIntArray()
    }

    private fun createPrimitive(a: JSONObject): ToolResult {
        val type = PrimitiveType.fromApiName(a.getString("type"))
            ?: return ToolResult(false, "Unknown primitive type")
        val size = a.optDouble("size", 1.0).toFloat()
        require(size.isFinite() && size > 0f)
        val position = if (a.has("position")) a.vec3("position")
        else floatArrayOf(0f, if (type == PrimitiveType.PLANE) 0f else size / 2f, 0f)
        val id = NativeBridge.addPrimitive(type.id, size, position[0], position[1], position[2])
        return if (id < 0) ToolResult(false, "Could not create") else ToolResult(true, "Created id $id")
    }

    private fun modelingExtensions(): ToolResult =
        ToolResult(true, JSONObject()
            .put("mesh_operations", JSONArray(listOf(
                "soft_normals", "hard_normals", "divide", "smooth", "weld", "mirror",
                "extrude", "bevel", "create_face", "connect", "target_merge"
            )))
            .put("formats", JSONArray(listOf("glb")))
            .put("pbr", true)
            .put("picking", JSONArray(listOf("object", "vertex", "edge", "face")))
            .toString())

    private fun report(ok: Boolean, okMsg: String, failMsg: String = "Failed") =
        ToolResult(ok, if (ok) okMsg else failMsg)

    private fun JSONObject.vec3(key: String): FloatArray {
        val array = getJSONArray(key)
        require(array.length() == 3)
        return FloatArray(3) { array.getDouble(it).toFloat().also { v -> require(v.isFinite()) } }
    }

    fun toolDefinitions(): JSONArray {
        fun number3(d: String) = JSONObject().put("type", "array").put("items", JSONObject().put("type", "number"))
            .put("minItems", 3).put("maxItems", 3).put("description", d)
        val oid = JSONObject().put("type", "integer")
        fun tool(n: String, d: String, p: JSONObject, r: List<String>) = JSONObject()
            .put("name", n).put("description", d)
            .put("parameters", JSONObject().put("type", "object").put("properties", p).put("required", JSONArray(r)))
        val prims = JSONArray(PrimitiveType.values().map { it.apiName })
        val ops = JSONArray(listOf(
            "soft_normals", "hard_normals", "divide", "smooth", "weld", "mirror",
            "extrude", "bevel", "create_face", "connect", "target_merge"
        ))
        return JSONArray()
            .put(tool("create_primitive", "Create primitive", JSONObject()
                .put("type", JSONObject().put("type", "string").put("enum", prims))
                .put("size", JSONObject().put("type", "number"))
                .put("position", number3("[x,y,z]")), listOf("type")))
            .put(tool("clone_object", "Clone", JSONObject().put("object_id", oid), listOf("object_id")))
            .put(tool("set_transform", "Set transform", JSONObject()
                .put("object_id", oid).put("position", number3("pos"))
                .put("rotation_degrees", number3("rot")).put("scale", number3("scale")),
                listOf("object_id", "position", "rotation_degrees", "scale")))
            .put(tool("move_by", "Move", JSONObject().put("object_id", oid).put("offset", number3("off")), listOf("object_id", "offset")))
            .put(tool("rotate_by", "Rotate", JSONObject().put("object_id", oid).put("degrees", number3("deg")), listOf("object_id", "degrees")))
            .put(tool("scale_by", "Scale", JSONObject().put("object_id", oid).put("factor", number3("f")), listOf("object_id", "factor")))
            .put(tool("set_color", "Color RGB 0-1", JSONObject().put("object_id", oid).put("color", number3("rgb")), listOf("object_id", "color")))
            .put(tool("set_pbr", "PBR baseColor + metallic + roughness", JSONObject()
                .put("object_id", oid).put("color", number3("rgb"))
                .put("metallic", JSONObject().put("type", "number"))
                .put("roughness", JSONObject().put("type", "number")),
                listOf("object_id", "color")))
            .put(tool("delete_object", "Delete", JSONObject().put("object_id", oid), listOf("object_id")))
            .put(tool("mesh_operation", "Mesh/topology op", JSONObject()
                .put("object_id", oid)
                .put("operation", JSONObject().put("type", "string").put("enum", ops))
                .put("amount", JSONObject().put("type", "number"))
                .put("segments", JSONObject().put("type", "integer"))
                .put("face_indices", JSONObject().put("type", "string")
                    .put("description", "csv: faces OR verts for create_face/connect/target_merge")),
                listOf("object_id", "operation")))
            .put(tool("combine_objects", "Merge B into A", JSONObject()
                .put("object_id_a", oid).put("object_id_b", oid), listOf("object_id_a", "object_id_b")))
            .put(tool("separate_faces", "Extract faces", JSONObject()
                .put("object_id", oid).put("face_indices", JSONObject().put("type", "string")),
                listOf("object_id", "face_indices")))
            .put(tool("export_glb", "Export object to GLB file path", JSONObject()
                .put("object_id", oid).put("path", JSONObject().put("type", "string")),
                listOf("object_id", "path")))
            .put(tool("import_glb", "Import GLB from path", JSONObject()
                .put("path", JSONObject().put("type", "string")), listOf("path")))
            .put(tool("set_display_mode", "Display", JSONObject()
                .put("mode", JSONObject().put("type", "string").put("enum",
                    JSONArray(listOf("shaded", "wireframe", "wireframe_on_shaded")))),
                listOf("mode")))
            .put(tool("set_select_mode", "Selection mode", JSONObject()
                .put("mode", JSONObject().put("type", "string").put("enum",
                    JSONArray(listOf("object", "face", "edge", "vertex")))),
                listOf("mode")))
            .put(tool("undo", "Undo", JSONObject(), emptyList()))
            .put(tool("redo", "Redo", JSONObject(), emptyList()))
            .put(tool("get_scene", "Scene JSON", JSONObject(), emptyList()))
            .put(tool("get_modeling_extensions", "Catalog", JSONObject(), emptyList()))
    }
}
