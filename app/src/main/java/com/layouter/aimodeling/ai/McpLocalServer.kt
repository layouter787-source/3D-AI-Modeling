package com.layouter.aimodeling.ai

import fi.iki.elonen.NanoHTTPD
import kotlinx.coroutines.runBlocking
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

/** Loopback-only MCP Streamable HTTP host. */
class McpLocalServer : NanoHTTPD("127.0.0.1", PORT) {
 override fun serve(session: IHTTPSession): Response {
  if(session.method==Method.OPTIONS)return newFixedLengthResponse(Response.Status.NO_CONTENT,"text/plain","")
  if(session.method!=Method.POST)return newFixedLengthResponse(Response.Status.METHOD_NOT_ALLOWED,"application/json","{}")
  val files=HashMap<String,String>()
  return try {
   session.parseBody(files); val req=JSONObject(files["postData"]?: "")
   val id=req.opt("id"); val method=req.optString("method"); val p=req.optJSONObject("params")?:JSONObject()
   val result=when(method){
    "initialize"->JSONObject().put("protocolVersion","2025-03-26").put("capabilities",JSONObject().put("tools",JSONObject())).put("serverInfo",JSONObject().put("name","manus-3d-ai").put("version","0.3.0"))
    "notifications/initialized","notifications/cancelled"->return newFixedLengthResponse(Response.Status.ACCEPTED,"application/json","")
    "ping"->JSONObject()
    "tools/list"->{val src=runBlocking{ToolDispatcher.toolDefinitions()};val arr=JSONArray();for(i in 0 until src.length()){val t=src.getJSONObject(i);arr.put(JSONObject().put("name",t.getString("name")).put("description",t.optString("description")).put("inputSchema",t.optJSONObject("parameters")?:JSONObject().put("type","object")))};JSONObject().put("tools",arr)}
    "tools/call"->{val name=p.getString("name");val args=p.optJSONObject("arguments")?:JSONObject();val r=runBlocking{ToolDispatcher.execute(ToolCall(UUID.randomUUID().toString(),name,args))};JSONObject().put("content",JSONArray().put(JSONObject().put("type","text").put("text",r.message))).put("isError",!r.ok)}
    else->return errorResponse(id,-32601,"Unknown method: $method")
   }
   newFixedLengthResponse(Response.Status.OK,"application/json; charset=utf-8",JSONObject().put("jsonrpc","2.0").put("id",id).put("result",result).toString())
    .also{it.addHeader("MCP-Protocol-Version","2025-03-26")}
  }catch(e:Exception){errorResponse(null,-32603,e.message?: "MCP internal error")}
 }
 private fun errorResponse(id:Any?,code:Int,msg:String)=newFixedLengthResponse(Response.Status.OK,"application/json",JSONObject().put("jsonrpc","2.0").put("id",id).put("error",JSONObject().put("code",code).put("message",msg)).toString())
 companion object{const val PORT=18765}
}
