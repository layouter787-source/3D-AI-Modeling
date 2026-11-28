package com.layouter.aimodeling.ai

import android.util.Base64
import okhttp3.*
import org.json.JSONObject
import java.security.SecureRandom
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit

/** Outbound WebSocket tunnel: Claude's HTTP requests are replayed against the app's loopback MCP host. */
class McpRelayClient(private val relayBase:String,private val secret:String,private val bearer:String,
 private val localUrl:String="http://127.0.0.1:${McpLocalServer.PORT}/mcp"):WebSocketListener(),AutoCloseable{
 private val client=OkHttpClient.Builder().pingInterval(25,TimeUnit.SECONDS).readTimeout(0,TimeUnit.MILLISECONDS).build()
 private val incoming=ConcurrentHashMap<String,FrameAssembly>()
 @Volatile private var socket:WebSocket?=null
 @Volatile var connected=false;private set
 fun start(){val endpoint=relayBase.trimEnd('/').replaceFirst("https://","wss://").replaceFirst("http://","ws://")+"/device?link="+secret
  socket=client.newWebSocket(Request.Builder().url(endpoint).header("Authorization","Bearer "+bearer).build(),this)}
 override fun onOpen(webSocket:WebSocket,response:Response){connected=true}
 override fun onMessage(webSocket:WebSocket,text:String){val f=runCatching{JSONObject(text)}.getOrNull()?:return;if(f.optString("t")!="req")return
  val id=f.optString("id");val total=f.optInt("n",1).coerceIn(1,4096);val assembly=incoming.getOrPut(id){FrameAssembly(total)}
  assembly.method=f.optString("method","POST");assembly.headers=f.optJSONObject("headers")?:JSONObject()
  if(!assembly.put(f.optInt("i",0),f.optString("data")))return
  incoming.remove(id);Thread{replay(webSocket,id,assembly)}.start()
 }
 private fun replay(ws:WebSocket,id:String,a:FrameAssembly){
  var status=502;var hdr=JSONObject().put("content-type","application/json");var body="""{"error":"local MCP server unavailable"}"""
  try{val request=Request.Builder().url(localUrl);for(k in listOf("accept","content-type","mcp-session-id","mcp-protocol-version","last-event-id"))if(a.headers.has(k))request.header(k,a.headers.optString(k))
   if(a.method=="DELETE")request.delete()else request.post(a.body().toRequestBody("application/json".toMediaType()))
   client.newBuilder().readTimeout(110,TimeUnit.SECONDS).build().newCall(request.build()).execute().use{r->status=r.code;hdr=JSONObject().put("content-type",r.header("content-type","application/json"));r.header("mcp-session-id")?.let{hdr.put("mcp-session-id",it)};body=r.body?.string().orEmpty()}
  }catch(_:Exception){}
  val size=180_000;val n=maxOf(1,(body.length+size-1)/size)
  for(i in 0 until n){val out=JSONObject().put("t","res").put("id",id).put("i",i).put("n",n).put("data",body.substring(i*size,minOf(body.length,(i+1)*size)));if(i==0)out.put("status",status).put("headers",hdr);ws.send(out.toString())}
 }
 override fun onClosed(webSocket:WebSocket,code:Int,reason:String){connected=false}
 override fun onFailure(webSocket:WebSocket,t:Throwable,response:Response?){connected=false}
 override fun close(){connected=false;socket?.close(1000,"link disabled");socket=null;client.dispatcher.cancelAll()}
 private class FrameAssembly(val n:Int){val chunks=arrayOfNulls<String>(n);var method="POST";var headers=JSONObject();@Synchronized fun put(i:Int,s:String):Boolean{if(i !in chunks.indices)return false;chunks[i]=s;return chunks.all{it!=null}};fun body()=chunks.joinToString(""){it.orEmpty()}}
 companion object{fun newSecret()=ByteArray(32).also{SecureRandom().nextBytes(it)}.let{Base64.encodeToString(it,Base64.URL_SAFE or Base64.NO_PADDING or Base64.NO_WRAP)};fun publicUrl(base:String,secret:String)=base.trimEnd('/')+"/c/"+secret+"/mcp"}
}
