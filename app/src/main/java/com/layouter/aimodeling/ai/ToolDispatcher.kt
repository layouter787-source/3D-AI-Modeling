package com.layouter.aimodeling.ai

import com.layouter.aimodeling.NativeBridge
import com.layouter.aimodeling.PrimitiveType
import org.json.JSONArray
import org.json.JSONObject

/** Routes branch tool names into the current Manus PolyMesh engine. */
object ToolDispatcher {
    suspend fun execute(call: ToolCall): ToolResult = try {
        dispatch(call)
    } catch (e: Exception) {
        ToolResult(false, "Invalid arguments or operation failed: " + (e.message ?: "unknown error"))
    }

    private suspend fun dispatch(call: ToolCall): ToolResult {
        val a = call.arguments
        return when (call.name) {
            "create_primitive" -> createPrimitive(a)
            "create_batch" -> createBatch(a)
            "create_asset_preset" -> createAssetPreset(a)
            "create_loft_mesh" -> createLoft(a)
            "duplicate_object", "clone_object" -> {
                val id = NativeBridge.duplicateObject(a.getInt("object_id"),
                    a.optDouble("dx", 0.15).toFloat(), a.optDouble("dy", 0.0).toFloat(),
                    a.optDouble("dz", 0.0).toFloat())
                result(id >= 0, "Duplicated object " + id, "Duplicate failed")
            }
            "delete_object" -> report(NativeBridge.deleteObject(a.getInt("object_id")), "Object deleted")
            "set_transform" -> {
                val p=a.vec3("position"); val r=a.vec3("rotation_degrees"); val s=a.vec3("scale")
                report(NativeBridge.setTransform(a.getInt("object_id"),p[0],p[1],p[2],r[0],r[1],r[2],s[0],s[1],s[2]),"Transform updated")
            }
            "move_by" -> { val v=a.vec3("offset"); report(NativeBridge.moveBy(a.getInt("object_id"),v[0],v[1],v[2]),"Object moved") }
            "rotate_by" -> { val v=a.vec3("degrees"); report(NativeBridge.rotateBy(a.getInt("object_id"),v[0],v[1],v[2]),"Object rotated") }
            "scale_by" -> { val v=a.vec3("factor"); report(NativeBridge.scaleBy(a.getInt("object_id"),v[0],v[1],v[2]),"Object scaled") }
            "set_color" -> { val v=a.vec3("color"); report(NativeBridge.setColor(a.getInt("object_id"),v[0],v[1],v[2]),"Base color updated") }
            "set_material" -> report(NativeBridge.setMaterial(a.getInt("object_id"),
                a.optDouble("metallic",0.0).toFloat(),a.optDouble("roughness",0.48).toFloat(),
                a.optDouble("clearcoat",0.0).toFloat(),a.optDouble("emission",0.0).toFloat()),"PBR material updated")
            "set_pbr" -> { val c=a.vec3("color"); report(NativeBridge.setPbr(a.getInt("object_id"),c[0],c[1],c[2],
                a.optDouble("metallic",0.0).toFloat(),a.optDouble("roughness",0.48).toFloat()),"PBR updated") }
            "sculpt_mesh" -> { val c=a.vec3("center"); val o=a.vec3("offset")
                report(NativeBridge.sculptMesh(a.getInt("object_id"),c[0],c[1],c[2],
                    a.getDouble("radius").toFloat(),o[0],o[1],o[2]),"Sculpt stroke applied") }
            "mesh_operation" -> meshOperation(a)
            "combine_objects" -> report(NativeBridge.combineObjects(a.getInt("object_id"),a.getInt("other_object_id")),"Objects combined")
            "detach_faces", "separate_faces" -> {
                val id=NativeBridge.detachFaces(a.getInt("object_id"),faces(a))
                result(id>=0,"Detached faces into object "+id,"Detach failed")
            }
            "get_edge_loop_faces" -> {
                val loop = NativeBridge.getEdgeLoopFaces(
                    a.getInt("object_id"), a.getInt("vertex_a"), a.getInt("vertex_b"))
                ToolResult(loop.isNotEmpty(), JSONArray(loop.toList()).toString().ifEmpty { "[]" })
            }
            "knife_cut_face" -> {
                val ok = NativeBridge.meshOperation(
                    a.getInt("object_id"),
                    "knife_cut_face",
                    intArrayOf(a.getInt("face_id"), a.getInt("edge_a"), a.getInt("edge_b")),
                    1,
                    a.optDouble("t", 0.5).toFloat(),
                    -1
                )
                report(ok, "Knife cut applied to face", "Knife cut failed: verify face and local edge indices")
            }
            "get_face_shell" -> ToolResult(true,JSONArray(
                NativeBridge.getFaceShell(a.getInt("object_id"),a.getInt("start_face")).toList()).toString())
            "paint_vertex_region" -> { val c=a.vec3("center")
                report(NativeBridge.paintVertexRegion(a.getInt("object_id"),c[0],c[1],c[2],
                    a.getDouble("radius").toFloat(),a.getDouble("r").toFloat(),a.getDouble("g").toFloat(),
                    a.getDouble("b").toFloat(),a.optDouble("opacity",1.0).toFloat()),"Vertex region painted") }
            "get_mesh_topology" -> ToolResult(true,NativeBridge.getMeshTopology(
                a.getInt("object_id"),a.optInt("offset",0),a.optInt("limit",256)))
            "get_object_details" -> objectDetails(a)
            "snap_to_ground" -> report(NativeBridge.snapToGround(a.getInt("object_id")),"Object snapped to ground")
            "snap_to_grid" -> report(NativeBridge.snapToGrid(a.getInt("object_id"),a.optDouble("step",1.0).toFloat()),"Object snapped to grid")
            "set_environment" -> { val sun=a.vec3("sun_direction"); val color=a.vec3("sun_color"); val sky=a.vec3("sky_color")
                NativeBridge.setEnvironment(sun[0],sun[1],sun[2],color[0],color[1],color[2],
                    a.optDouble("sun_intensity",1.0).toFloat(),a.optDouble("ambient_strength",0.16).toFloat(),
                    sky[0],sky[1],sky[2])
                ToolResult(true,"Viewport environment updated") }
            "set_display_mode" -> report(NativeBridge.setDisplayMode(a.getString("mode")),"Display mode updated")
            "set_select_mode" -> report(NativeBridge.setSelectMode(a.getString("mode")),"Selection mode updated")
            "pick_component" -> ToolResult(true,NativeBridge.pickAtComponent(a.getDouble("x").toFloat(),
                a.getDouble("y").toFloat(),a.getString("mode"),a.optInt("object_id",-1)))
            "undo" -> report(NativeBridge.undo(),"Undo completed","Nothing to undo")
            "redo" -> report(NativeBridge.redo(),"Redo completed","Nothing to redo")
            "get_scene" -> ToolResult(true,NativeBridge.describeScene())
            "inspect_scene" -> inspectScene()
            "get_modeling_extensions" -> modelingExtensions()
            "web_search" -> ToolResult(true,InternetTools.search(a.getString("query")))
            "read_webpage" -> ToolResult(true,InternetTools.readPage(a.getString("url")))
            else -> {
                val external = RemoteMcpConnector.execute(call.name, a)
                if (external != null) external
                else ToolResult(false,"Unknown tool: " + call.name)
            }
        }
    }

    private fun createPrimitive(a: JSONObject): ToolResult {
        val type=PrimitiveType.fromApiName(a.getString("type")) ?: return ToolResult(false,"Unknown primitive type")
        val size=a.optDouble("size",1.0).toFloat(); require(size.isFinite() && size>0f)
        val p=if(a.has("position"))a.vec3("position") else floatArrayOf(0f,if(type==PrimitiveType.PLANE)0f else size/2f,0f)
        val id=NativeBridge.addPrimitiveDetailed(type.id,size,p[0],p[1],p[2],a.optInt("detail",8).coerceIn(1,64))
        return result(id>=0,"Created "+type.apiName+" with id "+id,"Primitive creation failed")
    }

    private fun createBatch(a: JSONObject): ToolResult {
        val items=a.getJSONArray("objects"); require(items.length() in 1..64)
        val ids=JSONArray()
        for(i in 0 until items.length()){
            val item=items.getJSONObject(i)
            val type=PrimitiveType.fromApiName(item.getString("type")) ?: error("Unknown primitive at index "+i)
            val size=item.optDouble("size",1.0).toFloat()
            val p=if(item.has("position"))item.vec3("position") else floatArrayOf(0f,size/2f,0f)
            val id=NativeBridge.addPrimitiveDetailed(type.id,size,p[0],p[1],p[2],item.optInt("detail",8).coerceIn(1,64))
            if(id<0)return ToolResult(false,"Batch stopped at item "+i+"; earlier objects remain created")
            ids.put(id)
        }
        return ToolResult(true,ids.toString())
    }

    private fun createAssetPreset(a: JSONObject): ToolResult = when(a.getString("preset")){
        "humanoid_blockout" -> {
            val ids=JSONArray()
            fun add(name:String,size:Float,x:Float,y:Float,z:Float){
                val t=PrimitiveType.fromApiName(name) ?: error("Preset primitive unavailable")
                val id=NativeBridge.addPrimitiveDetailed(t.id,size,x,y,z,16)
                require(id>=0); ids.put(id)
            }
            add("capsule",0.72f,0f,1.48f,0f); add("sphere",0.34f,0f,2.12f,0f)
            add("cube",0.62f,0f,0.94f,0f); add("capsule",0.20f,-0.48f,1.35f,0f)
            add("capsule",0.20f,0.48f,1.35f,0f); add("capsule",0.18f,-0.16f,0.43f,0f)
            add("capsule",0.18f,0.16f,0.43f,0f)
            ToolResult(true,"Humanoid blockout part ids: "+ids)
        }
        "ergonomic_mouse" -> {
            val p=floatArrayOf(-0.95f,0.10f,0.22f,0.08f,-0.65f,0.36f,0.25f,0.16f,
                -0.20f,0.43f,0.28f,0.22f,0.35f,0.38f,0.25f,0.20f,0.78f,0.27f,0.22f,0.15f)
            val id=NativeBridge.createLoftMesh(p,24,0f,0f,0f)
            result(id>=0,"Ergonomic mouse loft created: "+id,"Loft preset failed")
        }
        else -> ToolResult(false,"Unknown preset; use humanoid_blockout or ergonomic_mouse")
    }

    private fun createLoft(a: JSONObject): ToolResult {
        val p=a.getJSONArray("profiles")
        val values=FloatArray(p.length()){p.getDouble(it).toFloat()}
        val id=NativeBridge.createLoftMesh(values,a.optInt("segments",24).coerceIn(8,128),
            a.optDouble("x",0.0).toFloat(),a.optDouble("y",0.0).toFloat(),a.optDouble("z",0.0).toFloat())
        return result(id>=0,"Loft mesh created: "+id,"Loft creation failed")
    }

    private fun meshOperation(a: JSONObject): ToolResult {
        val ok=NativeBridge.meshOperation(a.getInt("object_id"),a.getString("operation"),faces(a),
            a.optInt("parameter",1).coerceIn(1,100),a.optDouble("value",0.1).toFloat(),a.optInt("other_object_id",-1))
        return report(ok,"Mesh operation completed","Mesh operation failed")
    }

    private fun inspectScene(): ToolResult {
        val scene = JSONObject(NativeBridge.describeScene())
        val objects = scene.optJSONArray("objects") ?: JSONArray()
        val inspected = JSONArray()
        for (index in 0 until minOf(objects.length(), 32)) {
            val item = objects.optJSONObject(index) ?: continue
            val id = item.optInt("id", -1)
            if (id < 0) continue
            inspected.put(JSONObject().put("object", item)
                .put("topology_preview", NativeBridge.getMeshTopology(id, 0, 48)))
        }
        return ToolResult(true, JSONObject().put("scene", scene)
            .put("object_count", objects.length()).put("mesh_inspection", inspected)
            .put("note", "Structured scene and mesh data only; this response is not a rendered viewport screenshot.").toString())
    }

    private fun objectDetails(a: JSONObject): ToolResult {
        val objects=JSONObject(NativeBridge.describeScene()).optJSONArray("objects") ?: JSONArray()
        val id=a.getInt("object_id")
        for(i in 0 until objects.length()){
            val obj=objects.getJSONObject(i)
            if(obj.optInt("id",-1)==id)return ToolResult(true,obj.toString())
        }
        return ToolResult(false,"Object "+id+" was not found")
    }

    private fun modelingExtensions()=ToolResult(true,JSONObject()
        .put("native_mesh_operations",JSONArray(listOf("subdivide","extrude","inset","bevel","delete_faces",
            "clone_faces","create_face","merge_vertices","target_merge","soft_normals","hard_normals","normal_angle",
            "smooth","solidify","mirror","loop_cut","connect","triangulate","flip_normals","weld","crease_by_angle",
            "boolean_union","boolean_difference","boolean_intersection","draw_cut","split_edge","knife_cut_face")))
        .put("native_tools",JSONArray(listOf("GLB import/export","component picking","wireframe display",
            "vertex paint","PBR materials","loft meshes","sculpt strokes","OBJ/MTL export")))
        .put("research_only",JSONArray(listOf("voxel remesh","QuadriFlow","UV editing","rigging","animation",
            "external sculpt libraries"))).put("note","Research-only items are not installed native features").toString())

    private fun faces(a:JSONObject):IntArray{
        val array=a.optJSONArray("faces") ?: a.optJSONArray("face_indices")
        if(array!=null)return IntArray(array.length()){array.getInt(it)}
        val csv=a.optString("face_indices","")
        return if(csv.isBlank()) intArrayOf() else csv.split(',').map{it.trim().toInt()}.toIntArray()
    }
    private fun JSONObject.vec3(key:String):FloatArray{
        val v=getJSONArray(key); require(v.length()==3){key+" must contain three numbers"}
        return FloatArray(3){v.getDouble(it).toFloat().also{n->require(n.isFinite())}}
    }
    private fun result(ok:Boolean,yes:String,no:String)=ToolResult(ok,if(ok)yes else no)
    private fun report(ok:Boolean,yes:String,no:String="Operation failed")=result(ok,yes,no)
    private fun vecSchema(description:String)=JSONObject().put("type","array")
        .put("items",JSONObject().put("type","number")).put("minItems",3).put("maxItems",3).put("description",description)
    private fun tool(name:String,description:String,properties:JSONObject,required:List<String>)=JSONObject()
        .put("name",name).put("description",description)
        .put("parameters",JSONObject().put("type","object").put("properties",properties).put("required",JSONArray(required)))

    suspend fun toolDefinitions():JSONArray{
        val id=JSONObject().put("type","integer"); val num=JSONObject().put("type","number")
        val str=JSONObject().put("type","string"); val list=JSONArray()
        list.put(tool("create_primitive","Create an editable primitive.",JSONObject()
            .put("type",JSONObject().put("type","string").put("enum",JSONArray(PrimitiveType.values().map{it.apiName})))
            .put("size",num).put("detail",JSONObject().put("type","integer")).put("position",vecSchema("x,y,z")),listOf("type")))
        list.put(tool("create_batch","Create a batch of primitives.",JSONObject().put("objects",JSONObject()
            .put("type","array").put("items",JSONObject().put("type","object").put("properties",JSONObject()
                .put("type",str).put("size",num).put("position",vecSchema("x,y,z")).put("detail",JSONObject().put("type","integer"))))),listOf("objects")))
        list.put(tool("create_asset_preset","Create a native blockout preset.",JSONObject()
            .put("preset",JSONObject().put("type","string").put("enum",JSONArray(listOf("humanoid_blockout","ergonomic_mouse")))),listOf("preset")))
        list.put(tool("create_loft_mesh","Create an editable loft shell.",JSONObject()
            .put("profiles",JSONObject().put("type","array").put("items",num)).put("segments",JSONObject().put("type","integer")),
            listOf("profiles")))
        list.put(tool("duplicate_object","Duplicate an object.",JSONObject().put("object_id",id).put("dx",num).put("dy",num).put("dz",num),listOf("object_id")))
        list.put(tool("clone_object","Compatibility alias to duplicate an object.",JSONObject().put("object_id",id),listOf("object_id")))
        list.put(tool("delete_object","Delete an object.",JSONObject().put("object_id",id),listOf("object_id")))
        list.put(tool("set_transform","Set position, rotation and scale.",JSONObject().put("object_id",id)
            .put("position",vecSchema("position")).put("rotation_degrees",vecSchema("rotation")).put("scale",vecSchema("scale")),
            listOf("object_id","position","rotation_degrees","scale")))
        list.put(tool("move_by","Move an object.",JSONObject().put("object_id",id).put("offset",vecSchema("offset")),listOf("object_id","offset")))
        list.put(tool("rotate_by","Rotate an object.",JSONObject().put("object_id",id).put("degrees",vecSchema("degrees")),listOf("object_id","degrees")))
        list.put(tool("scale_by","Scale an object.",JSONObject().put("object_id",id).put("factor",vecSchema("factor")),listOf("object_id","factor")))
        list.put(tool("set_color","Set RGB base color.",JSONObject().put("object_id",id).put("color",vecSchema("RGB 0..1")),listOf("object_id","color")))
        list.put(tool("set_material","Set PBR material values.",JSONObject().put("object_id",id).put("metallic",num)
            .put("roughness",num).put("clearcoat",num).put("emission",num),listOf("object_id")))
        list.put(tool("set_pbr","Set base color and PBR values.",JSONObject().put("object_id",id).put("color",vecSchema("RGB"))
            .put("metallic",num).put("roughness",num),listOf("object_id","color")))
        list.put(tool("sculpt_mesh","Apply a sculpt displacement stroke.",JSONObject().put("object_id",id)
            .put("center",vecSchema("center")).put("radius",num).put("offset",vecSchema("offset")),listOf("object_id","center","radius","offset")))
        list.put(tool("mesh_operation","Apply a native polygon topology operation. For bevel, set parameter to 1-8 profile segments and value to the signed bevel depth/width.",JSONObject().put("object_id",id)
            .put("operation",str).put("faces",JSONObject().put("type","array").put("items",id))
            .put("parameter",JSONObject().put("type","integer")).put("value",num).put("other_object_id",id),listOf("object_id","operation")))
        list.put(tool("combine_objects","Combine two editable objects.",JSONObject().put("object_id",id).put("other_object_id",id),listOf("object_id","other_object_id")))
        list.put(tool("detach_faces","Detach polygon faces into a new object.",JSONObject().put("object_id",id)
            .put("faces",JSONObject().put("type","array").put("items",id)),listOf("object_id","faces")))
        list.put(tool("separate_faces","Compatibility alias for detach faces.",JSONObject().put("object_id",id)
            .put("face_indices",JSONObject().put("type","array").put("items",id)),listOf("object_id","face_indices")))
        list.put(tool("get_face_shell","Get connected face indices.",JSONObject().put("object_id",id).put("start_face",id),listOf("object_id","start_face")))
        list.put(tool("paint_vertex_region","Paint vertex colors.",JSONObject().put("object_id",id).put("center",vecSchema("center"))
            .put("radius",num).put("r",num).put("g",num).put("b",num).put("opacity",num),listOf("object_id","center","radius","r","g","b")))
        list.put(tool("get_edge_loop_faces","Return the face IDs along a quad edge loop seeded by two vertex IDs.",JSONObject()
            .put("object_id",id).put("vertex_a",id).put("vertex_b",id),listOf("object_id","vertex_a","vertex_b")))
        list.put(tool("knife_cut_face","Cut one polygon face by connecting points halfway along two of its boundary edges. edge_a and edge_b are local edge indices from get_mesh_topology; the cut is undoable.",JSONObject()
            .put("object_id",id).put("face_id",id).put("edge_a",id).put("edge_b",id).put("t",num),
            listOf("object_id","face_id","edge_a","edge_b")))
        list.put(tool("get_mesh_topology","Read topology with pagination.",JSONObject().put("object_id",id).put("offset",id).put("limit",id),listOf("object_id")))
        list.put(tool("get_object_details","Get object metadata.",JSONObject().put("object_id",id),listOf("object_id")))
        list.put(tool("snap_to_ground","Snap an object to the ground.",JSONObject().put("object_id",id),listOf("object_id")))
        list.put(tool("snap_to_grid","Snap an object to a grid.",JSONObject().put("object_id",id).put("step",num),listOf("object_id")))
        list.put(tool("set_environment","Set viewport lighting.",JSONObject().put("sun_direction",vecSchema("direction"))
            .put("sun_color",vecSchema("RGB")).put("sun_intensity",num).put("ambient_strength",num).put("sky_color",vecSchema("RGB")),
            listOf("sun_direction","sun_color","sky_color")))
        list.put(tool("set_display_mode","Set shaded or wireframe viewport display.",JSONObject()
            .put("mode",JSONObject().put("type","string").put("enum",JSONArray(listOf("shaded","wireframe","wireframe_on_shaded")))),listOf("mode")))
        list.put(tool("set_select_mode","Select objects, vertices, edges or faces.",JSONObject()
            .put("mode",JSONObject().put("type","string").put("enum",JSONArray(listOf("object","vertex","edge","face")))),listOf("mode")))
        list.put(tool("pick_component","Pick a viewport component.",JSONObject().put("x",num).put("y",num).put("mode",str).put("object_id",id),listOf("x","y","mode")))
        list.put(tool("undo","Undo the last edit.",JSONObject(),emptyList()))
        list.put(tool("redo","Redo the last undone edit.",JSONObject(),emptyList()))
        list.put(tool("get_scene","Get current scene JSON.",JSONObject(),emptyList()))
        list.put(tool("inspect_scene","Inspect scene objects and bounded mesh topology after edits; use this to verify actual changes and identify invalid or unchanged geometry. This is structured data, not a screenshot.",JSONObject(),emptyList()))
        list.put(tool("get_modeling_extensions","List native tools and research-only entries.",JSONObject(),emptyList()))
        list.put(tool("web_search","Search public references.",JSONObject().put("query",str),listOf("query")))
        list.put(tool("read_webpage","Read a public HTTPS reference.",JSONObject().put("url",str),listOf("url")))
        val external = RemoteMcpConnector.toolDefinitions()
        for (index in 0 until external.length()) list.put(external.getJSONObject(index))
        return list
    }
}
